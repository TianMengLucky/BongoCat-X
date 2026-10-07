#ifndef BONGO_CAT_RHI_PIXELS_H
#define BONGO_CAT_RHI_PIXELS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* GPU captures are top-down and may have padded rows. Present ops expose
   tightly packed bottom-up rows, with GL-style bottom-left rectangle origins.
   Channels stay premultiplied; only row order and RGBA/BGRA are converted. */
static inline bool bongo_cat_rhi_copy_pixels(const uint8_t *source,
    int source_width, int source_height, size_t stride, bool source_bgra,
    int x, int y, int width, int height, bool output_bgra, void *pixels) {
    if (!source || !pixels || source_width <= 0 || source_height <= 0 ||
        x < 0 || y < 0 || width <= 0 || height <= 0 ||
        x > source_width || y > source_height ||
        width > source_width - x || height > source_height - y ||
        (size_t)source_width > SIZE_MAX / 4 ||
        stride < (size_t)source_width * 4 ||
        stride > SIZE_MAX / (size_t)source_height ||
        (size_t)width * 4 > SIZE_MAX / (size_t)height) return false;
    uint8_t *output = pixels;
    for (int row = 0; row < height; ++row) {
        const uint8_t *input = source +
            (size_t)(source_height - 1 - y - row) * stride + (size_t)x * 4;
        uint8_t *dest = output + (size_t)row * (size_t)width * 4;
        if (source_bgra == output_bgra) {
            memcpy(dest, input, (size_t)width * 4);
            continue;
        }
        for (int column = 0; column < width; ++column) {
            size_t offset = (size_t)column * 4;
            dest[offset] = input[offset + 2];
            dest[offset + 1] = input[offset + 1];
            dest[offset + 2] = input[offset];
            dest[offset + 3] = input[offset + 3];
        }
    }
    return true;
}

#endif
