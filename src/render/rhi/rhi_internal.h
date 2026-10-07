#ifndef BONGO_CAT_RHI_INTERNAL_H
#define BONGO_CAT_RHI_INTERNAL_H

#include "bongo_cat/rhi.h"

/* Backend implementations behind the bongo_cat_rhi_* dispatchers. */

BongoCatResult bongo_cat_rhi_gl_create_window(const char *title, int width, int height,
    bool fallback_ladder, SDL_Window **window, BongoCatRhi *rhi,
    BongoCatError *error);
void bongo_cat_rhi_gl_shutdown(BongoCatRhi *rhi);
bool bongo_cat_rhi_gl_make_current(BongoCatRhi *rhi);
void bongo_cat_rhi_gl_detach(const BongoCatRhi *rhi);
void bongo_cat_rhi_gl_prepare_frame(BongoCatRhi *rhi, int width, int height);
void bongo_cat_rhi_gl_viewport(BongoCatRhi *rhi, int x, int y, int width,
    int height);
void bongo_cat_rhi_gl_clear(BongoCatRhi *rhi, float red, float green,
    float blue, float alpha);
const char *bongo_cat_rhi_gl_describe(const BongoCatRhi *rhi);

BongoCatResult bongo_cat_rhi_vk_create_window(const char *title, int width, int height,
    SDL_Window **window, BongoCatRhi *rhi, BongoCatError *error);
void bongo_cat_rhi_vk_shutdown(BongoCatRhi *rhi);
bool bongo_cat_rhi_vk_make_current(BongoCatRhi *rhi);
void bongo_cat_rhi_vk_detach(const BongoCatRhi *rhi);
void bongo_cat_rhi_vk_prepare_frame(BongoCatRhi *rhi, int width, int height);
void bongo_cat_rhi_vk_viewport(BongoCatRhi *rhi, int x, int y, int width,
    int height);
void bongo_cat_rhi_vk_clear(BongoCatRhi *rhi, float red, float green,
    float blue, float alpha);
const char *bongo_cat_rhi_vk_describe(const BongoCatRhi *rhi);
bool bongo_cat_rhi_vk_get_device_info(const BongoCatRhi *rhi,
    BongoCatRhiDeviceInfo *info);
void *bongo_cat_rhi_vk_begin_commands(const BongoCatRhi *rhi);
bool bongo_cat_rhi_vk_submit_commands(const BongoCatRhi *rhi, void *command);
bool bongo_cat_rhi_vk_render_frame(BongoCatRhi *rhi);
bool bongo_cat_rhi_vk_get_frame_info(const BongoCatRhi *rhi, BongoCatRhiVulkanFrameInfo *info);
bool bongo_cat_rhi_vk_wait_idle(const BongoCatRhi *rhi);

BongoCatResult bongo_cat_rhi_metal_create_window(const char *title, int width,
    int height, SDL_Window **window, BongoCatRhi *rhi, BongoCatError *error);
bool bongo_cat_rhi_metal_render_frame(BongoCatRhi *rhi);
void bongo_cat_rhi_metal_shutdown(BongoCatRhi *rhi);
bool bongo_cat_rhi_metal_make_current(BongoCatRhi *rhi);
void bongo_cat_rhi_metal_detach(const BongoCatRhi *rhi);
void bongo_cat_rhi_metal_prepare_frame(BongoCatRhi *rhi, int width, int height);
void bongo_cat_rhi_metal_viewport(BongoCatRhi *rhi, int x, int y, int width,
    int height);
void bongo_cat_rhi_metal_clear(BongoCatRhi *rhi, float red, float green,
    float blue, float alpha);
const char *bongo_cat_rhi_metal_describe(const BongoCatRhi *rhi);
bool bongo_cat_rhi_metal_get_device_info(const BongoCatRhi *rhi,
    BongoCatRhiDeviceInfo *info);

bool bongo_cat_rhi_metal_get_frame_info(const BongoCatRhi *rhi,
    BongoCatRhiMetalFrameInfo *info);

#endif
