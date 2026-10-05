/* Metal RHI backend (macOS). Presents through SDL's metal view and a
   CAMetalLayer with a clear-only render pass until the draw phases port
   to the RHI. The layer composites with alpha (isOpaque = NO), so the
   direct presentation path keeps the transparent pet window. Live2D
   rendering stays on the OpenGL backend until the Metal draw phase
   lands; see the Vulkan backend for the same staged milestone. */
#import <Metal/Metal.h>
#import <QuartzCore/QuartzCore.h>

#include "rhi_internal.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

typedef struct BongoCatRhiMetal {
    char describe[160];
    SDL_MetalView view;
    /* The SDL metal view owns the layer; borrow it, never retain. */
    CAMetalLayer *layer;
    float clear[4];
} BongoCatRhiMetal;

static const char *present_name(void *user) {
    BongoCatRhiMetal *metal = user;
    return metal && metal->describe[0] ? metal->describe : "Metal";
}

/* Presents one cleared drawable; also the backend's swap present op. */
static bool frame_present(void *user) {
    BongoCatRhiMetal *metal = user;
    if (!metal || !metal->layer || !metal->layer.device) return false;
    @autoreleasepool {
        id<CAMetalDrawable> drawable = [metal->layer nextDrawable];
        if (!drawable) return false;
        MTLRenderPassDescriptor *pass =
            [MTLRenderPassDescriptor renderPassDescriptor];
        pass.colorAttachments[0].texture = drawable.texture;
        pass.colorAttachments[0].loadAction = MTLLoadActionClear;
        pass.colorAttachments[0].storeAction = MTLStoreActionStore;
        pass.colorAttachments[0].clearColor = MTLClearColorMake(
            metal->clear[0], metal->clear[1], metal->clear[2],
            metal->clear[3]);
        id<MTLCommandBuffer> buffer =
            [metal->layer.device commandBuffer];
        id<MTLRenderCommandEncoder> encoder =
            [buffer renderCommandEncoderWithDescriptor:pass];
        [encoder endEncoding];
        [buffer presentDrawable:drawable];
        [buffer commit];
        return true;
    }
}

static bool read_fill(void *user, int width, int height, void *pixels,
    bool bottom_up) {
    BongoCatRhiMetal *metal = user;
    if (!metal || width <= 0 || height <= 0 || !pixels) return false;
    /* Premultiplied BGRA synthesized from the recorded clear color: until
       the draw phases port, that color is the complete frame content. */
    uint8_t b = (uint8_t)(metal->clear[2] * 255.0f * metal->clear[3]);
    uint8_t g = (uint8_t)(metal->clear[1] * 255.0f * metal->clear[3]);
    uint8_t r = (uint8_t)(metal->clear[0] * 255.0f * metal->clear[3]);
    uint8_t a = (uint8_t)(metal->clear[3] * 255.0f);
    uint8_t *rows = pixels;
    for (int y = 0; y < height; ++y) {
        uint8_t *row = rows + (size_t)(bottom_up ? height - 1 - y : y) *
            (size_t)width * 4;
        for (int x = 0; x < width; ++x) {
            row[x * 4 + 0] = b;
            row[x * 4 + 1] = g;
            row[x * 4 + 2] = r;
            row[x * 4 + 3] = a;
        }
    }
    return true;
}

static bool read_bgra(int width, int height, void *pixels, void *user) {
    return read_fill(user, width, height, pixels, true);
}

static bool read_rgba(int x, int y, int width, int height, bool back_buffer,
    void *pixels, void *user) {
    (void)x; (void)y; (void)back_buffer;
    return read_fill(user, width, height, pixels, false);
}

bool bongo_cat_rhi_metal_create_window(const char *title, int width,
    int height, SDL_Window **window, BongoCatRhi *rhi, BongoCatError *error) {
    @autoreleasepool {
        /* The CAMetalLayer composites with alpha (isOpaque = NO), so the
           pet window stays transparent without SDL's transparency flag. */
        SDL_WindowFlags flags = SDL_WINDOW_METAL | SDL_WINDOW_BORDERLESS |
            SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_HIDDEN;
        *window = SDL_CreateWindow(title, width, height, flags);
        if (!*window) {
            bongo_cat_error_set(error, BONGO_CAT_ERROR_PLATFORM,
                "Window creation: %s", SDL_GetError());
            return BONGO_CAT_ERROR_PLATFORM;
        }
        BongoCatRhiMetal *metal = calloc(1, sizeof(*metal));
        if (!metal) {
            SDL_DestroyWindow(*window);
            *window = NULL;
            bongo_cat_error_set(error, BONGO_CAT_ERROR_MEMORY,
                "Cannot allocate the Metal RHI state");
            return BONGO_CAT_ERROR_MEMORY;
        }
        id<MTLDevice> device = MTLCreateSystemDefaultDevice();
        if (!device) {
            free(metal);
            SDL_DestroyWindow(*window);
            *window = NULL;
            bongo_cat_error_set(error, BONGO_CAT_ERROR_PLATFORM,
                "No Metal device is available");
            return BONGO_CAT_ERROR_PLATFORM;
        }
        metal->view = SDL_Metal_CreateView(*window);
        if (!metal->view) {
            free(metal);
            SDL_DestroyWindow(*window);
            *window = NULL;
            bongo_cat_error_set(error, BONGO_CAT_ERROR_PLATFORM,
                "SDL cannot create the metal view: %s", SDL_GetError());
            return BONGO_CAT_ERROR_PLATFORM;
        }
        metal->layer = (__bridge CAMetalLayer *)SDL_Metal_GetLayer(
            metal->view);
        metal->layer.device = device;
        metal->layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
        metal->layer.opaque = NO;
        metal->layer.maximumDrawableCount = 3;
        snprintf(metal->describe, sizeof(metal->describe), "Metal %s",
            device.name ? device.name.UTF8String : "device");
        SDL_LogInfo(SDL_LOG_CATEGORY_VIDEO, "[render] %s ready",
            metal->describe);
        rhi->backend = BONGO_CAT_RHI_METAL;
        rhi->window = *window;
        rhi->impl = metal;
        rhi->present.swap = frame_present;
        rhi->present.read_bgra = read_bgra;
        rhi->present.read_rgba = read_rgba;
        rhi->present.name = present_name;
        rhi->present.requires_layered = false;
        rhi->present.user = metal;
        return BONGO_CAT_OK;
    }
}

void bongo_cat_rhi_metal_shutdown(BongoCatRhi *rhi) {
    BongoCatRhiMetal *metal = rhi ? rhi->impl : NULL;
    if (!metal) return;
    if (metal->view) SDL_Metal_DestroyView(metal->view);
    free(metal);
    rhi->impl = NULL;
}

bool bongo_cat_rhi_metal_make_current(BongoCatRhi *rhi) {
    (void)rhi;
    return true;
}

void bongo_cat_rhi_metal_detach(const BongoCatRhi *rhi) { (void)rhi; }

void bongo_cat_rhi_metal_prepare_frame(BongoCatRhi *rhi, int width,
    int height) {
    BongoCatRhiMetal *metal = rhi ? rhi->impl : NULL;
    if (!metal || !metal->layer || width <= 0 || height <= 0) return;
    metal->layer.drawableSize = CGSizeMake((CGFloat)width, (CGFloat)height);
}

void bongo_cat_rhi_metal_viewport(BongoCatRhi *rhi, int x, int y, int width,
    int height) {
    /* Recorded with the draw commands once the draw phases port. */
    (void)rhi; (void)x; (void)y; (void)width; (void)height;
}

void bongo_cat_rhi_metal_clear(BongoCatRhi *rhi, float red, float green,
    float blue, float alpha) {
    BongoCatRhiMetal *metal = rhi ? rhi->impl : NULL;
    if (!metal) return;
    metal->clear[0] = red;
    metal->clear[1] = green;
    metal->clear[2] = blue;
    metal->clear[3] = alpha;
}

const char *bongo_cat_rhi_metal_describe(const BongoCatRhi *rhi) {
    BongoCatRhiMetal *metal = rhi ? rhi->impl : NULL;
    return metal && metal->describe[0] ? metal->describe : "Metal";
}

bool bongo_cat_rhi_metal_get_device_info(const BongoCatRhi *rhi,
    BongoCatRhiDeviceInfo *info) {
    BongoCatRhiMetal *metal = rhi ? rhi->impl : NULL;
    if (!metal || !info || !metal->layer) return false;
    memset(info, 0, sizeof(*info));
    info->backend = BONGO_CAT_RHI_METAL;
    info->metal_device = (__bridge void *)metal->layer.device;
    info->metal_layer = (__bridge void *)metal->layer;
    return true;
}
