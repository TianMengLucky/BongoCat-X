#include "bongo_cat/model_plugin_host.h"
#include <cstdio>
#ifdef BONGO_CAT_HAS_CUBISM_VULKAN
namespace bongo_cat {
uint32_t configure_vulkan_optimization(BongoCatModelRuntime *, uint32_t, uint32_t);
}
#endif
#ifdef _WIN32
#include <windows.h>
#endif
namespace {
const BongoCatModelPluginHost *services;
}
extern "C" const BongoCatModelPluginHost *bongo_cat_model_plugin_host(void) {
    return services;
}
/* FILE handles stay within this DLL's CRT. */
extern "C" FILE *bongo_cat_file_open(const char *path, const char *mode) {
#ifdef _WIN32
    wchar_t wide_path[BONGO_CAT_PATH_CAP], wide_mode[32];
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1,
            wide_path, BONGO_CAT_PATH_CAP) ||
        !MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, mode, -1, wide_mode, 32)) return nullptr;
    FILE *file = nullptr;
    return _wfopen_s(&file, wide_path, wide_mode) == 0 ? file : nullptr;
#else
    return std::fopen(path, mode);
#endif
}
#ifdef _WIN32
#define PLUGIN_EXPORT __declspec(dllexport)
#else
#define PLUGIN_EXPORT __attribute__((visibility("default")))
#endif
extern "C" PLUGIN_EXPORT uint32_t bongo_cat_model_plugin_optimize_v1(
    BongoCatModelRuntime *runtime, uint32_t requested, uint32_t backend) {
#ifdef BONGO_CAT_HAS_CUBISM_VULKAN
    try { return bongo_cat::configure_vulkan_optimization(runtime, requested, backend); }
    catch (...) { return 0; }
#else
    (void)runtime; (void)requested; (void)backend;
    return 0;
#endif
}

extern "C" PLUGIN_EXPORT const BongoCatModelPlugin *bongo_cat_model_plugin_query(
    uint32_t abi, const BongoCatModelPluginHost *host) {
    if (abi != BONGO_CAT_MODEL_PLUGIN_ABI || !host ||
        host->abi_version != abi || host->struct_size != sizeof(*host)) return nullptr;
    services = host;
    static const BongoCatModelPlugin plugin = {
        BONGO_CAT_MODEL_PLUGIN_ABI, sizeof(BongoCatModelPlugin),
        BONGO_CAT_MODEL_ENGINE_LIVE2D,
        BONGO_CAT_PLUGIN_REQUIRES_CORE,
        BONGO_CAT_PLUGIN_OPENGL
#ifdef BONGO_CAT_HAS_CUBISM_VULKAN
            | BONGO_CAT_PLUGIN_VULKAN
#endif
#ifdef BONGO_CAT_HAS_CUBISM_METAL
            | BONGO_CAT_PLUGIN_METAL
#endif
        , "Live2D Cubism",
        {
            bongo_cat_model_runtime_create,
            bongo_cat_model_runtime_destroy,
            bongo_cat_model_runtime_set_rhi_info,
            bongo_cat_model_runtime_load,
            bongo_cat_model_runtime_load_ex,
            bongo_cat_model_runtime_ready,
            bongo_cat_model_runtime_canvas_size,
            bongo_cat_model_runtime_frame,
            bongo_cat_model_runtime_measure_frame,
            bongo_cat_model_runtime_set_frame,
            bongo_cat_model_runtime_viewport,
            bongo_cat_model_runtime_overlay_viewport,
            bongo_cat_model_runtime_resize,
            bongo_cat_model_runtime_reshape,
            bongo_cat_model_runtime_try_reuse_texture_quality,
            bongo_cat_model_runtime_texture_refresh_pending,
            bongo_cat_model_runtime_texture_refresh_due,
            bongo_cat_model_runtime_texture_refresh_busy,
            bongo_cat_model_runtime_cancel_texture_refresh,
            bongo_cat_model_runtime_refresh_textures,
            bongo_cat_model_runtime_update,
            bongo_cat_model_runtime_draw,
            bongo_cat_model_runtime_draw_checked,
            bongo_cat_model_runtime_set_mirror,
            bongo_cat_model_runtime_set_vertical_flip,
            bongo_cat_model_runtime_set_render_options,
            bongo_cat_model_runtime_set_tight_frame,
            bongo_cat_model_runtime_set_tight_overlay_rect,
            bongo_cat_model_runtime_set_dragging,
            bongo_cat_model_runtime_set_centered_dragging,
            bongo_cat_model_runtime_prepare_viewer_audit,
            bongo_cat_model_runtime_prepare_cover_capture,
            bongo_cat_model_runtime_set_parameter,
            bongo_cat_model_runtime_parameter,
            bongo_cat_model_runtime_start_motion,
            bongo_cat_model_runtime_restore_motion_state,
            bongo_cat_model_runtime_preview_motion,
            bongo_cat_model_runtime_restore_motion_preview,
            bongo_cat_model_runtime_commit_motion_preview,
            bongo_cat_model_runtime_motion_selected,
            bongo_cat_model_runtime_motion_persistent,
            bongo_cat_model_runtime_motion_visible,
            bongo_cat_model_runtime_motion_same_toggle,
            bongo_cat_model_runtime_set_expression,
            bongo_cat_model_runtime_expression,
            bongo_cat_model_runtime_visual_state,
        }
    };
    return &plugin;
}
