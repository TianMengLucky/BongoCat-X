#ifndef BONGO_CAT_IMAGE_TEXTURE_CACHE_H
#define BONGO_CAT_IMAGE_TEXTURE_CACHE_H

#include "image_internal.h"
#include "bongo_cat/path.h"

bool bongo_cat_texture_cache_path(const char *digest, int width, int height,
    char *path, size_t capacity);
bool bongo_cat_texture_cache_reserve(uint64_t bytes);

BongoCatResult bongo_cat_image_decode_cached_scaled_rows(const char *source,
    const char *digest, int max_width, int max_height,
    BongoCatImageRows consume, void *consumer, BongoCatImageProgress progress,
    void *userdata, BongoCatImageCancelled cancelled, void *cancel_data,
    BongoCatError *error);

#endif
