#include "cubism_vulkan_bindless.hpp"
#include <array>
#include <memory>
#include <stdexcept>

namespace bongo_cat {
namespace {
void checked(VkResult result, const char *message) {
    if (result != VK_SUCCESS) throw std::runtime_error(message);
}
struct Table {
    explicit Table(VkDevice value) : device(value) {}
    ~Table() {
        vkDeviceWaitIdle(device);
        if (pool) vkDestroyDescriptorPool(device, pool, nullptr);
        if (layout) vkDestroyDescriptorSetLayout(device, layout, nullptr);
    }
    void initialize() {
        VkDescriptorSetLayoutBinding binding{};
        binding.binding = 0;
        binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        binding.descriptorCount = vulkan_texture_table_capacity;
        binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        VkDescriptorSetLayoutCreateInfo create{};
        create.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        create.bindingCount = 1;
        create.pBindings = &binding;
        checked(vkCreateDescriptorSetLayout(device, &create, nullptr, &layout),
            "Cannot create Live2D texture table layout");
        VkDescriptorPoolSize size{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            vulkan_texture_table_capacity};
        VkDescriptorPoolCreateInfo descriptor_pool{};
        descriptor_pool.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        descriptor_pool.maxSets = 1;
        descriptor_pool.poolSizeCount = 1;
        descriptor_pool.pPoolSizes = &size;
        checked(vkCreateDescriptorPool(device, &descriptor_pool, nullptr, &pool),
            "Cannot create Live2D texture table pool");
        VkDescriptorSetAllocateInfo allocate{};
        allocate.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocate.descriptorPool = pool;
        allocate.descriptorSetCount = 1;
        allocate.pSetLayouts = &layout;
        checked(vkAllocateDescriptorSets(device, &allocate, &set),
            "Cannot allocate Live2D texture table");
    }
    VkDevice device;
    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    VkDescriptorPool pool = VK_NULL_HANDLE;
    VkDescriptorSet set = VK_NULL_HANDLE;
    size_t count = 0;
    bool active = false;
};
std::unique_ptr<Table> table;
}
bool vulkan_bindless_configure(VkDevice device, VkPhysicalDevice physical, bool enabled) {
    table.reset();
    if (!enabled || !device || !physical) return false;
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(physical, &properties);
    const auto &limits = properties.limits;
    // Set 0 retains the three SDK image bindings; the atlas array is set 1.
    constexpr uint32_t required = vulkan_texture_table_capacity + 3;
    if (limits.maxBoundDescriptorSets < 2 || limits.maxPushConstantsSize < sizeof(uint32_t) ||
        limits.maxPerStageDescriptorSamplers < required || limits.maxDescriptorSetSamplers < required ||
        limits.maxPerStageDescriptorSampledImages < required || limits.maxDescriptorSetSampledImages < required ||
        limits.maxPerStageResources < required + 1) return false;
    try {
        auto candidate = std::make_unique<Table>(device);
        candidate->initialize();
        table = std::move(candidate);
        return true;
    } catch (const std::exception &) { return false; }
}
void vulkan_bindless_release() { table.reset(); }
bool vulkan_bindless_active() { return table && table->active; }
VkDescriptorSetLayout vulkan_bindless_layout() { return table ? table->layout : VK_NULL_HANDLE; }
bool vulkan_bindless_model(const VkDescriptorImageInfo *images, size_t count, bool compatible) {
    if (!table) return false;
    table->active = false;
    table->count = 0;
    if (!compatible || !images || !count || count > vulkan_texture_table_capacity) return false;
    for (size_t i = 0; i < count; ++i)
        if (!images[i].imageView || !images[i].sampler ||
            images[i].imageLayout != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) return false;
    checked(vkDeviceWaitIdle(table->device), "Cannot retire Live2D texture table");
    std::array<VkDescriptorImageInfo, vulkan_texture_table_capacity> slots;
    // Every slot is valid; an unused slot samples atlas 0. No partially-bound
    // or runtime-descriptor-array feature is needed for this bounded table.
    slots.fill(images[0]);
    for (size_t i = 0; i < count; ++i) slots[i] = images[i];
    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = table->set;
    write.dstBinding = 0;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.descriptorCount = vulkan_texture_table_capacity;
    write.pImageInfo = slots.data();
    vkUpdateDescriptorSets(table->device, 1, &write, 0, nullptr);
    table->count = count;
    table->active = true;
    return true;
}
void vulkan_bindless_bind(VkCommandBuffer command, VkPipelineLayout layout, uint32_t texture) {
    if (!vulkan_bindless_active()) return;
    if (texture >= table->count) throw std::runtime_error("Live2D texture table index out of range");
    vkCmdBindDescriptorSets(command, VK_PIPELINE_BIND_POINT_GRAPHICS,
        layout, 1, 1, &table->set, 0, nullptr);
    // A draw-uniform index requires only sampled-image array dynamic indexing,
    // not non-uniform indexing. All invocations of this draw use the same atlas.
    vkCmdPushConstants(command, layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(texture), &texture);
}
}
