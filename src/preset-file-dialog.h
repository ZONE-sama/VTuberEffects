#pragma once

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

bool vtuber_effects_choose_preset_save(char *buffer, size_t buffer_size);
bool vtuber_effects_choose_preset_open(char *buffer, size_t buffer_size);
bool vtuber_effects_make_header_html(const char *image_path, char *buffer,
				     size_t buffer_size);
void vtuber_effects_align_header_rows(const char *version_text);

#ifdef __cplusplus
}
#endif
