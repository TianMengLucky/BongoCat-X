#include "overlay_internal.h"
#include "bongo_cat/memory_policy.h"
#include <SDL3/SDL.h>
#include <string.h>
#include <limits.h>

static bool draw_items_background(BongoCatOverlay *value, SDL_Surface *canvas,
    int width, int height, int x, int y, int cw, int ch) {
    SDL_Renderer *renderer = SDL_CreateSoftwareRenderer(canvas);
    bool ok = renderer && value->items_draw(value->items_userdata, renderer, false,
        width, height, x, y, cw, ch) && SDL_RenderPresent(renderer);
    SDL_DestroyRenderer(renderer);
    return ok;
}
bool bongo_cat_overlay_prepare_native_background(BongoCatOverlay *value,
    BongoCatRhi *rhi, int width, int height, int x, int y, int cw, int ch,
    bool mirror, bool flip, bool opaque, uint32_t rgb) {
    if (!value || (!value->native_background.surface && !value->custom_background.surface && !value->items_draw)) {
        if (value) { SDL_DestroySurface(value->native_canvas); value->native_canvas = NULL; value->native_canvas_uploaded = false; }
        return bongo_cat_rhi_set_background(rhi, NULL, 0, 0, 0, 0) &&
            bongo_cat_overlay_prepare_native_foreground(value, rhi, width, height, x, y, cw, ch, mirror, flip);
    }
    const int geometry[6] = {width, height, x, y, cw, ch};
    bool uploaded = value->native_canvas_uploaded &&
        bongo_cat_rhi_background_matches(rhi,width,height,value->native_revision);
    if ((!value->native_canvas && !uploaded) || memcmp(geometry, value->native_geometry,
        sizeof(geometry)) || value->native_mirror != mirror ||
        value->native_flip != flip || value->native_opaque != opaque ||
        value->native_color != rgb) {
        SDL_Surface *canvas = SDL_CreateSurface(width, height, SDL_PIXELFORMAT_RGBA32);
        bool copied = value->native_background.surface && (mirror || flip);
        SDL_Surface *image = copied ? SDL_DuplicateSurface(value->native_background.surface) :
            value->native_background.surface;
        bool ok = canvas && (!value->native_background.surface || image);
        if (ok) ok = SDL_FillSurfaceRect(canvas, NULL, SDL_MapSurfaceRGBA(canvas,
            opaque ? (Uint8)(rgb >> 16) : 0, opaque ? (Uint8)(rgb >> 8) : 0,
            opaque ? (Uint8)rgb : 0, opaque ? 255 : 0));
        if (ok && value->custom_background.surface) {
            SDL_Surface *background = value->custom_background.surface;
            double scale = SDL_max((double)width / background->w,
                (double)height / background->h);
            double dw = SDL_ceil(background->w * scale);
            double dh = SDL_ceil(background->h * scale);
            if (dw >= INT_MAX || dh >= INT_MAX) ok = false;
            else {
                SDL_Rect rect = {(width - (int)dw) / 2, (height - (int)dh) / 2,
                    (int)dw, (int)dh};
                ok = SDL_SetSurfaceBlendMode(background, SDL_BLENDMODE_BLEND) &&
                    SDL_BlitSurfaceScaled(background, NULL, canvas, &rect, SDL_SCALEMODE_LINEAR);
            }
        }
        if (ok && value->items_draw && value->composed_cover)
            ok = draw_items_background(value, canvas, width, height, x, y, cw, ch);
        if (image) {
            if (ok && mirror) ok = SDL_FlipSurface(image, SDL_FLIP_HORIZONTAL);
            if (ok && flip) ok = SDL_FlipSurface(image, SDL_FLIP_VERTICAL);
            float scale_x = value->reference_width > 0
                ? (float)value->native_background.width / value->reference_width : 1.0f;
            float scale_y = value->reference_height > 0
                ? (float)value->native_background.height / value->reference_height : 1.0f;
            float dw = cw * scale_x, dh = ch * scale_y;
            if (dw <= 0 || dh <= 0 || dw >= INT_MAX || dh >= INT_MAX) ok = false;
            if (ok) {
                SDL_Rect rect = {x, height - y - ch, (int)SDL_roundf(dw),
                    (int)SDL_roundf(dh)};
                if (mirror) rect.x += cw - rect.w;
                if (flip) rect.y += ch - rect.h;
                ok = SDL_SetSurfaceBlendMode(image, SDL_BLENDMODE_BLEND) &&
                    SDL_BlitSurfaceScaled(image, NULL, canvas, &rect, SDL_SCALEMODE_LINEAR);
            }
        }
        if (ok && value->items_draw && !value->composed_cover)
            ok = draw_items_background(value, canvas, width, height, x, y, cw, ch);
        if (copied) SDL_DestroySurface(image);
        if (!ok) { SDL_DestroySurface(canvas); return false; }
        SDL_DestroySurface(value->native_canvas);
        value->native_canvas = canvas;
        memcpy(value->native_geometry, geometry, sizeof(geometry));
        value->native_color = rgb;
        value->native_mirror = mirror;
        value->native_flip = flip;
        value->native_opaque = opaque;
        ++value->native_revision;
        value->native_canvas_uploaded = false;
    }
    SDL_Surface *canvas = value->native_canvas;
    if (!canvas) return uploaded && bongo_cat_overlay_prepare_native_foreground(
        value, rhi, width, height, x, y, cw, ch, mirror, flip);
    bool ok = bongo_cat_rhi_set_background(rhi, canvas->pixels, canvas->w,
        canvas->h, canvas->pitch, value->native_revision);
    if (ok && SDL_GetBooleanProperty(SDL_GetGlobalProperties(), "BongoCat.LowMemoryMode", false) &&
        bongo_cat_rhi_background_matches(rhi,width,height,value->native_revision)) {
        // Upload owns its staging copy before returning. Rebuild this temporary
        // composition only when geometry, art, or the renderer changes.
        SDL_DestroySurface(canvas); value->native_canvas = NULL;
        value->native_canvas_uploaded = true;
        bongo_cat_memory_policy_temporary_released();
    }
    return ok && bongo_cat_overlay_prepare_native_foreground(
        value, rhi, width, height, x, y, cw, ch, mirror, flip);
}

void bongo_cat_overlay_set_mouse_range(BongoCatOverlay *value, float x_scale, float y_scale) {
    if (!value || !value->mver_pointer) return;
    bongo_cat_mver_pointer_overlay_set_range(value->mver_pointer, x_scale, y_scale);
}
