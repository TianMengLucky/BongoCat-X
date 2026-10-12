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
  # Async submission waits its draw fence before the next backing-set mutation.
  # Keep one SDK set to avoid doubling model/mask memory; host frames use two slots.
  # Refuse this adaptation if an SDK update changes its submission boundaries.
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
    [=[    // Draw fence is waited before reuse; swapchain images are selected separately.
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
  bongo_cat_vertex_replace(source [=[            _descriptorSets[buffer][drawAssign].uniformBufferMask.CreateBuffer(s_device, s_physicalDevice, sizeof(ModelUBO),
                                                                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            _descriptorSets[buffer][drawAssign].uniformBufferMask.Map(s_device, VK_WHOLE_SIZE);]=]
    [=[            // Mask UBOs are created only for drawables actually used as mask sources.]=])
  bongo_cat_vertex_replace(source [=[    descriptor.uniformBufferMask.MemCpy(&ubo, sizeof(ModelUBO));]=]
    [=[    if (!descriptor.uniformBufferMask.GetBuffer()) {
        try {
            descriptor.uniformBufferMask.CreateBuffer(s_device, s_physicalDevice, sizeof(ModelUBO),
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            descriptor.uniformBufferMask.Map(s_device, VK_WHOLE_SIZE);
        } catch (...) {
            descriptor.uniformBufferMask.Destroy(s_device);
            descriptor.uniformBufferMask = CubismBufferVulkan();
            throw;
        }
    }
    descriptor.uniformBufferMask.MemCpy(&ubo, sizeof(ModelUBO));]=])
  # Invisible drawables and unused offscreen targets need no mapped UBO.
  bongo_cat_vertex_replace(source [=[            _descriptorSets[buffer][drawAssign].uniformBuffer.CreateBuffer(s_device, s_physicalDevice, sizeof(ModelUBO),
                                                                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            _descriptorSets[buffer][drawAssign].uniformBuffer.Map(s_device, VK_WHOLE_SIZE);]=]
    [=[            // Ordinary UBO allocation is deferred until this drawable is rendered.]=])
  set(ubo_write [=[    descriptor.uniformBuffer.MemCpy(&ubo, sizeof(ModelUBO));]=])
  string(REGEX MATCHALL "descriptor.uniformBuffer.MemCpy\\(&ubo, sizeof\\(ModelUBO\\)\\)" writes "${source}")
  list(LENGTH writes write_count)
  if(NOT write_count EQUAL 2)
    message(FATAL_ERROR "Cubism ordinary uniform write sites changed")
  endif()
  string(REPLACE "${ubo_write}" [=[    if (!descriptor.uniformBuffer.GetBuffer()) {
        try {
            descriptor.uniformBuffer.CreateBuffer(s_device, s_physicalDevice, sizeof(ModelUBO),
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            descriptor.uniformBuffer.Map(s_device, VK_WHOLE_SIZE);
        } catch (...) {
            descriptor.uniformBuffer.Destroy(s_device);
            descriptor.uniformBuffer = CubismBufferVulkan();
            throw;
        }
    }
    descriptor.uniformBuffer.MemCpy(&ubo, sizeof(ModelUBO));]=] source "${source}")
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
