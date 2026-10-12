#include "overlay_internal.h"
#include "bongo_cat/image.h"
#include "bongo_cat/log.h"

#include <SDL3/SDL_log.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Keep press order separately from the bounded set of visible cutouts. */

GLuint bongo_cat_overlay_keys_upload(BongoCatOverlay *value,
    const unsigned char *pixels, int width, int height, SDL_Surface **surface) {
    if (value->native) {
        *surface = SDL_CreateSurface(width, height, SDL_PIXELFORMAT_RGBA32);
        if (!*surface) return 0;
        for (int y = 0; y < height; ++y)
            memcpy((unsigned char *)(*surface)->pixels + (size_t)y * (*surface)->pitch,
                pixels + (size_t)y * width * 4, (size_t)width * 4);
        return 1;
    }
    GLuint texture = 0;
    glGenTextures(1, &texture);
    if (!texture) return 0;
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
        GL_UNSIGNED_BYTE, pixels);
    if (glGetError() != GL_NO_ERROR) {
        glDeleteTextures(1, &texture);
        return 0;
    }
    return texture;
}

/* Match the separator to the extracted glyph ink, with no frame fill. */
uint32_t bongo_cat_overlay_keys_ink_color(const unsigned char *pixels, int width, int height) {
    uint32_t counts[4096] = {0};
    for (size_t index = 0, total = (size_t)width * (size_t)height;
        index < total; ++index) {
        const unsigned char *pixel = pixels + index * 4;
        if (pixel[3] < 128) continue;
        unsigned bucket = ((unsigned)(pixel[0] >> 4) << 8) |
            ((unsigned)(pixel[1] >> 4) << 4) | (unsigned)(pixel[2] >> 4);
        counts[bucket]++;
    }
    unsigned best = 0;
    for (unsigned bucket = 1; bucket < 4096; ++bucket)
        if (counts[bucket] > counts[best]) best = bucket;
    if (!counts[best]) return 0x25325fu;
    return (uint32_t)((((best >> 8) & 0xfu) * 16u + 8u) << 16) |
        (uint32_t)((((best >> 4) & 0xfu) * 16u + 8u) << 8) |
        (uint32_t)(((best & 0xfu) * 16u + 8u));
}

bool bongo_cat_overlay_keys_enabled(const BongoCatOverlay *value) {
#ifndef BONGO_CAT_HAS_MODEL_PLUGINS
    (void)value;
    return false;
#else
    /* Single-key mode uses the same cutouts to keep mirrored glyphs readable. */
    return value && (value->native || value->keys_program) &&
        !value->composed_cover && value->reference_width > 0 &&
        value->reference_height > 0;
#endif
}

GLuint bongo_cat_overlay_keys_crop_texture(BongoCatOverlay *value, const char *path,
    int x, int y, int width, int height, bool mirrored, int *out_width,
    int *out_height, int *source_width, uint32_t *rgb, SDL_Surface **surface) {
    if (!value || !path || width <= 0 || height <= 0) return 0;
    BongoCatError error = {0};
    BongoCatImage image;
    if (bongo_cat_image_load(path, &image, &error) != BONGO_CAT_OK) {
        if (error.message[0]) SDL_LogWarn(BONGO_CAT_LOG_INPUT, "%s", error.message);
        return 0;
    }
    if (mirrored) x = image.width - x - width;
    GLuint texture = 0;
    /* A partial crop would move or stretch the artwork. Incompatible sprites
       keep their full-image overlay, including the bundled 612x354 hands. */
    if (x >= 0 && y >= 0 && x <= image.width - width &&
        y <= image.height - height) {
        unsigned char *buffer = malloc((size_t)width * (size_t)height * 4);
        if (buffer) {
            for (int row = 0; row < height; ++row)
                memcpy(buffer + (size_t)row * (size_t)width * 4,
                    image.pixels + ((size_t)(y + row) * (size_t)image.width +
                        (size_t)x) * 4, (size_t)width * 4);
            texture = bongo_cat_overlay_keys_upload(value, buffer, width, height, surface);
            if (texture && rgb) *rgb = bongo_cat_overlay_keys_ink_color(buffer, width, height);
            free(buffer);
        }
    }
    if (texture) {
        if (out_width) *out_width = width;
        if (out_height) *out_height = height;
        if (source_width) *source_width = image.width;
    }
    bongo_cat_image_free(&image);
    return texture;
}

static void release_texture(BongoCatOverlay *value, GLuint *texture, SDL_Surface **image);

static void update_plus(BongoCatOverlay *value, OverlayKeySide *side, uint32_t rgb) {
    const int size = 32;
    if (side->plus && rgb == side->plus_rgb) return;
    unsigned char buffer[32 * 32 * 4] = {0};
    const int thickness = size / 4, margin = size / 5, center = size / 2;
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            bool horizontal = y >= center - thickness / 2 &&
                y < center - thickness / 2 + thickness &&
                x >= margin && x < size - margin;
            bool vertical = x >= center - thickness / 2 &&
                x < center - thickness / 2 + thickness &&
                y >= margin && y < size - margin;
            if (!horizontal && !vertical) continue;
            unsigned char *pixel = buffer + ((size_t)y * size + (size_t)x) * 4;
            pixel[0] = (unsigned char)((rgb >> 16) & 0xffu);
            pixel[1] = (unsigned char)((rgb >> 8) & 0xffu);
            pixel[2] = (unsigned char)(rgb & 0xffu);
            pixel[3] = 255;
        }
    }
    release_texture(value, &side->plus, &side->plus_image);
    side->plus = bongo_cat_overlay_keys_upload(value, buffer, size, size, &side->plus_image);
    side->plus_rgb = side->plus ? rgb : 0;
}

static void release_texture(BongoCatOverlay *value, GLuint *texture, SDL_Surface **image) {
    if (!value->native && *texture) glDeleteTextures(1, texture);
    bongo_cat_overlay_native_forget(value, *image);
    SDL_DestroySurface(*image);
    *image = NULL; *texture = 0;
}

static void free_sprite(BongoCatOverlay *value, OverlayKeySprite *sprite) {
    release_texture(value, &sprite->bubble, &sprite->bubble_image);
    release_texture(value, &sprite->characters, &sprite->characters_image);
    memset(sprite, 0, sizeof(*sprite));
}

static void clear_side(BongoCatOverlay *value, OverlayKeySide *side) {
    for (size_t i = 0; i < side->count; ++i) free_sprite(value, &side->keys[i]);
    release_texture(value, &side->hand, &side->hand_image);
    release_texture(value, &side->plus, &side->plus_image);
    memset(side, 0, sizeof(*side));
}

void bongo_cat_overlay_keys_clear(BongoCatOverlay *value) {
    if (!value) return;
    clear_side(value, &value->left_keys);
    clear_side(value, &value->right_keys);
}

static void update_hand(BongoCatOverlay *value, OverlayKeySide *side,
    const OverlayKeyLayout *layout) {
    const char *path = side->count ? side->keys[0].path : "";
    if (side->hand && strcmp(side->hand_path, path) == 0) return;
    release_texture(value, &side->hand, &side->hand_image);
    side->hand = 0;
    side->hand_width = side->hand_height = 0;
    snprintf(side->hand_path, sizeof(side->hand_path), "%s", path);
    if (!path[0]) return;
    side->hand = bongo_cat_overlay_keys_crop_texture(value, path, layout->hand_x,
        layout->hand_y, layout->hand_width, layout->hand_height, layout->mirrored,
        &side->hand_width, &side->hand_height, &side->sprite_width, NULL, &side->hand_image);
}

static size_t side_capacity(const BongoCatOverlay *value,
    const OverlayKeyLayout *layout) {
    if (!value->multi_key_display) return 1;
    size_t capacity = layout->max_keys > 0 ? (size_t)layout->max_keys
        : BONGO_CAT_OVERLAY_KEYS_CAP;
    return capacity > BONGO_CAT_OVERLAY_KEYS_CAP ? BONGO_CAT_OVERLAY_KEYS_CAP
        : capacity;
}

static void refresh_side(BongoCatOverlay *value, bool right) {
    OverlayKeySide *side = right ? &value->right_keys : &value->left_keys;
    const OverlayKeyLayout *layout = bongo_cat_overlay_keys_layout(value, right);
    OverlayKeySprite previous[BONGO_CAT_OVERLAY_KEYS_CAP];
    memcpy(previous, side->keys, sizeof(previous));
    memset(side->keys, 0, sizeof(side->keys));
    size_t count = side->held_count;
    if (count > side_capacity(value, layout)) count = side_capacity(value, layout);
    side->count = count;
    side->fallback = false;
    for (size_t i = 0; i < count; ++i) {
        const char *name = side->held[value->multi_key_display ? i
            : side->held_count - 1];
        OverlayKeySprite *entry = &side->keys[i];
        for (size_t old = 0; old < BONGO_CAT_OVERLAY_KEYS_CAP; ++old) {
            if (strcmp(previous[old].name, name)) continue;
            *entry = previous[old];
            memset(&previous[old], 0, sizeof(previous[old]));
            break;
        }
        if (!entry->name[0]) {
            snprintf(entry->name, sizeof(entry->name), "%s", name);
            if (bongo_cat_overlay_key_path(value, right, name, entry->path))
                bongo_cat_overlay_keys_bubble(value, entry->path, layout, entry);
        }
        if (!entry->bubble || !entry->characters) side->fallback = true;
    }
    for (size_t i = 0; i < BONGO_CAT_OVERLAY_KEYS_CAP; ++i) free_sprite(value, &previous[i]);
    update_hand(value, side, layout);
    if (count && !side->hand) side->fallback = true;
    GLuint *active = right ? &value->right : &value->left;
    char *active_name = right ? value->right_name : value->left_name;
    char *active_path = right ? value->right_path : value->left_path;
    *active = 0;
    active_name[0] = active_path[0] = '\0';
#ifdef BONGO_CAT_HAS_MODEL_PLUGINS
    if (side->fallback && count) {
        snprintf(active_name, BONGO_CAT_ID_CAP, "%s", side->keys[0].name);
        snprintf(active_path, BONGO_CAT_PATH_CAP, "%s", side->keys[0].path);
        *active = bongo_cat_overlay_cached_texture(value, active_path);
        if (!*active) value->input_texture_failures++;
    }
#endif
    if (count && !side->fallback) update_plus(value, side, side->keys[0].rgb);
}

void bongo_cat_overlay_keys_press(BongoCatOverlay *value, bool right,
    const char *name, const char *path) {
    if (!bongo_cat_overlay_keys_enabled(value) || !name || !path) return;
    OverlayKeySide *side = right ? &value->right_keys : &value->left_keys;
    for (size_t i = 0; i < side->held_count; ++i)
        if (!strcmp(side->held[i], name)) return;
    if (side->held_count >= BONGO_CAT_OVERLAY_HELD_CAP) return;
    snprintf(side->held[side->held_count++], BONGO_CAT_ID_CAP, "%s", name);
    refresh_side(value, right);
}

bool bongo_cat_overlay_keys_release(BongoCatOverlay *value, bool right,
    const char *name) {
    if (!value || !name) return false;
    OverlayKeySide *side = right ? &value->right_keys : &value->left_keys;
    for (size_t i = 0; i < side->held_count; ++i) {
        if (strcmp(side->held[i], name)) continue;
        if (i + 1 < side->held_count)
            memmove(side->held[i], side->held[i + 1],
                (side->held_count - i - 1) * sizeof(side->held[0]));
        memset(side->held[--side->held_count], 0, sizeof(side->held[0]));
        refresh_side(value, right);
        return true;
    }
    return false;
}
