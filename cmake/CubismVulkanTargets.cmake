# Allocate only render targets and copy resources that this model can use.
function(bongo_cat_vulkan_targets_patch variable)
  set(source "${${variable}}")
  bongo_cat_vertex_replace(source "    csmInt32 createSize = 1;"
    "    csmInt32 createSize = model->IsBlendModeEnabled() ? 1 : 0;")
  foreach(pair IN ITEMS "copyVertexBufferSize|_copyVertexBuffer[buffer] = copyVertexBuffer;"
      "copyIndexBufferSize|stagingBuffer.Destroy(s_device);")
    string(REPLACE "|" ";" fields "${pair}")
    list(GET fields 0 name)
    list(GET fields 1 end)
    # Restore the statement terminator consumed as a CMake list separator.
    string(APPEND end ";")
    set(start "        VkDeviceSize ${name} = ")
    string(FIND "${source}" "${start}" begin)
    if(begin LESS 0)
      message(FATAL_ERROR "Cubism Vulkan copy buffer patch mismatch: ${name}")
    endif()
    string(SUBSTRING "${source}" ${begin} -1 tail)
    string(FIND "${tail}" "${end}" finish)
    if(finish LESS 0)
      message(FATAL_ERROR "Cubism Vulkan copy buffer boundary mismatch: ${name}")
    endif()
    string(LENGTH "${end}" end_length)
    math(EXPR length "${finish} + ${end_length}")
    string(SUBSTRING "${tail}" 0 ${length} original)
    bongo_cat_vertex_replace(source "${original}"
      "        if (GetModel()->IsBlendModeEnabled() || GetModel()->GetOffscreenCount() > 0) {\n${original}\n        }")
  endforeach()
  # Keep layout creation independent: lazy pipeline descriptions borrow it.
  bongo_cat_vertex_replace(source
    "    if (vkCreateDescriptorPool(s_device, &copyPoolInfo, nullptr, &_copyDescriptorPool) != VK_SUCCESS)"
    "    if ((GetModel()->IsBlendModeEnabled() || offscreenCount > 0) &&\n        vkCreateDescriptorPool(s_device, &copyPoolInfo, nullptr, &_copyDescriptorPool) != VK_SUCCESS)")
  bongo_cat_vertex_replace(source "        _copyDescriptorSets[buffer].uniformBuffer.CreateBuffer"
    "        if (!GetModel()->IsBlendModeEnabled() && offscreenCount == 0) continue;\n        _copyDescriptorSets[buffer].uniformBuffer.CreateBuffer")
  # Each manager needs only its own mask count, rather than the shared maximum.
  foreach(kind IN ITEMS drawable offscreen)
    bongo_cat_vertex_replace(source
      "        _${kind}ClippingManager->Initialize(\n            *model,\n            maskBufferCount,"
      "        const csmInt32 ${kind}MaskCount = bongo_cat::native_mask_counts.${kind} > 0 ?\n            bongo_cat::native_mask_counts.${kind} : maskBufferCount;\n        _${kind}ClippingManager->Initialize(\n            *model,\n            ${kind}MaskCount,")
    bongo_cat_vertex_replace(source "            _${kind}MaskBuffers[buffer].Resize(maskBufferCount);"
      "            _${kind}MaskBuffers[buffer].Resize(${kind}MaskCount);")
  endforeach()
  # These are the only remaining loops using the common count in Initialize.
  bongo_cat_vertex_replace(source "            for (csmInt32 i = 0; i < maskBufferCount; i++)"
    "            for (csmInt32 i = 0; i < drawableMaskCount; i++)")
  bongo_cat_vertex_replace(source "            for (csmInt32 i = 0; i < maskBufferCount; ++i)"
    "            for (csmInt32 i = 0; i < offscreenMaskCount; ++i)")
  set(${variable} "#include \"cubism_native_masks.hpp\"\n${source}" PARENT_SCOPE)
endfunction()
