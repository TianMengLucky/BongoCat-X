#ifndef BONGO_CAT_RHI_VK_RESOURCES_HPP
#define BONGO_CAT_RHI_VK_RESOURCES_HPP
#include "rhi_vk_internal.h"
#include <memory>
#include <new>
#include <type_traits>
static_assert(std::is_standard_layout_v<BongoCatRhiVk> &&
    std::is_trivially_copyable_v<BongoCatRhiVk>, "Vulkan shared state must keep its C layout");
/* The host only borrows the base state through its C ABI. Ownership remains
   here, including partially initialized devices and their loader lifetime. */
class BongoCatVulkanBackend final : public BongoCatRhiVk {
public:
    BongoCatVulkanBackend() noexcept : BongoCatRhiVk{} {}
    ~BongoCatVulkanBackend();
    BongoCatVulkanBackend(const BongoCatVulkanBackend &) = delete;
    BongoCatVulkanBackend &operator=(const BongoCatVulkanBackend &) = delete;
};
#endif
