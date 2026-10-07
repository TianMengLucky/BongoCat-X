#include "model_cover.h"

#include "preferences_model_cover.h"
#include "runtime.h"
#include "bongo_cat/file.h"
#include "../../platform/common/gl_readback.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stb_image_write.h>

typedef struct CoverWriter {
    FILE *file;
    bool ok;
} CoverWriter;

void bongo_cat_model_cover_capture_before_switch(BongoCatApp *app) {
    if (!app || !app->loaded_model[0]) return;
    const BongoCatModelEntry *entry = bongo_cat_models_find(&app->models,
        app->loaded_model);
    if (!entry) return;
    bongo_cat_model_cover_schedule(app, entry);
    if (!bongo_cat_model_cover_pending(app)) return;
    if (!bongo_cat_app_capture_pending_model_cover(app))
        SDL_Log("Model cover refresh before switch deferred: id=%s",
            entry->id);
}

static void cover_write(void *context, void *data, int size) {
    CoverWriter *writer = context;
    if (writer->ok && fwrite(data, 1, (size_t)size, writer->file) !=
            (size_t)size)
        writer->ok = false;
}

static void flip_cover_pixels(unsigned char *pixels, int height, size_t pitch) {
    for (int y = 0; y < height / 2; ++y) {
        unsigned char *top = pixels + (size_t)y * pitch;
        unsigned char *bottom = pixels + (size_t)(height - y - 1) * pitch;
        for (size_t i = 0; i < pitch; ++i) {
            unsigned char value = top[i];
            top[i] = bottom[i];
            bottom[i] = value;
        }
    }
}

static unsigned char *read_cover_pixels(const char *path, int width,
    int height, size_t pitch, size_t bytes) {
    unsigned char *pixels = malloc(bytes);
    if (!pixels) {
        SDL_LogWarn(SDL_LOG_CATEGORY_VIDEO,
            "Model cover refresh allocation failed for %zu bytes: %s",
            bytes, path);
        return NULL;
    }
    if (!bongo_cat_gl_read_window(0, 0, width, height, true, pixels)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_VIDEO,
            "Model cover framebuffer readback failed: path=%s error=%s",
            path, SDL_GetError());
        free(pixels);
        return NULL;
    }
    flip_cover_pixels(pixels, height, pitch);
    return pixels;
}

static bool write_cover_png(const char *temporary, const char *path,
    const unsigned char *pixels, int width, int height, size_t pitch) {
    FILE *file = bongo_cat_file_open(temporary, "wb");
    CoverWriter writer = {file, file != NULL};
    bool ok = file && stbi_write_png_to_func(cover_write, &writer,
        width, height, 4, pixels, (int)pitch) && writer.ok;
    if (file && fclose(file) != 0) ok = false;
    if (!ok) {
        bongo_cat_file_remove(temporary);
        SDL_LogWarn(SDL_LOG_CATEGORY_VIDEO,
            "Model cover could not be encoded: %s", path);
        return false;
    }
    if (bongo_cat_file_replace(temporary, path, false)) return true;
    bongo_cat_file_remove(temporary);
    SDL_LogWarn(SDL_LOG_CATEGORY_VIDEO,
        "Model cover temporary file could not replace target: %s", path);
    return false;
}

bool bongo_cat_model_cover_capture(BongoCatApp *app, int width, int height) {
    if (!app || !bongo_cat_model_cover_pending(app)) return false;
    uint64_t started_ns = SDL_GetTicksNS();
    const char *pending_path = bongo_cat_model_cover_pending_path(app);
    if (!pending_path) return false;
    char path[BONGO_CAT_PATH_CAP];
    int path_length = snprintf(path, sizeof(path), "%s", pending_path);
    if (path_length < 0 || (size_t)path_length >= sizeof(path)) return false;
    if (width < 2 || height < 2) return false;
    /* Keep the desktop's native pixel size while bounding readback memory. */
    if (width > 4096 || height > 4096) {
        SDL_LogWarn(SDL_LOG_CATEGORY_VIDEO,
            "Model cover refresh abandoned for unsupported size %dx%d: %s",
            width, height, path);
        bongo_cat_model_cover_finish(app);
        return true;
    }
    size_t pitch = (size_t)width * 4;
    size_t bytes = pitch * (size_t)height;
    if (pitch / 4 != (size_t)width || bytes / pitch != (size_t)height) {
        SDL_LogWarn(SDL_LOG_CATEGORY_VIDEO,
            "Model cover refresh skipped due to size overflow: %s", path);
        return false;
    }
    char temporary[BONGO_CAT_PATH_CAP];
    int temporary_length = snprintf(temporary, sizeof(temporary), "%s.tmp.%s",
        path, app->secondary_pet ? "secondary" : "primary");
    if (temporary_length < 0 || (size_t)temporary_length >= sizeof(temporary)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_VIDEO,
            "Model cover temporary path is too long: %s", path);
        bongo_cat_model_cover_finish(app);
        return true;
    }
    unsigned char *pixels = read_cover_pixels(path, width, height, pitch, bytes);
    if (!pixels) return false;
    bool written = write_cover_png(temporary, path, pixels,
        width, height, pitch);
    free(pixels);
    if (!written) return false;
    bongo_cat_model_cover_finish(app);
    if (app->preferences)
        bongo_cat_preferences_model_cover_reload(app, path);
    SDL_Log("Model cover refreshed: path=%s (%dx%d) elapsed_ms=%.2f", path,
        width, height, (double)(SDL_GetTicksNS() - started_ns) / 1000000.0);
    return true;
}
