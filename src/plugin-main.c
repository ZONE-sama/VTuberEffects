#include <obs-module.h>
#include <plugin-support.h>

#include "dynamic-avatar-lighting.h"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

MODULE_EXPORT const char *obs_module_description(void)
{
	return "Lighting, color protection, shadow, and glow effects for transparent VTuber captures.";
}

bool obs_module_load(void)
{
	obs_register_source(&dynamic_avatar_lighting_filter);
	obs_log(LOG_INFO, "version %s loaded", PLUGIN_VERSION);
	return true;
}

void obs_module_unload(void)
{
	obs_log(LOG_INFO, "plugin unloaded");
}
