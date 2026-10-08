/* OpenGL RHI backend. Owns the window/context creation ladder that used to
   live in runtime/shell/window.c and exposes the frame primitives through
   the backend-neutral RHI entry points. Rendering behavior is unchanged. */
#include "rhi_internal.h"
#include "bongo_cat/gl_api.h"
#include "../../platform/common/gl_readback.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct BongoCatRhiGl {
    char describe[128];
} BongoCatRhiGl;

static bool set_gl_attributes(int samples) {
    SDL_GL_ResetAttributes();
#ifdef __APPLE__
    const int major = 4, minor = 1, profile = SDL_GL_CONTEXT_PROFILE_CORE;
#elif defined(BONGO_CAT_HAS_MODEL_PLUGINS)
    const int major = 3, minor = 3, profile = SDL_GL_CONTEXT_PROFILE_COMPATIBILITY;
#else
    const int major = 3, minor = 3, profile = SDL_GL_CONTEXT_PROFILE_CORE;
#endif
    return SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, major) &&
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, minor) &&
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, profile) &&
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1) &&
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0) &&
        SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8) &&
        SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8) &&
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, samples > 0 ? 1 : 0) &&
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, samples);
}

static bool try_window(const char *title, int width, int height,
    bool transparent, int samples, SDL_Window **window, BongoCatRhi *rhi,
    char *failure, size_t capacity) {
    if (!set_gl_attributes(samples)) {
        snprintf(failure, capacity, "OpenGL attributes: %s", SDL_GetError());
        return false;
    }
    SDL_WindowFlags flags = SDL_WINDOW_OPENGL | SDL_WINDOW_BORDERLESS |
        SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_HIDDEN;
    if (transparent) flags |= SDL_WINDOW_TRANSPARENT;
    *window = SDL_CreateWindow(title, width, height, flags);
    if (!*window) {
        snprintf(failure, capacity, "Window creation: %s", SDL_GetError());
        return false;
    }
    rhi->context = SDL_GL_CreateContext(*window);
    if (!rhi->context || !SDL_GL_MakeCurrent(*window, rhi->context)) {
        snprintf(failure, capacity, "OpenGL context: %s", SDL_GetError());
        if (rhi->context) SDL_GL_DestroyContext(rhi->context);
        SDL_DestroyWindow(*window);
        rhi->context = NULL;
        *window = NULL;
        return false;
    }
    return true;
}

BongoCatResult bongo_cat_rhi_gl_create_window(const char *title, int width, int height,
    bool fallback_ladder, SDL_Window **window, BongoCatRhi *rhi,
    BongoCatError *error) {
    (void)fallback_ladder; /* OpenGL always uses the compatibility ladder. */
    /* Transparent OpenGL windows must never receive SDL's default black
       WM_ERASEBKGND fill before the first frame is submitted. */
    bool disable_transparent =
        SDL_getenv("BONGO_CAT_TEST_DISABLE_PREFERENCES_TRANSPARENCY") != NULL;
    /* Keep a lower-cost MSAA path for drivers that cannot provide 4 samples. */
    const int options[][2] = {{true, 4}, {true, 2}, {true, 0}, {false, 0}};
    char failure[256] = {0};
    bool force_fallback = SDL_getenv("BONGO_CAT_TEST_GL_FALLBACK") != NULL;
    for (size_t i = 0; i < sizeof(options) / sizeof(options[0]); ++i) {
        if (force_fallback && options[i][1] > 0) {
            SDL_LogWarn(SDL_LOG_CATEGORY_VIDEO, "Test requested OpenGL fallback");
            continue;
        }
        if (try_window(title, width, height, options[i][0] && !disable_transparent,
                options[i][1], window, rhi, failure, sizeof(failure))) {
            int sample_buffers = 0, sample_count = 0;
            SDL_GL_GetAttribute(SDL_GL_MULTISAMPLEBUFFERS, &sample_buffers);
            SDL_GL_GetAttribute(SDL_GL_MULTISAMPLESAMPLES, &sample_count);
            const GLubyte *vendor = glGetString(GL_VENDOR);
            const GLubyte *renderer = glGetString(GL_RENDERER);
            const GLubyte *version = glGetString(GL_VERSION);
            SDL_Log("[runtime] OpenGL window ready (transparent=%d, MSAA=%d, "
                "sample_buffers=%d, sample_count=%d)", options[i][0],
                options[i][1], sample_buffers, sample_count);
            SDL_Log("[runtime] OpenGL context: vendor=%s renderer=%s version=%s",
                vendor ? (const char *)vendor : "unknown",
                renderer ? (const char *)renderer : "unknown",
                version ? (const char *)version : "unknown");
            if (!SDL_GL_SetSwapInterval(1)) SDL_LogWarn(SDL_LOG_CATEGORY_VIDEO,
                "Vertical sync unavailable: %s", SDL_GetError());
            BongoCatRhiGl *impl = calloc(1, sizeof(*impl));
            if (!impl) {
                SDL_GL_DestroyContext(rhi->context);
                rhi->context = NULL;
                SDL_DestroyWindow(*window);
                *window = NULL;
                bongo_cat_error_set(error, BONGO_CAT_ERROR_MEMORY,
                    "Cannot allocate the OpenGL RHI state");
                bongo_cat_rhi_gl_shutdown(rhi);
                return BONGO_CAT_ERROR_MEMORY;
            }
            snprintf(impl->describe, sizeof(impl->describe),
                "OpenGL %s", version ? (const char *)version : "unknown");
            rhi->backend = BONGO_CAT_RHI_OPENGL;
            rhi->window = *window;
            rhi->impl = impl;
            rhi->present.swap = NULL;      /* platform keeps SDL_GL_SwapWindow */
            rhi->present.read_bgra = NULL; /* platform keeps its GL readback */
            rhi->present.read_rgba = NULL; /* platform keeps bongo_cat_gl_read_window */
            rhi->present.name = NULL;      /* platform falls back to glGetString */
            rhi->present.requires_layered = false;
            rhi->present.user = impl;
            return BONGO_CAT_OK;
        }
        SDL_LogWarn(SDL_LOG_CATEGORY_VIDEO, "OpenGL attempt %llu failed: %s",
            (unsigned long long)i + 1ULL, failure);
    }
    bongo_cat_error_set(error, BONGO_CAT_ERROR_PLATFORM,
        "Window and OpenGL initialization failed after compatibility retries: %s",
        failure);
    return BONGO_CAT_ERROR_PLATFORM;
}

void bongo_cat_rhi_gl_shutdown(BongoCatRhi *rhi) {
    /* The GL context and window are owned by the runtime teardown
       (bongo_cat_window_destroy) so shared preferences contexts outlive a
       frame-backend swap; only the small description state lives here. */
    free(rhi->impl);
    rhi->impl = NULL;
}

bool bongo_cat_rhi_gl_make_current(BongoCatRhi *rhi) {
    return SDL_GL_MakeCurrent(rhi->window, rhi->context);
}

void bongo_cat_rhi_gl_detach(const BongoCatRhi *rhi) {
    SDL_GL_MakeCurrent(NULL, NULL);
    (void)rhi;
}

void bongo_cat_rhi_gl_prepare_frame(BongoCatRhi *rhi, int width, int height) {
    (void)rhi; (void)width; (void)height;
    glEnable(GL_MULTISAMPLE);
    glDisable(GL_SCISSOR_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
}

void bongo_cat_rhi_gl_viewport(BongoCatRhi *rhi, int x, int y, int width,
    int height) {
    (void)rhi;
    glViewport(x, y, width, height);
}

void bongo_cat_rhi_gl_clear(BongoCatRhi *rhi, float red, float green,
    float blue, float alpha) {
    (void)rhi;
    glClearColor(red, green, blue, alpha);
    glClear(GL_COLOR_BUFFER_BIT);
}

const char *bongo_cat_rhi_gl_describe(const BongoCatRhi *rhi) {
    BongoCatRhiGl *impl = rhi ? rhi->impl : NULL;
    return impl ? impl->describe : "OpenGL";
}
