#pragma once
#include <vulkan/vulkan.h>
#include <cstddef>
#include <memory>
namespace bongo_cat {
struct VulkanGeometry;
class VulkanGeometryScope {
public:
    explicit VulkanGeometryScope(const std::shared_ptr<VulkanGeometry> &value);
    ~VulkanGeometryScope();
private:
    VulkanGeometry *previous_;
};
std::shared_ptr<VulkanGeometry> vulkan_geometry_create(VkDevice device, VkPhysicalDevice physical,
    const int *counts, size_t count);
bool vulkan_geometry_available();
void vulkan_geometry_prepare_begin();
void vulkan_geometry_prepare(size_t index, const float *positions, const float *uv);
void vulkan_geometry_commit(bool success);
bool vulkan_geometry_upload(VkCommandBuffer command);
VkBuffer vulkan_geometry_buffer(VkBuffer fallback);
VkDeviceSize vulkan_geometry_offset(size_t index);
}
