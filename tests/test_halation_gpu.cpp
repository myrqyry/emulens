#include <algorithm>
#include <cmath>
#include <cstdint>
#include <graphics/graphics.h>
#include <graphics/vec4.h>
#include <iostream>
#include <obs-module.h>
#include <obs.h>
#include <vector>
namespace {
constexpr unsigned W = 128, H = 128;
using Frame = std::vector<uint8_t>;
Frame input(int light) {
  Frame f(W * H * 4);
  for (unsigned y = 0; y < H; ++y)
    for (unsigned x = 0; x < W; ++x) {
      auto i = (y * W + x) * 4;
      bool spot =
          (int(x) - 64) * (int(x) - 64) + (int(y) - 64) * (int(y) - 64) < 64;
      f[i] = f[i + 1] = f[i + 2] = spot ? light : 0;
      f[i + 3] = 255;
    }
  return f;
}
Frame render(gs_effect_t *e, const Frame &f, float amount, float radius,
             float threshold) {
  const uint8_t *p = f.data();
  auto *tex = gs_texture_create(W, H, GS_RGBA, 1, &p, 0);
  auto *rt = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
  auto *st = gs_stagesurface_create(W, H, GS_RGBA);
  Frame out;
  auto set = [&](const char *n, float v) {
    auto *a = gs_effect_get_param_by_name(e, n);
    if (!a)
      return false;
    gs_effect_set_float(a, v);
    return true;
  };
  bool controls = set("grit_halation", amount) &&
                  set("grit_halation_radius", radius) &&
                  set("grit_halation_threshold", threshold);
  for (auto n : {"grit_dust", "grit_scratches", "grit_noise", "grit_flares",
                 "grit_instability", "grit_brightness", "grit_opacity"})
    set(n, 0);
  set("grit_contrast", 1);
  set("grit_saturation", 1);
  set("grit_blend_mode", 4);
  gs_effect_set_texture(gs_effect_get_param_by_name(e, "image"), tex);
  vec4 step{1.f / W, 1.f / H, 0, 0};
  gs_effect_set_vec4(gs_effect_get_param_by_name(e, "uv_pixel_interval"),
                     &step);
  if (controls && gs_texrender_begin(rt, W, H)) {
    vec4 clear{};
    gs_clear(GS_CLEAR_COLOR, &clear, 0, 0);
    gs_blend_state_push();
    gs_enable_blending(false);
    gs_projection_push();
    gs_matrix_push();
    gs_matrix_identity();
    gs_ortho(0, W, 0, H, -100, 100);
    while (gs_effect_loop(e, "DrawGrit"))
      gs_draw_sprite(nullptr, 0, W, H);
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
  gs_stagesurface_destroy(st);
  gs_texrender_destroy(rt);
  gs_texture_destroy(tex);
  return out;
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
  ok = ok && props && obs_properties_get(props, "grit_halation") &&
       obs_properties_get(props, "grit_halation_radius") &&
       obs_properties_get(props, "grit_halation_threshold");
  if (props)
    obs_properties_destroy(props);
  auto *defaults = obs_get_source_defaults("emulens");
  ok = ok && defaults && obs_data_get_double(defaults, "grit_halation") == 0.;
  if (defaults)
    obs_data_release(defaults);
  obs_enter_graphics();
  gs_set_cull_mode(GS_NEITHER);
  char *err = nullptr;
  auto *e = gs_effect_create_from_file(argv[1], &err);
  if (err) {
    std::cerr << err;
    bfree(err);
  }
  if (e) {
    auto bright = input(255), dim = input(100), black = input(0);
    auto off = render(e, bright, 0, 6, .7), on = render(e, bright, 1, 6, .7),
         wide = render(e, bright, 1, 12, .7), dark = render(e, black, 1, 6, .7),
         low = render(e, dim, 1, 6, .7), lowoff = render(e, dim, 0, 6, .7),
         zero = render(e, bright, 1, 0, .7);
    size_t red = 0, far = 0, alpha = 0;
    long long energy = 0;
    if (on.size() == bright.size() && wide.size() == bright.size()) {
      for (unsigned y = 0; y < H; ++y)
        for (unsigned x = 0; x < W; ++x) {
          auto i = (y * W + x) * 4;
          int r2 =
              (int(x) - 64) * (int(x) - 64) + (int(y) - 64) * (int(y) - 64);
          if (r2 >= 64 && on[i] > on[i + 1] + 2 && on[i] > on[i + 2] + 2)
            ++red;
          if (r2 > 196 && wide[i] > on[i] + 2)
            ++far;
          energy += on[i];
          alpha += on[i + 3] != 255;
        }
    }
    std::cout << "halation_red_neighbor_pixels=" << red
              << " wide_radius_delta=" << far << " alpha_errors=" << alpha
              << " energy=" << energy << "\n";
    ok = ok && off == bright && zero == bright && dark == black &&
         low == lowoff && red > 30 && far > 20 && alpha == 0;
    gs_effect_destroy(e);
  } else
    ok = false;
  obs_leave_graphics();
  obs_shutdown();
  std::cout << "HALATION_GPU_PASS=" << ok << "\n";
  return ok ? 0 : 1;
}
