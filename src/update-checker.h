#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void vtuber_effects_start_update_checker(const char *current_version);
void vtuber_effects_stop_update_checker(void);
void vtuber_effects_check_for_updates(const char *current_version);

#ifdef __cplusplus
}
#endif
