#include "rhi_internal.h"
#include <stddef.h>

/* Unreachable through the availability gate; keeps the symbols so the
   dispatcher links on every platform. */
bool bongo_cat_rhi_vk_get_device_info(const BongoCatRhi *rhi,
    BongoCatRhiDeviceInfo *info) {
    (void)rhi; (void)info;
    return false;
}

void *bongo_cat_rhi_vk_begin_commands(const BongoCatRhi *rhi) {
    (void)rhi;
    return NULL;
}

bool bongo_cat_rhi_vk_submit_commands(const BongoCatRhi *rhi, void *command) {
    (void)rhi; (void)command;
    return false;
}

bool bongo_cat_rhi_vk_render_frame(BongoCatRhi *rhi) { (void)rhi; return false; }
bool bongo_cat_rhi_vk_wait_idle(const BongoCatRhi *rhi) { (void)rhi; return false; }

BongoCatResult bongo_cat_rhi_vk_create_window(const char *title, int width, int height,
    SDL_Window **window, BongoCatRhi *rhi, BongoCatError *error) {
    (void)title; (void)width; (void)height; (void)window; (void)rhi;
    bongo_cat_error_set(error, BONGO_CAT_ERROR_PLATFORM,
        "Vulkan is not supported on this platform");
    return BONGO_CAT_ERROR_PLATFORM;
}

void bongo_cat_rhi_vk_shutdown(BongoCatRhi *rhi) { (void)rhi; }

bool bongo_cat_rhi_vk_make_current(BongoCatRhi *rhi) {
    (void)rhi;
    return false;
}

void bongo_cat_rhi_vk_detach(const BongoCatRhi *rhi) { (void)rhi; }

void bongo_cat_rhi_vk_prepare_frame(BongoCatRhi *rhi, int width, int height) {
    (void)rhi; (void)width; (void)height;
}

void bongo_cat_rhi_vk_viewport(BongoCatRhi *rhi, int x, int y, int width,
    int height) {
    (void)rhi; (void)x; (void)y; (void)width; (void)height;
}

void bongo_cat_rhi_vk_clear(BongoCatRhi *rhi, float red, float green,
    float blue, float alpha) {
    (void)rhi; (void)red; (void)green; (void)blue; (void)alpha;
}

const char *bongo_cat_rhi_vk_describe(const BongoCatRhi *rhi) {
    (void)rhi;
    return "Vulkan";
}

bool bongo_cat_rhi_vk_get_frame_info(const BongoCatRhi *rhi,
    BongoCatRhiVulkanFrameInfo *info) { (void)rhi; (void)info; return false; }
