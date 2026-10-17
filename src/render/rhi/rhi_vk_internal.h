#ifndef BONGO_CAT_RHI_VK_INTERNAL_H
#define BONGO_CAT_RHI_VK_INTERNAL_H

/* Shared state of the Vulkan RHI backend (see rhi_vk.cpp). Split across
   rhi_vk.cpp (device and swapchain lifetime) and rhi_vk_frame.cpp (frame
   submission and present ops) to keep both within the source size
   policy. The file is only compiled where the backend is available. */

#include "rhi_internal.h"

#if defined(BONGO_CAT_HAS_VULKAN_RHI) && \
    (defined(_WIN32) || (defined(__linux__) && defined(__x86_64__)))

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#ifdef BONGO_CAT_HAS_CUBISM_VULKAN
#include <volk.h>
#else
#include <vulkan/vulkan.h>
#endif
#include "rhi_vk_debug.h"

#ifdef __cplusplus
extern "C" {
#endif
struct BongoCatRhiVk;
bool bongo_cat_rhi_vk_create_layered_target(struct BongoCatRhiVk *vk, int width, int height);

enum { BONGO_CAT_VK_FRAMES = 2 };

#ifdef __cplusplus
#define BONGO_CAT_VK_INIT {}
#else
#define BONGO_CAT_VK_INIT
#endif
/* A slot is reused only after its completion fence, never by frame age. */
typedef struct BongoCatRhiVkFrame {
    VkCommandPool pool BONGO_CAT_VK_INIT;
    VkCommandBuffer initialize BONGO_CAT_VK_INIT, finish BONGO_CAT_VK_INIT;
    VkFence fence BONGO_CAT_VK_INIT;
    bool pending BONGO_CAT_VK_INIT;
} BongoCatRhiVkFrame;

typedef struct BongoCatRhiVkReadback {
    VkBuffer readback_buffer BONGO_CAT_VK_INIT;
    VkDeviceMemory readback_memory BONGO_CAT_VK_INIT;
    VkDeviceSize readback_size BONGO_CAT_VK_INIT;
    VkDeviceSize allocation_bytes BONGO_CAT_VK_INIT;
    uint8_t *pixels BONGO_CAT_VK_INIT;
} BongoCatRhiVkReadback;

typedef struct BongoCatRhiVkUpload {
    VkCommandPool pool BONGO_CAT_VK_INIT;
    VkCommandBuffer command BONGO_CAT_VK_INIT;
    VkFence fence BONGO_CAT_VK_INIT;
    VkBuffer buffer BONGO_CAT_VK_INIT;
    VkDeviceMemory memory BONGO_CAT_VK_INIT;
    bool pending BONGO_CAT_VK_INIT;
} BongoCatRhiVkUpload;

typedef struct BongoCatRhiVk {
    char describe[160] BONGO_CAT_VK_INIT;
    VkInstance instance BONGO_CAT_VK_INIT;
    VkDebugUtilsMessengerEXT debug_messenger BONGO_CAT_VK_INIT;
    VkDebugUtilsMessengerCreateInfoEXT debug_create BONGO_CAT_VK_INIT;
    const char **debug_extensions BONGO_CAT_VK_INIT;
    VkSurfaceKHR surface BONGO_CAT_VK_INIT;
    VkPhysicalDevice physical BONGO_CAT_VK_INIT;
    VkDevice device BONGO_CAT_VK_INIT;
    uint32_t queue_family BONGO_CAT_VK_INIT;
    VkQueue queue BONGO_CAT_VK_INIT;
    VkSwapchainKHR swapchain BONGO_CAT_VK_INIT;
    bool layered_target BONGO_CAT_VK_INIT;
    VkImage layered_image BONGO_CAT_VK_INIT;
    VkDeviceMemory layered_memory BONGO_CAT_VK_INIT;
    VkFormat format BONGO_CAT_VK_INIT;
    VkFormat depth_format BONGO_CAT_VK_INIT;
    VkExtent2D extent BONGO_CAT_VK_INIT;
    VkImage *images BONGO_CAT_VK_INIT;
    uint32_t image_count BONGO_CAT_VK_INIT;
    VkImageView *views BONGO_CAT_VK_INIT;
    VkFramebuffer *framebuffers BONGO_CAT_VK_INIT;
    VkRenderPass render_pass BONGO_CAT_VK_INIT, corner_pass BONGO_CAT_VK_INIT;
    VkPipeline corner_pipeline BONGO_CAT_VK_INIT;
    VkFormat corner_format BONGO_CAT_VK_INIT;
    VkPipelineLayout corner_layout BONGO_CAT_VK_INIT;
    VkFramebuffer *corner_framebuffers BONGO_CAT_VK_INIT;
    VkCommandPool pool BONGO_CAT_VK_INIT;
    VkCommandBuffer idle_command BONGO_CAT_VK_INIT;
    BongoCatRhiVkFrame frames[BONGO_CAT_VK_FRAMES] BONGO_CAT_VK_INIT;
    BongoCatRhiVkReadback readbacks[BONGO_CAT_VK_FRAMES] BONGO_CAT_VK_INIT;
    uint32_t frame_slot BONGO_CAT_VK_INIT, readback_slot BONGO_CAT_VK_INIT, readback_frame BONGO_CAT_VK_INIT;
    BongoCatRhiVkUpload uploads[BONGO_CAT_VK_FRAMES] BONGO_CAT_VK_INIT;
    unsigned upload_slot BONGO_CAT_VK_INIT;
    VkSemaphore *present_semaphores BONGO_CAT_VK_INIT;
    bool failed BONGO_CAT_VK_INIT;
    VkFence fence BONGO_CAT_VK_INIT, submit_fence BONGO_CAT_VK_INIT;
    bool frame_ready BONGO_CAT_VK_INIT;
    VkImageLayout frame_layout BONGO_CAT_VK_INIT;
    bool pixels_valid BONGO_CAT_VK_INIT;
    const BongoCatRhi *owner BONGO_CAT_VK_INIT;
    uint32_t acquired_index BONGO_CAT_VK_INIT;
    VkImage current_image BONGO_CAT_VK_INIT;
    VkImageView current_view BONGO_CAT_VK_INIT;
    float clear[4] BONGO_CAT_VK_INIT;
    VkImage background BONGO_CAT_VK_INIT;
    VkDeviceMemory background_memory BONGO_CAT_VK_INIT;
    uint64_t background_revision BONGO_CAT_VK_INIT;
    bool background_ready BONGO_CAT_VK_INIT;
    PFN_vkGetInstanceProcAddr instance_gpa BONGO_CAT_VK_INIT;
#define BONGO_CAT_VK_FN(name) PFN_##name name BONGO_CAT_VK_INIT;
    BONGO_CAT_VK_FN(vkCreateShaderModule)
    BONGO_CAT_VK_FN(vkDestroyShaderModule)
    BONGO_CAT_VK_FN(vkCreatePipelineLayout)
    BONGO_CAT_VK_FN(vkDestroyPipelineLayout)
    BONGO_CAT_VK_FN(vkCreateGraphicsPipelines)
    BONGO_CAT_VK_FN(vkDestroyPipeline)
    BONGO_CAT_VK_FN(vkCmdBindPipeline)
    BONGO_CAT_VK_FN(vkCmdPushConstants)
    BONGO_CAT_VK_FN(vkCmdDraw)
    BONGO_CAT_VK_FN(vkCmdSetViewport)
    BONGO_CAT_VK_FN(vkCmdSetScissor)
    BONGO_CAT_VK_FN(vkCreateInstance)
    BONGO_CAT_VK_FN(vkDestroyInstance)
    BONGO_CAT_VK_FN(vkEnumeratePhysicalDevices)
    BONGO_CAT_VK_FN(vkGetPhysicalDeviceProperties)
    BONGO_CAT_VK_FN(vkGetPhysicalDeviceQueueFamilyProperties)
    BONGO_CAT_VK_FN(vkGetPhysicalDeviceSurfaceSupportKHR)
    BONGO_CAT_VK_FN(vkGetPhysicalDeviceSurfaceCapabilitiesKHR)
    BONGO_CAT_VK_FN(vkGetPhysicalDeviceSurfaceFormatsKHR)
    BONGO_CAT_VK_FN(vkGetPhysicalDeviceFormatProperties)
    BONGO_CAT_VK_FN(vkCreateDevice)
    BONGO_CAT_VK_FN(vkDestroyDevice)
    BONGO_CAT_VK_FN(vkDeviceWaitIdle)
    BONGO_CAT_VK_FN(vkGetDeviceQueue)
    BONGO_CAT_VK_FN(vkCreateSwapchainKHR)
    BONGO_CAT_VK_FN(vkDestroySwapchainKHR)
    BONGO_CAT_VK_FN(vkGetSwapchainImagesKHR)
    BONGO_CAT_VK_FN(vkCreateImageView)
    BONGO_CAT_VK_FN(vkDestroyImageView)
    BONGO_CAT_VK_FN(vkCreateRenderPass)
    BONGO_CAT_VK_FN(vkDestroyRenderPass)
    BONGO_CAT_VK_FN(vkCreateFramebuffer)
    BONGO_CAT_VK_FN(vkDestroyFramebuffer)
    BONGO_CAT_VK_FN(vkCreateCommandPool)
    BONGO_CAT_VK_FN(vkDestroyCommandPool)
    BONGO_CAT_VK_FN(vkAllocateCommandBuffers)
    BONGO_CAT_VK_FN(vkCmdBeginRenderPass)
    BONGO_CAT_VK_FN(vkCmdEndRenderPass)
    BONGO_CAT_VK_FN(vkEndCommandBuffer)
    BONGO_CAT_VK_FN(vkQueueSubmit)
    BONGO_CAT_VK_FN(vkQueuePresentKHR)
    BONGO_CAT_VK_FN(vkWaitForFences)
    BONGO_CAT_VK_FN(vkResetFences)
    BONGO_CAT_VK_FN(vkCreateFence)
    BONGO_CAT_VK_FN(vkDestroyFence)
    BONGO_CAT_VK_FN(vkAcquireNextImageKHR)
    BONGO_CAT_VK_FN(vkBeginCommandBuffer)
    BONGO_CAT_VK_FN(vkQueueWaitIdle)
    BONGO_CAT_VK_FN(vkFreeCommandBuffers)
    BONGO_CAT_VK_FN(vkResetCommandBuffer)
    BONGO_CAT_VK_FN(vkResetCommandPool)
    BONGO_CAT_VK_FN(vkCreateSemaphore)
    BONGO_CAT_VK_FN(vkDestroySemaphore)
    BONGO_CAT_VK_FN(vkCreateBuffer)
    BONGO_CAT_VK_FN(vkDestroyBuffer)
    BONGO_CAT_VK_FN(vkGetBufferMemoryRequirements)
    BONGO_CAT_VK_FN(vkAllocateMemory)
    BONGO_CAT_VK_FN(vkFreeMemory)
    BONGO_CAT_VK_FN(vkBindBufferMemory)
    BONGO_CAT_VK_FN(vkMapMemory)
    BONGO_CAT_VK_FN(vkUnmapMemory)
    BONGO_CAT_VK_FN(vkGetPhysicalDeviceMemoryProperties)
    BONGO_CAT_VK_FN(vkCmdPipelineBarrier)
    BONGO_CAT_VK_FN(vkCmdCopyImageToBuffer)
    BONGO_CAT_VK_FN(vkCreateImage)
    BONGO_CAT_VK_FN(vkDestroyImage)
    BONGO_CAT_VK_FN(vkGetImageMemoryRequirements)
    BONGO_CAT_VK_FN(vkBindImageMemory)
    BONGO_CAT_VK_FN(vkCmdCopyBufferToImage)
    BONGO_CAT_VK_FN(vkCmdCopyImage)
#undef BONGO_CAT_VK_FN
} BongoCatRhiVk;

#undef BONGO_CAT_VK_INIT

bool bongo_cat_rhi_vk_load_global_fns(BongoCatRhiVk *vk,
    BongoCatError *error);
bool bongo_cat_rhi_vk_load_instance_fns(BongoCatRhiVk *vk);
#ifdef BONGO_CAT_HAS_CUBISM_VULKAN
bool bongo_cat_rhi_vk_cubism_features(BongoCatRhiVk *vk,
    VkPhysicalDevice device, VkPhysicalDeviceVulkan13Features *enabled);
#endif

void bongo_cat_rhi_vk_release_corner_framebuffers(BongoCatRhiVk *vk);
void bongo_cat_rhi_vk_release_corners(BongoCatRhiVk *vk);
bool bongo_cat_rhi_vk_draw_corners(BongoCatRhiVk *vk, VkCommandBuffer command);
void bongo_cat_rhi_vk_release_background(BongoCatRhiVk *vk);
void bongo_cat_rhi_vk_record_background(BongoCatRhiVk *vk, VkCommandBuffer command);
BongoCatRhiVkUpload *bongo_cat_rhi_vk_begin_upload(BongoCatRhiVk *vk);
bool bongo_cat_rhi_vk_submit_upload(BongoCatRhiVk *vk, BongoCatRhiVkUpload *slot,
    VkBuffer buffer, VkDeviceMemory memory);
bool bongo_cat_rhi_vk_collect_uploads(BongoCatRhiVk *vk);
void bongo_cat_rhi_vk_destroy_uploads(BongoCatRhiVk *vk);
bool bongo_cat_rhi_vk_create_frames(BongoCatRhiVk *vk);
void bongo_cat_rhi_vk_destroy_frames(BongoCatRhiVk *vk);
bool bongo_cat_rhi_vk_wait_frame(BongoCatRhiVk *vk, BongoCatRhiVkFrame *frame);
bool bongo_cat_rhi_vk_queue_checkpoint(BongoCatRhiVk *vk);
bool bongo_cat_rhi_vk_submit_frame(BongoCatRhiVk *vk, VkCommandBuffer command);
void bongo_cat_rhi_vk_release_readback(BongoCatRhiVk *vk);
bool bongo_cat_rhi_vk_capture_frame(BongoCatRhiVk *vk);
bool bongo_cat_rhi_vk_upload_composed_frame(BongoCatRhiVk *vk, SDL_Rect changed);
/* Chooses and records a depth format for the Cubism render passes. */
bool bongo_cat_rhi_vk_pick_depth_format(BongoCatRhiVk *vk);
/* Destroys views, framebuffers and the swapchain (device idle required). */
void bongo_cat_rhi_vk_destroy_swapchain_objects(BongoCatRhiVk *vk);
/* Re-creates the swapchain for the requested pixel size. */
bool bongo_cat_rhi_vk_create_swapchain(BongoCatRhiVk *vk, int width, int height,
    BongoCatError *error);
bool bongo_cat_rhi_vk_recreate_swapchain(BongoCatRhiVk *vk, int width,
    int height);
/* Presents the prepared acquired frame; also the swap present op. */
bool bongo_cat_rhi_vk_frame_present(SDL_Window *window, void *user);
/* Premultiplied-BGRA / RGBA window readback helpers for present ops. */
bool bongo_cat_rhi_vk_read_bgra(int width, int height, void *pixels,
    void *user);
bool bongo_cat_rhi_vk_read_rgba(int x, int y, int width, int height,
    bool back_buffer, void *pixels, void *user);
const char *bongo_cat_rhi_vk_present_name(void *user);

#ifdef __cplusplus
}
#endif

#endif

#endif
