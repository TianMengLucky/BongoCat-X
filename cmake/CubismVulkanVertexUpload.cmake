# Change only generated SDK copies. Coherent staging buffers are already mapped
# during renderer initialization; reuse that mapping without extra map/unmap calls.
function(bongo_cat_vertex_replace variable anchor replacement)
  set(source "${${variable}}")
  string(FIND "${source}" "${anchor}" first)
  if(first EQUAL -1)
    message(FATAL_ERROR "Cubism Vulkan vertex upload patch mismatch: ${anchor}")
  endif()
  string(LENGTH "${anchor}" length)
  math(EXPR after "${first} + ${length}")
  string(SUBSTRING "${source}" "${after}" -1 tail)
  string(FIND "${tail}" "${anchor}" duplicate)
  if(NOT duplicate EQUAL -1)
    message(FATAL_ERROR "Ambiguous Cubism Vulkan vertex upload patch: ${anchor}")
  endif()
  string(REPLACE "${anchor}" "${replacement}" source "${source}")
  set(${variable} "${source}" PARENT_SCOPE)
endfunction()

function(bongo_cat_vulkan_skip_empty_masks variable)
  set(source "${${variable}}")
  set(anchor "void CubismRenderer_Vulkan::DoDrawModel()\n{")
  string(FIND "${source}" "${anchor}" first)
  if(first EQUAL -1)
    message(FATAL_ERROR "Cubism Vulkan vertex upload patch: DoDrawModel missing")
  endif()
  string(SUBSTRING "${source}" "${first}" -1 tail)
  string(FIND "${tail}" "\nvoid CubismRenderer_Vulkan::PostDraw()" length)
  if(length EQUAL -1)
    message(FATAL_ERROR "Cubism Vulkan vertex upload patch: DoDrawModel boundary missing")
  endif()
  string(SUBSTRING "${tail}" 0 "${length}" original)
  set(body "${original}")
  bongo_cat_vertex_replace(body
    [=[    vkBeginCommandBuffer(updateCommandBuffer, &beginInfo);]=]
    [=[    // No mask manager means this preprocessing pass has no work.
    // DrawObjectLoop begins its own command buffers and keeps the final wait.
    if (!_drawableClippingManager && !_offscreenClippingManager)
    {
        DrawObjectLoop(updateCommandBuffer, drawCommandBuffer, beginInfo);
        return;
    }
    vkBeginCommandBuffer(updateCommandBuffer, &beginInfo);]=])
  string(REPLACE "${original}" "${body}" source "${source}")
  set(${variable} "${source}" PARENT_SCOPE)
endfunction()

function(bongo_cat_vulkan_vertex_upload_patch variable)
  set(source "${${variable}}")
  # Draw submissions wait for the graphics queue before returning. Buffer slots
  # are independent of acquired swapchain images, so only one set is in flight.
  # Refuse this adaptation if an SDK update changes the synchronization contract.
  set(submit_anchor "void CubismRenderer_Vulkan::SubmitCommand(")
  string(FIND "${source}" "${submit_anchor}" submit_start)
  string(FIND "${source}" "void CubismRenderer_Vulkan::SetConstantSettings(" submit_end)
  if(submit_start EQUAL -1 OR submit_end LESS submit_start)
    message(FATAL_ERROR "Cubism Vulkan vertex upload patch: submission boundary missing")
  endif()
  math(EXPR submit_length "${submit_end} - ${submit_start}")
  string(SUBSTRING "${source}" "${submit_start}" "${submit_length}" submit)
  set(wait_anchor [=[    if (waitUpdateFinishedSemaphore != VK_NULL_HANDLE)
    {
        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &waitUpdateFinishedSemaphore;
        submitInfo.pWaitDstStageMask = waitStages;
        vkQueueSubmit(s_queue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(s_queue);
    }]=])
  string(FIND "${submit}" "${wait_anchor}" wait_found)
  string(FIND "${submit}" "if (waitUpdateFinishedSemaphore != VK_NULL_HANDLE)" wait_branch)
  if(wait_found EQUAL -1 OR wait_branch EQUAL -1)
    message(FATAL_ERROR "Cubism Vulkan vertex upload patch: synchronous submission missing")
  endif()
  string(REPLACE "${submit}" [=[void CubismRenderer_Vulkan::SubmitCommand(VkCommandBuffer commandBuffer,
    VkSemaphore signalUpdateFinishedSemaphore, VkSemaphore waitUpdateFinishedSemaphore)
{
    bongo_cat::vulkan_submit(commandBuffer, signalUpdateFinishedSemaphore, waitUpdateFinishedSemaphore);
}

]=] source "${source}")
  bongo_cat_vertex_replace(source [=[    s_bufferSetNum = swapchainImageCount;]=]
    [=[    // Submission completes before reuse; swapchain images are selected separately.
    (void)swapchainImageCount;
    s_bufferSetNum = 1;]=])
  bongo_cat_vertex_replace(source [=[    _textures.PushBack(image);]=]
    [=[    const int slot = bongo_cat::vulkan_texture_rebind_slot;
    if (slot < 0) { _textures.PushBack(image); return; }
    if (slot >= _textures.GetSize()) throw std::runtime_error("Invalid texture replacement slot");
    _textures[slot] = image;
    // Idle replacement invalidates cached ordinary and mask descriptors.
    for (csmUint32 buffer = 0; buffer < s_bufferSetNum; ++buffer)
    {
        for (csmInt32 i = 0; i < _descriptorSets[buffer].GetSize(); ++i)
        {
            _descriptorSets[buffer][i].isDescriptorSetUpdated = false;
            _descriptorSets[buffer][i].isDescriptorSetMaskedUpdated = false;
            _descriptorSets[buffer][i].isDescriptorSetForMaskUpdated = false;
        }
        for (csmInt32 i = 0; i < _offscreenDescriptorSets[buffer].GetSize(); ++i)
        {
            _offscreenDescriptorSets[buffer][i].isDescriptorSetUpdated = false;
            _offscreenDescriptorSets[buffer][i].isDescriptorSetMaskedUpdated = false;
            _offscreenDescriptorSets[buffer][i].isDescriptorSetForMaskUpdated = false;
        }
    }]=])
  bongo_cat_vertex_replace(source [=[            if (vcount != 0)]=]
    [=[            if (vcount != 0 && !bongo_cat::vulkan_geometry_available())]=])
  bongo_cat_vertex_replace(source [=[    csmVector<ModelVertex> vertices;]=]
    [=[    if (vcount <= 0) return;
    static_assert(sizeof(ModelVertex) == sizeof(float) * 4, "Vertex arena layout changed");
    if (bongo_cat::vulkan_geometry_upload(commandBuffer)) return;
    csmVector<ModelVertex> vertices;]=])
  bongo_cat_vertex_replace(source [=[    VkBuffer vertexBuffers[] = {(*targetVertexBuffers)[_commandBufferCurrent][index].GetBuffer()};
    VkDeviceSize offsets[] = {0};]=]
    [=[    VkBuffer vertexBuffers[] = {(*targetVertexBuffers)[_commandBufferCurrent][index].GetBuffer()};
    VkDeviceSize offsets[] = {0};
    if (drawableObjectType == CubismRenderer::DrawableObjectType_Drawable) {
        vertexBuffers[0] = bongo_cat::vulkan_geometry_buffer(vertexBuffers[0]);
        offsets[0] = bongo_cat::vulkan_geometry_offset((size_t)index);
    }]=])
  bongo_cat_vulkan_skip_empty_masks(source)
  bongo_cat_vertex_replace(source [=[#include "CubismRenderer_Vulkan.hpp"]=]
    [=[#include "CubismRenderer_Vulkan.hpp"
#include "cubism_vulkan_memory.hpp"
#include "cubism_vulkan_geometry.hpp"
#include "cubism_vulkan_submission.hpp"
#include <cstring>
#include <stdexcept>]=])
  bongo_cat_vertex_replace(source [=[    csmVector<ModelVertex> vertices;]=]
    [=[    // Pooling off: retain the SDK upload, with one exact capacity reservation.
    csmVector<ModelVertex> vertices;
    auto *mapped = static_cast<unsigned char *>(bongo_cat::vulkan_buffer_mapped_data(
        _stagingBuffers[_commandBufferCurrent][drawAssign].GetBuffer()));
    if (!mapped) vertices.PrepareCapacity(vcount);]=])
  bongo_cat_vertex_replace(source [=[        vertices.PushBack(vertex);]=]
    [=[        if (mapped) std::memcpy(mapped + (size_t)(ct / 2) * sizeof(ModelVertex),
            &vertex, sizeof(vertex));
        else vertices.PushBack(vertex);]=])
  bongo_cat_vertex_replace(source
    [=[    csmUint32 bufferSize = sizeof(ModelVertex) * vertices.GetSize();
    _stagingBuffers[_commandBufferCurrent][drawAssign].MemCpy(vertices.GetPtr(), (size_t)bufferSize);]=]
    [=[    csmUint32 bufferSize = sizeof(ModelVertex) * vcount;
    if (!mapped)
        _stagingBuffers[_commandBufferCurrent][drawAssign].MemCpy(vertices.GetPtr(), (size_t)bufferSize);]=])
  set(${variable} "${source}" PARENT_SCOPE)
endfunction()
