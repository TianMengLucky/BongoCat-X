#include "bongo_cat/image.h"
#include "bongo_cat/safe_ffi.h"
#include "image_internal.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <stdlib.h>
#include <string.h>

BongoCatResult bongo_cat_image_load(const char *path, BongoCatImage *image, BongoCatError *error) {
    BongoCatResult result = bongo_cat_image_decode_pixels(path, image, error);
    if (result != BONGO_CAT_OK) return result;
    image->surface = SDL_CreateSurfaceFrom(image->width, image->height,
        SDL_PIXELFORMAT_RGBA32, image->pixels, image->width * 4);
    if (!image->surface) {
        bongo_cat_image_free(image);
        bongo_cat_error_set(error, BONGO_CAT_ERROR_PLATFORM, "Cannot create image surface");
        return BONGO_CAT_ERROR_PLATFORM;
    }
    return BONGO_CAT_OK;
}
void bongo_cat_image_free(BongoCatImage *image) {
    if (!image) return;
    if (image->surface) SDL_DestroySurface(image->surface);
    if (image->pixels) {
        if (image->pixels_ffi)
            bongo_safe_free_pixels(image->pixels,
                (size_t)image->width * (size_t)image->height * 4);
        else free(image->pixels);
    }
    memset(image, 0, sizeof(*image));
}
unsigned int bongo_cat_image_texture(const char *path, int *width, int *height, BongoCatError *error) {
    BongoCatImage image;
    if (bongo_cat_image_load(path, &image, error) != BONGO_CAT_OK) return 0;
    GLuint texture = bongo_cat_image_upload_texture(&image, 0, false, error);
    if (width) *width = image.width;
    if (height) *height = image.height;
    bongo_cat_image_free(&image);
    return texture;
}
bool bongo_cat_image_opaque_bounds(const char *path,
    float *min_u, float *min_v, float *max_u, float *max_v) {
    BongoCatImage image;
    BongoCatError ignored = {0};
    if (!min_u || !min_v || !max_u || !max_v ||
        bongo_cat_image_load(path, &image, &ignored) != BONGO_CAT_OK)
        return false;
    int min_x = image.width, min_y = image.height, max_x = -1, max_y = -1;
    for (int y = 0; y < image.height; ++y) {
        const unsigned char *row = image.pixels + (size_t)y * image.width * 4;
        for (int x = 0; x < image.width; ++x) {
            if (row[(size_t)x * 4 + 3] <= 8) continue;
            if (x < min_x) min_x = x;
            if (x > max_x) max_x = x;
            if (y < min_y) min_y = y;
            if (y > max_y) max_y = y;
        }
    }
    bool found = max_x >= 0;
    if (found) {
        float width = (float)image.width, height = (float)image.height;
        *min_u = (float)min_x / width;
        *min_v = (float)min_y / height;
        *max_u = (float)(max_x + 1) / width;
        *max_v = (float)(max_y + 1) / height;
    }
    bongo_cat_image_free(&image);
    return found;
}

unsigned int bongo_cat_image_texture_thumbnail(const char *path, int max_width,
    int max_height, int *width, int *height, BongoCatError *error) {
    BongoCatImage image;
#ifdef _WIN32
    if (max_width > 0 && max_height > 0 &&
        bongo_cat_image_decode_wic_responsive(path, &image,
            max_width, max_height, NULL, NULL)) {
        GLuint texture = bongo_cat_image_upload_texture(&image, 0, false, error);
        if (width) *width = image.width;
        if (height) *height = image.height;
        bongo_cat_image_free(&image);
        return texture;
    }
#endif
    if (bongo_cat_image_load(path, &image, error) != BONGO_CAT_OK) return 0;
    int target_width = image.width, target_height = image.height;
    if (max_width > 0 && max_height > 0 &&
        (target_width > max_width || target_height > max_height)) {
        float scale = SDL_min((float)max_width / target_width,
            (float)max_height / target_height);
        target_width = SDL_max(1, (int)(target_width * scale + .5f));
        target_height = SDL_max(1, (int)(target_height * scale + .5f));
        SDL_Surface *scaled = SDL_ScaleSurface(image.surface, target_width,
            target_height, SDL_SCALEMODE_LINEAR);
        if (scaled) {
            BongoCatImage thumbnail = {
                .pixels = scaled->pixels, .width = scaled->w, .height = scaled->h};
            GLuint texture = bongo_cat_image_upload_texture(&thumbnail, 0, false, error);
            if (width) *width = thumbnail.width;
            if (height) *height = thumbnail.height;
            SDL_DestroySurface(scaled);
            bongo_cat_image_free(&image);
            return texture;
        }
        target_width = image.width;
        target_height = image.height;
    }
    GLuint texture = bongo_cat_image_upload_texture(&image, 0, false, error);
    if (width) *width = target_width;
    if (height) *height = target_height;
    bongo_cat_image_free(&image);
    return texture;
}
static void erase_paw(BongoCatImage *image, bool left) {
    float cx = left ? .700f : .275f, cy = left ? .515f : .397f;
    float rx = left ? .080f : .070f, ry = left ? .170f : .160f;
    for (int y = 0; y < image->height; ++y) for (int x = 0; x < image->width; ++x) {
        float dx = (x / (float)image->width - cx) / rx;
        float dy = (y / (float)image->height - cy) / ry;
        unsigned char *pixel = image->pixels + ((size_t)y * image->width + x) * 4;
        if (dx * dx + dy * dy < 1.0f && pixel[3])
            pixel[0] = pixel[1] = pixel[2] = 255;
    }
}

static bool blend_file(BongoCatImage *base, const char *path, BongoCatError *error) {
    if (!path || !path[0]) return true;
    BongoCatImage layer;
    if (bongo_cat_image_load(path, &layer, error) != BONGO_CAT_OK) return false;
    bool valid = layer.width == base->width && layer.height == base->height;
    if (valid) for (int i = 0; i < base->width * base->height; ++i) {
        unsigned char *dst = base->pixels + i * 4, *src = layer.pixels + i * 4;
        unsigned alpha = src[3], inverse = 255 - alpha, destination_alpha = dst[3];
        unsigned output_alpha = alpha * 255 + destination_alpha * inverse;
        if (!output_alpha) { memset(dst, 0, 4); continue; }
        for (int channel = 0; channel < 3; ++channel) {
            unsigned color = src[channel] * alpha * 255 +
                dst[channel] * destination_alpha * inverse;
            dst[channel] = (unsigned char)((color + output_alpha / 2) / output_alpha);
        }
        dst[3] = (unsigned char)((output_alpha + 127) / 255);
    }
    bongo_cat_image_free(&layer);
    return valid;
}

unsigned int bongo_cat_image_composite_texture(const char *base, const char *left,
    const char *right, unsigned int texture, bool erase_left, bool erase_right,
    BongoCatError *error) {
    BongoCatImage image;
    if (bongo_cat_image_load(base, &image, error) != BONGO_CAT_OK) return 0;
    if (erase_left) erase_paw(&image, true);
    if (erase_right) erase_paw(&image, false);
    bool valid = blend_file(&image, left, error) && blend_file(&image, right, error);
    if (valid) {
        GLuint updated = bongo_cat_image_upload_texture(&image, texture, false, error);
        if (updated) texture = updated;
    }
    bongo_cat_image_free(&image);
    return texture;
}
