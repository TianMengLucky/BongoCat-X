#include "image_internal.h"

#include "bongo_cat/file.h"
#include "bongo_cat/safe_ffi.h"

#include <SDL3/SDL.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

unsigned char *bongo_cat_image_read_stream(FILE *file, size_t *length) {
    size_t capacity = 1 << 16, used = 0;
    unsigned char *buffer = malloc(capacity);
    if (!buffer) return NULL;
    for (;;) {
        if (used == capacity) {
            if (capacity > SIZE_MAX / 2) { free(buffer); return NULL; }
            unsigned char *grown = realloc(buffer, capacity *= 2);
            if (!grown) { free(buffer); return NULL; }
            buffer = grown;
        }
        size_t read = fread(buffer + used, 1, capacity - used, file);
        used += read;
        if (!read) break;
    }
    if (ferror(file)) { free(buffer); return NULL; }
    *length = used;
    return buffer;
}

BongoCatResult bongo_cat_image_decode_pixels(const char *path,
    BongoCatImage *image, BongoCatError *error) {
    if (!path || !image) return BONGO_CAT_ERROR_ARGUMENT;
    memset(image, 0, sizeof(*image));
    FILE *file = bongo_cat_file_open(path, "rb");
    size_t length = 0;
    unsigned char *data = file ? bongo_cat_image_read_stream(file, &length) : NULL;
    if (file) fclose(file);
    int width = 0, height = 0;
    /* Untrusted bytes: decoding happens in the bongo-safe Rust crate. */
    image->pixels = data ? bongo_safe_image_decode(data, length, &width, &height) : NULL;
    free(data);
    image->width = width;
    image->height = height;
    image->pixels_ffi = image->pixels != NULL;
    if (image->pixels) return BONGO_CAT_OK;
    bongo_cat_error_set(error, BONGO_CAT_ERROR_IO,
        "Cannot decode image: %s", path);
    return BONGO_CAT_ERROR_IO;
}

typedef struct PixelDecodeJob {
    const char *path;
    BongoCatImage *image;
    BongoCatError *error;
    BongoCatResult result;
} PixelDecodeJob;

static int SDLCALL decode_pixels_worker(void *userdata) {
    /* Workers only produce CPU pixels; Cubism and OpenGL remain on the caller. */
    PixelDecodeJob *job = userdata;
    job->result = bongo_cat_image_decode_pixels(job->path,
        job->image, job->error);
    return (int)job->result;
}

void bongo_cat_image_wait_for_decode(SDL_Thread *worker,
    BongoCatImageProgress progress, void *userdata) {
    while (SDL_GetThreadState(worker) == SDL_THREAD_ALIVE) {
        if (progress) progress(userdata, .5f);
        SDL_Delay(2);
    }
    SDL_WaitThread(worker, NULL);
    if (progress) progress(userdata, 1.0f);
}

BongoCatResult bongo_cat_image_decode_pixels_responsive(const char *path,
    BongoCatImage *image, BongoCatImageProgress progress, void *userdata,
    BongoCatError *error) {
    if (!progress)
        return bongo_cat_image_decode_pixels(path, image, error);
    PixelDecodeJob job = {path, image, error, BONGO_CAT_ERROR_PLATFORM};
    SDL_Thread *worker = SDL_CreateThread(decode_pixels_worker,
        BONGO_CAT_SLUG "-image-decode", &job);
    if (!worker)
        return bongo_cat_image_decode_pixels(path, image, error);
    bongo_cat_image_wait_for_decode(worker, progress, userdata);
    return job.result;
}
