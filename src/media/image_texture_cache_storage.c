#include "image_texture_cache.h"
#include "bongo_cat/safe_ffi.h"
#include "bongo_cat/safe_media.h"

void bongo_cat_image_set_texture_cache_root(const char *root) {
    bongo_safe_native_texture_cache_root(root);
    bongo_safe_rgba_cache_root(root);
}

bool bongo_cat_texture_cache_path(const char *digest, int width, int height,
    char *path, size_t capacity) {
    return bongo_safe_rgba_cache_path(digest, width, height, path, capacity);
}

void bongo_cat_image_forget_texture_cache(const char *digest,
    int max_width, int max_height) {
    bongo_safe_rgba_cache_forget(digest, max_width, max_height);
}

bool bongo_cat_texture_cache_reserve(uint64_t bytes) {
    return bongo_safe_rgba_cache_reserve(bytes);
}
