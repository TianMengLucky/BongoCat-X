#include "cubism_model.hpp"

extern BongoCatRhiBackend s_live2d_rhi_backend;
namespace bongo_cat {
#ifdef BONGO_CAT_HAS_CUBISM_VULKAN
Csm::Rendering::CubismRenderer *create_vulkan_renderer(Csm::csmUint32, Csm::csmUint32);
#endif
#ifdef BONGO_CAT_HAS_CUBISM_METAL
Csm::Rendering::CubismRenderer *create_metal_renderer(Csm::csmUint32, Csm::csmUint32);
#endif
Csm::Rendering::CubismRenderer *bongo_cat_cubism_create_renderer(
    Csm::csmUint32 width, Csm::csmUint32 height) {
#ifdef BONGO_CAT_HAS_CUBISM_VULKAN
    if (s_live2d_rhi_backend == BONGO_CAT_RHI_VULKAN)
        return create_vulkan_renderer(width, height);
#endif
#ifdef BONGO_CAT_HAS_CUBISM_METAL
    if (s_live2d_rhi_backend == BONGO_CAT_RHI_METAL)
        return create_metal_renderer(width, height);
#endif
    return Csm::Rendering::CubismRenderer::Create(width, height);
}
} // namespace bongo_cat
