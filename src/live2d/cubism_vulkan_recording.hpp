#ifndef BONGO_CAT_CUBISM_VULKAN_RECORDING_HPP
#define BONGO_CAT_CUBISM_VULKAN_RECORDING_HPP
#include <vulkan/vulkan.h>

namespace bongo_cat {
// Workers consume handles and values only; no mutable Cubism state is shared.
struct VulkanDrawSnapshot {
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkDescriptorSet descriptor = VK_NULL_HANDLE;
    VkBuffer vertices = VK_NULL_HANDLE;
    VkBuffer indices = VK_NULL_HANDLE;
    uint32_t index_count = 0;
    VkCullModeFlags cull = VK_CULL_MODE_NONE;
    uint32_t texture = 0;
};
bool vulkan_recording_configure(VkDevice device, uint32_t family, bool enabled);
void vulkan_recording_release();
void vulkan_recording_cancel();
bool vulkan_recording_active(VkCommandBuffer primary);
bool vulkan_recording_begin(VkCommandBuffer primary, VkFormat color,
    VkFormat depth, VkExtent2D extent, bool compatible);
bool vulkan_recording_draw(VkCommandBuffer primary, const VulkanDrawSnapshot &draw);
void vulkan_recording_end(VkCommandBuffer primary);
}
#endif
