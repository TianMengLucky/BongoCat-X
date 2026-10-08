/* Metal RHI stubs for platforms without the Metal backend: the dispatcher
   references the symbols unconditionally, while the real implementations
   live in rhi_metal.m (Apple only). Unreachable through the availability
   gate. */
#include "rhi_internal.h"

#ifndef __APPLE__

BongoCatResult bongo_cat_rhi_metal_create_window(const char *title, int width,
    int height, SDL_Window **window, BongoCatRhi *rhi, BongoCatError *error) {
    (void)title; (void)width; (void)height; (void)window; (void)rhi;
    bongo_cat_error_set(error, BONGO_CAT_ERROR_PLATFORM,
        "Metal is not supported on this platform");
    return BONGO_CAT_ERROR_PLATFORM;
}

bool bongo_cat_rhi_metal_render_frame(BongoCatRhi *rhi) { (void)rhi; return false; }

void bongo_cat_rhi_metal_shutdown(BongoCatRhi *rhi) { (void)rhi; }

bool bongo_cat_rhi_metal_make_current(BongoCatRhi *rhi) {
    (void)rhi;
    return false;
}

void bongo_cat_rhi_metal_detach(const BongoCatRhi *rhi) { (void)rhi; }

void bongo_cat_rhi_metal_prepare_frame(BongoCatRhi *rhi, int width,
    int height) {
    (void)rhi; (void)width; (void)height;
}

void bongo_cat_rhi_metal_viewport(BongoCatRhi *rhi, int x, int y, int width,
    int height) {
    (void)rhi; (void)x; (void)y; (void)width; (void)height;
}

void bongo_cat_rhi_metal_clear(BongoCatRhi *rhi, float red, float green,
    float blue, float alpha) {
    (void)rhi; (void)red; (void)green; (void)blue; (void)alpha;
}

const char *bongo_cat_rhi_metal_describe(const BongoCatRhi *rhi) {
    (void)rhi;
    return "Metal";
}

bool bongo_cat_rhi_metal_get_device_info(const BongoCatRhi *rhi,
    BongoCatRhiDeviceInfo *info) {
    (void)rhi; (void)info;
    return false;
}

bool bongo_cat_rhi_metal_set_background(BongoCatRhi *rhi, const void *pixels,
    int width, int height, int pitch, uint64_t revision) {
    (void)rhi; (void)pixels; (void)width; (void)height; (void)pitch; (void)revision;
    return false;
}

bool bongo_cat_rhi_metal_get_frame_info(const BongoCatRhi *rhi,
    BongoCatRhiMetalFrameInfo *info) {
    (void)rhi; (void)info;
    return false;
}

#endif
