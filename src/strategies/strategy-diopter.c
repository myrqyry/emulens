#include "../obs-utils.h"

void diopter_cache(struct lens_data *filter, gs_effect_t *effect) {
  GET_PARAM_SAFE(filter->mode_params.diopter.type, effect, P_DIO_TYPE);
  GET_PARAM_SAFE(filter->mode_params.diopter.strength, effect, P_DIO_STRENGTH);
  GET_PARAM_SAFE(filter->mode_params.diopter.position, effect, P_DIO_POSITION);
  GET_PARAM_SAFE(filter->mode_params.diopter.width, effect, P_DIO_WIDTH);
  GET_PARAM_SAFE(filter->mode_params.diopter.angle, effect, P_DIO_ANGLE);
  GET_PARAM_SAFE(filter->mode_params.diopter.feather, effect, P_DIO_FEATHER);
  GET_PARAM_SAFE(filter->mode_params.diopter.magnification, effect,
                 P_DIO_MAGNIFICATION);
  GET_PARAM_SAFE(filter->mode_params.diopter.blur, effect, P_DIO_BLUR);
  GET_PARAM_SAFE(filter->mode_params.diopter.chroma, effect, P_DIO_CHROMA);
}

void diopter_update(struct lens_data *filter, obs_data_t *settings) {
  SET_CLAMPED_INT_AS_FLOAT(filter->mode_params.diopter.type, settings,
                           P_DIO_TYPE, 0.0f, 3.0f);
  SET_CLAMPED_DOUBLE(filter->mode_params.diopter.strength, settings,
                     P_DIO_STRENGTH, 0.0f, 1.0f);
  SET_CLAMPED_DOUBLE(filter->mode_params.diopter.position, settings,
                     P_DIO_POSITION, 0.0f, 1.0f);
  SET_CLAMPED_DOUBLE(filter->mode_params.diopter.width, settings, P_DIO_WIDTH,
                     0.02f, 1.0f);
  SET_CLAMPED_DOUBLE(filter->mode_params.diopter.angle, settings, P_DIO_ANGLE,
                     -180.0f, 180.0f);
  SET_CLAMPED_DOUBLE(filter->mode_params.diopter.feather, settings,
                     P_DIO_FEATHER, 0.0f, 0.25f);
  SET_CLAMPED_DOUBLE(filter->mode_params.diopter.magnification, settings,
                     P_DIO_MAGNIFICATION, 0.0f, 1.0f);
  SET_CLAMPED_DOUBLE(filter->mode_params.diopter.blur, settings, P_DIO_BLUR,
                     0.0f, 12.0f);
  SET_CLAMPED_DOUBLE(filter->mode_params.diopter.chroma, settings, P_DIO_CHROMA,
                     0.0f, 1.0f);
}

gs_texture_t *diopter_render(struct lens_data *filter, gs_effect_t *effect,
                             gs_texture_t *source_tex, uint32_t width,
                             uint32_t height) {
  UNUSED_PARAMETER(effect);
  return render_passthrough_to_texrender(
      filter->texrenders[emulens_atomic_load(&filter->cur_write)],
      filter->tech.diopter, filter, source_tex, width, height);
}

void diopter_destroy_cpu(struct lens_data *filter) { (void)filter; }
void diopter_destroy(struct lens_data *filter) { (void)filter; }

void diopter_get_defaults(obs_data_t *settings) {
  obs_data_set_default_int(settings, P_DIO_TYPE, 1);
  obs_data_set_default_double(settings, P_DIO_STRENGTH, 0.45);
  obs_data_set_default_double(settings, P_DIO_POSITION, 0.50);
  obs_data_set_default_double(settings, P_DIO_WIDTH, 0.28);
  obs_data_set_default_double(settings, P_DIO_ANGLE, 0.0);
  obs_data_set_default_double(settings, P_DIO_FEATHER, 0.025);
  obs_data_set_default_double(settings, P_DIO_MAGNIFICATION, 0.16);
  obs_data_set_default_double(settings, P_DIO_BLUR, 2.4);
  obs_data_set_default_double(settings, P_DIO_CHROMA, 0.04);
}

void diopter_populate_presets(obs_property_t *list) {
  ADD_PRESET_ENTRY(list, "DiopterDefault", "default_diopter",
                   "DiopterDefaultDesc");
  ADD_PRESET_ENTRY(list, "DiopterFull", "diopter_full_field",
                   "DiopterFullDesc");
  ADD_PRESET_ENTRY(list, "DiopterStrip", "diopter_strip", "DiopterStripDesc");
  ADD_PRESET_ENTRY(list, "DiopterLetterbox", "diopter_letterbox",
                   "DiopterLetterboxDesc");
  ADD_PRESET_ENTRY(list, "DiopterRotatedSplit", "diopter_rotated_split",
                   "DiopterRotatedSplitDesc");
}
