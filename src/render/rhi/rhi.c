/* Backend-neutral dispatcher behind the public bongo_cat_rhi_* API. The pet
   process owns a single frame backend (the main window): presenters and
   readback helpers consult the active RHI so the legacy direct-OpenGL code
   paths stay byte-for-byte when OpenGL is selected. A requested backend that
   is unavailable on this platform, or whose device creation fails, falls
   back to the OpenGL compatibility ladder instead of failing startup. */
#include "rhi_internal.h"

#include <SDL3/SDL.h>
#include <string.h>

static const BongoCatRhi *active_rhi;

const BongoCatRhiPresentOps *bongo_cat_rhi_active_present_ops(void) {
    return active_rhi ? &active_rhi->present : NULL;
}

bool bongo_cat_rhi_active_is_gl(void) {
    return !active_rhi || active_rhi->backend == BONGO_CAT_RHI_OPENGL;
}

bool bongo_cat_rhi_backend_available(BongoCatRhiBackend backend) {
    switch (backend) {
    case BONGO_CAT_RHI_OPENGL: return true;
    case BONGO_CAT_RHI_VULKAN:
#if defined(BONGO_CAT_HAS_VULKAN_RHI) && \
    (defined(_WIN32) || (defined(__linux__) && defined(__x86_64__)))
        return true;
#else
        return false;
#endif
    case BONGO_CAT_RHI_METAL:
#ifdef __APPLE__
        return true;
#else
        return false;
#endif
    default: return false;
    }
}

static BongoCatResult create_backend(BongoCatRhiBackend backend,
    const char *title, int width, int height, bool fallback_ladder,
    SDL_Window **window, BongoCatRhi *rhi, BongoCatError *error) {
    switch (backend) {
    case BONGO_CAT_RHI_VULKAN:
        return bongo_cat_rhi_vk_create_window(title, width, height, window,
            rhi, error);
    case BONGO_CAT_RHI_METAL:
#ifdef __APPLE__
        return bongo_cat_rhi_metal_create_window(title, width, height,
            window, rhi, error);
#else
        return BONGO_CAT_ERROR_PLATFORM;
#endif
    default:
        return bongo_cat_rhi_gl_create_window(title, width, height,
            fallback_ladder, window, rhi, error);
    }
}

BongoCatResult bongo_cat_rhi_create_window(BongoCatRhiBackend backend,
    const char *title, int width, int height, bool fallback_ladder,
    SDL_Window **window, BongoCatRhi *rhi, BongoCatError *error) {
    if (!window || !rhi || !error) return BONGO_CAT_ERROR_ARGUMENT;
    memset(rhi, 0, sizeof(*rhi));
    if (!bongo_cat_rhi_backend_available(backend)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_VIDEO,
            "Render backend %d is unavailable on this platform; using OpenGL",
            (int)backend);
        backend = BONGO_CAT_RHI_OPENGL;
    }
    BongoCatResult result = create_backend(backend, title, width, height,
        fallback_ladder, window, rhi, error);
    if (result != BONGO_CAT_OK && backend != BONGO_CAT_RHI_OPENGL) {
        SDL_LogWarn(SDL_LOG_CATEGORY_VIDEO,
            "Render backend initialization failed; falling back to OpenGL");
        memset(rhi, 0, sizeof(*rhi));
        result = bongo_cat_rhi_gl_create_window(title, width, height,
            fallback_ladder, window, rhi, error);
    }
    if (result == BONGO_CAT_OK) {
        memset(error, 0, sizeof(*error));
        active_rhi = rhi;
    }
    return result;
}

void bongo_cat_rhi_destroy(BongoCatRhi *rhi) {
    if (!rhi) return;
    if (active_rhi == rhi) active_rhi = NULL;
    switch (rhi->backend) {
    case BONGO_CAT_RHI_VULKAN: bongo_cat_rhi_vk_shutdown(rhi); break;
    case BONGO_CAT_RHI_METAL: bongo_cat_rhi_metal_shutdown(rhi); break;
    default: bongo_cat_rhi_gl_shutdown(rhi); break;
    }
    memset(rhi, 0, sizeof(*rhi));
}

bool bongo_cat_rhi_get_device_info(const BongoCatRhi *rhi,
    BongoCatRhiDeviceInfo *info) {
    if (!info) return false;
    memset(info, 0, sizeof(*info));
    if (!rhi) return false;
    switch (rhi->backend) {
    case BONGO_CAT_RHI_VULKAN:
        return bongo_cat_rhi_vk_get_device_info(rhi, info);
    case BONGO_CAT_RHI_METAL:
        return bongo_cat_rhi_metal_get_device_info(rhi, info);
    default:
        info->backend = BONGO_CAT_RHI_OPENGL;
        return true;
    }
}

void *bongo_cat_rhi_begin_commands(const BongoCatRhi *rhi) {
    if (!rhi) return NULL;
    switch (rhi->backend) {
    case BONGO_CAT_RHI_VULKAN: return bongo_cat_rhi_vk_begin_commands(rhi);
    default: return NULL;
    }
}

bool bongo_cat_rhi_submit_commands_checked(const BongoCatRhi *rhi, void *command) {
    return rhi && rhi->backend == BONGO_CAT_RHI_VULKAN &&
        bongo_cat_rhi_vk_submit_commands(rhi, command);
}

void bongo_cat_rhi_submit_commands(const BongoCatRhi *rhi, void *command) {
    (void)bongo_cat_rhi_submit_commands_checked(rhi, command);
}

bool bongo_cat_rhi_get_active_device_info(BongoCatRhiDeviceInfo *info) {
    return bongo_cat_rhi_get_device_info(active_rhi, info);
}

bool bongo_cat_rhi_render_frame(BongoCatRhi *rhi) {
    if (!rhi) return false;
    switch (rhi->backend) {
    case BONGO_CAT_RHI_VULKAN: return bongo_cat_rhi_vk_render_frame(rhi);
    case BONGO_CAT_RHI_METAL: return bongo_cat_rhi_metal_render_frame(rhi);
    default: return true;
    }
}

bool bongo_cat_rhi_wait_idle(const BongoCatRhi *rhi) {
    return rhi && (rhi->backend != BONGO_CAT_RHI_VULKAN ||
        bongo_cat_rhi_vk_wait_idle(rhi));
}

bool bongo_cat_rhi_live2d_supported(const BongoCatRhi *rhi) {
    if (!rhi) return false;
    switch (rhi->backend) {
#ifdef BONGO_CAT_HAS_CUBISM_VULKAN
    case BONGO_CAT_RHI_VULKAN: return true;
#endif
#ifdef BONGO_CAT_HAS_CUBISM_METAL
    case BONGO_CAT_RHI_METAL: return true;
#endif
    case BONGO_CAT_RHI_OPENGL: return true;
    default: return false;
    }
}

bool bongo_cat_rhi_is_gl(const BongoCatRhi *rhi) {
    return !rhi || rhi->backend == BONGO_CAT_RHI_OPENGL;
}

const char *bongo_cat_rhi_describe(const BongoCatRhi *rhi) {
    if (!rhi) return "OpenGL";
    switch (rhi->backend) {
    case BONGO_CAT_RHI_VULKAN: return bongo_cat_rhi_vk_describe(rhi);
    case BONGO_CAT_RHI_METAL: return bongo_cat_rhi_metal_describe(rhi);
    default: return bongo_cat_rhi_gl_describe(rhi);
    }
}

bool bongo_cat_rhi_make_current(BongoCatRhi *rhi) {
    if (!rhi) return false;
    switch (rhi->backend) {
    case BONGO_CAT_RHI_VULKAN: return bongo_cat_rhi_vk_make_current(rhi);
    case BONGO_CAT_RHI_METAL: return bongo_cat_rhi_metal_make_current(rhi);
    default: return bongo_cat_rhi_gl_make_current(rhi);
    }
}

void bongo_cat_rhi_detach(const BongoCatRhi *rhi) {
    if (!rhi) return;
    switch (rhi->backend) {
    case BONGO_CAT_RHI_VULKAN: bongo_cat_rhi_vk_detach(rhi); break;
    case BONGO_CAT_RHI_METAL: bongo_cat_rhi_metal_detach(rhi); break;
    default: bongo_cat_rhi_gl_detach(rhi); break;
    }
}

void bongo_cat_rhi_prepare_frame(BongoCatRhi *rhi, int width, int height) {
    if (!rhi) return;
    switch (rhi->backend) {
    case BONGO_CAT_RHI_VULKAN:
        bongo_cat_rhi_vk_prepare_frame(rhi, width, height); break;
    case BONGO_CAT_RHI_METAL:
        bongo_cat_rhi_metal_prepare_frame(rhi, width, height); break;
    default: bongo_cat_rhi_gl_prepare_frame(rhi, width, height); break;
    }
}

void bongo_cat_rhi_viewport(BongoCatRhi *rhi, int x, int y, int width,
    int height) {
    if (!rhi) return;
    switch (rhi->backend) {
    case BONGO_CAT_RHI_VULKAN:
        bongo_cat_rhi_vk_viewport(rhi, x, y, width, height); break;
    case BONGO_CAT_RHI_METAL:
        bongo_cat_rhi_metal_viewport(rhi, x, y, width, height); break;
    default: bongo_cat_rhi_gl_viewport(rhi, x, y, width, height); break;
    }
}

void bongo_cat_rhi_clear(BongoCatRhi *rhi, float red, float green,
    float blue, float alpha) {
    if (!rhi) return;
    switch (rhi->backend) {
    case BONGO_CAT_RHI_VULKAN:
        bongo_cat_rhi_vk_clear(rhi, red, green, blue, alpha); break;
    case BONGO_CAT_RHI_METAL:
        bongo_cat_rhi_metal_clear(rhi, red, green, blue, alpha); break;
    default: bongo_cat_rhi_gl_clear(rhi, red, green, blue, alpha); break;
    }
}

bool bongo_cat_rhi_get_metal_frame_info(const BongoCatRhi *rhi,
    BongoCatRhiMetalFrameInfo *info) {
    if (!info) return false;
    memset(info, 0, sizeof(*info));
    return rhi && rhi->backend == BONGO_CAT_RHI_METAL &&
        bongo_cat_rhi_metal_get_frame_info(rhi, info);
}

bool bongo_cat_rhi_get_vulkan_frame_info(const BongoCatRhi *rhi,
    BongoCatRhiVulkanFrameInfo *info) {
    if (!info) return false;
    memset(info, 0, sizeof(*info));
    return rhi && rhi->backend == BONGO_CAT_RHI_VULKAN &&
        bongo_cat_rhi_vk_get_frame_info(rhi, info);
}
