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
  bongo_cat_vulkan_skip_empty_masks(source)
  bongo_cat_vertex_replace(source [=[#include "CubismRenderer_Vulkan.hpp"]=]
    [=[#include "CubismRenderer_Vulkan.hpp"
#include "cubism_vulkan_memory.hpp"
#include <cstring>]=])
  bongo_cat_vertex_replace(source [=[    csmVector<ModelVertex> vertices;]=]
    [=[    if (vcount <= 0) return;
    // Pooling off: retain the SDK upload, with one exact capacity reservation.
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
