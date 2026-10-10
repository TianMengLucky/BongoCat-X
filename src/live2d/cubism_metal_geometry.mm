#include "cubism_metal_geometry.hpp"
#include <array>
#include <cstring>
#include <stdexcept>
#include <unordered_map>
#include <vector>
namespace bongo_cat {
struct MetalGeometry {
    struct Slot {
        id<MTLBuffer> buffer = nil;
        id<MTLCommandBuffer> command = nil;
        bool valid = false;
    };
    id<MTLDevice> device;
    std::vector<size_t> offsets, bytes;
    std::unordered_map<const void *, size_t> sources, draws;
    std::array<Slot, 3> slots{};
    size_t plane = 0, current = 0;
};
namespace {
thread_local MetalGeometry *geometry;
struct EncoderState {
    __unsafe_unretained id<MTLRenderCommandEncoder> encoder = nil;
    __unsafe_unretained id<MTLRenderPipelineState> pipeline = nil;
    std::array<void *, 4> textures{};
    std::array<bool, 4> texture_valid{};
    MTLCullMode cull = MTLCullModeNone;
    MTLWinding winding = MTLWindingClockwise;
    bool cull_valid = false, winding_valid = false;
};
thread_local EncoderState state;
void select(id<MTLRenderCommandEncoder> encoder) {
    if (state.encoder != encoder) { state = {}; state.encoder = encoder; }
}
}
MetalGeometryScope::MetalGeometryScope(const std::shared_ptr<MetalGeometry> &value)
    : previous_(geometry) { geometry = value.get(); }
MetalGeometryScope::~MetalGeometryScope() { geometry = previous_; }
std::shared_ptr<MetalGeometry> metal_geometry_create(void *device, const int *counts, size_t count) {
    auto result = std::make_shared<MetalGeometry>();
    result->device = (__bridge id<MTLDevice>)device;
    result->offsets.resize(count); result->bytes.resize(count);
    for (size_t i = 0; i < count; ++i) {
        if (counts[i] < 0) return {};
        result->offsets[i] = result->plane;
        result->bytes[i] = (size_t)counts[i] * sizeof(float) * 2;
        result->plane += result->bytes[i];
    }
    if (!result->plane) return {};
    result->slots[0].buffer = [result->device newBufferWithLength:result->plane * 2
        options:MTLResourceStorageModeShared];
    return result->slots[0].buffer ? result : std::shared_ptr<MetalGeometry>{};
}
bool metal_geometry_available() { return geometry != nullptr; }
void metal_geometry_begin(void *command) {
    if (!geometry) return;
    const auto ready = [](const MetalGeometry::Slot &slot) {
        // The host commits and waits before the next draw callback. An older
        // NotEnqueued command therefore belongs to a discarded failed frame.
        return !slot.command || slot.command.status == MTLCommandBufferStatusNotEnqueued ||
            slot.command.status == MTLCommandBufferStatusCompleted ||
            slot.command.status == MTLCommandBufferStatusError;
    };
    size_t index = geometry->slots.size();
    for (size_t i = 0; i < geometry->slots.size(); ++i)
        if (ready(geometry->slots[i])) { index = i; break; }
    if (index == geometry->slots.size()) {
        index = (geometry->current + 1) % geometry->slots.size();
        [geometry->slots[index].command waitUntilCompleted];
    }
    auto &slot = geometry->slots[index];
    if (!slot.buffer) slot.buffer = [geometry->device newBufferWithLength:geometry->plane * 2
        options:MTLResourceStorageModeShared];
    if (!slot.buffer) throw std::runtime_error("Cannot allocate Metal vertex arena");
    slot.command = (__bridge id<MTLCommandBuffer>)command;
    geometry->current = index;
    geometry->sources.clear(); geometry->draws.clear();
}
void metal_geometry_prepare(size_t index, const float *positions, const float *uv) {
    if (!geometry) return;
    auto &slot = geometry->slots[geometry->current];
    auto *bytes = static_cast<unsigned char *>(slot.buffer.contents);
    const size_t length = geometry->bytes.at(index), offset = geometry->offsets.at(index);
    if (!length) return;
    geometry->sources.emplace(positions, index);
    if (!slot.valid || std::memcmp(bytes + offset, positions, length) != 0)
        std::memcpy(bytes + offset, positions, length);
    if (!slot.valid || std::memcmp(bytes + geometry->plane + offset, uv, length) != 0)
        std::memcpy(bytes + geometry->plane + offset, uv, length);
    // Every nonempty mesh is visited before encoding. First use may safely compare
    // later slots only after their initial bytes have been copied below.
}
void metal_geometry_prepare_end() {
    if (geometry) geometry->slots[geometry->current].valid = true;
}
bool metal_geometry_update(const void *draw, const void *positions) {
    if (!geometry) return false;
    const auto found = geometry->sources.find(positions);
    if (found == geometry->sources.end()) return false;
    geometry->draws[draw] = found->second;
    return true;
}
id<MTLBuffer> metal_geometry_buffer(const void *draw, id<MTLBuffer> fallback) {
    if (!geometry || geometry->draws.find(draw) == geometry->draws.end()) return fallback;
    return geometry->slots[geometry->current].buffer;
}
NSUInteger metal_geometry_offset(const void *draw, bool uv) {
    if (!geometry) return 0;
    const auto found = geometry->draws.find(draw);
    return found == geometry->draws.end() ? 0 :
        geometry->offsets[found->second] + (uv ? geometry->plane : 0);
}
void metal_state_begin() { state = {}; }
void metal_encoder_begin(id<MTLRenderCommandEncoder> encoder) {
    state = {}; state.encoder = encoder;
}
void metal_set_pipeline(id<MTLRenderCommandEncoder> encoder, id<MTLRenderPipelineState> pipeline) {
    select(encoder);
    if (state.pipeline == pipeline) return;
    [encoder setRenderPipelineState:pipeline]; state.pipeline = pipeline;
}
void metal_set_texture(id<MTLRenderCommandEncoder> encoder, id<MTLTexture> texture, NSUInteger index) {
    select(encoder);
    if (index < state.textures.size() && state.texture_valid[index] && state.textures[index] == (__bridge void *)texture) return;
    [encoder setFragmentTexture:texture atIndex:index];
    if (index < state.textures.size()) { state.textures[index] = (__bridge void *)texture; state.texture_valid[index] = true; }
}
void metal_set_cull(id<MTLRenderCommandEncoder> encoder, MTLCullMode mode) {
    select(encoder);
    if (state.cull_valid && state.cull == mode) return;
    [encoder setCullMode:mode]; state.cull = mode; state.cull_valid = true;
}
void metal_set_winding(id<MTLRenderCommandEncoder> encoder, MTLWinding winding) {
    select(encoder);
    if (state.winding_valid && state.winding == winding) return;
    [encoder setFrontFacingWinding:winding]; state.winding = winding; state.winding_valid = true;
}
}
