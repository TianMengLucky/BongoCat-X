#include "overlay_internal.h"
#include "bongo_cat/image.h"
#include "bongo_cat/log.h"

#include <SDL3/SDL_log.h>
#include <stdlib.h>
#include <string.h>

/* Extract glyph ink while retaining the model's original frame and tail. */
static uint32_t background_color(const unsigned char *pixels, int width,
    int height, int x, int y, int stride) {
    unsigned counts[4096] = {0};
    for (int row = 0; row < height; ++row) for (int col = 0; col < width; ++col) {
        const unsigned char *p = pixels + ((size_t)(y + row) * stride + x + col) * 4;
        if (p[3] < 128) continue;
        unsigned bucket = ((unsigned)(p[0] >> 4) << 8) |
            ((unsigned)(p[1] >> 4) << 4) | (unsigned)(p[2] >> 4);
        counts[bucket]++;
    }
    unsigned best = 0;
    for (unsigned i = 1; i < 4096; ++i)
        if (counts[i] > counts[best]) best = i;
    for (int row = 0; row < height; ++row) for (int col = 0; col < width; ++col) {
        const unsigned char *p = pixels + ((size_t)(y + row) * stride + x + col) * 4;
        unsigned bucket = ((unsigned)(p[0] >> 4) << 8) |
            ((unsigned)(p[1] >> 4) << 4) | (unsigned)(p[2] >> 4);
        if (p[3] >= 128 && bucket == best)
            return ((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | p[2];
    }
    return 0xffffffu;
}

static bool is_ink(const unsigned char *pixel, uint32_t background) {
    if (pixel[3] < 8) return false;
    for (unsigned channel = 0; channel < 3; ++channel) {
        int bg = (int)((background >> ((2u - channel) * 8u)) & 0xffu);
        if (abs((int)pixel[channel] - bg) > 12) return true;
    }
    return false;
}

static void queue_border(unsigned char *mask, size_t index,
    size_t *queue, size_t *count) {
    if (mask[index] != 1) return;
    mask[index] = 2;
    queue[(*count)++] = index;
}

/* The curved outline can enter the corners of the character rectangle.
   Edge-connected ink belongs to that outline, so it must stay in the frame. */
static void preserve_border(unsigned char *mask, int width, int height,
    size_t *queue) {
    size_t count = 0;
    for (int x = 0; x < width; ++x) {
        queue_border(mask, (size_t)x, queue, &count);
        queue_border(mask, (size_t)(height - 1) * width + x, queue, &count);
    }
    for (int y = 0; y < height; ++y) {
        queue_border(mask, (size_t)y * width, queue, &count);
        queue_border(mask, (size_t)y * width + width - 1, queue, &count);
    }
    for (size_t i = 0; i < count; ++i) {
        int x = (int)(queue[i] % (size_t)width);
        int y = (int)(queue[i] / (size_t)width);
        for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx) {
            int nx = x + dx, ny = y + dy;
            if (nx >= 0 && nx < width && ny >= 0 && ny < height)
                queue_border(mask, (size_t)ny * width + nx, queue, &count);
        }
    }
}

static bool extract(BongoCatOverlay *value, BongoCatImage *image, const OverlayKeyLayout *layout,
    OverlayKeySprite *entry) {
    int bw = layout->bubble_width, bh = layout->bubble_height;
    int cw = layout->characters_width, ch = layout->characters_height;
    int bx = layout->mirrored ? image->width - layout->bubble_x - bw
        : layout->bubble_x;
    int cx = layout->mirrored ? image->width - layout->characters_x - cw
        : layout->characters_x;
    int by = layout->bubble_y, cy = layout->characters_y;
    if (bx < 0 || by < 0 || bx > image->width - bw || by > image->height - bh ||
        cx < bx || cy < by || cx + cw > bx + bw || cy + ch > by + bh)
        return false;
    size_t cells = (size_t)cw * ch;
    unsigned char *mask = malloc(cells);
    size_t *queue = malloc(cells * sizeof(*queue));
    unsigned char *frame = malloc((size_t)bw * bh * 4);
    if (!mask || !queue || !frame) {
        free(mask); free(queue); free(frame);
        return false;
    }
    uint32_t bg = background_color(image->pixels, cw, ch, cx, cy, image->width);
    for (int y = 0; y < ch; ++y) for (int x = 0; x < cw; ++x)
        mask[(size_t)y * cw + x] = is_ink(image->pixels +
            ((size_t)(cy + y) * image->width + cx + x) * 4, bg) ? 1 : 0;
    preserve_border(mask, cw, ch, queue);
    free(queue);
    int min_x = cw, max_x = -1;
    for (int y = 0; y < ch; ++y) for (int x = 0; x < cw; ++x) {
        if (mask[(size_t)y * cw + x] != 1) continue;
        if (x < min_x) min_x = x;
        if (x > max_x) max_x = x;
    }
    int width = max_x - min_x + 1;
    unsigned char *glyph = width > 0 ? calloc((size_t)width * ch, 4) : NULL;
    if (!glyph) { free(mask); free(frame); return false; }
    for (int y = 0; y < bh; ++y)
        memcpy(frame + (size_t)y * bw * 4,
            image->pixels + ((size_t)(by + y) * image->width + bx) * 4,
            (size_t)bw * 4);
    for (int y = 0; y < ch; ++y) for (int x = min_x; x <= max_x; ++x) {
        if (mask[(size_t)y * cw + x] != 1) continue;
        const unsigned char *p = image->pixels +
            ((size_t)(cy + y) * image->width + cx + x) * 4;
        memcpy(glyph + ((size_t)y * width + x - min_x) * 4, p, 4);
        unsigned char *blank = frame +
            ((size_t)(cy - by + y) * bw + cx - bx + x) * 4;
        blank[0] = (unsigned char)(bg >> 16);
        blank[1] = (unsigned char)(bg >> 8);
        blank[2] = (unsigned char)bg;
    }
    entry->characters = bongo_cat_overlay_keys_upload(value, glyph, width, ch, &entry->characters_image);
    entry->bubble = bongo_cat_overlay_keys_upload(value, frame, bw, bh, &entry->bubble_image);
    entry->characters_width = width;
    entry->characters_height = ch;
    entry->rgb = bongo_cat_overlay_keys_ink_color(glyph, width, ch);
    free(glyph); free(mask); free(frame);
    return entry->characters && entry->bubble;
}

bool bongo_cat_overlay_keys_bubble(BongoCatOverlay *value, const char *path,
    const OverlayKeyLayout *layout, OverlayKeySprite *entry) {
    if (!value || !path || !layout || !entry) return false;
    BongoCatError error = {0};
    BongoCatImage image;
    if (bongo_cat_image_load(path, &image, &error) != BONGO_CAT_OK) {
        SDL_LogWarn(BONGO_CAT_LOG_INPUT, "%s", error.message);
        return false;
    }
    bool ok = extract(value, &image, layout, entry);
    bongo_cat_image_free(&image);
    return ok;
}
