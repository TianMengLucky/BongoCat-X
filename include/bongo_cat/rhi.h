#ifndef BONGO_CAT_RHI_H
#define BONGO_CAT_RHI_H

#include "bongo_cat/common.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SDL_Window SDL_Window;

/* Resolved render backends. The persisted setting additionally has an
   "auto" value (see BongoCatRenderBackend in config.h) that resolves to
   OpenGL before a device is created. Vulkan is offered on Windows and
   64-bit Linux, Metal on macOS; the availability helper decides. */
typedef enum BongoCatRhiBackend {
    BONGO_CAT_RHI_OPENGL = 0,
    BONGO_CAT_RHI_VULKAN = 1,
    BONGO_CAT_RHI_METAL = 2
} BongoCatRhiBackend;

/* Whether this build/platform can create the backend's device at all. */
bool bongo_cat_rhi_backend_available(BongoCatRhiBackend backend);

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
    /* Per-frame draw hook (see above); consulted by backends whose present
       path can hand the acquired frame to the bridge. */
    bool (*draw_frame)(void *hook_user);
    void *hook_user;
} BongoCatRhiPresentOps;

typedef struct BongoCatRhi {
    BongoCatRhiBackend backend;
    SDL_Window *window;
    void *context; /* SDL GL context (OpenGL backend only). */
    void *impl;    /* Backend-private state. */
    BongoCatRhiPresentOps present;
} BongoCatRhi;

/* One-shot command buffers for bridge-side device work (texture uploads).
   Returned buffers are submitted with submit_commands on the same queue. */
void *bongo_cat_rhi_begin_commands(const BongoCatRhi *rhi);
void bongo_cat_rhi_submit_commands(const BongoCatRhi *rhi, void *command);

/* Creates the native window (and, for OpenGL, the context) for a backend.
   `fallback_ladder` enables the transparent/MSAA retry ladder; it only
   applies to OpenGL. On success `rhi` holds an initialized backend state
   and its present ops. */
BongoCatResult bongo_cat_rhi_create_window(BongoCatRhiBackend backend,
    const char *title, int width, int height, bool fallback_ladder,
    SDL_Window **window, BongoCatRhi *rhi, BongoCatError *error);
void bongo_cat_rhi_destroy(BongoCatRhi *rhi);

/* Device-level handles for the Live2D bridge's Cubism Vulkan/Metal
   renderers (see docs/live2d-vulkan-metal.md). Filled by the active
   backend; zero handles for OpenGL. */
typedef struct BongoCatRhiDeviceInfo {
    BongoCatRhiBackend backend;
    /* Vulkan */
    void *vulkan_device;          /* VkDevice */
    void *vulkan_physical_device; /* VkPhysicalDevice */
    void *vulkan_command_pool;    /* VkCommandPool */
    void *vulkan_queue;           /* VkQueue */
    uint32_t queue_family;
    uint32_t image_count;
    uint32_t extent_width, extent_height;
    int color_format;             /* VkFormat */
    int depth_format;             /* VkFormat */
    void **swapchain_views;       /* VkImageView[image_count], RHI-owned */
    void *current_image;          /* VkImage of the acquired frame */
    void *current_view;           /* VkImageView of the acquired frame */
    /* Metal */
    void *metal_device;           /* id<MTLDevice> */
    void *metal_layer;            /* CAMetalLayer * */
    /* The owning RHI (for bongo_cat_rhi_begin_commands on bridge side). */
    const void *rhi_handle;
} BongoCatRhiDeviceInfo;

bool bongo_cat_rhi_get_device_info(const BongoCatRhi *rhi,
    BongoCatRhiDeviceInfo *info);

bool bongo_cat_rhi_is_gl(const BongoCatRhi *rhi);
/* "OpenGL 3.3 ..." style device description for logs. */
const char *bongo_cat_rhi_describe(const BongoCatRhi *rhi);

/* The pet process owns a single frame backend. Presenters and readback
   helpers that predate the RHI consult these hooks so the legacy
   direct-OpenGL paths stay untouched when OpenGL is selected. */
const BongoCatRhiPresentOps *bongo_cat_rhi_active_present_ops(void);
bool bongo_cat_rhi_active_is_gl(void);

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

#ifdef __cplusplus
}
#endif

#endif
