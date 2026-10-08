#ifndef BONGO_CAT_MVER_CONFIG_TEXT_H
#define BONGO_CAT_MVER_CONFIG_TEXT_H

#include "mver_config.h"

#define MVER_CONFIG_MAX_BYTES (16 * 1024 * 1024)
char *mver_text_read(const char *path, size_t *length);
#endif
