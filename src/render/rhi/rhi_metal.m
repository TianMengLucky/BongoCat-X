/* Native macOS Metal frame submission and readback. A frame hook receives
   borrowed command-buffer/render-pass handles through the additive frame API.
   The RHI owns encoding completion, GPU readback, commit and presentation. */
#import <Metal/Metal.h>
#import <QuartzCore/QuartzCore.h>

#include "rhi_internal.h"
#include "rhi_pixels.h"
#include "bongo_cat/log.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_metal.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

/* Objective-C ownership belongs to ARC-managed ivars, not calloc'ed C
   storage. Only the SDL view owns the layer. The retained impl handle is
   released in shutdown before the runtime destroys its SDL window. */
@interface BongoCatRhiMetal : NSObject {
@public
    char describe[160];
    const BongoCatRhi *owner;
    SDL_MetalView view;
    __unsafe_unretained CAMetalLayer *layer;
    id<MTLCommandQueue> queue;
    id<CAMetalDrawable> pending_drawable;
    id<MTLCommandBuffer> frame_buffer;
    MTLRenderPassDescriptor *frame_pass;
    id<MTLTexture> frame_texture;
    id<MTLBuffer> readback;
    id<MTLTexture> depth_texture;
    NSUInteger stride;
    int frame_width, frame_height;
    bool has_frame;
    float clear[4];
}
@end
@implementation BongoCatRhiMetal
@end

static BongoCatRhiMetal *state(const BongoCatRhi *rhi) {
    return rhi ? (__bridge BongoCatRhiMetal *)rhi->impl : nil;
}

static const char *present_name(void *user) {
    BongoCatRhiMetal *metal = (__bridge BongoCatRhiMetal *)user;
    return metal ? metal->describe : "Metal";
}

static bool frame_present(SDL_Window *window, void *user);

bool bongo_cat_rhi_metal_render_frame(BongoCatRhi *rhi) {
    BongoCatRhiMetal *metal = state(rhi);
    if (!metal || !metal->layer || !metal->queue) return false;
    @autoreleasepool {
        if (metal->pending_drawable && !frame_present(rhi->window, rhi->impl))
            return false;
        id<CAMetalDrawable> drawable = [metal->layer nextDrawable];
        id<MTLCommandBuffer> buffer = [metal->queue commandBuffer];
        if (!drawable || !buffer) return false;
        NSUInteger width = drawable.texture.width;
        NSUInteger height = drawable.texture.height;
        if (!width || !height || width > INT_MAX || height > INT_MAX ||
            width > (NSUIntegerMax - 255) / 4) return false;
        NSUInteger stride = (width * 4 + 255) & ~(NSUInteger)255;
        if (height > NSUIntegerMax / stride) return false;
        NSUInteger length = stride * height;
        if (!metal->readback || metal->readback.length < length)
            metal->readback = [metal->layer.device newBufferWithLength:length
                options:MTLResourceStorageModeShared];
        if (!metal->readback) return false;
        metal->has_frame = false;
        if (!metal->depth_texture || metal->depth_texture.width != width ||
            metal->depth_texture.height != height) {
            MTLTextureDescriptor *depth = [MTLTextureDescriptor
                texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float
                width:width height:height mipmapped:NO];
            depth.storageMode = MTLStorageModePrivate;
            depth.usage = MTLTextureUsageRenderTarget;
            metal->depth_texture = [metal->layer.device newTextureWithDescriptor:depth];
        }
        if (!metal->depth_texture) return false;
        MTLRenderPassDescriptor *pass =
            [MTLRenderPassDescriptor renderPassDescriptor];
        pass.colorAttachments[0].texture = drawable.texture;
        pass.colorAttachments[0].loadAction = MTLLoadActionClear;
        pass.colorAttachments[0].storeAction = MTLStoreActionStore;
        pass.depthAttachment.texture = metal->depth_texture;
        pass.depthAttachment.loadAction = MTLLoadActionClear;
        pass.depthAttachment.storeAction = MTLStoreActionStore;
        pass.depthAttachment.clearDepth = 1.0;
        float alpha = metal->clear[3];
        pass.colorAttachments[0].clearColor = MTLClearColorMake(
            metal->clear[0] * alpha, metal->clear[1] * alpha,
            metal->clear[2] * alpha, alpha);
        id<MTLRenderCommandEncoder> encoder =
            [buffer renderCommandEncoderWithDescriptor:pass];
        if (!encoder) return false;
        [encoder endEncoding];
        pass.colorAttachments[0].loadAction = MTLLoadActionLoad;
        pass.depthAttachment.loadAction = MTLLoadActionLoad;
        metal->frame_buffer = buffer;
        metal->frame_pass = pass;
        metal->frame_texture = drawable.texture;
        const BongoCatRhiPresentOps *ops = &metal->owner->present;
        bool drawn = !ops->draw_frame || ops->draw_frame(ops->hook_user);
        /* Borrowed handles are valid only while the draw callback encodes. */
        metal->frame_buffer = nil;
        metal->frame_pass = nil;
        metal->frame_texture = nil;
        if (!drawn) return false;
        id<MTLBlitCommandEncoder> blit = [buffer blitCommandEncoder];
        if (!blit) return false;
        [blit copyFromTexture:drawable.texture sourceSlice:0 sourceLevel:0
            sourceOrigin:MTLOriginMake(0, 0, 0)
            sourceSize:MTLSizeMake(width, height, 1)
            toBuffer:metal->readback destinationOffset:0
            destinationBytesPerRow:stride destinationBytesPerImage:length];
        [blit endEncoding];
        [buffer commit];
        /* This milestone keeps one frame in flight and caches real pixels for
           hit testing. Asynchronous readback can replace the wait later. */
        [buffer waitUntilCompleted];
        if (buffer.status != MTLCommandBufferStatusCompleted) {
            SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Metal frame failed: %s",
                buffer.error.localizedDescription.UTF8String ?: "unknown error");
            return false;
        }
        metal->stride = stride;
        metal->frame_width = (int)width;
        metal->frame_height = (int)height;
        metal->has_frame = true;
        metal->pending_drawable = drawable;
        return true;
    }
}

static bool frame_present(SDL_Window *window, void *user) {
    (void)window;
    BongoCatRhiMetal *metal = (__bridge BongoCatRhiMetal *)user;
    if (!metal) return false;
    if (!metal->pending_drawable &&
        !bongo_cat_rhi_metal_render_frame((BongoCatRhi *)metal->owner)) return false;
    id<MTLCommandBuffer> buffer = [metal->queue commandBuffer];
    if (!buffer) return false;
    [buffer presentDrawable:metal->pending_drawable];
    [buffer commit];
    [buffer waitUntilCompleted];
    metal->pending_drawable = nil;
    return buffer.status == MTLCommandBufferStatusCompleted;
}

static bool read_bgra(int width, int height, void *pixels, void *user) {
    BongoCatRhiMetal *metal = (__bridge BongoCatRhiMetal *)user;
    return metal && metal->has_frame && width == metal->frame_width &&
        height == metal->frame_height && bongo_cat_rhi_copy_pixels(
            metal->readback.contents, width, height, metal->stride, true,
            0, 0, width, height, true, pixels);
}

static bool read_rgba(int x, int y, int width, int height, bool back_buffer,
    void *pixels, void *user) {
    (void)back_buffer;
    BongoCatRhiMetal *metal = (__bridge BongoCatRhiMetal *)user;
    return metal && metal->has_frame && bongo_cat_rhi_copy_pixels(
        metal->readback.contents, metal->frame_width, metal->frame_height,
        metal->stride, true, x, y, width, height, false, pixels);
}

BongoCatResult bongo_cat_rhi_metal_create_window(const char *title, int width,
    int height, SDL_Window **window, BongoCatRhi *rhi, BongoCatError *error) {
    @autoreleasepool {
        SDL_WindowFlags flags = SDL_WINDOW_METAL | SDL_WINDOW_BORDERLESS |
            SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_HIDDEN | SDL_WINDOW_TRANSPARENT;
        *window = SDL_CreateWindow(title, width, height, flags);
        if (!*window) {
            bongo_cat_error_set(error, BONGO_CAT_ERROR_PLATFORM,
                "Window creation: %s", SDL_GetError());
            return BONGO_CAT_ERROR_PLATFORM;
        }
        BongoCatRhiMetal *metal = [BongoCatRhiMetal new];
        if (!metal) {
            SDL_DestroyWindow(*window);
            *window = NULL;
            bongo_cat_error_set(error, BONGO_CAT_ERROR_MEMORY,
                "Cannot allocate the Metal RHI state");
            return BONGO_CAT_ERROR_MEMORY;
        }
        rhi->impl = (__bridge_retained void *)metal;
        metal->owner = rhi;
        id<MTLDevice> device = MTLCreateSystemDefaultDevice();
        metal->queue = [device newCommandQueue];
        if (!device || !metal->queue) {
            bongo_cat_error_set(error, BONGO_CAT_ERROR_PLATFORM,
                "No Metal device or command queue is available");
            goto failed;
        }
        metal->view = SDL_Metal_CreateView(*window);
        if (!metal->view) {
            bongo_cat_error_set(error, BONGO_CAT_ERROR_PLATFORM,
                "SDL cannot create the Metal view: %s", SDL_GetError());
            goto failed;
        }
        metal->layer = (__bridge CAMetalLayer *)SDL_Metal_GetLayer(metal->view);
        if (!metal->layer) {
            bongo_cat_error_set(error, BONGO_CAT_ERROR_PLATFORM,
                "SDL cannot provide the Metal layer");
            goto failed;
        }
        metal->layer.device = device;
        metal->layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
        metal->layer.opaque = NO;
        metal->layer.framebufferOnly = NO; /* The blit reads the drawable. */
        metal->layer.maximumDrawableCount = 3;
        snprintf(metal->describe, sizeof(metal->describe), "Metal %s",
            device.name ? device.name.UTF8String : "device");
        rhi->backend = BONGO_CAT_RHI_METAL;
        rhi->window = *window;
        rhi->present.swap = frame_present;
        rhi->present.read_bgra = read_bgra;
        rhi->present.read_rgba = read_rgba;
        rhi->present.name = present_name;
        rhi->present.requires_layered = false;
        rhi->present.user = rhi->impl;
        bongo_cat_rhi_metal_prepare_frame(rhi, width, height);
        SDL_LogInfo(BONGO_CAT_LOG_LIFECYCLE, "[render] %s ready", metal->describe);
        return BONGO_CAT_OK;

failed:
        bongo_cat_rhi_metal_shutdown(rhi);
        SDL_DestroyWindow(*window);
        *window = NULL;
        return error->code;
    }
}

void bongo_cat_rhi_metal_shutdown(BongoCatRhi *rhi) {
    if (!rhi || !rhi->impl) return;
    BongoCatRhiMetal *metal = (__bridge_transfer BongoCatRhiMetal *)rhi->impl;
    rhi->impl = NULL;
    if (metal->view) SDL_Metal_DestroyView(metal->view);
    metal->layer = nil;
    metal->view = NULL;
}

bool bongo_cat_rhi_metal_make_current(BongoCatRhi *rhi) {
    BongoCatRhiMetal *metal = state(rhi);
    return metal && metal->layer && metal->queue;
}

void bongo_cat_rhi_metal_detach(const BongoCatRhi *rhi) { (void)rhi; }

void bongo_cat_rhi_metal_prepare_frame(BongoCatRhi *rhi, int width, int height) {
    BongoCatRhiMetal *metal = state(rhi);
    if (!metal || !metal->layer || width <= 0 || height <= 0) return;
    metal->layer.drawableSize = CGSizeMake((CGFloat)width, (CGFloat)height);
    if (width != metal->frame_width || height != metal->frame_height)
        metal->has_frame = false;
}

void bongo_cat_rhi_metal_viewport(BongoCatRhi *rhi, int x, int y, int width,
    int height) {
    (void)rhi; (void)x; (void)y; (void)width; (void)height;
}

void bongo_cat_rhi_metal_clear(BongoCatRhi *rhi, float red, float green,
    float blue, float alpha) {
    BongoCatRhiMetal *metal = state(rhi);
    if (!metal) return;
    metal->clear[0] = red;
    metal->clear[1] = green;
    metal->clear[2] = blue;
    metal->clear[3] = alpha;
}

const char *bongo_cat_rhi_metal_describe(const BongoCatRhi *rhi) {
    BongoCatRhiMetal *metal = state(rhi);
    return metal ? metal->describe : "Metal";
}

bool bongo_cat_rhi_metal_get_device_info(const BongoCatRhi *rhi,
    BongoCatRhiDeviceInfo *info) {
    BongoCatRhiMetal *metal = state(rhi);
    if (!metal || !info || !metal->layer) return false;
    memset(info, 0, sizeof(*info));
    info->backend = BONGO_CAT_RHI_METAL;
    info->metal_device = (__bridge void *)metal->layer.device;
    info->metal_layer = (__bridge void *)metal->layer;
    info->extent_width = (uint32_t)metal->layer.drawableSize.width;
    info->extent_height = (uint32_t)metal->layer.drawableSize.height;
    info->rhi_handle = rhi;
    return true;
}

bool bongo_cat_rhi_metal_get_frame_info(const BongoCatRhi *rhi,
    BongoCatRhiMetalFrameInfo *info) {
    if (!info) return false;
    memset(info, 0, sizeof(*info));
    BongoCatRhiMetal *metal = state(rhi);
    if (!metal || !metal->frame_buffer || !metal->frame_pass) return false;
    info->command_buffer = (__bridge void *)metal->frame_buffer;
    info->render_pass = (__bridge void *)metal->frame_pass;
    info->color_texture = (__bridge void *)metal->frame_texture;
    return true;
}
