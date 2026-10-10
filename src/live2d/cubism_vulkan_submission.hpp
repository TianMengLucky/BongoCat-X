#pragma once
#include <vulkan/vulkan.h>
namespace bongo_cat {
void vulkan_submit_initialize(VkDevice device, VkQueue queue, VkCommandPool pool);
void vulkan_submit_release();
void vulkan_submit_cancel();
void vulkan_upload_batch_begin();
void vulkan_upload_budget(VkDeviceSize bytes);
VkCommandBuffer vulkan_upload_begin();
void vulkan_upload_batch_end();
bool vulkan_upload_defer_buffer(VkBuffer buffer, VkDeviceMemory memory) noexcept;
void vulkan_submit(VkCommandBuffer command, VkSemaphore signal, VkSemaphore wait);
VkResult vulkan_begin_commands(VkCommandBuffer command, const VkCommandBufferBeginInfo *info);
void vulkan_bind_pipeline(VkCommandBuffer command, VkPipelineBindPoint point, VkPipeline pipeline);
void vulkan_bind_descriptors(VkCommandBuffer command, VkPipelineBindPoint point,
    VkPipelineLayout layout, uint32_t first, uint32_t count, const VkDescriptorSet *sets,
    uint32_t dynamic_count, const uint32_t *dynamic_offsets);
void vulkan_set_cull(VkCommandBuffer command, VkCullModeFlags flags);
VkResult vulkan_create_pipelines(VkDevice device, VkPipelineCache fallback, uint32_t count,
    const VkGraphicsPipelineCreateInfo *info, const VkAllocationCallbacks *callbacks, VkPipeline *pipelines);
}
