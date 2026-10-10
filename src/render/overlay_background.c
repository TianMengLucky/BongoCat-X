#include "overlay_internal.h"
#include "bongo_cat/path.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

bool bongo_cat_overlay_set_custom_background(BongoCatOverlay *value,
    const char *path, BongoCatError *error) {
    if (!value) return false;
    if (!path) path = "";
    if (!strcmp(path, value->custom_path)) return true;
    BongoCatImage image = {0}, resized = {0};
    GLuint texture = 0;
    int width = 0, height = 0;
    if (path[0]) {
        uint64_t size = 0;
        if (strlen(path) >= sizeof(value->custom_path) ||
            !bongo_cat_path_file_size(path, &size) || !size || size > 64 * 1024 * 1024 ||
            !bongo_cat_image_info(path, &width, &height) || width <= 0 || height <= 0 ||
            (uint64_t)width * height > 16777216) {
            bongo_cat_error_set(error, BONGO_CAT_ERROR_FORMAT, "Invalid or oversized background image");
            return false;
        }
        if (value->native) {
            if (bongo_cat_image_load(path, &image, error) != BONGO_CAT_OK) return false;
            bool ok = bongo_cat_image_resize_rgba_take(&image, 4096, 4096, &resized, error);
            bongo_cat_image_free(&image);
            if (ok && !resized.surface)
                resized.surface = SDL_CreateSurfaceFrom(resized.width, resized.height,
                    SDL_PIXELFORMAT_RGBA32, resized.pixels, resized.width * 4);
            if (!ok || !resized.surface) {
                bongo_cat_image_free(&resized);
                return false;
            }
        } else {
            texture = bongo_cat_image_texture_thumbnail(path, 4096, 4096, &width, &height, error);
            if (!texture) return false;
        }
    }
    bongo_cat_image_free(&value->custom_background);
    if (value->custom_texture) glDeleteTextures(1, &value->custom_texture);
    value->custom_background = resized;
    value->custom_texture = texture;
    snprintf(value->custom_path, sizeof(value->custom_path), "%s", path);
    SDL_DestroySurface(value->native_canvas);
    value->native_canvas = NULL;
    return true;
}
