#ifndef BONGO_CAT_SAFE_ARCHIVE_H
#define BONGO_CAT_SAFE_ARCHIVE_H
#include <stdbool.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
int bongo_safe_archive_extract(const char *source, const char *base,
    char *directory, char *temporary, size_t capacity, char *error, size_t error_capacity);
bool bongo_safe_sdk_extract(const char *source, const char *target,
    char *core, size_t capacity);
#ifdef __cplusplus
}
#endif
#endif
