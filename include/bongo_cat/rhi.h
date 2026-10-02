#ifndef BONGO_CAT_RHI_H
#define BONGO_CAT_RHI_H

#include "bongo_cat/common.h"
#include <stdbool.h>

typedef struct SDL_Window SDL_Window;

/* Resolved render backends. The persisted setting additionally has an
   "auto" value (see BongoCatRenderBackend in config.h) that resolves to
   OpenGL before a device is created. */
typedef enum BongoCatRhiBackend {
    BONGO_CAT_RHI_OPENGL = 0,
    BONGO_CAT_RHI_VULKAN = 1
} BongoCatRhiBackend;

/* Backend-neutral presentation hooks consumed by the platform presenters
   (the Windows layered window reads pixels instead of showing a surface).
   Every hook falls back to the legacy OpenGL path when no RHI is attached. */
typedef struct BongoCatRhiPresentOps {
    /* Presents the default framebuffer on the direct (non-layered) path. */
    bool (*swap)(SDL_Window *window, void *user);
    /* Reads the whole frame as premultiplied BGRA, bottom-up, for the
       Windows layered presenter. */
    bool (*read_bgra)(int width, int height, void *pixels, void *user);
    /* Reads an RGBA rectangle from the window back buffer (hit tests,
       diagnostics). Coordinates use the GL convention: origin bottom-left. */
    bool (*read_rgba)(int x, int y, int width, int height, bool back_buffer,
        void *pixels, void *user);
    /* Human-readable renderer name for diagnostics logs. */
    const char *(*name)(void *user);
    /* True when the only viable presentation path is a pixel readback
       (Vulkan has no alpha-capable swapchain on Windows). */
    bool requires_layered;
    void *user;
} BongoCatRhiPresentOps;

typedef struct BongoCatRhi {
    BongoCatRhiBackend backend;
    SDL_Window *window;
    void *context; /* SDL GL context (OpenGL backend only). */
    void *impl;    /* Backend-private state. */
    BongoCatRhiPresentOps present;
} BongoCatRhi;

/* Creates the native window (and, for OpenGL, the context) for a backend.
   `fallback_ladder` enables the transparent/MSAA retry ladder; it only
   applies to OpenGL. On success `rhi` holds an initialized backend state
   and its present ops. */
BongoCatResult bongo_cat_rhi_create_window(BongoCatRhiBackend backend,
    const char *title, int width, int height, bool fallback_ladder,
    SDL_Window **window, BongoCatRhi *rhi, BongoCatError *error);
void bongo_cat_rhi_destroy(BongoCatRhi *rhi);

bool bongo_cat_rhi_is_gl(const BongoCatRhi *rhi);
/* "OpenGL 3.3 ..." style device description for logs. */
const char *bongo_cat_rhi_describe(const BongoCatRhi *rhi);

/* Frame boundaries. make_current binds the device to the main window
   (OpenGL) or validates the device (Vulkan); detach releases it so other
   windows (preferences) can use their own context. */
bool bongo_cat_rhi_make_current(BongoCatRhi *rhi);
void bongo_cat_rhi_detach(const BongoCatRhi *rhi);
/* Per-frame fixed-function state; also refreshes the offscreen target
   size on Vulkan. */
void bongo_cat_rhi_prepare_frame(BongoCatRhi *rhi, int width, int height);
void bongo_cat_rhi_viewport(BongoCatRhi *rhi, int x, int y, int width,
    int height);
void bongo_cat_rhi_clear(BongoCatRhi *rhi, float red, float green,
    float blue, float alpha);

#endif
