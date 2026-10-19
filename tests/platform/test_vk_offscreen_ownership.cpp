/* Device lifetime is exercised without a Vulkan loader or GPU. */
#include "rhi_vk_resources.hpp"
#include "test.h"
int bongo_cat_test_failures;
#include <cstdint>
static unsigned device_destroys, instance_destroys, waits, debug_stops;
static VKAPI_ATTR VkResult VKAPI_CALL wait_idle(VkDevice) { ++waits; return VK_SUCCESS; }
static VKAPI_ATTR void VKAPI_CALL destroy_device(VkDevice, const VkAllocationCallbacks *) { ++device_destroys; }
static VKAPI_ATTR void VKAPI_CALL destroy_instance(VkInstance, const VkAllocationCallbacks *) { ++instance_destroys; }
void bongo_cat_rhi_vk_release_corners(BongoCatRhiVk *) {}
void bongo_cat_rhi_vk_destroy_swapchain_objects(BongoCatRhiVk *) {}
void bongo_cat_rhi_vk_destroy_frames(BongoCatRhiVk *) {}
void bongo_cat_rhi_vk_destroy_uploads(BongoCatRhiVk *) {}
void bongo_cat_rhi_vk_debug_stop(BongoCatRhiVk *) { ++debug_stops; }
int main() {
    {
        BongoCatVulkanBackend parent;
        parent.device = reinterpret_cast<VkDevice>(uintptr_t{1});
        parent.instance = reinterpret_cast<VkInstance>(uintptr_t{2});
        parent.vkDeviceWaitIdle = wait_idle;
        parent.vkDestroyDevice = destroy_device; parent.vkDestroyInstance = destroy_instance;
        {
            BongoCatVulkanBackend child;
            child.borrowed_device = true;
            child.device = parent.device; child.instance = parent.instance;
            child.vkDeviceWaitIdle = wait_idle;
            child.vkDestroyDevice = destroy_device; child.vkDestroyInstance = destroy_instance;
        }
        CHECK(waits == 1 && device_destroys == 0 && instance_destroys == 0 && debug_stops == 0);
        {
            BongoCatVulkanBackend partial;
            partial.borrowed_device = true;
            partial.device = parent.device; partial.instance = parent.instance;
        }
        CHECK(device_destroys == 0 && instance_destroys == 0);
    }
    CHECK(waits == 2 && device_destroys == 1 && instance_destroys == 1 && debug_stops == 1);
    return bongo_cat_test_failures ? 1 : 0;
}
