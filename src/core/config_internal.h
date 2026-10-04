#ifndef BONGO_CAT_CONFIG_INTERNAL_H
#define BONGO_CAT_CONFIG_INTERNAL_H

#include "bongo_cat/config.h"

#define BONGO_CAT_SETTINGS_SCHEMA 1
#define BONGO_CAT_SESSION_SCHEMA 1
#define BONGO_CAT_CONFIG_FILE_CAP (1024u * 1024u)

/* Reads the whole configuration file (1 byte to BONGO_CAT_CONFIG_FILE_CAP).
   The caller frees *bytes with free(). */
BongoCatResult bongo_cat_config_read_bytes(const char *path,
    unsigned char **bytes, size_t *length, BongoCatError *error);
/* Atomically writes bytes: temporary file, flush, replace, directory sync. */
BongoCatResult bongo_cat_config_write_bytes(const char *path,
    const unsigned char *bytes, size_t length, const char *description,
    BongoCatError *error);

#endif
