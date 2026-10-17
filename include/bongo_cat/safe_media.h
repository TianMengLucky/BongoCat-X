#ifndef BONGO_CAT_SAFE_MEDIA_H
#define BONGO_CAT_SAFE_MEDIA_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Pixels are borrowed until the callback returns and may be modified there.
   Callbacks run synchronously on the caller; returning false cancels the job. */
typedef bool (*BongoSafeRows)(void *, unsigned char *, int, int, int, int);
typedef struct BongoSafeRowCallbacks {
    BongoSafeRows rows;
    void *consumer;
    void (*progress)(void *, float);
    void *userdata;
    bool (*cancelled)(void *);
    void *cancel_data;
} BongoSafeRowCallbacks;
int bongo_safe_png_rows(const char *path, int width, int height,
    BongoSafeRowCallbacks callbacks);
int bongo_safe_cached_png_rows(const char *path, const char *digest,
    int width, int height, BongoSafeRowCallbacks callbacks);
void bongo_safe_rgba_cache_root(const char *root);
bool bongo_safe_rgba_cache_path(const char *digest, int width, int height,
    char *path, size_t capacity);
bool bongo_safe_rgba_cache_reserve(uint64_t bytes);
void bongo_safe_rgba_cache_forget(const char *digest, int width, int height);
/* Reader used by the file-backed corruption/streaming contract tests. */
int bongo_safe_rgba_cache_read(const char *path, const char *digest,
    int width, int height, BongoSafeRowCallbacks callbacks);
#ifdef __cplusplus
}
#endif
#endif
