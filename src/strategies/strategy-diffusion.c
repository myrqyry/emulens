#include "../obs-utils.h"

void diffusion_cache(struct lens_data *filter, gs_effect_t *effect) {
  GET_PARAM_SAFE(filter->mode_params.diffusion.type, effect, P_DIFF_TYPE);
  GET_PARAM_SAFE(filter->mode_params.diffusion.strength, effect,
                 P_DIFF_STRENGTH);
  GET_PARAM_SAFE(filter->mode_params.diffusion.radius, effect, P_DIFF_RADIUS);
  GET_PARAM_SAFE(filter->mode_params.diffusion.halation, effect,
                 P_DIFF_HALATION);
  GET_PARAM_SAFE(filter->mode_params.diffusion.contrast, effect,
                 P_DIFF_CONTRAST);
  GET_PARAM_SAFE(filter->mode_params.diffusion.warmth, effect, P_DIFF_WARMTH);
  GET_PARAM_SAFE(filter->mode_params.diffusion.highlight_threshold, effect,
                 P_DIFF_HIGHLIGHT_THRESHOLD);
  GET_PARAM_SAFE(filter->mode_params.diffusion.net_density, effect,
                 P_DIFF_NET_DENSITY);
  GET_PARAM_SAFE(filter->mode_params.diffusion.net_angle, effect,
                 P_DIFF_NET_ANGLE);
}

void diffusion_update(struct lens_data *filter, obs_data_t *settings) {
  SET_CLAMPED_INT_AS_FLOAT(filter->mode_params.diffusion.type, settings,
                           P_DIFF_TYPE, 0.0f, 4.0f);
  SET_CLAMPED_DOUBLE(filter->mode_params.diffusion.strength, settings,
                     P_DIFF_STRENGTH, 0.0f, 1.0f);
  SET_CLAMPED_DOUBLE(filter->mode_params.diffusion.radius, settings,
                     P_DIFF_RADIUS, 0.0f, 30.0f);
  SET_CLAMPED_DOUBLE(filter->mode_params.diffusion.halation, settings,
                     P_DIFF_HALATION, 0.0f, 2.0f);
  SET_CLAMPED_DOUBLE(filter->mode_params.diffusion.contrast, settings,
                     P_DIFF_CONTRAST, 0.0f, 1.0f);
  SET_CLAMPED_DOUBLE(filter->mode_params.diffusion.warmth, settings,
                     P_DIFF_WARMTH, 0.0f, 1.0f);
  SET_CLAMPED_DOUBLE(filter->mode_params.diffusion.highlight_threshold,
                     settings, P_DIFF_HIGHLIGHT_THRESHOLD, 0.0f, 1.0f);
  SET_CLAMPED_DOUBLE(filter->mode_params.diffusion.net_density, settings,
                     P_DIFF_NET_DENSITY, 20.0f, 240.0f);
  SET_CLAMPED_DOUBLE(filter->mode_params.diffusion.net_angle, settings,
                     P_DIFF_NET_ANGLE, -180.0f, 180.0f);
}

gs_texture_t *diffusion_render(struct lens_data *filter, gs_effect_t *effect,
                               gs_texture_t *source_tex, uint32_t width,
                               uint32_t height) {
  UNUSED_PARAMETER(effect);
  return render_passthrough_to_texrender(
      filter->texrenders[emulens_atomic_load(&filter->cur_write)],
      filter->tech.diffusion, filter, source_tex, width, height);
}

void diffusion_destroy_cpu(struct lens_data *filter) { (void)filter; }
void diffusion_destroy(struct lens_data *filter) { (void)filter; }

void diffusion_get_defaults(obs_data_t *settings) {
  obs_data_set_default_int(settings, P_DIFF_TYPE, 0);
  obs_data_set_default_double(settings, P_DIFF_STRENGTH, 0.28);
  obs_data_set_default_double(settings, P_DIFF_RADIUS, 5.0);
  obs_data_set_default_double(settings, P_DIFF_HALATION, 0.32);
  obs_data_set_default_double(settings, P_DIFF_CONTRAST, 0.12);
  obs_data_set_default_double(settings, P_DIFF_WARMTH, 0.08);
  obs_data_set_default_double(settings, P_DIFF_HIGHLIGHT_THRESHOLD, 0.68);
  obs_data_set_default_double(settings, P_DIFF_NET_DENSITY, 95.0);
  obs_data_set_default_double(settings, P_DIFF_NET_ANGLE, 0.0);
}

void diffusion_populate_presets(obs_property_t *list) {
  ADD_PRESET_ENTRY(list, "DiffusionDefault", "default_diffusion",
                   "DiffusionDefaultDesc");
  ADD_PRESET_ENTRY(list, "DiffusionBlackProMist", "diffusion_black_pro_mist",
                   "DiffusionBlackProMistDesc");
  ADD_PRESET_ENTRY(list, "DiffusionSatin", "diffusion_satin",
                   "DiffusionSatinDesc");
  ADD_PRESET_ENTRY(list, "DiffusionDigitalFX", "diffusion_digital_fx",
                   "DiffusionDigitalFXDesc");
  ADD_PRESET_ENTRY(list, "DiffusionBlackNet", "diffusion_black_net",
                   "DiffusionBlackNetDesc");
}
