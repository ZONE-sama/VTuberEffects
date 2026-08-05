#include "dynamic-avatar-lighting.h"
#include "preset-file-dialog.h"
#include <plugin-support.h>

#include <graphics/graphics.h>
#include <graphics/vec2.h>
#include <graphics/vec4.h>
#include <util/dstr.h>

#include <math.h>
#include <stdio.h>
#include <string.h>

#define FILTER_ID "dynamic_avatar_lighting_filter"
#define KEY_COUNT 10
#define BLUR_LEVEL_COUNT 8

#define S_ENVIRONMENT "environment_source"
#define S_RIM_ENVIRONMENT "rim_environment_source"
#define S_ENVIRONMENT_WARNING "environment_warning"
#define S_PRESET_STATUS "preset_status"
#define S_AMBIENT_ENABLED "ambient_enabled"
#define S_AMBIENT_BASE "ambient_base"
#define S_AMBIENT_AMOUNT "ambient_amount"
#define S_AMBIENT_BLUR "ambient_blur"
#define S_EXPOSURE_ENABLED "exposure_enabled"
#define S_EXPOSURE_AFFECTS_RIM "exposure_affects_rim"
#define S_EXPOSURE_TARGET "exposure_target"
#define S_EXPOSURE_STRENGTH "exposure_strength"
#define S_EXPOSURE_MIN "exposure_min"
#define S_EXPOSURE_MAX "exposure_max"
#define S_COLOR_SATURATION "color_saturation"
#define S_COLOR_VIBRANCE "color_vibrance"
#define S_COLOR_CONTRAST "color_contrast"
#define S_COLOR_LIMIT "color_limit"
#define S_RIM_ENABLED "rim_enabled"
#define S_RIM_AMOUNT "rim_amount"
#define S_RIM_COLOR_AMOUNT "rim_color_amount"
#define S_RIM_LAYER_BASE "rim_layer_base"
#define S_RIM_DARKNESS_CUTOFF "rim_darkness_cutoff"
#define S_RIM_BLEND_MODE "rim_blend_mode"
#define S_RIM_POSITION_MODE "rim_position_mode"
#define S_RIM_WIDTH "rim_width"
#define S_RIM_SOFTNESS "rim_softness"
#define S_RIM_LOCAL_EXPANSION "rim_local_expansion"
#define S_RIM_SCALE "rim_scale"
#define S_RIM_AUTO_PIVOT "rim_auto_pivot"
#define S_RIM_STABLE_TRACKING "rim_stable_tracking"
#define S_RIM_PIVOT_X "rim_pivot_x"
#define S_RIM_PIVOT_Y "rim_pivot_y"
#define S_RIM_OFFSET_X "rim_offset_x"
#define S_RIM_OFFSET_Y "rim_offset_y"
#define S_KEYS_ENABLED "keys_enabled"
#define S_KEY_COUNT "key_count"
#define S_BLOOM_ENABLED "bloom_enabled"
#define S_BLOOM_AMOUNT "bloom_amount"
#define S_BLOOM_RADIUS "bloom_radius"
#define S_SHADOW_ENABLED "shadow_enabled"
#define S_SHADOW_COLOR "shadow_color"
#define S_SHADOW_OPACITY "shadow_opacity"
#define S_SHADOW_BLUR "shadow_blur"
#define S_SHADOW_OFFSET_X "shadow_offset_x"
#define S_SHADOW_OFFSET_Y "shadow_offset_y"
#define S_GLOW_ENABLED "glow_enabled"
#define S_GLOW_AMOUNT "glow_amount"
#define S_GLOW_COLOR "glow_color"
#define S_GLOW_BLEND_MODE "glow_blend_mode"
#define S_GLOW_WIDTH "glow_width"
#define S_DEBUG_VIEW "debug_view"

enum debug_view {
	DEBUG_FINAL = 0,
	DEBUG_ENVIRONMENT = 1,
	DEBUG_AMBIENT = 2,
	DEBUG_RIM = 3,
	DEBUG_EMISSIVE_MASK = 4,
	DEBUG_ORIGINAL = 5,
	DEBUG_SHADOW = 6,
	DEBUG_GLOW = 7,
	DEBUG_EMISSIVE_BLOOM = 8,
};

enum rim_blend_mode {
	RIM_BLEND_ADDITIVE = 0,
	RIM_BLEND_SCREEN = 1,
	RIM_BLEND_NORMAL = 2,
	RIM_BLEND_MASKED_DUPLICATE = 3,
};

enum rim_position_mode {
	RIM_POSITION_LOCAL_SILHOUETTE = 0,
	RIM_POSITION_SCALED_DUPLICATE = 1,
};

enum glow_blend_mode {
	GLOW_BLEND_ADDITIVE = 0,
	GLOW_BLEND_SCREEN = 1,
	GLOW_BLEND_NORMAL = 2,
};

static const uint32_t default_key_colors[KEY_COUNT] = {
	0xFF0000FF, /* red */
	0xFFFF7F7F, /* purple */
	0xFF00FF00, /* green */
	0xFFFF0000, /* blue */
	0xFFFFFF00, /* cyan */
	0xFF00FFFF, /* yellow */
	0xFF0080FF, /* orange */
	0xFFFF40FF, /* pink */
	0xFFFFFFFF, /* white */
	0xFF000000  /* spare */
};

struct dal_filter {
	obs_source_t *context;
	obs_source_t *environment;
	char *environment_name;
	obs_source_t *rim_environment;
	char *rim_environment_name;

	gs_effect_t *effect;
	gs_effect_t *blur_effect;
	gs_texrender_t *environment_render;
	gs_texrender_t *environment_final_render;
	gs_texrender_t *rim_environment_render;
	gs_texrender_t *blur_down[BLUR_LEVEL_COUNT];
	gs_texrender_t *blur_up[BLUR_LEVEL_COUNT];

	gs_eparam_t *p_environment_image;
	gs_eparam_t *p_rim_environment_image;
	gs_eparam_t *p_background_image;
	gs_eparam_t *p_texel_size;
	gs_eparam_t *p_ambient_enabled;
	gs_eparam_t *p_ambient_base;
	gs_eparam_t *p_ambient_amount;
	gs_eparam_t *p_exposure_enabled;
	gs_eparam_t *p_exposure_affects_rim;
	gs_eparam_t *p_exposure_target;
	gs_eparam_t *p_exposure_strength;
	gs_eparam_t *p_exposure_min;
	gs_eparam_t *p_exposure_max;
	gs_eparam_t *p_color_saturation;
	gs_eparam_t *p_color_vibrance;
	gs_eparam_t *p_color_contrast;
	gs_eparam_t *p_color_limit;
	gs_eparam_t *p_rim_enabled;
	gs_eparam_t *p_rim_amount;
	gs_eparam_t *p_rim_color_amount;
	gs_eparam_t *p_rim_layer_base;
	gs_eparam_t *p_rim_darkness_cutoff;
	gs_eparam_t *p_rim_blend_mode;
	gs_eparam_t *p_rim_position_mode;
	gs_eparam_t *p_rim_width;
	gs_eparam_t *p_rim_softness;
	gs_eparam_t *p_rim_local_expansion;
	gs_eparam_t *p_rim_scale;
	gs_eparam_t *p_rim_auto_pivot;
	gs_eparam_t *p_rim_stable_tracking;
	gs_eparam_t *p_rim_pivot;
	gs_eparam_t *p_rim_offset;
	gs_eparam_t *p_keys_enabled;
	gs_eparam_t *p_bloom_enabled;
	gs_eparam_t *p_bloom_amount;
	gs_eparam_t *p_bloom_radius;
	gs_eparam_t *p_shadow_enabled;
	gs_eparam_t *p_shadow_color;
	gs_eparam_t *p_shadow_opacity;
	gs_eparam_t *p_shadow_blur;
	gs_eparam_t *p_shadow_offset;
	gs_eparam_t *p_glow_enabled;
	gs_eparam_t *p_glow_amount;
	gs_eparam_t *p_glow_color;
	gs_eparam_t *p_glow_blend_mode;
	gs_eparam_t *p_glow_width;
	gs_eparam_t *p_debug_view;
	gs_eparam_t *p_key_colors[KEY_COUNT];
	gs_eparam_t *p_key_tolerances[KEY_COUNT];
	gs_eparam_t *p_key_softnesses[KEY_COUNT];
	gs_eparam_t *p_key_min_saturations[KEY_COUNT];
	gs_eparam_t *p_key_min_brightnesses[KEY_COUNT];
	gs_eparam_t *p_key_strengths[KEY_COUNT];
	gs_eparam_t *p_blur_image;
	gs_eparam_t *p_blur_texel_size;
	gs_eparam_t *p_blur_offset;

	bool ambient_enabled;
	float ambient_base;
	float ambient_amount;
	float ambient_blur;
	bool exposure_enabled;
	bool exposure_affects_rim;
	float exposure_target;
	float exposure_strength;
	float exposure_min;
	float exposure_max;
	float color_saturation;
	float color_vibrance;
	float color_contrast;
	float color_limit;
	bool rim_enabled;
	float rim_amount;
	float rim_color_amount;
	float rim_layer_base;
	float rim_darkness_cutoff;
	int rim_blend_mode;
	int rim_position_mode;
	float rim_width;
	float rim_softness;
	float rim_local_expansion;
	float rim_scale;
	bool rim_auto_pivot;
	bool rim_stable_tracking;
	struct vec2 rim_pivot;
	struct vec2 rim_offset;
	bool keys_enabled;
	int key_count;
	bool bloom_enabled;
	float bloom_amount;
	float bloom_radius;
	bool shadow_enabled;
	struct vec4 shadow_color;
	float shadow_opacity;
	float shadow_blur;
	struct vec2 shadow_offset;
	bool glow_enabled;
	float glow_amount;
	struct vec4 glow_color;
	int glow_blend_mode;
	float glow_width;
	int debug_view;
	struct vec4 key_colors[KEY_COUNT];
	float key_tolerances[KEY_COUNT];
	float key_softnesses[KEY_COUNT];
	float key_min_saturations[KEY_COUNT];
	float key_min_brightnesses[KEY_COUNT];
	float key_strengths[KEY_COUNT];
};

static void key_setting_name(char *buffer, size_t size, int index, const char *suffix)
{
	snprintf(buffer, size, "key_%02d_%s", index + 1, suffix);
}

static const char *dal_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("Filter.Name");
}

static void release_environment(struct dal_filter *filter)
{
	if (filter->environment) {
		obs_source_release(filter->environment);
		filter->environment = NULL;
	}
}

static void release_rim_environment(struct dal_filter *filter)
{
	if (filter->rim_environment) {
		obs_source_release(filter->rim_environment);
		filter->rim_environment = NULL;
	}
}

static void load_effect(struct dal_filter *filter)
{
	char *path = obs_module_file("effects/dynamic-avatar-lighting.effect");
	char *errors = NULL;

	obs_enter_graphics();
	filter->effect = gs_effect_create_from_file(path, &errors);
	obs_leave_graphics();

	if (!filter->effect) {
		blog(LOG_ERROR, "[VTuber Effects] Could not load effect: %s",
		     errors ? errors : "unknown shader error");
	}

	bfree(errors);
	bfree(path);

	if (!filter->effect)
		return;

	filter->p_environment_image = gs_effect_get_param_by_name(filter->effect, "environment_image");
	filter->p_rim_environment_image =
		gs_effect_get_param_by_name(filter->effect,
					    "rim_environment_image");
	filter->p_background_image = gs_effect_get_param_by_name(filter->effect, "background_image");
	filter->p_texel_size = gs_effect_get_param_by_name(filter->effect, "texel_size");
	filter->p_ambient_enabled = gs_effect_get_param_by_name(filter->effect, "ambient_enabled");
	filter->p_ambient_base = gs_effect_get_param_by_name(filter->effect, "ambient_base");
	filter->p_ambient_amount = gs_effect_get_param_by_name(filter->effect, "ambient_amount");
	filter->p_exposure_enabled =
		gs_effect_get_param_by_name(filter->effect,
					    "exposure_enabled");
	filter->p_exposure_affects_rim =
		gs_effect_get_param_by_name(filter->effect,
					    "exposure_affects_rim");
	filter->p_exposure_target =
		gs_effect_get_param_by_name(filter->effect,
					    "exposure_target");
	filter->p_exposure_strength =
		gs_effect_get_param_by_name(filter->effect,
					    "exposure_strength");
	filter->p_exposure_min =
		gs_effect_get_param_by_name(filter->effect, "exposure_min");
	filter->p_exposure_max =
		gs_effect_get_param_by_name(filter->effect, "exposure_max");
	filter->p_color_saturation =
		gs_effect_get_param_by_name(filter->effect,
					    "color_saturation");
	filter->p_color_vibrance =
		gs_effect_get_param_by_name(filter->effect,
					    "color_vibrance");
	filter->p_color_contrast =
		gs_effect_get_param_by_name(filter->effect,
					    "color_contrast");
	filter->p_color_limit =
		gs_effect_get_param_by_name(filter->effect, "color_limit");
	filter->p_rim_enabled = gs_effect_get_param_by_name(filter->effect, "rim_enabled");
	filter->p_rim_amount = gs_effect_get_param_by_name(filter->effect, "rim_amount");
	filter->p_rim_color_amount =
		gs_effect_get_param_by_name(filter->effect, "rim_color_amount");
	filter->p_rim_layer_base =
		gs_effect_get_param_by_name(filter->effect, "rim_layer_base");
	filter->p_rim_darkness_cutoff =
		gs_effect_get_param_by_name(filter->effect,
					    "rim_darkness_cutoff");
	filter->p_rim_blend_mode =
		gs_effect_get_param_by_name(filter->effect, "rim_blend_mode");
	filter->p_rim_position_mode = gs_effect_get_param_by_name(
		filter->effect, "rim_position_mode");
	filter->p_rim_width = gs_effect_get_param_by_name(filter->effect, "rim_width");
	filter->p_rim_softness = gs_effect_get_param_by_name(filter->effect, "rim_softness");
	filter->p_rim_local_expansion = gs_effect_get_param_by_name(
		filter->effect, "rim_local_expansion");
	filter->p_rim_scale = gs_effect_get_param_by_name(filter->effect, "rim_scale");
	filter->p_rim_auto_pivot =
		gs_effect_get_param_by_name(filter->effect, "rim_auto_pivot");
	filter->p_rim_stable_tracking = gs_effect_get_param_by_name(
		filter->effect, "rim_stable_tracking");
	filter->p_rim_pivot = gs_effect_get_param_by_name(filter->effect, "rim_pivot");
	filter->p_rim_offset = gs_effect_get_param_by_name(filter->effect, "rim_offset");
	filter->p_keys_enabled = gs_effect_get_param_by_name(filter->effect, "keys_enabled");
	filter->p_bloom_enabled =
		gs_effect_get_param_by_name(filter->effect, "bloom_enabled");
	filter->p_bloom_amount =
		gs_effect_get_param_by_name(filter->effect, "bloom_amount");
	filter->p_bloom_radius =
		gs_effect_get_param_by_name(filter->effect, "bloom_radius");
	filter->p_shadow_enabled = gs_effect_get_param_by_name(filter->effect, "shadow_enabled");
	filter->p_shadow_color = gs_effect_get_param_by_name(filter->effect, "shadow_color");
	filter->p_shadow_opacity = gs_effect_get_param_by_name(filter->effect, "shadow_opacity");
	filter->p_shadow_blur = gs_effect_get_param_by_name(filter->effect, "shadow_blur");
	filter->p_shadow_offset = gs_effect_get_param_by_name(filter->effect, "shadow_offset");
	filter->p_glow_enabled = gs_effect_get_param_by_name(filter->effect, "glow_enabled");
	filter->p_glow_amount = gs_effect_get_param_by_name(filter->effect, "glow_amount");
	filter->p_glow_color = gs_effect_get_param_by_name(filter->effect, "glow_color");
	filter->p_glow_blend_mode =
		gs_effect_get_param_by_name(filter->effect, "glow_blend_mode");
	filter->p_glow_width = gs_effect_get_param_by_name(filter->effect, "glow_width");
	filter->p_debug_view = gs_effect_get_param_by_name(filter->effect, "debug_view");

	for (int i = 0; i < KEY_COUNT; i++) {
		char name[32];
		snprintf(name, sizeof(name), "key_color_%02d", i + 1);
		filter->p_key_colors[i] = gs_effect_get_param_by_name(filter->effect, name);
		snprintf(name, sizeof(name), "key_tolerance_%02d", i + 1);
		filter->p_key_tolerances[i] = gs_effect_get_param_by_name(filter->effect, name);
		snprintf(name, sizeof(name), "key_softness_%02d", i + 1);
		filter->p_key_softnesses[i] = gs_effect_get_param_by_name(filter->effect, name);
		snprintf(name, sizeof(name), "key_min_saturation_%02d", i + 1);
		filter->p_key_min_saturations[i] =
			gs_effect_get_param_by_name(filter->effect, name);
		snprintf(name, sizeof(name), "key_min_brightness_%02d", i + 1);
		filter->p_key_min_brightnesses[i] =
			gs_effect_get_param_by_name(filter->effect, name);
		snprintf(name, sizeof(name), "key_strength_%02d", i + 1);
		filter->p_key_strengths[i] = gs_effect_get_param_by_name(filter->effect, name);
	}

	path = obs_module_file("effects/environment-blur.effect");
	errors = NULL;

	obs_enter_graphics();
	filter->blur_effect = gs_effect_create_from_file(path, &errors);
	obs_leave_graphics();

	if (!filter->blur_effect) {
		blog(LOG_ERROR, "[VTuber Effects] Could not load blur effect: %s",
		     errors ? errors : "unknown shader error");
	} else {
		filter->p_blur_image = gs_effect_get_param_by_name(filter->blur_effect, "image");
		filter->p_blur_texel_size =
			gs_effect_get_param_by_name(filter->blur_effect, "texel_size");
		filter->p_blur_offset = gs_effect_get_param_by_name(filter->blur_effect, "sample_offset");
	}

	bfree(errors);
	bfree(path);
}

static void *dal_create(obs_data_t *settings, obs_source_t *context)
{
	struct dal_filter *filter = bzalloc(sizeof(*filter));
	filter->context = context;

	obs_enter_graphics();
	filter->environment_render = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
	filter->environment_final_render =
		gs_texrender_create(GS_RGBA, GS_ZS_NONE);
	filter->rim_environment_render =
		gs_texrender_create(GS_RGBA, GS_ZS_NONE);
	for (int i = 0; i < BLUR_LEVEL_COUNT; i++) {
		filter->blur_down[i] = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
		filter->blur_up[i] = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
	}
	obs_leave_graphics();

	load_effect(filter);
	obs_source_update(context, settings);
	return filter;
}

static void dal_destroy(void *data)
{
	struct dal_filter *filter = data;
	release_environment(filter);
	release_rim_environment(filter);
	bfree(filter->environment_name);
	bfree(filter->rim_environment_name);

	obs_enter_graphics();
	if (filter->effect)
		gs_effect_destroy(filter->effect);
	if (filter->blur_effect)
		gs_effect_destroy(filter->blur_effect);
	if (filter->environment_render)
		gs_texrender_destroy(filter->environment_render);
	if (filter->environment_final_render)
		gs_texrender_destroy(filter->environment_final_render);
	if (filter->rim_environment_render)
		gs_texrender_destroy(filter->rim_environment_render);
	for (int i = 0; i < BLUR_LEVEL_COUNT; i++) {
		if (filter->blur_down[i])
			gs_texrender_destroy(filter->blur_down[i]);
		if (filter->blur_up[i])
			gs_texrender_destroy(filter->blur_up[i]);
	}
	obs_leave_graphics();

	bfree(filter);
}

static void dal_update(void *data, obs_data_t *settings)
{
	struct dal_filter *filter = data;
	const char *new_name = obs_data_get_string(settings, S_ENVIRONMENT);
	const char *new_rim_name =
		obs_data_get_string(settings, S_RIM_ENVIRONMENT);

	if (!filter->environment_name || strcmp(filter->environment_name, new_name) != 0) {
		release_environment(filter);
		bfree(filter->environment_name);
		filter->environment_name = bstrdup(new_name);

		if (new_name && *new_name)
			filter->environment = obs_get_source_by_name(new_name);
	}

	if (!filter->rim_environment_name ||
	    strcmp(filter->rim_environment_name, new_rim_name) != 0) {
		release_rim_environment(filter);
		bfree(filter->rim_environment_name);
		filter->rim_environment_name = bstrdup(new_rim_name);

		if (new_rim_name && *new_rim_name)
			filter->rim_environment =
				obs_get_source_by_name(new_rim_name);
	}

	filter->ambient_enabled = obs_data_get_bool(settings, S_AMBIENT_ENABLED);
	filter->ambient_base = (float)obs_data_get_double(settings, S_AMBIENT_BASE);
	filter->ambient_amount = (float)obs_data_get_double(settings, S_AMBIENT_AMOUNT);
	filter->ambient_blur = (float)obs_data_get_double(settings, S_AMBIENT_BLUR);
	filter->exposure_enabled =
		obs_data_get_bool(settings, S_EXPOSURE_ENABLED);
	filter->exposure_affects_rim =
		obs_data_get_bool(settings, S_EXPOSURE_AFFECTS_RIM);
	filter->exposure_target =
		(float)obs_data_get_double(settings, S_EXPOSURE_TARGET);
	filter->exposure_strength =
		(float)obs_data_get_double(settings, S_EXPOSURE_STRENGTH);
	filter->exposure_min =
		(float)obs_data_get_double(settings, S_EXPOSURE_MIN);
	filter->exposure_max =
		(float)obs_data_get_double(settings, S_EXPOSURE_MAX);
	filter->color_saturation =
		(float)obs_data_get_double(settings, S_COLOR_SATURATION);
	filter->color_vibrance =
		(float)obs_data_get_double(settings, S_COLOR_VIBRANCE);
	filter->color_contrast =
		(float)obs_data_get_double(settings, S_COLOR_CONTRAST);
	filter->color_limit =
		(float)obs_data_get_double(settings, S_COLOR_LIMIT);
	filter->rim_enabled = obs_data_get_bool(settings, S_RIM_ENABLED);
	filter->rim_amount = (float)obs_data_get_double(settings, S_RIM_AMOUNT);
	filter->rim_color_amount = (float)obs_data_get_double(settings, S_RIM_COLOR_AMOUNT);
	filter->rim_layer_base = (float)obs_data_get_double(settings, S_RIM_LAYER_BASE);
	filter->rim_darkness_cutoff =
		(float)obs_data_get_double(settings, S_RIM_DARKNESS_CUTOFF);
	filter->rim_blend_mode = (int)obs_data_get_int(settings, S_RIM_BLEND_MODE);
	filter->rim_position_mode =
		(int)obs_data_get_int(settings, S_RIM_POSITION_MODE);
	filter->rim_width = (float)obs_data_get_double(settings, S_RIM_WIDTH);
	filter->rim_softness = (float)obs_data_get_double(settings, S_RIM_SOFTNESS);
	filter->rim_local_expansion =
		(float)obs_data_get_double(settings, S_RIM_LOCAL_EXPANSION);
	filter->rim_scale = (float)obs_data_get_double(settings, S_RIM_SCALE);
	filter->rim_auto_pivot =
		obs_data_get_bool(settings, S_RIM_AUTO_PIVOT);
	filter->rim_stable_tracking =
		obs_data_get_bool(settings, S_RIM_STABLE_TRACKING);
	filter->rim_pivot.x =
		(float)obs_data_get_double(settings, S_RIM_PIVOT_X) * 0.01f;
	filter->rim_pivot.y =
		(float)obs_data_get_double(settings, S_RIM_PIVOT_Y) * 0.01f;
	filter->rim_offset.x = (float)obs_data_get_double(settings, S_RIM_OFFSET_X);
	filter->rim_offset.y = (float)obs_data_get_double(settings, S_RIM_OFFSET_Y);
	filter->keys_enabled = obs_data_get_bool(settings, S_KEYS_ENABLED);
	filter->key_count = (int)obs_data_get_int(settings, S_KEY_COUNT);
	if (filter->key_count < 0)
		filter->key_count = 0;
	if (filter->key_count > KEY_COUNT)
		filter->key_count = KEY_COUNT;
	filter->bloom_enabled =
		obs_data_get_bool(settings, S_BLOOM_ENABLED);
	filter->bloom_amount =
		(float)obs_data_get_double(settings, S_BLOOM_AMOUNT);
	filter->bloom_radius =
		(float)obs_data_get_double(settings, S_BLOOM_RADIUS);
	filter->shadow_enabled = obs_data_get_bool(settings, S_SHADOW_ENABLED);
	vec4_from_rgba(&filter->shadow_color,
		       (uint32_t)obs_data_get_int(settings, S_SHADOW_COLOR));
	filter->shadow_opacity = (float)obs_data_get_double(settings, S_SHADOW_OPACITY);
	filter->shadow_blur = (float)obs_data_get_double(settings, S_SHADOW_BLUR);
	filter->shadow_offset.x = (float)obs_data_get_double(settings, S_SHADOW_OFFSET_X);
	filter->shadow_offset.y = (float)obs_data_get_double(settings, S_SHADOW_OFFSET_Y);
	filter->glow_enabled = obs_data_get_bool(settings, S_GLOW_ENABLED);
	filter->glow_amount = (float)obs_data_get_double(settings, S_GLOW_AMOUNT);
	vec4_from_rgba(&filter->glow_color,
		       (uint32_t)obs_data_get_int(settings, S_GLOW_COLOR));
	filter->glow_blend_mode =
		(int)obs_data_get_int(settings, S_GLOW_BLEND_MODE);
	filter->glow_width = (float)obs_data_get_double(settings, S_GLOW_WIDTH);
	filter->debug_view = (int)obs_data_get_int(settings, S_DEBUG_VIEW);

	for (int i = 0; i < KEY_COUNT; i++) {
		char color_name[32];
		char tolerance_name[32];
		char softness_name[32];
		char min_saturation_name[32];
		char min_brightness_name[32];
		char strength_name[32];
		key_setting_name(color_name, sizeof(color_name), i, "color");
		key_setting_name(tolerance_name, sizeof(tolerance_name), i, "tolerance");
		key_setting_name(softness_name, sizeof(softness_name), i, "softness");
		key_setting_name(min_saturation_name, sizeof(min_saturation_name), i,
				 "min_saturation");
		key_setting_name(min_brightness_name, sizeof(min_brightness_name), i,
				 "min_brightness");
		key_setting_name(strength_name, sizeof(strength_name), i, "strength");

		const uint32_t color = (uint32_t)obs_data_get_int(settings, color_name);
		vec4_from_rgba(&filter->key_colors[i], color);
		filter->key_colors[i].w = i < filter->key_count ? 1.0f : 0.0f;
		filter->key_tolerances[i] =
			(float)obs_data_get_double(settings, tolerance_name);
		filter->key_softnesses[i] =
			(float)obs_data_get_double(settings, softness_name);
		filter->key_min_saturations[i] =
			(float)obs_data_get_double(settings, min_saturation_name);
		filter->key_min_brightnesses[i] =
			(float)obs_data_get_double(settings, min_brightness_name);
		filter->key_strengths[i] =
			(float)obs_data_get_double(settings, strength_name);
	}
}

static void dal_tick(void *data, float seconds)
{
	UNUSED_PARAMETER(seconds);
	struct dal_filter *filter = data;

	if (!filter->environment && filter->environment_name &&
	    *filter->environment_name)
		filter->environment =
			obs_get_source_by_name(filter->environment_name);
	if (!filter->rim_environment && filter->rim_environment_name &&
	    *filter->rim_environment_name)
		filter->rim_environment =
			obs_get_source_by_name(filter->rim_environment_name);
}

static void dal_defaults(obs_data_t *settings)
{
	obs_data_set_default_string(settings, S_RIM_ENVIRONMENT, "");
	obs_data_set_default_bool(settings, S_AMBIENT_ENABLED, true);
	obs_data_set_default_double(settings, S_AMBIENT_BASE, 0.20);
	obs_data_set_default_double(settings, S_AMBIENT_AMOUNT, 2.00);
	obs_data_set_default_double(settings, S_AMBIENT_BLUR, 330.0);
	obs_data_set_default_bool(settings, S_EXPOSURE_ENABLED, false);
	obs_data_set_default_bool(settings, S_EXPOSURE_AFFECTS_RIM, true);
	obs_data_set_default_double(settings, S_EXPOSURE_TARGET, 0.45);
	obs_data_set_default_double(settings, S_EXPOSURE_STRENGTH, 1.00);
	obs_data_set_default_double(settings, S_EXPOSURE_MIN, 0.35);
	obs_data_set_default_double(settings, S_EXPOSURE_MAX, 2.00);
	obs_data_set_default_double(settings, S_COLOR_SATURATION, 1.00);
	obs_data_set_default_double(settings, S_COLOR_VIBRANCE, 0.00);
	obs_data_set_default_double(settings, S_COLOR_CONTRAST, 1.00);
	obs_data_set_default_double(settings, S_COLOR_LIMIT, 1.00);
	obs_data_set_default_bool(settings, S_RIM_ENABLED, true);
	obs_data_set_default_double(settings, S_RIM_AMOUNT, 0.50);
	obs_data_set_default_double(settings, S_RIM_COLOR_AMOUNT, 2.0);
	obs_data_set_default_double(settings, S_RIM_LAYER_BASE, 1.0);
	obs_data_set_default_double(settings, S_RIM_DARKNESS_CUTOFF, 0.15);
	obs_data_set_default_int(settings, S_RIM_BLEND_MODE,
				 RIM_BLEND_MASKED_DUPLICATE);
	obs_data_set_default_int(settings, S_RIM_POSITION_MODE,
				 RIM_POSITION_SCALED_DUPLICATE);
	obs_data_set_default_double(settings, S_RIM_WIDTH, 25.0);
	obs_data_set_default_double(settings, S_RIM_SOFTNESS, 0.75);
	obs_data_set_default_double(settings, S_RIM_LOCAL_EXPANSION, 0.0);
	obs_data_set_default_double(settings, S_RIM_SCALE, 0.935);
	obs_data_set_default_bool(settings, S_RIM_AUTO_PIVOT, true);
	obs_data_set_default_bool(settings, S_RIM_STABLE_TRACKING, false);
	obs_data_set_default_double(settings, S_RIM_PIVOT_X, 50.0);
	obs_data_set_default_double(settings, S_RIM_PIVOT_Y, 50.0);
	obs_data_set_default_double(settings, S_RIM_OFFSET_X, -25.0);
	obs_data_set_default_double(settings, S_RIM_OFFSET_Y, -15.0);
	obs_data_set_default_bool(settings, S_KEYS_ENABLED, true);
	obs_data_set_default_int(settings, S_KEY_COUNT, 1);
	obs_data_set_default_bool(settings, S_BLOOM_ENABLED, false);
	obs_data_set_default_double(settings, S_BLOOM_AMOUNT, 1.00);
	obs_data_set_default_double(settings, S_BLOOM_RADIUS, 18.00);
	obs_data_set_default_bool(settings, S_SHADOW_ENABLED, false);
	obs_data_set_default_int(settings, S_SHADOW_COLOR, 0xFF000000);
	obs_data_set_default_double(settings, S_SHADOW_OPACITY, 0.65);
	obs_data_set_default_double(settings, S_SHADOW_BLUR, 24.0);
	obs_data_set_default_double(settings, S_SHADOW_OFFSET_X, 12.0);
	obs_data_set_default_double(settings, S_SHADOW_OFFSET_Y, 12.0);
	obs_data_set_default_bool(settings, S_GLOW_ENABLED, false);
	obs_data_set_default_double(settings, S_GLOW_AMOUNT, 1.0);
	obs_data_set_default_int(settings, S_GLOW_COLOR, 0xFFFFFFFF);
	obs_data_set_default_int(settings, S_GLOW_BLEND_MODE,
				 GLOW_BLEND_ADDITIVE);
	obs_data_set_default_double(settings, S_GLOW_WIDTH, 30.0);
	obs_data_set_default_int(settings, S_DEBUG_VIEW, DEBUG_FINAL);

	for (int i = 0; i < KEY_COUNT; i++) {
		char color_name[32];
		char tolerance_name[32];
		char softness_name[32];
		char min_saturation_name[32];
		char min_brightness_name[32];
		char strength_name[32];
		key_setting_name(color_name, sizeof(color_name), i, "color");
		key_setting_name(tolerance_name, sizeof(tolerance_name), i, "tolerance");
		key_setting_name(softness_name, sizeof(softness_name), i, "softness");
		key_setting_name(min_saturation_name, sizeof(min_saturation_name), i,
				 "min_saturation");
		key_setting_name(min_brightness_name, sizeof(min_brightness_name), i,
				 "min_brightness");
		key_setting_name(strength_name, sizeof(strength_name), i, "strength");
		obs_data_set_default_int(settings, color_name,
					 default_key_colors[i]);
		obs_data_set_default_double(settings, tolerance_name, 0.10);
		obs_data_set_default_double(settings, softness_name, 0.04);
		obs_data_set_default_double(settings, min_saturation_name, 0.35);
		obs_data_set_default_double(settings, min_brightness_name, 0.25);
		obs_data_set_default_double(settings, strength_name, 1.0);
	}
}

static bool source_enum_callback(void *param, obs_source_t *source)
{
	obs_property_t *list = param;
	const char *name = obs_source_get_name(source);
	uint32_t flags = obs_source_get_output_flags(source);

	if (name && *name && (flags & OBS_SOURCE_VIDEO))
		obs_property_list_add_string(list, name, name);

	return true;
}

static bool key_count_modified(obs_properties_t *props, obs_property_t *property,
			       obs_data_t *settings)
{
	UNUSED_PARAMETER(property);
	int count = (int)obs_data_get_int(settings, S_KEY_COUNT);

	for (int i = 0; i < KEY_COUNT; i++) {
		char group_name[32];
		key_setting_name(group_name, sizeof(group_name), i, "group");
		obs_property_t *group = obs_properties_get(props, group_name);
		if (group)
			obs_property_set_visible(group, i < count);
	}

	return true;
}

static bool environment_modified(obs_properties_t *props,
				 obs_property_t *property,
				 obs_data_t *settings)
{
	UNUSED_PARAMETER(property);
	const char *environment_name =
		obs_data_get_string(settings, S_ENVIRONMENT);
	obs_property_t *warning =
		obs_properties_get(props, S_ENVIRONMENT_WARNING);

	if (warning)
		obs_property_set_visible(
			warning,
			!environment_name || !*environment_name);

	return true;
}

static bool rim_position_mode_modified(obs_properties_t *props,
				       obs_property_t *property,
				       obs_data_t *settings)
{
	UNUSED_PARAMETER(property);
	const bool scaled = obs_data_get_int(settings, S_RIM_POSITION_MODE) ==
			    RIM_POSITION_SCALED_DUPLICATE;
	obs_property_t *local_expansion =
		obs_properties_get(props, S_RIM_LOCAL_EXPANSION);
	if (local_expansion)
		obs_property_set_visible(local_expansion, !scaled);

	const char *scaled_controls[] = {
		S_RIM_SCALE,       S_RIM_AUTO_PIVOT,
		S_RIM_STABLE_TRACKING, S_RIM_PIVOT_X,
		S_RIM_PIVOT_Y,
	};

	for (size_t i = 0;
	     i < sizeof(scaled_controls) / sizeof(scaled_controls[0]); i++) {
		obs_property_t *control =
			obs_properties_get(props, scaled_controls[i]);
		if (control)
			obs_property_set_visible(control, scaled);
	}

	return true;
}

static void set_tooltip(obs_property_t *property, const char *text_key)
{
	if (property)
		obs_property_set_long_description(property, obs_module_text(text_key));
}

static void set_preset_status(obs_properties_t *props, const char *message)
{
	obs_property_t *status = obs_properties_get(props, S_PRESET_STATUS);
	if (status)
		obs_property_set_description(status, message);
}

static void copy_effect_settings(obs_data_t *destination,
				 obs_data_t *source)
{
	obs_data_set_bool(destination, S_AMBIENT_ENABLED,
			  obs_data_get_bool(source, S_AMBIENT_ENABLED));
	obs_data_set_double(destination, S_AMBIENT_BASE,
			    obs_data_get_double(source, S_AMBIENT_BASE));
	obs_data_set_double(destination, S_AMBIENT_AMOUNT,
			    obs_data_get_double(source, S_AMBIENT_AMOUNT));
	obs_data_set_double(destination, S_AMBIENT_BLUR,
			    obs_data_get_double(source, S_AMBIENT_BLUR));
	obs_data_set_bool(destination, S_EXPOSURE_ENABLED,
			  obs_data_get_bool(source, S_EXPOSURE_ENABLED));
	obs_data_set_bool(
		destination, S_EXPOSURE_AFFECTS_RIM,
		obs_data_has_user_value(source, S_EXPOSURE_AFFECTS_RIM)
			? obs_data_get_bool(source, S_EXPOSURE_AFFECTS_RIM)
			: true);
	obs_data_set_double(destination, S_EXPOSURE_TARGET,
			    obs_data_has_user_value(source, S_EXPOSURE_TARGET)
				    ? obs_data_get_double(source,
							 S_EXPOSURE_TARGET)
				    : 0.45);
	obs_data_set_double(destination, S_EXPOSURE_STRENGTH,
			    obs_data_has_user_value(source,
						    S_EXPOSURE_STRENGTH)
				    ? obs_data_get_double(
					      source,
					      S_EXPOSURE_STRENGTH)
				    : 1.00);
	obs_data_set_double(destination, S_EXPOSURE_MIN,
			    obs_data_has_user_value(source, S_EXPOSURE_MIN)
				    ? obs_data_get_double(source,
							 S_EXPOSURE_MIN)
				    : 0.35);
	obs_data_set_double(destination, S_EXPOSURE_MAX,
			    obs_data_has_user_value(source, S_EXPOSURE_MAX)
				    ? obs_data_get_double(source,
							 S_EXPOSURE_MAX)
				    : 2.00);
	obs_data_set_double(destination, S_COLOR_SATURATION,
			    obs_data_has_user_value(source,
						    S_COLOR_SATURATION)
				    ? obs_data_get_double(
					      source,
					      S_COLOR_SATURATION)
				    : 1.00);
	obs_data_set_double(destination, S_COLOR_VIBRANCE,
			    obs_data_has_user_value(source, S_COLOR_VIBRANCE)
				    ? obs_data_get_double(source,
							 S_COLOR_VIBRANCE)
				    : 0.00);
	obs_data_set_double(destination, S_COLOR_CONTRAST,
			    obs_data_has_user_value(source, S_COLOR_CONTRAST)
				    ? obs_data_get_double(source,
							 S_COLOR_CONTRAST)
				    : 1.00);
	obs_data_set_double(destination, S_COLOR_LIMIT,
			    obs_data_has_user_value(source, S_COLOR_LIMIT)
				    ? obs_data_get_double(source,
							 S_COLOR_LIMIT)
				    : 1.00);

	obs_data_set_bool(destination, S_RIM_ENABLED,
			  obs_data_get_bool(source, S_RIM_ENABLED));
	obs_data_set_double(destination, S_RIM_AMOUNT,
			    obs_data_get_double(source, S_RIM_AMOUNT));
	obs_data_set_double(destination, S_RIM_COLOR_AMOUNT,
			    obs_data_get_double(source, S_RIM_COLOR_AMOUNT));
	obs_data_set_double(destination, S_RIM_LAYER_BASE,
			    obs_data_get_double(source, S_RIM_LAYER_BASE));
	obs_data_set_double(
		destination, S_RIM_DARKNESS_CUTOFF,
		obs_data_has_user_value(source, S_RIM_DARKNESS_CUTOFF)
			? obs_data_get_double(source, S_RIM_DARKNESS_CUTOFF)
			: 0.15);
	obs_data_set_int(destination, S_RIM_BLEND_MODE,
			 obs_data_get_int(source, S_RIM_BLEND_MODE));
	obs_data_set_int(
		destination, S_RIM_POSITION_MODE,
		obs_data_has_user_value(source, S_RIM_POSITION_MODE)
			? obs_data_get_int(source, S_RIM_POSITION_MODE)
			: RIM_POSITION_SCALED_DUPLICATE);
	obs_data_set_double(destination, S_RIM_WIDTH,
			    obs_data_get_double(source, S_RIM_WIDTH));
	obs_data_set_double(destination, S_RIM_SOFTNESS,
			    obs_data_get_double(source, S_RIM_SOFTNESS));
	obs_data_set_double(
		destination, S_RIM_LOCAL_EXPANSION,
		obs_data_has_user_value(source, S_RIM_LOCAL_EXPANSION)
			? obs_data_get_double(source, S_RIM_LOCAL_EXPANSION)
			: 0.0);
	obs_data_set_double(destination, S_RIM_SCALE,
			    obs_data_get_double(source, S_RIM_SCALE));
	obs_data_set_bool(
		destination, S_RIM_AUTO_PIVOT,
		obs_data_has_user_value(source, S_RIM_AUTO_PIVOT)
			? obs_data_get_bool(source, S_RIM_AUTO_PIVOT)
			: true);
	obs_data_set_bool(
		destination, S_RIM_STABLE_TRACKING,
		obs_data_has_user_value(source, S_RIM_STABLE_TRACKING)
			? obs_data_get_bool(source, S_RIM_STABLE_TRACKING)
			: false);
	obs_data_set_double(destination, S_RIM_PIVOT_X,
			    obs_data_has_user_value(source, S_RIM_PIVOT_X)
				    ? obs_data_get_double(source,
							 S_RIM_PIVOT_X)
				    : 50.0);
	obs_data_set_double(destination, S_RIM_PIVOT_Y,
			    obs_data_has_user_value(source, S_RIM_PIVOT_Y)
				    ? obs_data_get_double(source,
							 S_RIM_PIVOT_Y)
				    : 50.0);
	obs_data_set_double(destination, S_RIM_OFFSET_X,
			    obs_data_get_double(source, S_RIM_OFFSET_X));
	obs_data_set_double(destination, S_RIM_OFFSET_Y,
			    obs_data_get_double(source, S_RIM_OFFSET_Y));

	obs_data_set_bool(destination, S_KEYS_ENABLED,
			  obs_data_get_bool(source, S_KEYS_ENABLED));
	obs_data_set_int(destination, S_KEY_COUNT,
			 obs_data_get_int(source, S_KEY_COUNT));
	obs_data_set_bool(destination, S_BLOOM_ENABLED,
			  obs_data_get_bool(source, S_BLOOM_ENABLED));
	obs_data_set_double(destination, S_BLOOM_AMOUNT,
			    obs_data_has_user_value(source, S_BLOOM_AMOUNT)
				    ? obs_data_get_double(source,
							 S_BLOOM_AMOUNT)
				    : 1.00);
	obs_data_set_double(destination, S_BLOOM_RADIUS,
			    obs_data_has_user_value(source, S_BLOOM_RADIUS)
				    ? obs_data_get_double(source,
							 S_BLOOM_RADIUS)
				    : 18.00);
	for (int i = 0; i < KEY_COUNT; i++) {
		char color_name[32];
		char tolerance_name[32];
		char softness_name[32];
		char min_saturation_name[32];
		char min_brightness_name[32];
		char strength_name[32];
		key_setting_name(color_name, sizeof(color_name), i, "color");
		key_setting_name(tolerance_name, sizeof(tolerance_name), i,
				 "tolerance");
		key_setting_name(softness_name, sizeof(softness_name), i,
				 "softness");
		key_setting_name(min_saturation_name,
				 sizeof(min_saturation_name), i,
				 "min_saturation");
		key_setting_name(min_brightness_name,
				 sizeof(min_brightness_name), i,
				 "min_brightness");
		key_setting_name(strength_name, sizeof(strength_name), i,
				 "strength");
		obs_data_set_int(destination, color_name,
				 obs_data_get_int(source, color_name));
		obs_data_set_double(destination, tolerance_name,
				    obs_data_get_double(source, tolerance_name));
		obs_data_set_double(destination, softness_name,
				    obs_data_get_double(source, softness_name));
		obs_data_set_double(
			destination, min_saturation_name,
			obs_data_get_double(source, min_saturation_name));
		obs_data_set_double(
			destination, min_brightness_name,
			obs_data_get_double(source, min_brightness_name));
		obs_data_set_double(destination, strength_name,
				    obs_data_get_double(source, strength_name));
	}

	obs_data_set_bool(destination, S_SHADOW_ENABLED,
			  obs_data_get_bool(source, S_SHADOW_ENABLED));
	obs_data_set_int(destination, S_SHADOW_COLOR,
			 obs_data_get_int(source, S_SHADOW_COLOR));
	obs_data_set_double(destination, S_SHADOW_OPACITY,
			    obs_data_get_double(source, S_SHADOW_OPACITY));
	obs_data_set_double(destination, S_SHADOW_BLUR,
			    obs_data_get_double(source, S_SHADOW_BLUR));
	obs_data_set_double(destination, S_SHADOW_OFFSET_X,
			    obs_data_get_double(source, S_SHADOW_OFFSET_X));
	obs_data_set_double(destination, S_SHADOW_OFFSET_Y,
			    obs_data_get_double(source, S_SHADOW_OFFSET_Y));

	obs_data_set_bool(destination, S_GLOW_ENABLED,
			  obs_data_get_bool(source, S_GLOW_ENABLED));
	obs_data_set_int(destination, S_GLOW_COLOR,
			 obs_data_get_int(source, S_GLOW_COLOR));
	obs_data_set_int(destination, S_GLOW_BLEND_MODE,
			 obs_data_get_int(source, S_GLOW_BLEND_MODE));
	obs_data_set_double(destination, S_GLOW_AMOUNT,
			    obs_data_get_double(source, S_GLOW_AMOUNT));
	obs_data_set_double(destination, S_GLOW_WIDTH,
			    obs_data_get_double(source, S_GLOW_WIDTH));
}

static bool export_preset_clicked(obs_properties_t *props,
				  obs_property_t *property, void *data)
{
	UNUSED_PARAMETER(property);
	struct dal_filter *filter = data;
	if (!filter || !filter->context)
		return false;

	char path[4096];
	if (!vtuber_effects_choose_preset_save(path, sizeof(path)))
		return false;

	obs_data_t *current = obs_source_get_settings(filter->context);
	obs_data_t *preset = obs_data_create();
	obs_data_set_int(preset, "vtuber_effects_preset_version", 1);
	copy_effect_settings(preset, current);
	const bool saved =
		obs_data_save_json_safe(preset, path, "tmp", "bak");
	set_preset_status(
		props, saved ? obs_module_text("Preset.Status.Exported")
			     : obs_module_text("Preset.Status.ExportFailed"));
	if (saved)
		blog(LOG_INFO, "[VTuber Effects] Exported preset to %s",
		     path);
	else
		blog(LOG_WARNING,
		     "[VTuber Effects] Could not export preset to %s",
		     path);

	obs_data_release(preset);
	obs_data_release(current);
	return false;
}

static bool import_preset_clicked(obs_properties_t *props,
				  obs_property_t *property, void *data)
{
	UNUSED_PARAMETER(property);
	struct dal_filter *filter = data;
	if (!filter || !filter->context)
		return false;

	char path[4096];
	if (!vtuber_effects_choose_preset_open(path, sizeof(path)))
		return false;

	obs_data_t *loaded = obs_data_create_from_json_file_safe(path, "bak");
	if (!loaded) {
		set_preset_status(props,
				  obs_module_text("Preset.Status.ImportFailed"));
		blog(LOG_WARNING,
		     "[VTuber Effects] Could not import preset from %s",
		     path);
		return false;
	}

	obs_data_t *imported_settings = obs_data_create();
	copy_effect_settings(imported_settings, loaded);
	obs_source_update(filter->context, imported_settings);
	set_preset_status(props, obs_module_text("Preset.Status.Imported"));
	blog(LOG_INFO, "[VTuber Effects] Imported preset from %s", path);

	obs_data_release(imported_settings);
	obs_data_release(loaded);
	return true;
}

static bool update_filter_settings(void *data, obs_data_t *settings)
{
	struct dal_filter *filter = data;
	if (!filter || !filter->context || !settings)
		return false;

	obs_source_update(filter->context, settings);
	return true;
}

static bool reset_ambient_clicked(obs_properties_t *props,
				  obs_property_t *property, void *data)
{
	UNUSED_PARAMETER(props);
	UNUSED_PARAMETER(property);
	obs_data_t *settings = obs_data_create();
	obs_data_set_bool(settings, S_AMBIENT_ENABLED, true);
	obs_data_set_double(settings, S_AMBIENT_BASE, 0.20);
	obs_data_set_double(settings, S_AMBIENT_AMOUNT, 2.00);
	obs_data_set_double(settings, S_AMBIENT_BLUR, 330.0);
	bool updated = update_filter_settings(data, settings);
	obs_data_release(settings);
	return updated;
}

static bool reset_environment_processing_clicked(
	obs_properties_t *props, obs_property_t *property, void *data)
{
	UNUSED_PARAMETER(props);
	UNUSED_PARAMETER(property);
	obs_data_t *settings = obs_data_create();
	obs_data_set_bool(settings, S_EXPOSURE_ENABLED, false);
	obs_data_set_bool(settings, S_EXPOSURE_AFFECTS_RIM, true);
	obs_data_set_double(settings, S_EXPOSURE_TARGET, 0.45);
	obs_data_set_double(settings, S_EXPOSURE_STRENGTH, 1.00);
	obs_data_set_double(settings, S_EXPOSURE_MIN, 0.35);
	obs_data_set_double(settings, S_EXPOSURE_MAX, 2.00);
	obs_data_set_double(settings, S_COLOR_SATURATION, 1.00);
	obs_data_set_double(settings, S_COLOR_VIBRANCE, 0.00);
	obs_data_set_double(settings, S_COLOR_CONTRAST, 1.00);
	obs_data_set_double(settings, S_COLOR_LIMIT, 1.00);
	bool updated = update_filter_settings(data, settings);
	obs_data_release(settings);
	return updated;
}

static bool reset_rim_clicked(obs_properties_t *props,
			      obs_property_t *property, void *data)
{
	UNUSED_PARAMETER(props);
	UNUSED_PARAMETER(property);
	obs_data_t *settings = obs_data_create();
	obs_data_set_bool(settings, S_RIM_ENABLED, true);
	obs_data_set_double(settings, S_RIM_AMOUNT, 0.50);
	obs_data_set_double(settings, S_RIM_COLOR_AMOUNT, 2.00);
	obs_data_set_double(settings, S_RIM_LAYER_BASE, 1.00);
	obs_data_set_double(settings, S_RIM_DARKNESS_CUTOFF, 0.15);
	obs_data_set_int(settings, S_RIM_BLEND_MODE,
			 RIM_BLEND_MASKED_DUPLICATE);
	obs_data_set_int(settings, S_RIM_POSITION_MODE,
			 RIM_POSITION_SCALED_DUPLICATE);
	obs_data_set_double(settings, S_RIM_WIDTH, 25.0);
	obs_data_set_double(settings, S_RIM_SOFTNESS, 0.75);
	obs_data_set_double(settings, S_RIM_LOCAL_EXPANSION, 0.0);
	obs_data_set_double(settings, S_RIM_SCALE, 0.935);
	obs_data_set_bool(settings, S_RIM_AUTO_PIVOT, true);
	obs_data_set_bool(settings, S_RIM_STABLE_TRACKING, false);
	obs_data_set_double(settings, S_RIM_PIVOT_X, 50.0);
	obs_data_set_double(settings, S_RIM_PIVOT_Y, 50.0);
	obs_data_set_double(settings, S_RIM_OFFSET_X, -25.0);
	obs_data_set_double(settings, S_RIM_OFFSET_Y, -15.0);
	bool updated = update_filter_settings(data, settings);
	obs_data_release(settings);
	return updated;
}

static bool reset_keys_clicked(obs_properties_t *props,
			       obs_property_t *property, void *data)
{
	UNUSED_PARAMETER(props);
	UNUSED_PARAMETER(property);
	obs_data_t *settings = obs_data_create();
	obs_data_set_bool(settings, S_KEYS_ENABLED, true);
	obs_data_set_int(settings, S_KEY_COUNT, 1);
	obs_data_set_bool(settings, S_BLOOM_ENABLED, false);
	obs_data_set_double(settings, S_BLOOM_AMOUNT, 1.00);
	obs_data_set_double(settings, S_BLOOM_RADIUS, 18.00);

	for (int i = 0; i < KEY_COUNT; i++) {
		char color_name[32];
		char tolerance_name[32];
		char softness_name[32];
		char min_saturation_name[32];
		char min_brightness_name[32];
		char strength_name[32];
		key_setting_name(color_name, sizeof(color_name), i, "color");
		key_setting_name(tolerance_name, sizeof(tolerance_name), i,
				 "tolerance");
		key_setting_name(softness_name, sizeof(softness_name), i,
				 "softness");
		key_setting_name(min_saturation_name,
				 sizeof(min_saturation_name), i,
				 "min_saturation");
		key_setting_name(min_brightness_name,
				 sizeof(min_brightness_name), i,
				 "min_brightness");
		key_setting_name(strength_name, sizeof(strength_name), i,
				 "strength");
		obs_data_set_int(settings, color_name,
				 default_key_colors[i]);
		obs_data_set_double(settings, tolerance_name, 0.10);
		obs_data_set_double(settings, softness_name, 0.04);
		obs_data_set_double(settings, min_saturation_name, 0.35);
		obs_data_set_double(settings, min_brightness_name, 0.25);
		obs_data_set_double(settings, strength_name, 1.0);
	}

	bool updated = update_filter_settings(data, settings);
	obs_data_release(settings);
	return updated;
}

static bool reset_shadow_clicked(obs_properties_t *props,
				 obs_property_t *property, void *data)
{
	UNUSED_PARAMETER(props);
	UNUSED_PARAMETER(property);
	obs_data_t *settings = obs_data_create();
	obs_data_set_bool(settings, S_SHADOW_ENABLED, false);
	obs_data_set_int(settings, S_SHADOW_COLOR, 0xFF000000);
	obs_data_set_double(settings, S_SHADOW_OPACITY, 0.65);
	obs_data_set_double(settings, S_SHADOW_BLUR, 24.0);
	obs_data_set_double(settings, S_SHADOW_OFFSET_X, 12.0);
	obs_data_set_double(settings, S_SHADOW_OFFSET_Y, 12.0);
	bool updated = update_filter_settings(data, settings);
	obs_data_release(settings);
	return updated;
}

static bool reset_glow_clicked(obs_properties_t *props,
			       obs_property_t *property, void *data)
{
	UNUSED_PARAMETER(props);
	UNUSED_PARAMETER(property);
	obs_data_t *settings = obs_data_create();
	obs_data_set_bool(settings, S_GLOW_ENABLED, false);
	obs_data_set_int(settings, S_GLOW_COLOR, 0xFFFFFFFF);
	obs_data_set_int(settings, S_GLOW_BLEND_MODE,
			 GLOW_BLEND_ADDITIVE);
	obs_data_set_double(settings, S_GLOW_AMOUNT, 1.0);
	obs_data_set_double(settings, S_GLOW_WIDTH, 30.0);
	bool updated = update_filter_settings(data, settings);
	obs_data_release(settings);
	return updated;
}

static obs_properties_t *dal_properties(void *data)
{
	obs_properties_t *props = obs_properties_create();

	char header_text[2048] = {0};
	char *header_path =
		obs_module_file("images/vtuber-effects-header.png");
	if (header_path)
		vtuber_effects_make_header_html(
			header_path, header_text, sizeof(header_text));
	obs_property_t *header = obs_properties_add_text(
		props, "plugin_header", header_text, OBS_TEXT_INFO);
	obs_property_text_set_info_type(header, OBS_TEXT_INFO_NORMAL);
	obs_property_text_set_info_word_wrap(header, false);
	bfree(header_path);

	char version_text[128];
	snprintf(version_text, sizeof(version_text), "%s %s",
		 obs_module_text("Header.Version"), PLUGIN_VERSION);
	obs_property_t *version = obs_properties_add_text(
		props, "plugin_version", version_text, OBS_TEXT_INFO);
	obs_property_text_set_info_type(version, OBS_TEXT_INFO_NORMAL);
	obs_property_text_set_info_word_wrap(version, false);
	vtuber_effects_align_header_rows(version_text);

	obs_property_t *environment = obs_properties_add_list(
		props, S_ENVIRONMENT, obs_module_text("Environment.Source"),
		OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(environment, obs_module_text("Environment.None"), "");
	obs_enum_sources(source_enum_callback, environment);
	set_tooltip(environment, "Environment.Source.Tooltip");
	obs_property_set_modified_callback(environment,
					   environment_modified);

	obs_property_t *rim_environment = obs_properties_add_list(
		props, S_RIM_ENVIRONMENT,
		obs_module_text("Environment.RimSource"),
		OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(
		rim_environment,
		obs_module_text("Environment.RimSource.Same"), "");
	obs_enum_sources(source_enum_callback, rim_environment);
	set_tooltip(rim_environment, "Environment.RimSource.Tooltip");

	obs_property_t *environment_warning = obs_properties_add_text(
		props, S_ENVIRONMENT_WARNING,
		obs_module_text("Environment.Warning"), OBS_TEXT_INFO);
	obs_property_text_set_info_type(environment_warning,
					OBS_TEXT_INFO_ERROR);
	obs_property_text_set_info_word_wrap(environment_warning, true);

	obs_property_t *debug = obs_properties_add_list(
		props, S_DEBUG_VIEW, obs_module_text("Debug.View"),
		OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(debug, obs_module_text("Debug.Final"), DEBUG_FINAL);
	obs_property_list_add_int(debug, obs_module_text("Debug.Environment"), DEBUG_ENVIRONMENT);
	obs_property_list_add_int(debug, obs_module_text("Debug.Ambient"), DEBUG_AMBIENT);
	obs_property_list_add_int(debug, obs_module_text("Debug.Rim"), DEBUG_RIM);
	obs_property_list_add_int(debug, obs_module_text("Debug.Emissive"), DEBUG_EMISSIVE_MASK);
	obs_property_list_add_int(debug, obs_module_text("Debug.Original"), DEBUG_ORIGINAL);
	obs_property_list_add_int(debug, obs_module_text("Debug.Shadow"), DEBUG_SHADOW);
	obs_property_list_add_int(debug, obs_module_text("Debug.Glow"), DEBUG_GLOW);
	obs_property_list_add_int(debug,
				  obs_module_text("Debug.EmissiveBloom"),
				  DEBUG_EMISSIVE_BLOOM);
	set_tooltip(debug, "Debug.View.Tooltip");

	obs_properties_t *ambient = obs_properties_create();
	obs_properties_add_bool(ambient, S_AMBIENT_ENABLED, obs_module_text("Ambient.Enabled"));
	obs_properties_add_float_slider(ambient, S_AMBIENT_BASE, obs_module_text("Ambient.Base"),
					0.0, 2.0, 0.01);
	obs_properties_add_float_slider(ambient, S_AMBIENT_AMOUNT, obs_module_text("Ambient.Amount"),
					0.0, 5.0, 0.01);
	obs_properties_add_float_slider(ambient, S_AMBIENT_BLUR, obs_module_text("Ambient.Blur"),
					0.0, 500.0, 1.0);
	set_tooltip(obs_properties_get(ambient, S_AMBIENT_ENABLED),
		    "Ambient.Group.Tooltip");
	set_tooltip(obs_properties_get(ambient, S_AMBIENT_BASE), "Ambient.Base.Tooltip");
	set_tooltip(obs_properties_get(ambient, S_AMBIENT_AMOUNT), "Ambient.Amount.Tooltip");
	set_tooltip(obs_properties_get(ambient, S_AMBIENT_BLUR), "Ambient.Blur.Tooltip");
	obs_properties_add_button2(
		ambient, "reset_ambient", obs_module_text("Ambient.Reset"),
		reset_ambient_clicked, data);
	obs_property_t *ambient_group = obs_properties_add_group(
		props, "ambient_group", obs_module_text("Ambient.Group"),
		OBS_GROUP_NORMAL, ambient);
	set_tooltip(ambient_group, "Ambient.Group.Tooltip");

	obs_properties_t *environment_processing =
		obs_properties_create();
	obs_properties_add_bool(
		environment_processing, S_EXPOSURE_ENABLED,
		obs_module_text("Processing.ExposureEnabled"));
	obs_properties_add_bool(
		environment_processing, S_EXPOSURE_AFFECTS_RIM,
		obs_module_text("Processing.ExposureAffectsRim"));
	obs_properties_add_float_slider(
		environment_processing, S_EXPOSURE_TARGET,
		obs_module_text("Processing.ExposureTarget"), 0.10, 1.50,
		0.01);
	obs_properties_add_float_slider(
		environment_processing, S_EXPOSURE_STRENGTH,
		obs_module_text("Processing.ExposureStrength"), 0.0, 1.0,
		0.01);
	obs_properties_add_float_slider(
		environment_processing, S_EXPOSURE_MIN,
		obs_module_text("Processing.ExposureMin"), 0.10, 1.0,
		0.01);
	obs_properties_add_float_slider(
		environment_processing, S_EXPOSURE_MAX,
		obs_module_text("Processing.ExposureMax"), 1.0, 5.0,
		0.01);
	obs_properties_add_float_slider(
		environment_processing, S_COLOR_SATURATION,
		obs_module_text("Processing.Saturation"), 0.0, 2.0, 0.01);
	obs_properties_add_float_slider(
		environment_processing, S_COLOR_VIBRANCE,
		obs_module_text("Processing.Vibrance"), -1.0, 1.0, 0.01);
	obs_properties_add_float_slider(
		environment_processing, S_COLOR_CONTRAST,
		obs_module_text("Processing.Contrast"), 0.0, 2.0, 0.01);
	obs_properties_add_float_slider(
		environment_processing, S_COLOR_LIMIT,
		obs_module_text("Processing.ColorLimit"), 0.10, 2.0, 0.01);
	set_tooltip(
		obs_properties_get(environment_processing,
				   S_EXPOSURE_ENABLED),
		"Processing.ExposureEnabled.Tooltip");
	set_tooltip(
		obs_properties_get(environment_processing,
				   S_EXPOSURE_AFFECTS_RIM),
		"Processing.ExposureAffectsRim.Tooltip");
	set_tooltip(
		obs_properties_get(environment_processing,
				   S_EXPOSURE_TARGET),
		"Processing.ExposureTarget.Tooltip");
	set_tooltip(
		obs_properties_get(environment_processing,
				   S_EXPOSURE_STRENGTH),
		"Processing.ExposureStrength.Tooltip");
	set_tooltip(
		obs_properties_get(environment_processing, S_EXPOSURE_MIN),
		"Processing.ExposureMin.Tooltip");
	set_tooltip(
		obs_properties_get(environment_processing, S_EXPOSURE_MAX),
		"Processing.ExposureMax.Tooltip");
	set_tooltip(
		obs_properties_get(environment_processing,
				   S_COLOR_SATURATION),
		"Processing.Saturation.Tooltip");
	set_tooltip(
		obs_properties_get(environment_processing,
				   S_COLOR_VIBRANCE),
		"Processing.Vibrance.Tooltip");
	set_tooltip(
		obs_properties_get(environment_processing, S_COLOR_CONTRAST),
		"Processing.Contrast.Tooltip");
	set_tooltip(
		obs_properties_get(environment_processing, S_COLOR_LIMIT),
		"Processing.ColorLimit.Tooltip");
	obs_properties_add_button2(
		environment_processing, "reset_environment_processing",
		obs_module_text("Processing.Reset"),
		reset_environment_processing_clicked, data);
	obs_property_t *environment_processing_group =
		obs_properties_add_group(
			props, "environment_processing_group",
			obs_module_text("Processing.Group"), OBS_GROUP_NORMAL,
			environment_processing);
	set_tooltip(environment_processing_group,
		    "Processing.Group.Tooltip");

	obs_properties_t *rim = obs_properties_create();
	obs_properties_add_bool(rim, S_RIM_ENABLED, obs_module_text("Rim.Enabled"));
	obs_properties_add_float_slider(rim, S_RIM_AMOUNT, obs_module_text("Rim.Amount"),
					0.0, 5.0, 0.01);
	obs_properties_add_float_slider(rim, S_RIM_COLOR_AMOUNT,
					obs_module_text("Rim.ColorAmount"),
					0.0, 5.0, 0.01);
	obs_properties_add_float_slider(rim, S_RIM_LAYER_BASE,
					obs_module_text("Rim.LayerBase"),
					0.0, 3.0, 0.01);
	obs_properties_add_float_slider(
		rim, S_RIM_DARKNESS_CUTOFF,
		obs_module_text("Rim.DarknessCutoff"), 0.0, 0.75, 0.01);
	obs_property_t *rim_blend = obs_properties_add_list(
		rim, S_RIM_BLEND_MODE, obs_module_text("Rim.BlendMode"),
		OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(rim_blend, obs_module_text("Rim.Blend.Additive"),
				  RIM_BLEND_ADDITIVE);
	obs_property_list_add_int(rim_blend, obs_module_text("Rim.Blend.Screen"),
				  RIM_BLEND_SCREEN);
	obs_property_list_add_int(rim_blend, obs_module_text("Rim.Blend.Normal"),
				  RIM_BLEND_NORMAL);
	obs_property_list_add_int(
		rim_blend, obs_module_text("Rim.Blend.MaskedDuplicate"),
		RIM_BLEND_MASKED_DUPLICATE);
	obs_property_t *rim_position = obs_properties_add_list(
		rim, S_RIM_POSITION_MODE,
		obs_module_text("Rim.PositionMode"), OBS_COMBO_TYPE_LIST,
		OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(
		rim_position,
		obs_module_text("Rim.Position.LocalSilhouette"),
		RIM_POSITION_LOCAL_SILHOUETTE);
	obs_property_list_add_int(
		rim_position,
		obs_module_text("Rim.Position.ScaledDuplicate"),
		RIM_POSITION_SCALED_DUPLICATE);
	obs_property_set_modified_callback(rim_position,
					   rim_position_mode_modified);
	obs_properties_add_float_slider(rim, S_RIM_WIDTH, obs_module_text("Rim.Width"),
					0.0, 50.0, 0.25);
	obs_properties_add_float_slider(rim, S_RIM_SOFTNESS, obs_module_text("Rim.Softness"),
					0.01, 1.0, 0.01);
	obs_properties_add_float_slider(
		rim, S_RIM_LOCAL_EXPANSION,
		obs_module_text("Rim.LocalExpansion"), -100.0, 100.0,
		0.25);
	obs_properties_add_float_slider(rim, S_RIM_SCALE, obs_module_text("Rim.Scale"),
					0.50, 1.50, 0.005);
	obs_properties_add_bool(
		rim, S_RIM_AUTO_PIVOT,
		obs_module_text("Rim.AutoPivot"));
	obs_properties_add_bool(
		rim, S_RIM_STABLE_TRACKING,
		obs_module_text("Rim.StableTracking"));
	obs_properties_add_float_slider(
		rim, S_RIM_PIVOT_X, obs_module_text("Rim.PivotX"), 0.0,
		100.0, 0.5);
	obs_properties_add_float_slider(
		rim, S_RIM_PIVOT_Y, obs_module_text("Rim.PivotY"), 0.0,
		100.0, 0.5);
	obs_properties_add_float_slider(rim, S_RIM_OFFSET_X, obs_module_text("Rim.OffsetX"),
					-50.0, 50.0, 0.25);
	obs_properties_add_float_slider(rim, S_RIM_OFFSET_Y, obs_module_text("Rim.OffsetY"),
					-50.0, 50.0, 0.25);
	set_tooltip(obs_properties_get(rim, S_RIM_ENABLED),
		    "Rim.Group.Tooltip");
	set_tooltip(obs_properties_get(rim, S_RIM_AMOUNT), "Rim.Amount.Tooltip");
	set_tooltip(obs_properties_get(rim, S_RIM_COLOR_AMOUNT), "Rim.ColorAmount.Tooltip");
	set_tooltip(obs_properties_get(rim, S_RIM_LAYER_BASE), "Rim.LayerBase.Tooltip");
	set_tooltip(obs_properties_get(rim, S_RIM_DARKNESS_CUTOFF),
		    "Rim.DarknessCutoff.Tooltip");
	set_tooltip(rim_blend, "Rim.BlendMode.Tooltip");
	set_tooltip(rim_position, "Rim.PositionMode.Tooltip");
	set_tooltip(obs_properties_get(rim, S_RIM_WIDTH), "Rim.Width.Tooltip");
	set_tooltip(obs_properties_get(rim, S_RIM_SOFTNESS), "Rim.Softness.Tooltip");
	set_tooltip(obs_properties_get(rim, S_RIM_LOCAL_EXPANSION),
		    "Rim.LocalExpansion.Tooltip");
	set_tooltip(obs_properties_get(rim, S_RIM_SCALE), "Rim.Scale.Tooltip");
	set_tooltip(obs_properties_get(rim, S_RIM_AUTO_PIVOT),
		    "Rim.AutoPivot.Tooltip");
	set_tooltip(obs_properties_get(rim, S_RIM_STABLE_TRACKING),
		    "Rim.StableTracking.Tooltip");
	set_tooltip(obs_properties_get(rim, S_RIM_PIVOT_X),
		    "Rim.PivotX.Tooltip");
	set_tooltip(obs_properties_get(rim, S_RIM_PIVOT_Y),
		    "Rim.PivotY.Tooltip");
	set_tooltip(obs_properties_get(rim, S_RIM_OFFSET_X), "Rim.OffsetX.Tooltip");
	set_tooltip(obs_properties_get(rim, S_RIM_OFFSET_Y), "Rim.OffsetY.Tooltip");
	obs_properties_add_button2(
		rim, "reset_rim", obs_module_text("Rim.Reset"),
		reset_rim_clicked, data);
	obs_property_t *rim_group = obs_properties_add_group(
		props, "rim_group", obs_module_text("Rim.Group"),
		OBS_GROUP_NORMAL, rim);
	set_tooltip(rim_group, "Rim.Group.Tooltip");

	obs_properties_t *keys = obs_properties_create();
	obs_property_t *keys_enabled =
		obs_properties_add_bool(keys, S_KEYS_ENABLED, obs_module_text("Keys.Enabled"));
	obs_property_t *key_count = obs_properties_add_int_slider(
		keys, S_KEY_COUNT, obs_module_text("Keys.Count"), 0, KEY_COUNT, 1);
	obs_properties_add_bool(keys, S_BLOOM_ENABLED,
				obs_module_text("Bloom.Enabled"));
	obs_properties_add_float_slider(
		keys, S_BLOOM_AMOUNT, obs_module_text("Bloom.Amount"), 0.0,
		5.0, 0.01);
	obs_properties_add_float_slider(
		keys, S_BLOOM_RADIUS, obs_module_text("Bloom.Radius"), 0.0,
		60.0, 0.5);
	set_tooltip(obs_properties_get(keys, S_BLOOM_ENABLED),
		    "Bloom.Enabled.Tooltip");
	set_tooltip(obs_properties_get(keys, S_BLOOM_AMOUNT),
		    "Bloom.Amount.Tooltip");
	set_tooltip(obs_properties_get(keys, S_BLOOM_RADIUS),
		    "Bloom.Radius.Tooltip");
	for (int i = 0; i < KEY_COUNT; i++) {
		char group_name[32];
		char color_name[32];
		char tolerance_name[32];
		char softness_name[32];
		char min_saturation_name[32];
		char min_brightness_name[32];
		char strength_name[32];
		char label[64];
		key_setting_name(group_name, sizeof(group_name), i, "group");
		key_setting_name(color_name, sizeof(color_name), i, "color");
		key_setting_name(tolerance_name, sizeof(tolerance_name), i, "tolerance");
		key_setting_name(softness_name, sizeof(softness_name), i, "softness");
		key_setting_name(min_saturation_name, sizeof(min_saturation_name), i,
				 "min_saturation");
		key_setting_name(min_brightness_name, sizeof(min_brightness_name), i,
				 "min_brightness");
		key_setting_name(strength_name, sizeof(strength_name), i, "strength");
		snprintf(label, sizeof(label), "%s %d", obs_module_text("Keys.Color"), i + 1);

		obs_properties_t *key = obs_properties_create();
		obs_property_t *color =
			obs_properties_add_color(key, color_name, obs_module_text("Keys.Color"));
		obs_property_t *tolerance = obs_properties_add_float_slider(
			key, tolerance_name, obs_module_text("Keys.Tolerance"),
			0.0, 0.5, 0.005);
		obs_property_t *softness = obs_properties_add_float_slider(
			key, softness_name, obs_module_text("Keys.Softness"),
			0.001, 0.25, 0.001);
		obs_property_t *min_saturation = obs_properties_add_float_slider(
			key, min_saturation_name, obs_module_text("Keys.MinSaturation"),
			0.0, 1.0, 0.01);
		obs_property_t *min_brightness = obs_properties_add_float_slider(
			key, min_brightness_name, obs_module_text("Keys.MinBrightness"),
			0.0, 1.0, 0.01);
		obs_property_t *strength = obs_properties_add_float_slider(
			key, strength_name, obs_module_text("Keys.Strength"),
			0.0, 1.0, 0.01);
		set_tooltip(color, "Keys.Color.Tooltip");
		set_tooltip(tolerance, "Keys.Tolerance.Tooltip");
		set_tooltip(softness, "Keys.Softness.Tooltip");
		set_tooltip(min_saturation, "Keys.MinSaturation.Tooltip");
		set_tooltip(min_brightness, "Keys.MinBrightness.Tooltip");
		set_tooltip(strength, "Keys.Strength.Tooltip");
		obs_property_t *key_group = obs_properties_add_group(
			keys, group_name, label, OBS_GROUP_NORMAL, key);
		set_tooltip(key_group, "Keys.ColorGroup.Tooltip");
	}
	obs_property_set_modified_callback(key_count, key_count_modified);
	set_tooltip(keys_enabled, "Keys.Group.Tooltip");
	set_tooltip(key_count, "Keys.Count.Tooltip");
	obs_properties_add_button2(
		keys, "reset_keys", obs_module_text("Keys.Reset"),
		reset_keys_clicked, data);
	obs_property_t *keys_group = obs_properties_add_group(
		props, "keys_group", obs_module_text("Keys.Group"),
		OBS_GROUP_NORMAL, keys);
	set_tooltip(keys_group, "Keys.Group.Tooltip");

	obs_properties_t *shadow = obs_properties_create();
	obs_properties_add_bool(shadow, S_SHADOW_ENABLED, obs_module_text("Shadow.Enabled"));
	obs_properties_add_color(shadow, S_SHADOW_COLOR, obs_module_text("Shadow.Color"));
	obs_properties_add_float_slider(shadow, S_SHADOW_OPACITY,
					obs_module_text("Shadow.Opacity"), 0.0, 1.0, 0.01);
	obs_properties_add_float_slider(shadow, S_SHADOW_BLUR,
					obs_module_text("Shadow.Blur"), 0.0, 100.0, 0.5);
	obs_properties_add_float_slider(shadow, S_SHADOW_OFFSET_X,
					obs_module_text("Shadow.OffsetX"), -100.0, 100.0, 0.5);
	obs_properties_add_float_slider(shadow, S_SHADOW_OFFSET_Y,
					obs_module_text("Shadow.OffsetY"), -100.0, 100.0, 0.5);
	set_tooltip(obs_properties_get(shadow, S_SHADOW_ENABLED),
		    "Shadow.Group.Tooltip");
	set_tooltip(obs_properties_get(shadow, S_SHADOW_COLOR), "Shadow.Color.Tooltip");
	set_tooltip(obs_properties_get(shadow, S_SHADOW_OPACITY), "Shadow.Opacity.Tooltip");
	set_tooltip(obs_properties_get(shadow, S_SHADOW_BLUR), "Shadow.Blur.Tooltip");
	set_tooltip(obs_properties_get(shadow, S_SHADOW_OFFSET_X), "Shadow.OffsetX.Tooltip");
	set_tooltip(obs_properties_get(shadow, S_SHADOW_OFFSET_Y), "Shadow.OffsetY.Tooltip");
	obs_properties_add_button2(
		shadow, "reset_shadow", obs_module_text("Shadow.Reset"),
		reset_shadow_clicked, data);
	obs_property_t *shadow_group = obs_properties_add_group(
		props, "shadow_group", obs_module_text("Shadow.Group"),
		OBS_GROUP_NORMAL, shadow);
	set_tooltip(shadow_group, "Shadow.Group.Tooltip");

	obs_properties_t *glow = obs_properties_create();
	obs_properties_add_bool(glow, S_GLOW_ENABLED, obs_module_text("Glow.Enabled"));
	obs_properties_add_color(glow, S_GLOW_COLOR,
				 obs_module_text("Glow.Color"));
	obs_property_t *glow_blend = obs_properties_add_list(
		glow, S_GLOW_BLEND_MODE, obs_module_text("Glow.BlendMode"),
		OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(glow_blend,
				  obs_module_text("Glow.Blend.Additive"),
				  GLOW_BLEND_ADDITIVE);
	obs_property_list_add_int(glow_blend,
				  obs_module_text("Glow.Blend.Screen"),
				  GLOW_BLEND_SCREEN);
	obs_property_list_add_int(glow_blend,
				  obs_module_text("Glow.Blend.Normal"),
				  GLOW_BLEND_NORMAL);
	obs_properties_add_float_slider(glow, S_GLOW_AMOUNT,
					obs_module_text("Glow.Amount"), 0.0, 5.0, 0.01);
	obs_properties_add_float_slider(glow, S_GLOW_WIDTH,
					obs_module_text("Glow.Width"), 0.0, 100.0, 0.5);
	set_tooltip(obs_properties_get(glow, S_GLOW_ENABLED),
		    "Glow.Group.Tooltip");
	set_tooltip(obs_properties_get(glow, S_GLOW_COLOR), "Glow.Color.Tooltip");
	set_tooltip(glow_blend, "Glow.BlendMode.Tooltip");
	set_tooltip(obs_properties_get(glow, S_GLOW_AMOUNT), "Glow.Amount.Tooltip");
	set_tooltip(obs_properties_get(glow, S_GLOW_WIDTH), "Glow.Width.Tooltip");
	obs_properties_add_button2(
		glow, "reset_glow", obs_module_text("Glow.Reset"),
		reset_glow_clicked, data);
	obs_property_t *glow_group = obs_properties_add_group(
		props, "glow_group", obs_module_text("Glow.Group"),
		OBS_GROUP_NORMAL, glow);
	set_tooltip(glow_group, "Glow.Group.Tooltip");

	obs_properties_t *presets = obs_properties_create();
	obs_properties_add_button2(
		presets, "preset_export", obs_module_text("Preset.Export"),
		export_preset_clicked, data);
	obs_properties_add_button2(
		presets, "preset_import", obs_module_text("Preset.Import"),
		import_preset_clicked, data);
	obs_property_t *preset_status = obs_properties_add_text(
		presets, S_PRESET_STATUS,
		obs_module_text("Preset.Status.Ready"), OBS_TEXT_INFO);
	obs_property_text_set_info_word_wrap(preset_status, true);
	obs_property_t *preset_group = obs_properties_add_group(
		props, "preset_group", obs_module_text("Preset.Group"),
		OBS_GROUP_NORMAL, presets);
	set_tooltip(preset_group, "Preset.Group.Tooltip");

	return props;
}

static bool render_blur_pass(struct dal_filter *filter, gs_texture_t *input,
			     gs_texrender_t *output, uint32_t width, uint32_t height,
			     float sample_offset, const char *technique)
{
	if (!filter->blur_effect || !input || !output)
		return false;

	gs_texrender_reset(output);
	if (!gs_texrender_begin(output, width, height))
		return false;

	struct vec4 clear = {0.0f, 0.0f, 0.0f, 0.0f};
	const uint32_t input_width = gs_texture_get_width(input);
	const uint32_t input_height = gs_texture_get_height(input);
	struct vec2 texel_size = {
		1.0f / (float)(input_width ? input_width : 1),
		1.0f / (float)(input_height ? input_height : 1),
	};

	gs_viewport_push();
	gs_projection_push();
	gs_matrix_push();
	gs_matrix_identity();
	gs_clear(GS_CLEAR_COLOR, &clear, 0.0f, 0);
	gs_ortho(0.0f, (float)width, 0.0f, (float)height, -100.0f, 100.0f);
	gs_set_viewport(0, 0, width, height);

	gs_effect_set_texture(filter->p_blur_image, input);
	gs_effect_set_vec2(filter->p_blur_texel_size, &texel_size);
	gs_effect_set_float(filter->p_blur_offset, sample_offset);
	while (gs_effect_loop(filter->blur_effect, technique))
		gs_draw_sprite(input, 0, width, height);

	gs_matrix_pop();
	gs_projection_pop();
	gs_viewport_pop();
	gs_texrender_end(output);
	return true;
}

static int blur_levels_for_radius(float radius)
{
	if (radius <= 0.01f)
		return 0;

	int levels = (int)ceilf(log2f(radius * 0.5f + 1.0f));
	if (levels < 1)
		levels = 1;
	if (levels > BLUR_LEVEL_COUNT)
		levels = BLUR_LEVEL_COUNT;
	return levels;
}

static gs_texture_t *render_environment(struct dal_filter *filter,
					obs_source_t *source,
					gs_texrender_t *source_render,
					uint32_t width, uint32_t height)
{
	obs_source_t *parent = obs_filter_get_parent(filter->context);
	if (!source || !source_render || source == parent ||
	    source == filter->context || width == 0 || height == 0)
		return NULL;

	const uint32_t env_width = obs_source_get_width(source);
	const uint32_t env_height = obs_source_get_height(source);
	if (env_width == 0 || env_height == 0)
		return NULL;

	gs_texrender_reset(source_render);
	if (!gs_texrender_begin(source_render, width, height))
		return NULL;

	struct vec4 clear = {0.0f, 0.0f, 0.0f, 0.0f};
	gs_viewport_push();
	gs_projection_push();
	gs_matrix_push();
	gs_matrix_identity();
	gs_clear(GS_CLEAR_COLOR, &clear, 0.0f, 0);
	gs_ortho(0.0f, (float)env_width, 0.0f, (float)env_height, -100.0f, 100.0f);
	gs_set_viewport(0, 0, width, height);
	obs_source_video_render(source);
	gs_matrix_pop();
	gs_projection_pop();
	gs_viewport_pop();
	gs_texrender_end(source_render);

	gs_texture_t *texture = gs_texrender_get_texture(source_render);
	const int levels = blur_levels_for_radius(filter->ambient_blur);
	if (levels == 0) {
		if (!render_blur_pass(filter, texture, filter->blur_down[0],
				      width, height, 1.0f, "AlphaWeight"))
			return NULL;
		return gs_texrender_get_texture(filter->blur_down[0]);
	}

	uint32_t level_widths[BLUR_LEVEL_COUNT];
	uint32_t level_heights[BLUR_LEVEL_COUNT];
	uint32_t pass_width = width;
	uint32_t pass_height = height;
	const float level_floor = exp2f((float)(levels - 1));
	const float residual = fmaxf(filter->ambient_blur / (level_floor * 2.0f), 1.0f);
	const float sample_offset = fminf(residual, 2.5f);

	for (int i = 0; i < levels; i++) {
		pass_width = pass_width > 1 ? pass_width / 2 : 1;
		pass_height = pass_height > 1 ? pass_height / 2 : 1;
		level_widths[i] = pass_width;
		level_heights[i] = pass_height;

		const char *technique =
			i == 0 ? "DownsampleAlphaWeighted" : "Downsample";
		if (!render_blur_pass(filter, texture, filter->blur_down[i],
				      pass_width, pass_height, sample_offset,
				      technique))
			return texture;
		texture = gs_texrender_get_texture(filter->blur_down[i]);
	}

	int up_index = 0;
	for (int i = levels - 2; i >= 0; i--, up_index++) {
		if (!render_blur_pass(filter, texture, filter->blur_up[up_index],
				      level_widths[i], level_heights[i], sample_offset,
				      "Upsample"))
			return texture;
		texture = gs_texrender_get_texture(filter->blur_up[up_index]);
	}

	if (!render_blur_pass(filter, texture, filter->blur_up[up_index], width, height,
			      sample_offset, "Upsample"))
		return texture;

	return gs_texrender_get_texture(filter->blur_up[up_index]);
}

static void dal_render(void *data, gs_effect_t *unused)
{
	UNUSED_PARAMETER(unused);
	struct dal_filter *filter = data;
	obs_source_t *target = obs_filter_get_target(filter->context);

	if (!target || !filter->effect || !filter->blur_effect ||
	    !filter->environment_render ||
	    !filter->environment_final_render ||
	    !filter->rim_environment_render) {
		obs_source_skip_video_filter(filter->context);
		return;
	}

	const uint32_t width = obs_source_get_base_width(target);
	const uint32_t height = obs_source_get_base_height(target);
	gs_texture_t *environment_texture = render_environment(
		filter, filter->environment, filter->environment_render,
		width, height);
	gs_texture_t *background_texture =
		gs_texrender_get_texture(filter->environment_render);
	if (!environment_texture || !background_texture) {
		obs_source_skip_video_filter(filter->context);
		return;
	}

	if (!render_blur_pass(filter, environment_texture,
			      filter->environment_final_render, width, height,
			      1.0f, "Copy")) {
		obs_source_skip_video_filter(filter->context);
		return;
	}
	environment_texture =
		gs_texrender_get_texture(filter->environment_final_render);

	obs_source_t *rim_source = filter->rim_environment
					   ? filter->rim_environment
					   : filter->environment;
	gs_texture_t *rim_environment_texture = environment_texture;
	if (rim_source != filter->environment) {
		rim_environment_texture = render_environment(
			filter, rim_source, filter->rim_environment_render,
			width, height);
		if (!rim_environment_texture)
			rim_environment_texture = environment_texture;
	}

	if (!obs_source_process_filter_begin(filter->context, GS_RGBA, OBS_NO_DIRECT_RENDERING)) {
		obs_source_skip_video_filter(filter->context);
		return;
	}

	struct vec2 texel_size = {1.0f / (float)width, 1.0f / (float)height};
	gs_effect_set_texture(filter->p_environment_image, environment_texture);
	gs_effect_set_texture(filter->p_rim_environment_image,
			      rim_environment_texture);
	gs_effect_set_texture(filter->p_background_image, background_texture);
	gs_effect_set_vec2(filter->p_texel_size, &texel_size);
	gs_effect_set_bool(filter->p_ambient_enabled, filter->ambient_enabled);
	gs_effect_set_float(filter->p_ambient_base, filter->ambient_base);
	gs_effect_set_float(filter->p_ambient_amount, filter->ambient_amount);
	gs_effect_set_bool(filter->p_exposure_enabled,
			   filter->exposure_enabled);
	gs_effect_set_bool(filter->p_exposure_affects_rim,
			   filter->exposure_affects_rim);
	gs_effect_set_float(filter->p_exposure_target,
			    filter->exposure_target);
	gs_effect_set_float(filter->p_exposure_strength,
			    filter->exposure_strength);
	gs_effect_set_float(filter->p_exposure_min,
			    filter->exposure_min);
	gs_effect_set_float(filter->p_exposure_max,
			    filter->exposure_max);
	gs_effect_set_float(filter->p_color_saturation,
			    filter->color_saturation);
	gs_effect_set_float(filter->p_color_vibrance,
			    filter->color_vibrance);
	gs_effect_set_float(filter->p_color_contrast,
			    filter->color_contrast);
	gs_effect_set_float(filter->p_color_limit,
			    filter->color_limit);
	gs_effect_set_bool(filter->p_rim_enabled, filter->rim_enabled);
	gs_effect_set_float(filter->p_rim_amount, filter->rim_amount);
	gs_effect_set_float(filter->p_rim_color_amount, filter->rim_color_amount);
	gs_effect_set_float(filter->p_rim_layer_base, filter->rim_layer_base);
	gs_effect_set_float(filter->p_rim_darkness_cutoff,
			    filter->rim_darkness_cutoff);
	gs_effect_set_int(filter->p_rim_blend_mode, filter->rim_blend_mode);
	gs_effect_set_int(filter->p_rim_position_mode,
			  filter->rim_position_mode);
	gs_effect_set_float(filter->p_rim_width, filter->rim_width);
	gs_effect_set_float(filter->p_rim_softness, filter->rim_softness);
	gs_effect_set_float(filter->p_rim_local_expansion,
			    filter->rim_local_expansion);
	gs_effect_set_float(filter->p_rim_scale, filter->rim_scale);
	gs_effect_set_bool(filter->p_rim_auto_pivot,
			   filter->rim_auto_pivot);
	gs_effect_set_bool(filter->p_rim_stable_tracking,
			   filter->rim_stable_tracking);
	gs_effect_set_vec2(filter->p_rim_pivot, &filter->rim_pivot);
	gs_effect_set_vec2(filter->p_rim_offset, &filter->rim_offset);
	gs_effect_set_bool(filter->p_keys_enabled, filter->keys_enabled);
	gs_effect_set_bool(filter->p_bloom_enabled, filter->bloom_enabled);
	gs_effect_set_float(filter->p_bloom_amount,
			    filter->bloom_amount);
	gs_effect_set_float(filter->p_bloom_radius,
			    filter->bloom_radius);
	gs_effect_set_bool(filter->p_shadow_enabled, filter->shadow_enabled);
	gs_effect_set_vec4(filter->p_shadow_color, &filter->shadow_color);
	gs_effect_set_float(filter->p_shadow_opacity, filter->shadow_opacity);
	gs_effect_set_float(filter->p_shadow_blur, filter->shadow_blur);
	gs_effect_set_vec2(filter->p_shadow_offset, &filter->shadow_offset);
	gs_effect_set_bool(filter->p_glow_enabled, filter->glow_enabled);
	gs_effect_set_float(filter->p_glow_amount, filter->glow_amount);
	gs_effect_set_vec4(filter->p_glow_color, &filter->glow_color);
	gs_effect_set_int(filter->p_glow_blend_mode,
			  filter->glow_blend_mode);
	gs_effect_set_float(filter->p_glow_width, filter->glow_width);
	gs_effect_set_int(filter->p_debug_view, filter->debug_view);

	for (int i = 0; i < KEY_COUNT; i++) {
		gs_effect_set_vec4(filter->p_key_colors[i], &filter->key_colors[i]);
		gs_effect_set_float(filter->p_key_tolerances[i],
				    filter->key_tolerances[i]);
		gs_effect_set_float(filter->p_key_softnesses[i],
				    filter->key_softnesses[i]);
		gs_effect_set_float(filter->p_key_min_saturations[i],
				    filter->key_min_saturations[i]);
		gs_effect_set_float(filter->p_key_min_brightnesses[i],
				    filter->key_min_brightnesses[i]);
		gs_effect_set_float(filter->p_key_strengths[i],
				    filter->key_strengths[i]);
	}

	obs_source_process_filter_end(filter->context, filter->effect, width, height);
}

struct obs_source_info dynamic_avatar_lighting_filter = {
	.id = FILTER_ID,
	.type = OBS_SOURCE_TYPE_FILTER,
	.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW,
	.get_name = dal_get_name,
	.create = dal_create,
	.destroy = dal_destroy,
	.update = dal_update,
	.get_defaults = dal_defaults,
	.get_properties = dal_properties,
	.video_tick = dal_tick,
	.video_render = dal_render,
};
