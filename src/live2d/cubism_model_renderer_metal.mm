#include "cubism_plugin_services.hpp"
#import <Metal/Metal.h>
#include "cubism_model.hpp"
#include <Rendering/Metal/CubismRenderer_Metal.hpp>
#include <Rendering/Metal/CubismDeviceInfo_Metal.hpp>

namespace bongo_cat {
Csm::Rendering::CubismRenderer *create_metal_renderer(Csm::csmUint32 width,
    Csm::csmUint32 height) {
    class Renderer final : public Csm::Rendering::CubismRenderer_Metal {
    public:
        Renderer(Csm::csmUint32 w, Csm::csmUint32 h) : CubismRenderer_Metal(w, h) {}
    };
    return CSM_NEW Renderer(width, height);
}
void release_metal_device(void *device) {
    Csm::Rendering::CubismDeviceInfo_Metal::ReleaseDeviceInfo((__bridge id<MTLDevice>)device);
}

bool NativeModel::upload_texture_metal(int index,
    const std::vector<unsigned char> &pixels, int width, int height, BongoCatError *error) {
    @autoreleasepool {
        id<MTLDevice> device = (__bridge id<MTLDevice>)rhi_info_.metal_device;
        MTLTextureDescriptor *desc = [MTLTextureDescriptor
            texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
            width:(NSUInteger)width height:(NSUInteger)height mipmapped:NO];
        desc.storageMode = MTLStorageModeShared;
        desc.usage = MTLTextureUsageShaderRead;
        id<MTLTexture> texture = [device newTextureWithDescriptor:desc];
        if (!texture) {
            bongo_cat_error_set(error, BONGO_CAT_ERROR_MEMORY, "Cannot allocate Live2D Metal texture");
            return false;
        }
        [texture replaceRegion:MTLRegionMake2D(0, 0, (NSUInteger)width, (NSUInteger)height)
            mipmapLevel:0 withBytes:pixels.data() bytesPerRow:(NSUInteger)width * 4];
        if (!metal_textures_) metal_textures_ = (__bridge_retained void *)[NSMutableArray new];
        NSMutableArray *textures = (__bridge NSMutableArray *)metal_textures_;
        if (!textures || (NSUInteger)index != textures.count) return false;
        [textures addObject:texture];
        return true;
    }
}
void NativeModel::release_textures_metal() {
    if (!metal_textures_) return;
    id textures = (__bridge_transfer id)metal_textures_;
    metal_textures_ = nullptr;
    (void)textures;
}
bool NativeModel::create_renderer_metal(BongoCatError *error) {
    @autoreleasepool {
        id<MTLDevice> device = (__bridge id<MTLDevice>)rhi_info_.metal_device;
        if (!device) {
            bongo_cat_error_set(error, BONGO_CAT_ERROR_CUBISM, "No Live2D Metal device attached");
            return false;
        }
        Csm::Rendering::CubismRenderer_Metal::SetConstantSettings(device,
            (Csm::csmUint32)prepare_mask_layout());
        CreateRenderer((Csm::csmUint32)width_, (Csm::csmUint32)height_, prepare_mask_layout());
        auto *renderer = GetRenderer<Csm::Rendering::CubismRenderer_Metal>();
        if (!renderer) {
            bongo_cat_error_set(error, BONGO_CAT_ERROR_CUBISM, "Cannot create Live2D Metal renderer");
            return false;
        }
        NSArray *textures = (__bridge NSArray *)metal_textures_;
        for (NSUInteger i = 0; i < textures.count; ++i)
            renderer->BindTexture((Csm::csmUint32)i, textures[i]);
        renderer->IsPremultipliedAlpha(true);
        renderer_width_ = width_; renderer_height_ = height_;
        return true;
    }
}
void NativeModel::draw_metal() {
    auto *renderer = GetRenderer<Csm::Rendering::CubismRenderer_Metal>();
    if (!_model || !renderer || width_ <= 0 || height_ <= 0) return;
    BongoCatRhiMetalFrameInfo frame{};
    if (!bongo_cat_rhi_get_metal_frame_info(
        static_cast<const BongoCatRhi *>(rhi_info_.rhi_handle), &frame)) return;
    renderer->StartFrame((__bridge id<MTLCommandBuffer>)frame.command_buffer,
        (__bridge MTLRenderPassDescriptor *)frame.render_pass);
    renderer->SetRenderViewport(MTLViewport{0.0, 0.0,
        (double)rhi_info_.extent_width, (double)rhi_info_.extent_height, 0.0, 1.0});
    Csm::CubismMatrix44 projection;
    native_projection(projection);
    renderer->SetMvpMatrix(&projection);
    renderer->SetModelColor(1.0f, 1.0f, 1.0f, _model->GetModelOpacity());
    renderer->DrawModel();
    native_visual_state(projection);
}
} // namespace bongo_cat
