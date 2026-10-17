#ifndef BONGO_CAT_CUBISM_VULKAN_MEMORY_HPP
#define BONGO_CAT_CUBISM_VULKAN_MEMORY_HPP
#include <vulkan/vulkan.h>
#include <cstddef>

namespace bongo_cat {
// Main render thread only; -1 appends during initialization.
extern int vulkan_texture_rebind_slot;
bool vulkan_memory_configure(VkInstance instance, VkPhysicalDevice physical,
    VkDevice device, bool enabled, bool large_allocation = false, bool system_memory = false,
    bool memory_budget = false);
struct VulkanMemoryUsage {
    VkDeviceSize used = 0, reserved = 0, heap_usage = 0, heap_budget = 0;
    VkDeviceSize device_usage = 0, device_budget = 0;
    VkDeviceSize atlas_bytes = 0, target_bytes = 0, geometry_bytes = 0;
    VkDeviceSize uniform_bytes = 0, staging_bytes = 0;
    size_t buffers = 0, images = 0;
};
VulkanMemoryUsage vulkan_memory_usage();
void vulkan_memory_next_frame();
void vulkan_memory_release();
bool vulkan_memory_prefers_device();
bool vulkan_buffer_create(VkDevice device, VkDeviceSize size,
    VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
    VkBuffer *buffer, VkDeviceMemory *memory, bool transient = false);
bool vulkan_image_create(VkDevice device, const VkImageCreateInfo &info,
    VkImage *image, VkDeviceMemory *memory);
bool vulkan_buffer_map(VkBuffer buffer, void **mapped);
// Borrow the SDK's existing coherent mapping; never changes its map count.
void *vulkan_buffer_mapped_data(VkBuffer buffer);
bool vulkan_buffer_unmap(VkBuffer buffer);
bool vulkan_buffer_destroy(VkBuffer buffer);
bool vulkan_image_destroy(VkImage image);
}
#endif
