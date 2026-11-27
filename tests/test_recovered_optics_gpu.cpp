#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <graphics/graphics.h>
#include <graphics/vec4.h>
#include <iostream>
#include <obs-module.h>
#include <obs.h>
#include <thread>
#include <vector>
namespace {
constexpr unsigned W = 128, H = 128;
using Frame = std::vector<uint8_t>;
Frame fixture(int kind) {
  Frame f(W * H * 4);
  for (unsigned y = 0; y < H; ++y)
    for (unsigned x = 0; x < W; ++x) {
      auto i = (y * W + x) * 4;
      int val =
          kind == 0 ? 0
          : kind == 1
              ? ((int(x) - 64) * (int(x) - 64) + (int(y) - 64) * (int(y) - 64) <
                         81
                     ? 245
                     : 0)
          : kind == 2 ? 100
                      : ((x / 4 + y / 4) % 2 ? 210 : 40);
      f[i] = f[i + 1] = f[i + 2] = val;
      f[i + 3] = 255;
    }
  return f;
}
struct Input {
  gs_texture_t *texture = nullptr;
};
const char *name(void *) { return "Optics regression source"; }
void *create(obs_data_t *, obs_source_t *) { return new Input; }
void destroy(void *data) {
  auto *in = static_cast<Input *>(data);
  obs_enter_graphics();
  if (in->texture)
    gs_texture_destroy(in->texture);
  obs_leave_graphics();
  delete in;
}
uint32_t width(void *) { return W; }
uint32_t height(void *) { return H; }
void render(void *data, gs_effect_t *) {
  auto *in = static_cast<Input *>(data);
  if (!in->texture)
    return;
  auto *e = obs_get_base_effect(OBS_EFFECT_DEFAULT);
  gs_effect_set_texture(gs_effect_get_param_by_name(e, "image"), in->texture);
  while (gs_effect_loop(e, "Draw"))
    gs_draw_sprite(in->texture, 0, W, H);
}
Frame capture(obs_source_t *source, Input *input, const Frame &pixels) {
  obs_enter_graphics();
  if (input->texture)
    gs_texture_destroy(input->texture);
  const uint8_t *p = pixels.data();
  input->texture = gs_texture_create(W, H, GS_RGBA, 1, &p, 0);
  obs_leave_graphics();
  std::this_thread::sleep_for(std::chrono::milliseconds(90));
  obs_enter_graphics();
  auto *rt = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
  auto *st = gs_stagesurface_create(W, H, GS_RGBA);
  Frame out;
  if (rt && st && gs_texrender_begin(rt, W, H)) {
    vec4 clear{};
    gs_clear(GS_CLEAR_COLOR, &clear, 0, 0);
    gs_blend_state_push();
    gs_reset_blend_state();
    gs_enable_blending(false);
    gs_projection_push();
    gs_matrix_push();
    gs_matrix_identity();
    gs_ortho(0, W, 0, H, -100, 100);
    obs_source_video_render(source);
    gs_matrix_pop();
    gs_projection_pop();
    gs_blend_state_pop();
    gs_texrender_end(rt);
    gs_stage_texture(st, gs_texrender_get_texture(rt));
    uint8_t *m = nullptr;
    uint32_t stride = 0;
    if (gs_stagesurface_map(st, &m, &stride)) {
      out.resize(W * H * 4);
      for (unsigned y = 0; y < H; ++y)
        std::copy_n(m + y * stride, W * 4, out.data() + y * W * 4);
      gs_stagesurface_unmap(st);
    }
  }
  if (st)
    gs_stagesurface_destroy(st);
  if (rt)
    gs_texrender_destroy(rt);
  obs_leave_graphics();
  return out;
}
size_t changed(const Frame &a, const Frame &b, int xmin = 0, int xmax = W,
               int ymin = 0, int ymax = H) {
  if (a.size() != W * H * 4 || b.size() != a.size())
    return 0;
  size_t n = 0;
  for (int y = ymin; y < ymax; ++y)
    for (int x = xmin; x < xmax; ++x) {
      auto i = (y * W + x) * 4;
      if (std::abs(int(a[i]) - int(b[i])) +
              std::abs(int(a[i + 1]) - int(b[i + 1])) +
              std::abs(int(a[i + 2]) - int(b[i + 2])) >
          3)
        ++n;
    }
  return n;
}
long energy(const Frame &f) {
  long n = 0;
  if (f.size() != W * H * 4)
    return -1;
  for (int y = 0; y < int(H); ++y)
    for (int x = 0; x < int(W); ++x) {
      auto i = (y * W + x) * 4;
      if ((x - 64) * (x - 64) + (y - 64) * (y - 64) >= 100)
        n += f[i] + f[i + 1] + f[i + 2];
    }
  return n;
}
} // namespace
int main(int argc, char **argv) {
  if (argc != 4 || !obs_startup("en-US", nullptr, nullptr))
    return 2;
  obs_video_info v{};
  v.graphics_module = "libobs-opengl";
  v.fps_num = 30;
  v.fps_den = 1;
  v.base_width = v.output_width = W;
  v.base_height = v.output_height = H;
  v.output_format = VIDEO_FORMAT_RGBA;
  v.colorspace = VIDEO_CS_DEFAULT;
  v.range = VIDEO_RANGE_DEFAULT;
  v.scale_type = OBS_SCALE_DISABLE;
  if (obs_reset_video(&v) != OBS_VIDEO_SUCCESS) {
    obs_shutdown();
    return 2;
  }
  obs_module_t *m = nullptr;
  bool ok = obs_open_module(&m, argv[2], argv[3]) == MODULE_SUCCESS && m &&
            obs_init_module(m);
  auto *props = ok ? obs_get_source_properties("emulens") : nullptr;
  auto *modes = props ? obs_properties_get(props, "mode") : nullptr;
  bool ids = false;
  if (modes && obs_property_list_item_count(modes) == 19) {
    ids = true;
    for (int i = 0; i < 19; ++i)
      ids = ids && obs_property_list_item_int(modes, i) == i;
  }
  ok = ok && ids && obs_properties_get(props, "diff_highlight_threshold") &&
       obs_properties_get(props, "dio_angle");
  if (props)
    obs_properties_destroy(props);
  if (!ok) {
    std::cerr
        << "FAIL: expected append-only 19-mode optical property surface\n";
    obs_shutdown();
    return 1;
  }
  obs_source_info info{};
  info.id = "recovery_test_input";
  info.type = OBS_SOURCE_TYPE_INPUT;
  info.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW;
  info.get_name = name;
  info.create = create;
  info.destroy = destroy;
  info.get_width = width;
  info.get_height = height;
  info.video_render = render;
  obs_register_source(&info);
  auto *source = obs_source_create_private(info.id, "Fixture", nullptr);
  auto *filter = obs_source_create_private("emulens", "Optics", nullptr);
  if (!source || !filter) {
    if (source)
      obs_source_release(source);
    if (filter)
      obs_source_release(filter);
    obs_shutdown();
    return 2;
  }
  auto *input = static_cast<Input *>(obs_obj_get_data(source));
  obs_source_filter_add(source, filter);
  obs_source_inc_showing(source);
  obs_source_inc_active(source);
  auto *s = obs_data_create();
  auto set = [&](const char *k, double val) { obs_data_set_double(s, k, val); };
  auto mode = [&](int n) { obs_data_set_int(s, "mode", n); };
  auto draw = [&](const Frame &f) {
    obs_source_update(filter, s);
    return capture(source, input, f);
  };
  auto light = fixture(1), dark = fixture(0), flat = fixture(2),
       grid = fixture(3);
  mode(17);
  set("diff_strength", 0);
  set("diff_contrast", 0);
  set("diff_radius", 8);
  set("diff_halation", 1);
  set("diff_highlight_threshold", .6);
  auto off = draw(light);
  set("diff_strength", 1);
  auto on = draw(light);
  set("diff_highlight_threshold", 1);
  auto high = draw(light);
  set("diff_highlight_threshold", .6);
  auto black = draw(dark), uniform = draw(flat);
  obs_data_set_int(s, "diff_type", 3);
  auto digital = draw(light);
  set("diff_halation", 0);
  auto digital_off = draw(light);
  auto digital_grid = draw(grid);
  set("diff_radius", 0);
  auto digital_zero = draw(grid);
  set("diff_radius", 8);
  std::cout << "DIGITAL_DETAIL changed=" << changed(grid, digital_grid)
            << " zero_radius=" << (digital_zero == grid) << "\n";
  ok = ok && changed(grid, digital_grid) > 1000 && digital_zero == grid;
  obs_data_set_int(s, "diff_type", 4);
  auto net = draw(grid);
  set("diff_net_angle", 45);
  auto rotated_net = draw(grid);
  std::cout << "DIFFUSION energy=" << energy(on)
            << " high_threshold=" << energy(high) << " off=" << energy(off)
            << " net_rotation_delta=" << changed(net, rotated_net)
            << " identity=" << (off == light) << " dark=" << (black == dark)
            << " flat=" << (uniform == flat)
            << " digital=" << (digital == digital_off) << "\n";
  ok = ok && off == light && black == dark && uniform == flat &&
       energy(on) > energy(high) + 100 && energy(high) > energy(off) &&
       digital == digital_off && changed(net, rotated_net) > 20;
  mode(18);
  obs_data_set_int(s, "dio_type", 1);
  set("dio_strength", 0);
  set("dio_blur", 8);
  set("dio_magnification", 0);
  set("dio_chroma", 0);
  set("dio_feather", .01);
  set("dio_position", .5);
  set("dio_angle", 0);
  auto dio_off = draw(grid);
  set("dio_strength", 1);
  auto split = draw(grid);
  set("dio_angle", 90);
  auto rotated = draw(grid);
  set("dio_angle", 0);
  obs_data_set_int(s, "dio_type", 2);
  set("dio_width", .3);
  auto strip = draw(grid);
  obs_data_set_int(s, "dio_type", 3);
  auto box = draw(grid);
  obs_data_set_int(s, "dio_type", 0);
  auto full = draw(grid);
  set("dio_blur", 0);
  set("dio_magnification", 0);
  set("dio_chroma", 0);
  auto identity = draw(grid);
  auto left = changed(grid, split, 0, 56),
       right = changed(grid, split, 72, 128),
       top = changed(grid, rotated, 0, W, 0, 56),
       bottom = changed(grid, rotated, 0, W, 72, 128);
  std::cout << "DIOPTER split_left=" << left << " right=" << right
            << " rotated_top=" << top << " bottom=" << bottom
            << " strip_center=" << changed(grid, strip, 0, W, 54, 74)
            << " box_center=" << changed(grid, box, 0, W, 54, 74)
            << " full=" << changed(grid, full)
            << " identity=" << (identity == grid)
            << " disabled=" << (dio_off == grid) << "\n";
  ok = ok && dio_off == grid && identity == grid && left == 0 && right > 500 &&
       std::min(top, bottom) == 0 && std::max(top, bottom) > 500 &&
       changed(grid, strip, 0, W, 54, 74) > 500 &&
       changed(grid, box, 0, W, 54, 74) == 0 && changed(grid, full) > 1000;
  // Actual Film Grit strategy and preset settings, not just a standalone
  // shader.
  mode(7);
  for (auto n : {"grit_dust", "grit_scratches", "grit_noise", "grit_flares",
                 "grit_instability", "grit_brightness", "grit_opacity"})
    set(n, 0);
  set("grit_contrast", 1);
  set("grit_saturation", 1);
  obs_data_set_int(s, "grit_blend_mode", 4);
  set("grit_halation_radius", 6);
  set("grit_halation_threshold", .6);
  set("grit_halation", 0);
  auto grit_off = draw(light);
  set("grit_halation", 1);
  auto grit_on = draw(light);
  std::cout << "GRIT_CHAIN off=" << (grit_off == light)
            << " changed=" << changed(grit_off, grit_on) << "\n";
  ok = ok && grit_off == light && changed(grit_off, grit_on) > 30;
  size_t alpha = 0;
  for (auto *f : {&on, &high, &black, &uniform, &net, &rotated_net, &split,
                  &rotated, &strip, &box, &full}) {
    ok = ok && f->size() == W * H * 4;
    for (size_t i = 3; i < f->size(); i += 4)
      alpha += (*f)[i] != 255;
  }
  ok = ok && alpha == 0;
  obs_data_release(s);
  obs_source_dec_active(source);
  obs_source_dec_showing(source);
  obs_source_filter_remove(source, filter);
  obs_source_release(filter);
  obs_source_release(source);
  obs_shutdown();
  std::cout << "RECOVERED_OPTICS_GPU_PASS=" << ok << " alpha_errors=" << alpha
            << "\n";
  return ok ? 0 : 1;
}
