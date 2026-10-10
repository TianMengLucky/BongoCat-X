/* Metal corner pipeline is cached per device and encodes into the model's
   command buffer. In-place blending requires no extra texture or readback. */
#include "rhi_corners.h"
static id<MTLRenderPipelineState> create_corner_pipeline(id<MTLDevice> device) {
    NSString *source = @"#include <metal_stdlib>\n"
        "using namespace metal;\n"
        "struct Params { float4 rect; float4 surface; };\n"
        "vertex float4 corners_vertex(uint index [[vertex_id]], constant Params &p [[buffer(0)]]) {\n"
        " const float2 vertices[6] = {float2(0,0),float2(1,0),float2(0,1),\n"
        " float2(0,1),float2(1,0),float2(1,1)};\n"
        " uint corner = index / 6; float2 side(corner & 1, (corner >> 1) & 1);\n"
        " float2 span = min(float2(p.surface.x + 0.5), p.rect.zw * 0.5);\n"
        " float2 pixel = p.rect.xy + side * (p.rect.zw - span) + vertices[index % 6] * span;\n"
        " float2 ndc = pixel / p.surface.yz * 2.0 - 1.0;\n"
        " return float4(ndc.x, -ndc.y, 0, 1); }\n"
        "fragment float4 corners_fragment(float4 position [[position]], constant Params &p [[buffer(0)]]) {\n"
        " float2 halfSize = p.rect.zw * 0.5;\n"
        " float2 q = abs(position.xy - p.rect.xy - halfSize) - (halfSize - p.surface.x);\n"
        " float d = length(max(q, float2(0))) + min(max(q.x,q.y),0.0) - p.surface.x;\n"
        " return float4(clamp(0.5 - d, 0.0, 1.0)); }\n";
    NSError *error = nil;
    id<MTLLibrary> library = [device newLibraryWithSource:source options:nil error:&error];
    if (!library) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Metal corner shader: %s",
            error.localizedDescription.UTF8String ?: "unknown error");
        return nil;
    }
    MTLRenderPipelineDescriptor *description = [MTLRenderPipelineDescriptor new];
    description.vertexFunction = [library newFunctionWithName:@"corners_vertex"];
    description.fragmentFunction = [library newFunctionWithName:@"corners_fragment"];
    MTLRenderPipelineColorAttachmentDescriptor *color = description.colorAttachments[0];
    color.pixelFormat = MTLPixelFormatBGRA8Unorm;
    color.blendingEnabled = YES;
    color.sourceRGBBlendFactor = color.sourceAlphaBlendFactor = MTLBlendFactorZero;
    color.destinationRGBBlendFactor = color.destinationAlphaBlendFactor = MTLBlendFactorSourceAlpha;
    return [device newRenderPipelineStateWithDescriptor:description error:&error];
}
static bool encode_corners(SDL_Window *window, id<MTLCommandBuffer> buffer,
    id<MTLTexture> texture, id<MTLRenderPipelineState> __strong *pipeline) {
    float params[8];
    if (!bongo_cat_rhi_corner_params(window, (int)texture.width,
        (int)texture.height, params)) return true;
    if (!*pipeline) *pipeline = create_corner_pipeline(texture.device);
    if (!*pipeline) return false;
    MTLRenderPassDescriptor *pass = [MTLRenderPassDescriptor renderPassDescriptor];
    pass.colorAttachments[0].texture = texture;
    pass.colorAttachments[0].loadAction = MTLLoadActionLoad;
    pass.colorAttachments[0].storeAction = MTLStoreActionStore;
    id<MTLRenderCommandEncoder> encoder = [buffer renderCommandEncoderWithDescriptor:pass];
    if (!encoder) return false;
    [encoder setRenderPipelineState:*pipeline];
    [encoder setVertexBytes:params length:sizeof(params) atIndex:0];
    [encoder setFragmentBytes:params length:sizeof(params) atIndex:0];
    [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:24];
    [encoder endEncoding];
    return true;
}
