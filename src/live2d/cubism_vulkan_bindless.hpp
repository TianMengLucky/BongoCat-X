#ifndef BONGO_CAT_CUBISM_VULKAN_BINDLESS_HPP
#define BONGO_CAT_CUBISM_VULKAN_BINDLESS_HPP
#include <vulkan/vulkan.h>
#include <cstddef>

namespace bongo_cat {
constexpr uint32_t vulkan_texture_table_capacity = 64;
bool vulkan_bindless_configure(VkDevice device, VkPhysicalDevice physical, bool enabled);
void vulkan_bindless_release();
// Atlas descriptors are written only before renderer creation, with the GPU idle.
// Masks retain the SDK interface; no update-after-bind features are required.
bool vulkan_bindless_model(const VkDescriptorImageInfo *images, size_t count, bool compatible);
bool vulkan_bindless_active();
VkDescriptorSetLayout vulkan_bindless_layout();
void vulkan_bindless_bind(VkCommandBuffer command, VkPipelineLayout layout, uint32_t texture);
}
#endif
