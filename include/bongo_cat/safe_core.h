#ifndef BONGO_CAT_SAFE_CORE_H
#define BONGO_CAT_SAFE_CORE_H
#include <stdbool.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
bool bongo_safe_utf8_valid(const char *text);
bool bongo_safe_utf8_normalize(const char *text, char *output, size_t capacity);
bool bongo_safe_version_valid(const char *text);
int bongo_safe_version_compare(const char *left, const char *right);
#ifdef __cplusplus
}
#endif
#endif
