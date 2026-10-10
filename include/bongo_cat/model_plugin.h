#ifndef BONGO_CAT_MODEL_PLUGIN_H
#define BONGO_CAT_MODEL_PLUGIN_H
#include "bongo_cat/model.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define BONGO_CAT_MODEL_PLUGIN_ABI 1u
#define BONGO_CAT_MODEL_PLUGIN_SYMBOL "bongo_cat_model_plugin_query"
/* Optional, additive extension: legacy plugin descriptor/host ABI stays v1.
   Called on the owning thread, after set_rhi_info and before load. A plugin
   returns ONLY requested features it actually enables. Missing = none. */
#define BONGO_CAT_PLUGIN_OPTIMIZE_SYMBOL "bongo_cat_model_plugin_optimize_v1"
#define BONGO_CAT_OPTIMIZE_PARALLEL_RECORDING (1u << 0)
#define BONGO_CAT_OPTIMIZE_BINDLESS (1u << 1)
#define BONGO_CAT_OPTIMIZE_ASYNC_COMPUTE (1u << 2) /* Reserved v1 bit; no longer requested. */
#define BONGO_CAT_OPTIMIZE_LARGE_ALLOCATION (1u << 3)
#define BONGO_CAT_OPTIMIZE_ALL 11u
/* Additive device-feature handoff. Old hosts omit it, so new plugins must
   default to no enabled optional device features. Never infer enablement
   from physical-device support alone. Called before optimize_v1. */
#define BONGO_CAT_PLUGIN_VULKAN_FEATURES_SYMBOL "bongo_cat_model_plugin_vulkan_features_v1"
#define BONGO_CAT_VULKAN_SAMPLED_IMAGE_DYNAMIC_INDEXING (1u << 0)
typedef void (*BongoCatModelPluginVulkanFeatures)(BongoCatModelRuntime *instance,
    uint32_t enabled_features);
typedef uint32_t (*BongoCatModelPluginOptimize)(BongoCatModelRuntime *instance,
    uint32_t requested, uint32_t graphics_backend);
uint32_t bongo_cat_model_runtime_optimizations(const BongoCatModelRuntime *runtime);
#define BONGO_CAT_PLUGIN_REQUIRES_CORE 1u
#define BONGO_CAT_PLUGIN_OPENGL (1u << 0)
#define BONGO_CAT_PLUGIN_VULKAN (1u << 1)
#define BONGO_CAT_PLUGIN_METAL (1u << 2)
/* Same compiler ABI within an architecture; size/version reject drift.
   Instances, jobs and GPU resources must be destroyed before unloading.
   All calls except resource worker callbacks run on the owning main thread. */
typedef struct BongoCatModelPluginOps {
    BongoCatModelRuntime * (*create)(const char *asset_root, BongoCatError *error);
    void (*destroy)(BongoCatModelRuntime *live2d);
    void (*set_rhi_info)(BongoCatModelRuntime *live2d, const BongoCatRhiDeviceInfo *info);
    BongoCatResult (*load)(BongoCatModelRuntime *live2d, const char *model_dir, const char *setting_file, bool preset, const BongoCatModelRuntimeRenderOptions *render_options, BongoCatModelRuntimeLoadProgress progress, void *userdata, BongoCatError *error);
    BongoCatResult (*load_ex)(BongoCatModelRuntime *live2d, const char *model_dir, const char *setting_file, bool preset, const BongoCatModelRuntimeRenderOptions *render_options, const BongoCatModelRuntimeTextureOptions *texture_options, BongoCatModelRuntimeLoadProgress progress, void *userdata, BongoCatError *error);
    bool (*ready)(const BongoCatModelRuntime *live2d);
    bool (*canvas_size)(const BongoCatModelRuntime *live2d, int *width, int *height);
    bool (*frame)(const BongoCatModelRuntime *live2d, BongoCatModelRuntimeFrame *frame);
    bool (*measure_frame)(BongoCatModelRuntime *live2d, BongoCatModelRuntimeFrame *required);
    void (*set_frame)(BongoCatModelRuntime *live2d, const BongoCatModelRuntimeFrame *frame);
    bool (*viewport)(const BongoCatModelRuntime *live2d, int *x, int *y, int *width, int *height);
    bool (*overlay_viewport)(const BongoCatModelRuntime *live2d, int *x, int *y, int *width, int *height);
    void (*resize)(BongoCatModelRuntime *live2d, int width, int height);
    void (*reshape)(BongoCatModelRuntime *live2d, int width, int height);
    bool (*try_reuse_texture_quality)(BongoCatModelRuntime *live2d, float quality_percent);
    bool (*texture_refresh_pending)(const BongoCatModelRuntime *live2d, bool active);
    bool (*texture_refresh_due)(const BongoCatModelRuntime *live2d, bool active, bool allow_start);
    bool (*texture_refresh_busy)(const BongoCatModelRuntime *live2d);
    void (*cancel_texture_refresh)(BongoCatModelRuntime *live2d);
    bool (*refresh_textures)(BongoCatModelRuntime *live2d, bool active, bool allow_start);
    bool (*update)(BongoCatModelRuntime *live2d, float delta_seconds);
    void (*draw)(BongoCatModelRuntime *live2d);
    bool (*draw_checked)(BongoCatModelRuntime *live2d);
    void (*set_mirror)(BongoCatModelRuntime *live2d, bool mirror);
    void (*set_vertical_flip)(BongoCatModelRuntime *live2d, bool flipped);
    void (*set_render_options)(BongoCatModelRuntime *live2d, const BongoCatModelRuntimeRenderOptions *options);
    void (*set_tight_frame)(BongoCatModelRuntime *live2d, bool tight);
    void (*set_tight_overlay_rect)(BongoCatModelRuntime *live2d, const float *rect);
    void (*set_dragging)(BongoCatModelRuntime *live2d, float x, float y);
    void (*set_centered_dragging)(BongoCatModelRuntime *live2d, float x, float y);
    void (*prepare_viewer_audit)(BongoCatModelRuntime *live2d);
    bool (*prepare_cover_capture)(BongoCatModelRuntime *live2d);
    bool (*set_parameter)(BongoCatModelRuntime *live2d, const char *id, float value);
    bool (*parameter)(BongoCatModelRuntime *live2d, const char *id, BongoCatParameterRange *range);
    bool (*start_motion)(BongoCatModelRuntime *live2d, const char *group, int index);
    bool (*restore_motion_state)(BongoCatModelRuntime *live2d, const char *group, int index);
    bool (*preview_motion)(BongoCatModelRuntime *live2d, const char *group, int index);
    bool (*restore_motion_preview)(BongoCatModelRuntime *live2d);
    bool (*commit_motion_preview)(BongoCatModelRuntime *live2d, const char *group, int index);
    bool (*motion_selected)(const BongoCatModelRuntime *live2d, const char *group, int index);
    bool (*motion_persistent)(const BongoCatModelRuntime *live2d, const char *group, int index);
    bool (*motion_visible)(const BongoCatModelRuntime *live2d, const char *group, int index);
    bool (*motion_same_toggle)(const BongoCatModelRuntime *live2d, const char *left_group, int left_index, const char *right_group, int right_index);
    bool (*set_expression)(BongoCatModelRuntime *live2d, int index);
    int (*expression)(const BongoCatModelRuntime *live2d);
    bool (*visual_state)(const BongoCatModelRuntime *live2d, BongoCatModelRuntimeVisualState *state);
} BongoCatModelPluginOps;
struct BongoCatModelPluginHost;
typedef struct BongoCatModelPlugin {
    uint32_t abi_version, struct_size, engine, flags, graphics_backends;
    const char *name;
    BongoCatModelPluginOps ops;
} BongoCatModelPlugin;
typedef const BongoCatModelPlugin *(*BongoCatModelPluginQuery)(
    uint32_t abi, const struct BongoCatModelPluginHost *host);
BongoCatModelEngine bongo_cat_model_engine(const BongoCatModelEntry *entry);
BongoCatModelEngine bongo_cat_model_runtime_engine(const BongoCatModelRuntime *runtime);
bool bongo_cat_model_runtime_rendering(const BongoCatModelRuntime *runtime);
#ifdef __cplusplus
}
#endif
#endif
