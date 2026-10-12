#include "rhi_internal.h"
#include "rhi_corners.h"
#include <limits.h>
#include <math.h>
#include <string.h>

/* Cache on the window, like corner parameters, without extending the RHI ABI.
   A missing/invalid property falls back to scanning the complete layer. */
static void store_bounds(const BongoCatRhi *rhi, SDL_Rect bounds) {
    if (!rhi->window) return;
    SDL_PropertiesID p = SDL_GetWindowProperties(rhi->window);
    SDL_ClearProperty(p, "BongoCat.ForegroundWidth");
    if (SDL_SetNumberProperty(p, "BongoCat.ForegroundX", bounds.x) &&
        SDL_SetNumberProperty(p, "BongoCat.ForegroundY", bounds.y) &&
        SDL_SetNumberProperty(p, "BongoCat.ForegroundHeight", bounds.h))
        SDL_SetNumberProperty(p, "BongoCat.ForegroundWidth", bounds.w);
}

static SDL_Rect cached_bounds(const BongoCatRhi *rhi) {
    SDL_Rect full = {0, 0, rhi->foreground_width, rhi->foreground_height};
    if (!rhi->window) return full;
    SDL_PropertiesID p = SDL_GetWindowProperties(rhi->window);
    SDL_Rect bounds = {
        (int)SDL_GetNumberProperty(p, "BongoCat.ForegroundX", -1),
        (int)SDL_GetNumberProperty(p, "BongoCat.ForegroundY", -1),
        (int)SDL_GetNumberProperty(p, "BongoCat.ForegroundWidth", -1),
        (int)SDL_GetNumberProperty(p, "BongoCat.ForegroundHeight", -1)};
    if (bounds.x < 0 || bounds.y < 0 || bounds.w < 0 || bounds.h < 0 ||
        bounds.x > full.w || bounds.y > full.h ||
        bounds.w > full.w - bounds.x || bounds.h > full.h - bounds.y) return full;
    return bounds;
}

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
        (uint64_t)pitch * height > SIZE_MAX ||
        (uint64_t)width * height > SIZE_MAX / 4) return false;
    if (rhi->foreground_pixels && rhi->foreground_width == width &&
        rhi->foreground_height == height && rhi->foreground_revision == revision) return true;
    size_t bytes = (size_t)width * height * 4;
    unsigned char *copy = rhi->foreground_pixels;
    if (!copy || rhi->foreground_width != width || rhi->foreground_height != height) {
        copy = SDL_realloc(copy, bytes);
        if (!copy) return false;
        rhi->foreground_pixels = copy;
    }
    int min_x = width, min_y = height, max_x = -1, max_y = -1;
    for (int y = 0; y < height; ++y) {
        unsigned char *row = copy + (size_t)y * width * 4;
        memcpy(row, (const unsigned char *)pixels + (size_t)y * pitch, (size_t)width * 4);
        int first = width, last = -1;
        for (int x = 0; x < width; ++x) {
            if (!row[(size_t)x * 4 + 3]) continue;
            if (last < 0) first = x;
            last = x;
        }
        if (last < 0) continue;
        min_x = SDL_min(min_x, first); max_x = SDL_max(max_x, last);
        if (max_y < 0) min_y = y;
        max_y = y;
    }
    rhi->foreground_width = width; rhi->foreground_height = height;
    rhi->foreground_revision = revision;
    SDL_Rect bounds = {0};
    if (max_x >= 0)
        bounds = (SDL_Rect){min_x, min_y, max_x - min_x + 1, max_y - min_y + 1};
    store_bounds(rhi, bounds);
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
    int width, int height, size_t pitch, bool bgra, SDL_Rect *changed) {
    if (!changed) return false;
    *changed = (SDL_Rect){0};
    if (!rhi || !rhi->foreground_pixels || !pixels || width <= 0 || height <= 0 ||
        (size_t)width > SIZE_MAX / 4 || pitch < (size_t)width * 4 ||
        pitch > SIZE_MAX / (size_t)height ||
        width != rhi->foreground_width || height != rhi->foreground_height) return false;
    float corners[8] = {0};
    SDL_Rect bounds = cached_bounds(rhi);
    if (!bounds.w || !bounds.h) return false;
    bool rounded = bongo_cat_rhi_corner_params(rhi->window, width, height, corners);
    bool composed = false;
    for (int y = bounds.y; y < bounds.y + bounds.h; ++y) {
        for (int x = bounds.x; x < bounds.x + bounds.w; ++x) {
            const unsigned char *source = rhi->foreground_pixels + ((size_t)y * width + x) * 4;
            unsigned a = source[3];
            if (!a) continue;
            composed = true;
            unsigned mask = rounded ? corner_coverage(x, y, corners) : 255;
            unsigned char *dest = (unsigned char *)pixels + (size_t)y * pitch + (size_t)x * 4;
            if (a == 255 && mask == 255) {
                dest[0] = source[bgra ? 2 : 0]; dest[1] = source[1];
                dest[2] = source[bgra ? 0 : 2]; dest[3] = 255;
                continue;
            }
            for (unsigned c = 0; c < 4; ++c) {
                unsigned index = bgra && c < 3 ? 2 - c : c;
                unsigned color = (source[index] * mask + 127) / 255 +
                    (dest[c] * (255 - a) + 127) / 255;
                dest[c] = (unsigned char)(color > 255 ? 255 : color);
            }
        }
    }
    /* Transparent layers need neither a GPU upload nor a second submission. */
    if (!composed) return false;
    *changed = bounds;
    return true;
}
