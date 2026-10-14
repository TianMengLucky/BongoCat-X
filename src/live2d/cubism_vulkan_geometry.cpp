#include "cubism_vulkan_geometry.hpp"
#include "cubism_vulkan_memory.hpp"
#include "cubism_vulkan_copy_ranges.hpp"
#include <Rendering/Vulkan/CubismClass_Vulkan.hpp>
#include <cstring>
#include <algorithm>
#include <memory>
#include <stdexcept>
#include <vector>
namespace bongo_cat {
namespace {
struct Mesh {
    VkDeviceSize offset = 0;
    int count = 0;
};
}
struct VulkanGeometry {
    explicit VulkanGeometry(VkDevice value) : device(value) {}
    ~VulkanGeometry() { staging.Destroy(device); vertices.Destroy(device); }
    VkDevice device;
    Live2D::Cubism::Framework::CubismBufferVulkan staging, vertices;
    std::vector<Mesh> meshes;
    VkDeviceSize size = 0, dirty_begin = 0, dirty_end = 0;
    std::vector<float> packed;
    float *packed_data = nullptr;
    std::vector<VkBufferCopy> copies;
    bool mapped_packing = false;
    bool valid = false, dirty = false, uploaded = false, direct = false;
};
namespace {
thread_local VulkanGeometry *geometry = nullptr;
}
VulkanGeometryScope::VulkanGeometryScope(const std::shared_ptr<VulkanGeometry> &value)
    : previous_(geometry) { geometry = value.get(); }
VulkanGeometryScope::~VulkanGeometryScope() { geometry = previous_; }
std::shared_ptr<VulkanGeometry> vulkan_geometry_create(VkDevice device, VkPhysicalDevice physical,
    const int *counts, size_t count) {
    try {
        auto next = std::make_shared<VulkanGeometry>(device);
        next->meshes.resize(count);
        for (size_t i = 0; i < count; ++i) {
            auto &mesh = next->meshes[i];
            if (counts[i] < 0) return {};
            mesh.offset = next->size;
            mesh.count = counts[i];
            next->size += (VkDeviceSize)counts[i] * sizeof(float) * 4;
        }
        if (!next->size) return {};
        VkPhysicalDeviceMemoryProperties properties{};
        vkGetPhysicalDeviceMemoryProperties(physical, &properties);
        const VkMemoryPropertyFlags visible = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        bool supported = false;
        for (uint32_t i = 0; i < properties.memoryTypeCount; ++i)
            if ((properties.memoryTypes[i].propertyFlags & visible) == visible) supported = true;
        if (supported) {
            try {
                next->vertices.CreateBuffer(device, physical, next->size,
                    VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, visible);
                next->vertices.Map(device, next->size);
                next->direct = true;
            } catch (const std::exception &) { next->vertices.Destroy(device); }
        }
        if (!next->direct) {
            next->staging.CreateBuffer(device, physical, next->size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            next->staging.Map(device, next->size);
        }
        if (!next->direct) next->vertices.CreateBuffer(device, physical, next->size,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        // VMA mappings live until buffer destruction. Reuse this storage for
        // packing instead of retaining a second interleaved CPU vertex array.
        next->packed_data = static_cast<float *>(vulkan_buffer_mapped_data(
            next->direct ? next->vertices.GetBuffer() : next->staging.GetBuffer()));
        next->mapped_packing = next->packed_data != nullptr;
        if (!next->mapped_packing) {
            next->packed.resize((size_t)next->size / sizeof(float));
            next->packed_data = next->packed.data();
        }
        next->copies.reserve(count);
        return next;
    } catch (const std::exception &) { return {}; }
}
bool vulkan_geometry_available() { return geometry != nullptr; }
void vulkan_geometry_prepare_begin() {
    if (!geometry) return;
    geometry->dirty = !geometry->valid;
    geometry->uploaded = false;
    geometry->copies.clear();
    geometry->dirty_begin = geometry->valid ? geometry->size : 0;
    geometry->dirty_end = geometry->valid ? 0 : geometry->size;
}
void vulkan_geometry_prepare(size_t index, const float *positions, const float *uv) {
    if (!geometry) return;
    auto &mesh = geometry->meshes.at(index);
    const size_t bytes = (size_t)mesh.count * sizeof(float) * 2;
    if (!bytes) return;
    auto *target = geometry->packed_data + (size_t)mesh.offset / sizeof(float);
    // The draw fence was waited before this call. The packed mapping (or the
    // raw-SDK fallback vector) already holds the preceding vertices and UVs.
    // Reuse it for comparison without another per-mesh copy on the CPU.
    bool unchanged = geometry->valid;
    for (int i = 0; unchanged && i < mesh.count; ++i)
        unchanged = std::memcmp(target + (size_t)i * 4,
            positions + (size_t)i * 2, sizeof(float) * 2) == 0 &&
            std::memcmp(target + (size_t)i * 4 + 2,
                uv + (size_t)i * 2, sizeof(float) * 2) == 0;
    if (unchanged) return;
    for (int i = 0; i < mesh.count; ++i) {
        std::memcpy(target + (size_t)i * 4, positions + (size_t)i * 2, sizeof(float) * 2);
        std::memcpy(target + (size_t)i * 4 + 2, uv + (size_t)i * 2, sizeof(float) * 2);
    }
    vulkan_copy_range_append(geometry->copies, mesh.offset, bytes * 2);
    geometry->dirty = true;
    geometry->dirty_begin = std::min(geometry->dirty_begin, mesh.offset);
    geometry->dirty_end = std::max(geometry->dirty_end, mesh.offset + bytes * 2);
}
void vulkan_geometry_commit(bool success) {
    if (!geometry) return;
    if (!success) geometry->valid = false;
    else if (geometry->uploaded) geometry->valid = true;
}
bool vulkan_geometry_upload(VkCommandBuffer command) {
    if (!geometry) return false;
    if (geometry->dirty && !geometry->uploaded) {
        if (geometry->direct) {
            if (!geometry->mapped_packing)
                geometry->vertices.MemCpy(geometry->packed_data, geometry->size);
            geometry->uploaded = true;
            return true;
        }
        // The draw fence is waited before reuse. Only copy the changed span;
        // Raw SDK staging starts at zero; borrowed VMA staging keeps mesh offsets.
        const VkDeviceSize length = geometry->dirty_end - geometry->dirty_begin;
        if (!geometry->mapped_packing) {
            geometry->staging.MemCpy(geometry->packed_data +
                (size_t)geometry->dirty_begin / sizeof(float), length);
            vulkan_copy_ranges_rebase(geometry->copies, geometry->dirty_begin);
        }
        // Adjacent dirty meshes share one region; unchanged gaps are not copied.
        vkCmdCopyBuffer(command, geometry->staging.GetBuffer(), geometry->vertices.GetBuffer(),
            (uint32_t)geometry->copies.size(), geometry->copies.data());
        geometry->uploaded = true;
    }
    return true;
}
VkBuffer vulkan_geometry_buffer(VkBuffer fallback) {
    return geometry ? geometry->vertices.GetBuffer() : fallback;
}
VkDeviceSize vulkan_geometry_offset(size_t index) {
    return geometry ? geometry->meshes.at(index).offset : 0;
}
}
