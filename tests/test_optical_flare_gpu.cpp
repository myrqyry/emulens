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
constexpr uint32_t W = 128, H = 128;
using Frame = std::vector<uint8_t>;

Frame make_frame(bool lamp) {
  Frame pixels(size_t(W) * H * 4, 0);
  for (uint32_t y = 0; y < H; ++y)
    for (uint32_t x = 0; x < W; ++x) {
      const size_t idx = (size_t(y) * W + x) * 4;
      if (lamp &&
          (int(x) - 36) * (int(x) - 36) + (int(y) - 64) * (int(y) - 64) < 49) {
        pixels[idx + 0] = 245;
        pixels[idx + 1] = 226;
        pixels[idx + 2] = 205;
      }
      pixels[idx + 3] = 255;
    }
  return pixels;
}

Frame draw(gs_effect_t *effect, gs_texture_t *base, gs_texture_t *highlights,
           gs_texture_t *blur, gs_texrender_t *target, gs_stagesurf_t *stage,
           float amount, float elements, float dispersion,
           bool starburst = false) {
  auto bind_texture = [&](const char *key, gs_texture_t *tex) -> bool {
    gs_eparam_t *p = gs_effect_get_param_by_name(effect, key);
    if (!p)
      return false;
    gs_effect_set_texture(p, tex);
    return true;
  };
  auto bind_float = [&](const char *key, float val) -> bool {
    gs_eparam_t *p = gs_effect_get_param_by_name(effect, key);
    if (!p)
      return false;
    gs_effect_set_float(p, val);
    return true;
  };
  if (!bind_texture("image", base))
    return {};
  if (starburst) {
    if (!bind_float("sb_chroma", dispersion) ||
        !bind_float("sb_threshold", 0.38f) ||
        !bind_float("sb_intensity", 1.8f) || !bind_float("sb_points", 4.f) ||
        !bind_float("sb_ray_length", 0.32f) || !bind_float("sb_samples", 8.f) ||
        !bind_float("sb_rot_speed", 0.f) || !bind_float("sb_colorize", 0.f))
      return {};
  } else if (!bind_texture("highlight_image", highlights) ||
             !bind_texture("blurred_image", blur) ||
             !bind_float("bloom_ghost_amount", amount) ||
             !bind_float("bloom_ghost_scale", 2.f) ||
             !bind_float("bloom_ghost_elements", elements) ||
             !bind_float("bloom_ghost_dispersion", dispersion) ||
             !bind_float("bloom_intensity", 0.f) ||
             !bind_float("bloom_dust_amount", 0.f) ||
             !bind_float("bloom_haze_amount", 0.f) ||
             !bind_float("bloom_refract_amount", 0.f))
    return {};
  auto *pixel = gs_effect_get_param_by_name(effect, "uv_pixel_interval");
  if (!pixel)
    return {};
  struct vec4 step {
    1.f / W, 1.f / H, 0.f, 0.f
  };
  gs_effect_set_vec4(pixel, &step);

  gs_texrender_reset(target);
  if (!gs_texrender_begin(target, W, H))
    return {};
  struct vec4 clear {};
  gs_clear(GS_CLEAR_COLOR, &clear, 0.f, 0);
  gs_blend_state_push();
  gs_reset_blend_state();
  gs_enable_blending(false);
  gs_blend_function(GS_BLEND_ONE, GS_BLEND_ZERO);
  gs_projection_push();
  gs_matrix_push();
  gs_matrix_identity();
  gs_ortho(0.f, float(W), 0.f, float(H), -100.f, 100.f);
  int passes = 0;
  auto *tech = gs_effect_get_technique(effect, starburst ? "DrawStarburst"
                                                         : "DrawBloomCombine");
  if (tech) {
    const size_t count = gs_technique_begin(tech);
    for (size_t i = 0; i < count; ++i) {
      if (gs_technique_begin_pass(tech, uint32_t(i))) {
        ++passes;
        gs_draw_sprite(nullptr, 0, W, H);
        gs_technique_end_pass(tech);
      }
    }
    gs_technique_end(tech);
  }
  gs_matrix_pop();
  gs_projection_pop();
  gs_blend_state_pop();
  gs_texrender_end(target);
  if (passes != 1)
    return {};
  gs_stage_texture(stage, gs_texrender_get_texture(target));
  uint8_t *mapped = nullptr;
  uint32_t pitch = 0;
  if (!gs_stagesurface_map(stage, &mapped, &pitch))
    return {};
  Frame output(size_t(W) * H * 4);
  if (pitch < W * 4)
    output.clear();
  else
    for (uint32_t y = 0; y < H; ++y)
      std::copy_n(mapped + size_t(y) * pitch, W * 4,
                  output.data() + size_t(y) * W * 4);
  gs_stagesurface_unmap(stage);
  return output;
}

size_t count_changed(const Frame &a, const Frame &b) {
  if (a.size() != b.size() || a.empty())
    return 0;
  size_t changed = 0;
  for (size_t i = 0; i < a.size(); i += 4)
    if (std::abs(int(a[i]) - int(b[i])) +
            std::abs(int(a[i + 1]) - int(b[i + 1])) +
            std::abs(int(a[i + 2]) - int(b[i + 2])) >
        4)
      ++changed;
  return changed;
}

size_t glow_near(const Frame &f, int cx, int cy) {
  size_t sum = 0;
  if (f.empty())
    return 0;
  for (int y = cy - 8; y <= cy + 8; ++y)
    for (int x = cx - 8; x <= cx + 8; ++x) {
      const auto off = (size_t(y) * W + x) * 4;
      sum += f[off] + f[off + 1] + f[off + 2];
    }
  return sum;
}
} // namespace

int main(int argc, char **argv) {
  if (argc != 4)
    return 2;
  if (!obs_startup("en-US", nullptr, nullptr))
    return 2;
  obs_video_info settings{};
  settings.graphics_module = "libobs-opengl";
  settings.fps_num = 30;
  settings.fps_den = 1;
  settings.base_width = W;
  settings.base_height = H;
  settings.output_width = W;
  settings.output_height = H;
  settings.output_format = VIDEO_FORMAT_RGBA;
  settings.colorspace = VIDEO_CS_DEFAULT;
  settings.range = VIDEO_RANGE_DEFAULT;
  settings.scale_type = OBS_SCALE_DISABLE;
  if (obs_reset_video(&settings) != OBS_VIDEO_SUCCESS) {
    obs_shutdown();
    return 2;
  }

  // Compile/load the actual staged OBS module and inspect its property
  // surface; shader-only success would not prove native UI wiring.
  obs_module_t *module = nullptr;
  int opened = obs_open_module(&module, argv[2], argv[3]);
  if (opened != MODULE_SUCCESS || !module || !obs_init_module(module)) {
    std::cerr << "FAIL: staged EmuLens module registration: " << opened << "\n";
    obs_shutdown();
    return 1;
  }
  auto *properties = obs_get_source_properties("emulens");
  bool controls = properties &&
                  obs_properties_get(properties, "bloom_ghost_elements") &&
                  obs_properties_get(properties, "bloom_ghost_dispersion");
  if (properties)
    obs_properties_destroy(properties);
  if (!controls) {
    std::cerr << "FAIL: native Bloom ghost controls missing\n";
    obs_shutdown();
    return 1;
  }

  bool ok = false;
  obs_enter_graphics();
  auto prev_cull = gs_get_cull_mode();
  gs_set_cull_mode(GS_NEITHER);
  char *errors = nullptr;
  auto *effect = gs_effect_create_from_file(argv[1], &errors);
  if (errors) {
    std::cerr << errors << "\n";
    bfree(errors);
  }
  if (effect) {
    auto bright = make_frame(true);
    auto black = make_frame(false);
    const uint8_t *bptr = bright.data(), *nptr = black.data();
    auto *b = gs_texture_create(W, H, GS_RGBA, 1, &bptr, 0);
    auto *n = gs_texture_create(W, H, GS_RGBA, 1, &nptr, 0);
    auto *rt = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
    auto *stage = gs_stagesurface_create(W, H, GS_RGBA);
    if (b && n && rt && stage) {
      auto off = draw(effect, n, b, n, rt, stage, 0.f, 4.f, 0.f);
      auto one = draw(effect, n, b, n, rt, stage, 1.f, 1.f, 0.f);
      auto four = draw(effect, n, b, n, rt, stage, 1.f, 4.f, 0.f);
      auto rgb = draw(effect, n, b, n, rt, stage, 1.f, 4.f, 1.f);
      auto dark = draw(effect, n, n, n, rt, stage, 1.f, 4.f, 1.f);
      auto star_off = draw(effect, b, b, n, rt, stage, 1.f, 4.f, 0.f, true);
      auto star_prism = draw(effect, b, b, n, rt, stage, 1.f, 4.f, 1.8f, true);
      size_t alpha_errors = 0;
      for (const auto *f :
           {&off, &one, &four, &rgb, &dark, &star_off, &star_prism})
        for (size_t i = 3; i < f->size(); i += 4)
          alpha_errors += ((*f)[i] != 255);
      const auto ghost = count_changed(off, four);
      const auto detail = count_changed(one, four);
      const auto chroma = count_changed(four, rgb);
      const auto dark_spill = count_changed(off, dark);
      const auto star_spectral = count_changed(star_off, star_prism);
      const auto mirrored = glow_near(four, 93, 64);
      std::cout << "ghost_pixels=" << ghost << " multi_element_delta=" << detail
                << " spectral_delta=" << chroma << " mirror_energy=" << mirrored
                << " dark_spill=" << dark_spill
                << " star_spectral=" << star_spectral
                << " alpha_errors=" << alpha_errors << "\n";
      ok = off.size() == size_t(W) * H * 4 && ghost > 40 && detail > 15 &&
           chroma > 5 && mirrored > 300 && dark_spill == 0 &&
           star_spectral > 20 && alpha_errors == 0;
    }
    if (stage)
      gs_stagesurface_destroy(stage);
    if (rt)
      gs_texrender_destroy(rt);
    if (b)
      gs_texture_destroy(b);
    if (n)
      gs_texture_destroy(n);
    gs_effect_destroy(effect);
  }
  gs_set_cull_mode(prev_cull);
  obs_leave_graphics();
  obs_shutdown();
  if (!ok) {
    std::cerr << "FAIL: optical ghost GPU output\n";
    return 1;
  }
  std::cout << "PASS: real OBS/OpenGL highlight ghosts and alpha\n";
}
