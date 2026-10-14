#include "overlay_internal.h"
#include <SDL3/SDL.h>
#include <string.h>

static void full_sprite(BongoCatOverlay *value, const char *path, bool mirror) {
    SDL_Surface *surface = bongo_cat_overlay_native_image(value, path);
    if (!surface) { value->native_draw_failed = true; return; }
    int width = value->reference_width > 0 ? surface->w : value->native_foreground_geometry[4];
    int height = value->reference_height > 0 ? surface->h : value->native_foreground_geometry[5];
    int rw = value->reference_width, rh = value->reference_height;
    if (rw <= 0) value->reference_width = width;
    if (rh <= 0) value->reference_height = height;
    bongo_cat_overlay_native_sprite(value, surface, 0, 0, width, height, mirror, 0, 0);
    value->reference_width = rw; value->reference_height = rh;
}

bool bongo_cat_overlay_prepare_native_foreground(BongoCatOverlay *value,
    BongoCatRhi *rhi, int width, int height, int x, int y, int cw, int ch,
    bool mirror, bool flip) {
    if (!value || !value->native) return bongo_cat_rhi_set_foreground(rhi, NULL, 0, 0, 0, 0);
    if (!value->left && !value->right && !value->effect &&
        !value->left_keys.count && !value->right_keys.count &&
        !bongo_cat_overlay_mver_pointer_enabled(value) && !value->items_draw)
        return bongo_cat_rhi_set_foreground(rhi, NULL, 0, 0, 0, 0);
    int geometry[6] = {width, height, x, y, cw, ch};
    bool resized = !value->native_foreground || !value->native_renderer ||
        value->native_foreground->w != width ||
        value->native_foreground->h != height;
    if (resized) {
        bongo_cat_overlay_native_reset(value);
        value->native_foreground = SDL_CreateSurface(width, height, SDL_PIXELFORMAT_RGBA32);
        if (!value->native_foreground) return false;
        value->native_renderer = SDL_CreateSoftwareRenderer(value->native_foreground);
        if (!value->native_renderer || !SDL_SetRenderDrawBlendMode(
            value->native_renderer, SDL_BLENDMODE_BLEND)) return false;
    }
    if (resized || value->native_input_revision != value->native_input_seen ||
        memcmp(geometry, value->native_foreground_geometry, sizeof(geometry)) ||
        mirror != value->native_foreground_mirror || flip != value->native_foreground_flip) {
        memcpy(value->native_foreground_geometry, geometry, sizeof(geometry));
        value->native_foreground_mirror = mirror; value->native_foreground_flip = flip;
        bongo_cat_overlay_set_vertical_flip(value, flip);
        value->native_draw_failed = false;
        SDL_SetRenderDrawColor(value->native_renderer, 0, 0, 0, 0);
        SDL_Rect clip = {x, height - y - ch, cw, ch};
        if (!SDL_RenderClear(value->native_renderer) ||
            !SDL_SetRenderClipRect(value->native_renderer, &clip)) return false;
        bongo_cat_mver_pointer_overlay_draw_before_keys(value->mver_pointer);
        if (value->left) full_sprite(value, value->left_path, mirror);
        if (value->right) full_sprite(value, value->right_path, mirror);
        bongo_cat_overlay_keys_draw(value, mirror);
        if (value->effect) full_sprite(value, value->effect_path, mirror);
        bongo_cat_mver_pointer_overlay_draw_after_keys(value->mver_pointer);
        if (value->items_draw && (!SDL_SetRenderClipRect(value->native_renderer, NULL) ||
            !value->items_draw(value->items_userdata, value->native_renderer, true,
                width, height, x, y, cw, ch))) return false;
        if (value->native_draw_failed || !SDL_RenderPresent(value->native_renderer)) return false;
        value->native_input_seen = value->native_input_revision;
        ++value->native_foreground_revision;
    }
    SDL_Surface *canvas = value->native_foreground;
    return bongo_cat_rhi_set_foreground(rhi, canvas->pixels, width, height,
        canvas->pitch, value->native_foreground_revision);
}
