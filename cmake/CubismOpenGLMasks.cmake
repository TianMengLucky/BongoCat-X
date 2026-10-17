function(bongo_cat_gl_mask_counts variable)
  set(source "${${variable}}")
  foreach(kind IN ITEMS drawable offscreen)
    set(anchor "        _${kind}ClippingManager->Initialize(\n            *model,\n            maskBufferCount,")
    bongo_cat_replace_cubism_text(source "${anchor}"
      "        const csmInt32 ${kind}MaskCount = bongo_cat::native_mask_counts.${kind} > 0 ?\n            bongo_cat::native_mask_counts.${kind} : maskBufferCount;\n        _${kind}ClippingManager->Initialize(\n            *model,\n            ${kind}MaskCount," "${kind} OpenGL mask count")
    set(anchor "        _${kind}Masks.Clear();\n        for (csmInt32 i = 0; i < maskBufferCount; ++i)")
    bongo_cat_replace_cubism_text(source "${anchor}"
      "        _${kind}Masks.Clear();\n        for (csmInt32 i = 0; i < ${kind}MaskCount; ++i)" "${kind} OpenGL mask targets")
  endforeach()
  set(${variable} "#include \"cubism_native_masks.hpp\"\n${source}" PARENT_SCOPE)
endfunction()
