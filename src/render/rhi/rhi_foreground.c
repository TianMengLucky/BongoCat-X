#include "rhi_internal.h"
#include "rhi_corners.h"
#include <limits.h>
#include <math.h>
#include <string.h>

bool bongo_cat_rhi_set_foreground(BongoCatRhi *rhi, const void *pixels,
    int width, int height, int pitch, uint64_t revision) {
    if (!rhi) return false;
    if (!pixels) {
        SDL_free(rhi->foreground_pixels);
        rhi->foreground_pixels = NULL;
        rhi->foreground_width = rhi->foreground_height = 0;
        return true;
    }
    if (width <= 0 || height <= 0 || width > INT_MAX / 4 || pitch < width * 4 ||
        (uint64_t)width * height > SIZE_MAX / 4) return false;
    if (rhi->foreground_pixels && rhi->foreground_width == width &&
        rhi->foreground_height == height && rhi->foreground_revision == revision) return true;
    size_t bytes = (size_t)width * height * 4;
    unsigned char *copy = SDL_realloc(rhi->foreground_pixels, bytes);
    if (!copy) return false;
    rhi->foreground_pixels = copy;
    for (int y = 0; y < height; ++y)
        memcpy(copy + (size_t)y * width * 4,
            (const unsigned char *)pixels + (size_t)y * pitch, (size_t)width * 4);
    rhi->foreground_width = width; rhi->foreground_height = height;
    rhi->foreground_revision = revision;
    return true;
}

static unsigned corner_coverage(int x, int y, const float p[8]) {
    float px = x + 0.5f - p[0], py = y + 0.5f - p[1];
    float sx = fminf(p[4] + 0.5f, p[2] * 0.5f);
    float sy = fminf(p[4] + 0.5f, p[3] * 0.5f);
    /* Match the four GPU corner quads; leave pixels outside their rect alone. */
    if (px < 0 || py < 0 || px >= p[2] || py >= p[3] ||
        (px >= sx && px < p[2] - sx) || (py >= sy && py < p[3] - sy)) return 255;
    float dx = fabsf(px - p[2] * 0.5f) - p[2] * 0.5f + p[4];
    float dy = fabsf(py - p[3] * 0.5f) - p[3] * 0.5f + p[4];
    float distance = hypotf(fmaxf(dx, 0), fmaxf(dy, 0)) +
        fminf(fmaxf(dx, dy), 0) - p[4];
    return (unsigned)(fmaxf(0, fminf(1, 0.5f - distance)) * 255 + 0.5f);
}

/* The existing native backends already retire a frame before exposing its
   coherent readback. Composite the cached SDL software layer at that boundary,
   preserving premultiplied alpha and the same pixels for hit tests/captures. */
bool bongo_cat_rhi_compose_foreground(const BongoCatRhi *rhi, void *pixels,
    int width, int height, size_t pitch, bool bgra) {
    if (!rhi || !rhi->foreground_pixels || !pixels ||
        width != rhi->foreground_width || height != rhi->foreground_height) return false;
    float corners[8] = {0};
    bool rounded = bongo_cat_rhi_corner_params(rhi->window, width, height, corners);
    for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x) {
        const unsigned char *source = rhi->foreground_pixels + ((size_t)y * width + x) * 4;
        unsigned a = source[3];
        if (!a) continue;
        unsigned mask = rounded ? corner_coverage(x, y, corners) : 255;
        unsigned char *dest = (unsigned char *)pixels + (size_t)y * pitch + x * 4;
        for (unsigned c = 0; c < 4; ++c) {
            unsigned index = bgra && c < 3 ? 2 - c : c;
            unsigned color = (source[index] * mask + 127) / 255 +
                (dest[c] * (255 - a) + 127) / 255;
            dest[c] = (unsigned char)(color > 255 ? 255 : color);
        }
    }
    return true;
}
