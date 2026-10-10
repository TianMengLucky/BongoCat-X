# Adapt only a generated renderer copy; SDK updates must match every anchor.
function(bongo_cat_recording_replace variable anchor replacement)
  set(source "${${variable}}")
  string(FIND "${source}" "${anchor}" begin)
  if(begin EQUAL -1)
    message(FATAL_ERROR "Cubism Vulkan recording patch mismatch: ${anchor}")
  endif()
  string(LENGTH "${anchor}" length)
  math(EXPR after "${begin} + ${length}")
  string(SUBSTRING "${source}" "${after}" -1 tail)
  string(FIND "${tail}" "${anchor}" duplicate)
  if(NOT duplicate EQUAL -1)
    message(FATAL_ERROR "Ambiguous Cubism Vulkan recording patch: ${anchor}")
  endif()
  string(REPLACE "${anchor}" "${replacement}" source "${source}")
  set(${variable} "${source}" PARENT_SCOPE)
endfunction()

function(bongo_cat_recording_function variable function_name anchor replacement)
  set(source "${${variable}}")
  string(FIND "${source}" "void CubismRenderer_Vulkan::${function_name}(" begin)
  if(begin EQUAL -1)
    message(FATAL_ERROR "Cubism Vulkan recording function missing: ${function_name}")
  endif()
  string(SUBSTRING "${source}" "${begin}" -1 tail)
  string(FIND "${tail}" "\nvoid CubismRenderer_Vulkan::" length)
  if(length EQUAL -1)
    message(FATAL_ERROR "Cubism Vulkan recording function boundary missing: ${function_name}")
  endif()
  string(SUBSTRING "${tail}" 0 "${length}" original)
  set(patched "${original}")
  bongo_cat_recording_replace(patched "${anchor}" "${replacement}")
  string(REPLACE "${original}" "${patched}" source "${source}")
  set(${variable} "${source}" PARENT_SCOPE)
endfunction()

function(bongo_cat_vulkan_recording_patch variable)
  set(source "${${variable}}")
  bongo_cat_recording_replace(source [=[#include "CubismRenderer_Vulkan.hpp"]=]
    [=[#include "CubismRenderer_Vulkan.hpp"
#include "cubism_vulkan_recording.hpp"]=])
  bongo_cat_recording_function(source BeginRendering
    [=[    vkCmdBeginRendering(drawCommandBuffer, &renderingInfo);]=]
    [=[    // Masks remain serial; only an ordinary model pass uses secondary commands.
    const bool compatible = !isResume && !GetModel()->IsBlendModeEnabled() &&
        GetModel()->GetOffscreenCount() == 0 && !IsUsingHighPrecisionMask() &&
        GetModel()->GetDrawableCount() >= 128;
    if (bongo_cat::vulkan_recording_begin(drawCommandBuffer, s_imageFormat,
            s_depthFormat, s_renderExtent, compatible))
        renderingInfo.flags |= VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT;
    vkCmdBeginRendering(drawCommandBuffer, &renderingInfo);]=])
  bongo_cat_recording_function(source EndRendering
    [=[    vkCmdEndRendering(drawCommandBuffer);]=]
    [=[    bongo_cat::vulkan_recording_end(drawCommandBuffer);
    vkCmdEndRendering(drawCommandBuffer);]=])
  bongo_cat_recording_function(source ExecuteDrawForDrawable
    [=[    BindVertexAndIndexBuffers(index, cmdBuffer, DrawableObjectType_Drawable);]=]
    [=[    if (!bongo_cat::vulkan_recording_active(cmdBuffer))
        BindVertexAndIndexBuffers(index, cmdBuffer, DrawableObjectType_Drawable);]=])
  bongo_cat_recording_function(source ExecuteDrawForDrawable
    [=[    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,]=]
    [=[    // Descriptor/UBO preparation stays on the calling thread. Workers see
    // immutable handles and never call DrawMeshVulkan or touch clipping contexts.
    if (bongo_cat::vulkan_recording_draw(cmdBuffer, {
            pipeline, pipelineLayout, *descriptorSet,
            _vertexBuffers[_commandBufferCurrent][index].GetBuffer(),
            _indexBuffers[_commandBufferCurrent][index].GetBuffer(),
            static_cast<uint32_t>(model.GetDrawableVertexIndexCount(index)),
            static_cast<VkCullModeFlags>(IsCulling() ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_NONE),
            static_cast<uint32_t>(textureIndex)})) return;
    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,]=])
  bongo_cat_recording_function(source DrawMeshVulkan [=[    if (IsCulling())
    {
        vkCmdSetCullModeEXT(commandBuffer, VK_CULL_MODE_BACK_BIT);
    }
    else
    {
        vkCmdSetCullModeEXT(commandBuffer, VK_CULL_MODE_NONE);
    }]=] [=[    if (!bongo_cat::vulkan_recording_active(commandBuffer))
    {
        vkCmdSetCullModeEXT(commandBuffer,
            IsCulling() ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_NONE);
    }]=])
  # Each command appears twice in DrawDrawable (high-precision masks and the
  # ordinary draw). Only the final viewport/scissor pair is replaced.
  bongo_cat_recording_function(source DrawDrawable
    [=[    vkCmdSetViewport(drawCommandBuffer, 0, 1, &viewport);

    const VkRect2D rect = GetScissor(]=]
    [=[    if (!bongo_cat::vulkan_recording_active(drawCommandBuffer))
        vkCmdSetViewport(drawCommandBuffer, 0, 1, &viewport);

    const VkRect2D rect = GetScissor(]=])
  bongo_cat_recording_function(source DrawDrawable
    [=[        static_cast<csmFloat32>(s_renderExtent.height)
    );
    vkCmdSetScissor(drawCommandBuffer, 0, 1, &rect);]=]
    [=[        static_cast<csmFloat32>(s_renderExtent.height)
    );
    if (!bongo_cat::vulkan_recording_active(drawCommandBuffer))
        vkCmdSetScissor(drawCommandBuffer, 0, 1, &rect);]=])
  set(${variable} "${source}" PARENT_SCOPE)
endfunction()
