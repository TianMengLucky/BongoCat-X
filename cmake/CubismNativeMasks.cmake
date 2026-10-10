function(bongo_cat_native_masks_patch variable backend)
  set(source "${${variable}}")
  string(ASCII 239 187 191 utf8_bom)
  string(REPLACE "${utf8_bom}" "" source "${source}")
  set(body [=[
    const auto maskSizes = bongo_cat::native_mask_sizes;
    bool masksChanged = false;
    if (_drawableClippingManager && maskSizes.drawable.width > 0 &&
        (_drawableClippingManager->GetClippingMaskBufferSize().X != maskSizes.drawable.width ||
         _drawableClippingManager->GetClippingMaskBufferSize().Y != maskSizes.drawable.height)) {
        _drawableClippingManager->SetClippingMaskBufferSize(
            (csmFloat32)maskSizes.drawable.width, (csmFloat32)maskSizes.drawable.height);
        masksChanged = true;
    }
    if (_offscreenClippingManager && maskSizes.offscreen.width > 0 &&
        (_offscreenClippingManager->GetClippingMaskBufferSize().X != maskSizes.offscreen.width ||
         _offscreenClippingManager->GetClippingMaskBufferSize().Y != maskSizes.offscreen.height)) {
        _offscreenClippingManager->SetClippingMaskBufferSize(
            (csmFloat32)maskSizes.offscreen.width, (csmFloat32)maskSizes.offscreen.height);
        masksChanged = true;
    }
]=])
  if(backend STREQUAL "Vulkan")
    string(APPEND body [=[
    if (masksChanged) {
        for (csmUint32 buffer = 0; buffer < s_bufferSetNum; ++buffer) {
            for (csmInt32 i = 0; i < _descriptorSets[buffer].GetSize(); ++i) {
                _descriptorSets[buffer][i].isDescriptorSetUpdated = false;
                _descriptorSets[buffer][i].isDescriptorSetMaskedUpdated = false;
                _descriptorSets[buffer][i].isDescriptorSetForMaskUpdated = false;
            }
            for (csmInt32 i = 0; i < _offscreenDescriptorSets[buffer].GetSize(); ++i) {
                _offscreenDescriptorSets[buffer][i].isDescriptorSetUpdated = false;
                _offscreenDescriptorSets[buffer][i].isDescriptorSetMaskedUpdated = false;
                _offscreenDescriptorSets[buffer][i].isDescriptorSetForMaskUpdated = false;
            }
        }
    }
]=])
  else()
    string(APPEND body "    (void)masksChanged;\n")
  endif()
  set(anchor "void CubismRenderer_${backend}::DoDrawModel()\n{")
  bongo_cat_replace_cubism_text(source "${anchor}" "${anchor}${body}"
    "${backend} automatic mask resolution")
  set(${variable} "#include \"cubism_native_masks.hpp\"\n${source}" PARENT_SCOPE)
endfunction()
