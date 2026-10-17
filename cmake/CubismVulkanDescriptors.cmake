# Drawable masking is fixed by the model: ordinary/masked drawing can share
# one set, and only drawables referenced by a mask need a mask-generation set.
function(bongo_cat_vulkan_compact_descriptors variable)
  set(source "${${variable}}")
  bongo_cat_vertex_replace(source
    [=[    csmInt32 descriptorSetCountPerBuffer = drawableCount * drawModeCount;]=]
    [=[    csmVector<csmBool> maskSources;
    maskSources.Resize(drawableCount, false);
    csmInt32 descriptorSetCountPerBuffer = drawableCount;
    // Offscreen masks also draw their source meshes through drawable descriptors.
    for (csmInt32 kind = 0; kind < 2; ++kind)
    {
        const csmInt32 objectCount = kind == 0 ? drawableCount : GetModel()->GetOffscreenCount();
        const csmInt32* maskCounts = kind == 0 ? GetModel()->GetDrawableMaskCounts() :
            GetModel()->GetOffscreenMaskCounts();
        const csmInt32** masks = kind == 0 ? GetModel()->GetDrawableMasks() :
            GetModel()->GetOffscreenMasks();
        for (csmInt32 object = 0; object < objectCount; ++object)
        {
            for (csmInt32 mask = 0; mask < maskCounts[object]; ++mask)
            {
                const csmInt32 source = masks[object][mask];
                if (source < 0 || source >= drawableCount)
                    throw std::runtime_error("Invalid Live2D Vulkan mask source");
                if (!maskSources[source])
                {
                    maskSources[source] = true;
                    ++descriptorSetCountPerBuffer;
                }
            }
        }
    }]=])
  bongo_cat_vertex_replace(source [=[        for (csmInt32 drawAssign = 0; drawAssign < drawableCount; drawAssign++)
        {
            _descriptorSets[buffer][drawAssign].descriptorSet = descriptorSets[drawAssign * 3];
            _descriptorSets[buffer][drawAssign].descriptorSetMasked = descriptorSets[drawAssign * 3 + 1];
            _descriptorSets[buffer][drawAssign].descriptorSetForMask = descriptorSets[drawAssign * 3 + 2];
        }]=] [=[        csmInt32 nextDescriptor = 0;
        for (csmInt32 drawAssign = 0; drawAssign < drawableCount; drawAssign++)
        {
            Descriptor& descriptor = _descriptorSets[buffer][drawAssign];
            // A drawable's clipping relationship never changes during animation.
            descriptor.descriptorSet = descriptorSets[nextDescriptor++];
            descriptor.descriptorSetMasked = descriptor.descriptorSet;
            if (maskSources[drawAssign])
                descriptor.descriptorSetForMask = descriptorSets[nextDescriptor++];
        }]=])
  set(${variable} "${source}" PARENT_SCOPE)
endfunction()
