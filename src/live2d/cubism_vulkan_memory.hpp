#ifndef BONGO_CAT_CUBISM_VULKAN_MEMORY_HPP
#define BONGO_CAT_CUBISM_VULKAN_MEMORY_HPP
#include <vulkan/vulkan.h>

namespace bongo_cat {
bool vulkan_memory_configure(VkInstance instance, VkPhysicalDevice physical,
    VkDevice device, bool enabled);
void vulkan_memory_release();
bool vulkan_buffer_create(VkDevice device, VkDeviceSize size,
    VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
    VkBuffer *buffer, VkDeviceMemory *memory);
bool vulkan_image_create(VkDevice device, const VkImageCreateInfo &info,
    VkImage *image, VkDeviceMemory *memory);
bool vulkan_buffer_map(VkBuffer buffer, void **mapped);
bool vulkan_buffer_unmap(VkBuffer buffer);
bool vulkan_buffer_destroy(VkBuffer buffer);
bool vulkan_image_destroy(VkImage image);
}
#endif
