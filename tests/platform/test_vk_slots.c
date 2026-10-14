/* Run the real slot helpers against a delayed, failure-injectable Vulkan dispatch. */
#include "rhi_vk_internal.h"
#include "test.h"
#include <string.h>
int bongo_cat_test_failures;
static uintptr_t handles;
static unsigned pools, fences, commands, buffers_freed, memory_freed, blocking_waits;
static bool pending[64];
static unsigned pool_commands[64];
static VkResult submit_result = VK_SUCCESS, wait_result = VK_SUCCESS;
static VkSemaphore signaled;
static uintptr_t id(VkFence fence) { return (uintptr_t)fence; }
static VKAPI_ATTR VkResult VKAPI_CALL create_pool(VkDevice d,
    const VkCommandPoolCreateInfo *info, const VkAllocationCallbacks *a, VkCommandPool *out) {
    (void)d; (void)a; CHECK(info->flags & VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);
    *out = (VkCommandPool)++handles; pools++; return VK_SUCCESS;
}
static VKAPI_ATTR void VKAPI_CALL destroy_pool(VkDevice d, VkCommandPool p,
    const VkAllocationCallbacks *a) {
    (void)d; (void)a;
    commands -= pool_commands[(uintptr_t)p];
    pool_commands[(uintptr_t)p] = 0;
    pools--;
}
static VKAPI_ATTR VkResult VKAPI_CALL allocate_commands(VkDevice d,
    const VkCommandBufferAllocateInfo *info, VkCommandBuffer *out) {
    (void)d;
    for (uint32_t i = 0; i < info->commandBufferCount; ++i) out[i] = (VkCommandBuffer)++handles;
    commands += info->commandBufferCount;
    pool_commands[(uintptr_t)info->commandPool] += info->commandBufferCount;
    return VK_SUCCESS;
}
static VKAPI_ATTR VkResult VKAPI_CALL create_fence(VkDevice d,
    const VkFenceCreateInfo *info, const VkAllocationCallbacks *a, VkFence *out) {
    (void)d; (void)info; (void)a; *out = (VkFence)++handles; fences++; return VK_SUCCESS;
}
static VKAPI_ATTR void VKAPI_CALL destroy_fence(VkDevice d, VkFence f,
    const VkAllocationCallbacks *a) { (void)d; (void)f; (void)a; fences--; }
static VKAPI_ATTR VkResult VKAPI_CALL reset_fences(VkDevice d, uint32_t count, const VkFence *f) {
    (void)d; CHECK(count == 1); CHECK(!pending[id(*f)]); return VK_SUCCESS;
}
static VKAPI_ATTR VkResult VKAPI_CALL wait_fences(VkDevice d, uint32_t count,
    const VkFence *f, VkBool32 all, uint64_t timeout) {
    (void)d; CHECK(count == 1 && all == VK_TRUE);
    if (!timeout) return wait_result != VK_SUCCESS ? wait_result :
        pending[id(*f)] ? VK_TIMEOUT : VK_SUCCESS;
    blocking_waits++;
    if (wait_result == VK_SUCCESS) pending[id(*f)] = false;
    return wait_result;
}
static VKAPI_ATTR VkResult VKAPI_CALL submit(VkQueue q, uint32_t count,
    const VkSubmitInfo *info, VkFence f) {
    (void)q; CHECK(count == 1); CHECK(!pending[id(f)]);
    if (submit_result != VK_SUCCESS) return submit_result;
    pending[id(f)] = true;
    if (info->signalSemaphoreCount) signaled = *info->pSignalSemaphores;
    return VK_SUCCESS;
}
static VKAPI_ATTR VkResult VKAPI_CALL reset_pool(VkDevice d, VkCommandPool p, VkCommandPoolResetFlags flags) {
    (void)d; (void)p; CHECK(flags & VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT); return VK_SUCCESS;
}
static VKAPI_ATTR VkResult VKAPI_CALL begin(VkCommandBuffer cmd, const VkCommandBufferBeginInfo *info) {
    (void)cmd; CHECK(info->flags & VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT); return VK_SUCCESS;
}
static VKAPI_ATTR VkResult VKAPI_CALL end(VkCommandBuffer cmd) { (void)cmd; return VK_SUCCESS; }
static VKAPI_ATTR void VKAPI_CALL destroy_buffer(VkDevice d, VkBuffer b, const VkAllocationCallbacks *a) {
    (void)d; (void)b; (void)a; buffers_freed++;
}
static VKAPI_ATTR void VKAPI_CALL free_memory(VkDevice d, VkDeviceMemory m, const VkAllocationCallbacks *a) {
    (void)d; (void)m; (void)a; memory_freed++;
}
static unsigned readbacks_released;
void bongo_cat_rhi_vk_release_readback(BongoCatRhiVk *vk) {
    for (unsigned i = 0; i < BONGO_CAT_VK_FRAMES; ++i) {
        if (vk->readbacks[i].readback_memory) readbacks_released++;
        memset(&vk->readbacks[i], 0, sizeof(vk->readbacks[i]));
    }
    vk->pixels_valid = false;
}
static BongoCatRhiVk mock(void) {
    BongoCatRhiVk vk = {0};
    vk.device = (VkDevice)(uintptr_t)1;
    vk.vkCreateCommandPool = create_pool; vk.vkDestroyCommandPool = destroy_pool;
    vk.vkAllocateCommandBuffers = allocate_commands;
    vk.vkCreateFence = create_fence; vk.vkDestroyFence = destroy_fence;
    vk.vkResetFences = reset_fences; vk.vkWaitForFences = wait_fences;
    vk.vkQueueSubmit = submit; vk.vkEndCommandBuffer = end;
    vk.vkBeginCommandBuffer = begin; vk.vkResetCommandPool = reset_pool;
    vk.vkDestroyBuffer = destroy_buffer; vk.vkFreeMemory = free_memory;
    return vk;
}
static void frame_slots(void) {
    BongoCatRhiVk vk = mock();
    VkSemaphore signals[3] = {(VkSemaphore)(uintptr_t)41, (VkSemaphore)(uintptr_t)42,
        (VkSemaphore)(uintptr_t)43};
    vk.present_semaphores = signals;
    CHECK(bongo_cat_rhi_vk_create_frames(&vk));
    CHECK(pools == 2 && fences == 2 && commands == 4);
    for (unsigned i = 0; i < 100; ++i) {
        vk.frame_slot = i % BONGO_CAT_VK_FRAMES;
        vk.acquired_index = i % 3;
        BongoCatRhiVkFrame *frame = &vk.frames[vk.frame_slot];
        unsigned waits = blocking_waits;
        CHECK(bongo_cat_rhi_vk_wait_frame(&vk, frame));
        CHECK(blocking_waits == waits + (i >= 2 ? 1u : 0u));
        waits = blocking_waits;
        CHECK(bongo_cat_rhi_vk_submit_frame(&vk, frame->finish));
        CHECK(blocking_waits == waits); /* Return while GPU work is still pending. */
        CHECK(frame->pending && signaled == signals[vk.acquired_index]);
        CHECK(commands == 4 && pools == 2 && fences == 2);
    }
    for (unsigned i = 0; i < 2; ++i) CHECK(bongo_cat_rhi_vk_wait_frame(&vk, &vk.frames[i]));
    submit_result = VK_ERROR_OUT_OF_HOST_MEMORY;
    CHECK(!bongo_cat_rhi_vk_submit_frame(&vk, vk.frames[vk.frame_slot].finish));
    unsigned waits = blocking_waits;
    CHECK(!vk.frames[vk.frame_slot].pending);
    CHECK(!bongo_cat_rhi_vk_wait_frame(&vk, &vk.frames[vk.frame_slot]));
    CHECK(waits == blocking_waits); /* Never wait a reset fence after failed submit. */
    submit_result = VK_SUCCESS;
    bongo_cat_rhi_vk_destroy_frames(&vk);
    CHECK(!pools && !fences);
}
static void upload_slots(void) {
    BongoCatRhiVk vk = mock();
    unsigned freed = buffers_freed, waits = blocking_waits, allocated = commands;
    for (unsigned i = 0; i < 100; ++i) {
        BongoCatRhiVkUpload *slot = bongo_cat_rhi_vk_begin_upload(&vk);
        CHECK(slot != NULL);
        if (!slot) break;
        CHECK(blocking_waits == waits + (i >= 2 ? i - 1 : 0));
        unsigned before = blocking_waits;
        CHECK(bongo_cat_rhi_vk_submit_upload(&vk, slot, (VkBuffer)(uintptr_t)(100 + i),
            (VkDeviceMemory)(uintptr_t)(200 + i)));
        CHECK(blocking_waits == before);
        CHECK(slot->pending && slot->buffer);
        CHECK(buffers_freed == freed + (i >= 2 ? i - 1 : 0));
        CHECK(commands <= allocated + 2 && pools <= 2 && fences <= 2);
    }
    CHECK(buffers_freed == freed + 98); /* Only two delayed staging allocations remain. */
    BongoCatRhiVkUpload *old = &vk.uploads[vk.upload_slot];
    wait_result = VK_ERROR_DEVICE_LOST;
    unsigned before = buffers_freed;
    CHECK(!bongo_cat_rhi_vk_begin_upload(&vk));
    CHECK(vk.failed && old->pending && old->buffer);
    CHECK(buffers_freed == before); /* Unproven completion must not destroy staging. */
    wait_result = VK_SUCCESS;
    // Simulate the explicit device-idle teardown boundary.
    for (unsigned i = 0; i < 2; ++i) pending[id(vk.uploads[i].fence)] = false;
    bongo_cat_rhi_vk_destroy_uploads(&vk);
    CHECK(buffers_freed == freed + 100 && memory_freed == buffers_freed);
    CHECK(!pools && !fences);
}
static void cached_background(void) {
    BongoCatRhiVk vk=mock(); BongoCatRhi rhi={0}; rhi.impl=&vk;
    vk.extent=(VkExtent2D){80,60}; vk.background=(VkImage)(uintptr_t)123;
    vk.background_revision=7;
    CHECK(!bongo_cat_rhi_vk_background_matches(&rhi,80,60,7));
    vk.background_ready=true;
    CHECK(bongo_cat_rhi_vk_background_matches(&rhi,80,60,7));
    CHECK(!bongo_cat_rhi_vk_background_matches(&rhi,81,60,7));
    CHECK(!bongo_cat_rhi_vk_background_matches(&rhi,80,60,8));
    vk.background_ready=false; // Rebuild/clear invalidates the retained GPU copy.
    CHECK(!bongo_cat_rhi_vk_background_matches(&rhi,80,60,7));
    vk.background_ready=true; vk.failed=true;
    CHECK(!bongo_cat_rhi_vk_background_matches(&rhi,80,60,7));
}
static void idle_memory(void) {
    BongoCatRhiVk vk = mock(); BongoCatRhi rhi = {0}; rhi.impl = &vk;
    CHECK(bongo_cat_rhi_vk_create_frames(&vk));
    vk.layered_target = true; vk.frame_ready = true; vk.pixels_valid = true;
    vk.current_image = (VkImage)(uintptr_t)101;
    vk.readbacks[0].readback_memory = (VkDeviceMemory)(uintptr_t)102;
    CHECK(bongo_cat_rhi_vk_submit_frame(&vk, vk.frames[0].finish));
    unsigned released = readbacks_released;
    wait_result = VK_ERROR_DEVICE_LOST;
    CHECK(!bongo_cat_rhi_vk_release_idle_memory(&rhi));
    CHECK(readbacks_released == released && vk.readbacks[0].readback_memory);
    vk.failed = false; wait_result = VK_SUCCESS;
    submit_result = VK_ERROR_OUT_OF_HOST_MEMORY;
    CHECK(!bongo_cat_rhi_vk_release_idle_memory(&rhi));
    CHECK(readbacks_released == released && vk.readbacks[0].readback_memory);
    vk.failed = false; submit_result = VK_SUCCESS;
    CHECK(bongo_cat_rhi_vk_release_idle_memory(&rhi));
    CHECK(readbacks_released == released + 1 && !vk.readbacks[0].readback_memory);
    CHECK(!vk.frame_ready && !vk.pixels_valid && !vk.current_image && vk.frame_slot == 1);
    unsigned waits = blocking_waits;
    CHECK(bongo_cat_rhi_vk_release_idle_memory(&rhi));
    CHECK(blocking_waits == waits); /* Repeated hidden polls do not wait or allocate. */
    vk.layered_target = false; vk.frame_ready = true;
    vk.readbacks[0].readback_memory = (VkDeviceMemory)(uintptr_t)103;
    CHECK(!bongo_cat_rhi_vk_release_idle_memory(&rhi));
    CHECK(readbacks_released == released + 1 && vk.frame_ready);
    bongo_cat_rhi_vk_destroy_frames(&vk);
    if (vk.submit_fence) destroy_fence(vk.device, vk.submit_fence, NULL);
    CHECK(!pools && !fences);
}
static void idle_upload_memory(void) {
    BongoCatRhiVk vk = mock(); BongoCatRhi rhi = {0}; rhi.impl = &vk;
    BongoCatRhiVkUpload *slot = bongo_cat_rhi_vk_begin_upload(&vk);
    CHECK(slot != NULL);
    if (!slot) return;
    CHECK(bongo_cat_rhi_vk_submit_upload(&vk, slot, (VkBuffer)(uintptr_t)501,
        (VkDeviceMemory)(uintptr_t)502));
    unsigned freed = buffers_freed;
    wait_result = VK_ERROR_DEVICE_LOST;
    CHECK(!bongo_cat_rhi_vk_release_idle_memory(&rhi));
    CHECK(slot->buffer && slot->pending && buffers_freed == freed);
    // Simulate completion before retrying the failed checkpoint fence.
    pending[id(vk.submit_fence)] = false;
    pending[id(slot->fence)] = false;
    vk.failed = false; wait_result = VK_SUCCESS;
    // The queue checkpoint completes all earlier uploads.
    CHECK(bongo_cat_rhi_vk_release_idle_memory(&rhi));
    CHECK(buffers_freed == freed + 1 && !slot->buffer && !slot->pool);
    unsigned waits = blocking_waits;
    CHECK(bongo_cat_rhi_vk_release_idle_memory(&rhi));
    CHECK(waits == blocking_waits);
    if (vk.submit_fence) destroy_fence(vk.device, vk.submit_fence, NULL);
    CHECK(!pools && !fences);
}
int main(void) {
    idle_upload_memory();
    frame_slots();
    // Layered presentation has no WSI consumer or signal semaphore.
    BongoCatRhiVk layered = mock();
    layered.layered_target = true;
    CHECK(bongo_cat_rhi_vk_create_frames(&layered));
    signaled = VK_NULL_HANDLE;
    CHECK(bongo_cat_rhi_vk_submit_frame(&layered, layered.frames[0].finish));
    CHECK(layered.frames[0].pending && signaled == VK_NULL_HANDLE);
    BongoCatRhi rhi = {0}; rhi.impl = &layered;
    CHECK(bongo_cat_rhi_vk_async_frame_state(&rhi) == BONGO_CAT_RHI_ASYNC_IDLE);
    layered.frame_ready = true;
    unsigned waits = blocking_waits;
    CHECK(bongo_cat_rhi_vk_async_frame_state(&rhi) == BONGO_CAT_RHI_ASYNC_PENDING);
    CHECK(layered.frames[0].pending && blocking_waits == waits);
    pending[id(layered.frames[0].fence)] = false; // GPU completes independently.
    CHECK(bongo_cat_rhi_vk_async_frame_state(&rhi) == BONGO_CAT_RHI_ASYNC_READY);
    CHECK(!layered.frames[0].pending && blocking_waits == waits);
    layered.frames[0].pending = true; wait_result = VK_ERROR_DEVICE_LOST;
    CHECK(bongo_cat_rhi_vk_async_frame_state(&rhi) == BONGO_CAT_RHI_ASYNC_FAILED);
    CHECK(layered.failed && layered.frames[0].pending && blocking_waits == waits);
    wait_result = VK_SUCCESS; layered.failed = false;
    layered.frames[0].pending = false;
    CHECK(bongo_cat_rhi_vk_wait_frame(&layered, &layered.frames[0]));
    bongo_cat_rhi_vk_destroy_frames(&layered);
    upload_slots();
    cached_background(); idle_memory();
    return bongo_cat_test_failures ? 1 : 0;
}
