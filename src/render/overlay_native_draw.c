#include "overlay_internal.h"
#include "bongo_cat/log.h"
#include <SDL3/SDL.h>
#include <string.h>

void bongo_cat_overlay_native_forget(BongoCatOverlay *value, SDL_Surface *surface) {
    if (!surface) return;
    for (size_t i = 0; i < 32; ++i) {
        OverlayNativeTexture *slot = &value->native_textures[i];
        if (slot->surface != surface) continue;
        SDL_DestroyTexture(slot->texture);
        memset(slot, 0, sizeof(*slot));
    }
}

void bongo_cat_overlay_native_reset(BongoCatOverlay *value) {
    for (size_t i = 0; i < 32; ++i) {
        SDL_DestroyTexture(value->native_textures[i].texture);
        memset(&value->native_textures[i], 0, sizeof(value->native_textures[i]));
    }
    SDL_DestroyRenderer(value->native_renderer);
    SDL_DestroySurface(value->native_foreground);
    value->native_renderer = NULL; value->native_foreground = NULL;
    ++value->native_input_revision;
}

SDL_Surface *bongo_cat_overlay_native_image(BongoCatOverlay *value, const char *path) {
    if (!path || !path[0]) return NULL;
    TextureSlot *chosen = &value->cache[0];
    for (size_t i = 0; i < 4; ++i) {
        TextureSlot *slot = &value->cache[i];
        if (slot->image.surface && !strcmp(slot->path, path)) {
            slot->used = ++value->clock;
            return slot->image.surface;
        }
        if (slot->used < chosen->used) chosen = slot;
    }
    bongo_cat_overlay_native_forget(value, chosen->image.surface);
    bongo_cat_image_free(&chosen->image);
    memset(chosen, 0, sizeof(*chosen));
    BongoCatError error = {0};
    if (bongo_cat_image_load(path, &chosen->image, &error) != BONGO_CAT_OK) {
        SDL_LogWarn(BONGO_CAT_LOG_INPUT, "%s", error.message);
        return NULL;
    }
    SDL_snprintf(chosen->path, sizeof(chosen->path), "%s", path);
    chosen->used = ++value->clock;
    return chosen->image.surface;
}

static SDL_Texture *texture_for(BongoCatOverlay *value, SDL_Surface *surface) {
    if (!surface) return NULL;
    OverlayNativeTexture *empty = NULL;
    for (size_t i = 0; i < 32; ++i) {
        OverlayNativeTexture *slot = &value->native_textures[i];
        if (slot->surface == surface) return slot->texture;
        if (!slot->texture && !empty) empty = slot;
    }
    if (!empty) return NULL;
    SDL_Texture *texture = SDL_CreateTextureFromSurface(value->native_renderer, surface);
    if (!texture) return NULL;
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_LINEAR);
    *empty = (OverlayNativeTexture){surface, texture};
    return texture;
}

/* Reuse the GL pointer geometry: NDC plus UV/color, converted to the actual
   native window viewport. SDL rasterizes strips/fans using triangle indices. */
void bongo_cat_overlay_native_geometry(void *userdata, unsigned primitive,
    const float *vertices, size_t count, SDL_Surface *surface) {
    BongoCatOverlay *value = userdata;
    if (!value->native_renderer || count < 3 || count > 128) return;
    SDL_Vertex converted[128];
    int indices[126 * 3];
    const int *g = value->native_foreground_geometry;
    for (size_t i = 0; i < count; ++i) {
        const float *v = vertices + i * 8;
        converted[i] = (SDL_Vertex){
            {(v[0] + 1) * g[4] * 0.5f + g[2],
             (1 - v[1]) * g[5] * 0.5f + g[1] - g[3] - g[5]},
            {v[4], v[5], v[6], v[7]}, {v[2], v[3]}};
    }
    for (size_t i = 0; i + 2 < count; ++i) {
        indices[i * 3] = primitive == GL_TRIANGLE_FAN ? 0 : (int)i;
        indices[i * 3 + 1] = (int)i + 1;
        indices[i * 3 + 2] = (int)i + 2;
    }
    SDL_Texture *texture = texture_for(value, surface);
    if ((surface && !texture) || !SDL_RenderGeometry(value->native_renderer,
        texture, converted, (int)count, indices, (int)(count - 2) * 3))
        value->native_draw_failed = true;
}

void bongo_cat_overlay_native_sprite(BongoCatOverlay *value, SDL_Surface *surface,
    int x, int y, int width, int height, bool mirror, int source_x, int source_width) {
    if (!surface || width <= 0 || height <= 0) return;
    float u0 = source_width > 0 ? (float)source_x / surface->w : 0;
    float u1 = source_width > 0 ? (float)(source_x + source_width) / surface->w : 1;
    float left = 2.0f * x / value->reference_width - 1;
    float right = 2.0f * (x + width) / value->reference_width - 1;
    float top = 1 - 2.0f * y / value->reference_height;
    float bottom = 1 - 2.0f * (y + height) / value->reference_height;
    if (mirror) { left = -left; right = -right; }
    if (value->vertical_flip) { top = -top; bottom = -bottom; }
    const float vertices[] = {
        left, top, u0, 0, 1, 1, 1, 1, left, bottom, u0, 1, 1, 1, 1, 1,
        right, top, u1, 0, 1, 1, 1, 1, right, bottom, u1, 1, 1, 1, 1, 1};
    bongo_cat_overlay_native_geometry(value, GL_TRIANGLE_STRIP, vertices, 4, surface);
}
