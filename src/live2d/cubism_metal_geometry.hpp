#pragma once
#include <cstddef>
#include <memory>
namespace bongo_cat {
struct MetalGeometry;
class MetalGeometryScope {
public:
    explicit MetalGeometryScope(const std::shared_ptr<MetalGeometry> &value);
    ~MetalGeometryScope();
private:
    MetalGeometry *previous_;
};
std::shared_ptr<MetalGeometry> metal_geometry_create(void *device, const int *counts, size_t count);
bool metal_geometry_available();
void metal_geometry_begin(void *command);
void metal_geometry_prepare_end();
void metal_geometry_prepare(size_t index, const float *positions, const float *uv);
bool metal_geometry_update(const void *draw, const void *positions);
#ifdef __OBJC__
}
#import <Metal/Metal.h>
namespace bongo_cat {
id<MTLBuffer> metal_geometry_buffer(const void *draw, id<MTLBuffer> fallback);
NSUInteger metal_geometry_offset(const void *draw, bool uv);
void metal_state_begin();
void metal_encoder_begin(id<MTLRenderCommandEncoder> encoder);
void metal_set_pipeline(id<MTLRenderCommandEncoder> encoder, id<MTLRenderPipelineState> pipeline);
void metal_set_texture(id<MTLRenderCommandEncoder> encoder, id<MTLTexture> texture, NSUInteger index);
void metal_set_cull(id<MTLRenderCommandEncoder> encoder, MTLCullMode mode);
void metal_set_winding(id<MTLRenderCommandEncoder> encoder, MTLWinding winding);
#endif
}
