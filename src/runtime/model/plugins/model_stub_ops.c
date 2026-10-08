#include "bongo_cat/model_plugin.h"
BongoCatModelRuntime * stub_model_runtime_create(const char *asset_root, BongoCatError *error);
void stub_model_runtime_destroy(BongoCatModelRuntime *live2d);
void stub_model_runtime_set_rhi_info(BongoCatModelRuntime *live2d, const BongoCatRhiDeviceInfo *info);
BongoCatResult stub_model_runtime_load(BongoCatModelRuntime *live2d, const char *model_dir, const char *setting_file, bool preset, const BongoCatModelRuntimeRenderOptions *render_options, BongoCatModelRuntimeLoadProgress progress, void *userdata, BongoCatError *error);
BongoCatResult stub_model_runtime_load_ex(BongoCatModelRuntime *live2d, const char *model_dir, const char *setting_file, bool preset, const BongoCatModelRuntimeRenderOptions *render_options, const BongoCatModelRuntimeTextureOptions *texture_options, BongoCatModelRuntimeLoadProgress progress, void *userdata, BongoCatError *error);
bool stub_model_runtime_ready(const BongoCatModelRuntime *live2d);
bool stub_model_runtime_canvas_size(const BongoCatModelRuntime *live2d, int *width, int *height);
bool stub_model_runtime_frame(const BongoCatModelRuntime *live2d, BongoCatModelRuntimeFrame *frame);
bool stub_model_runtime_measure_frame(BongoCatModelRuntime *live2d, BongoCatModelRuntimeFrame *required);
void stub_model_runtime_set_frame(BongoCatModelRuntime *live2d, const BongoCatModelRuntimeFrame *frame);
bool stub_model_runtime_viewport(const BongoCatModelRuntime *live2d, int *x, int *y, int *width, int *height);
bool stub_model_runtime_overlay_viewport(const BongoCatModelRuntime *live2d, int *x, int *y, int *width, int *height);
void stub_model_runtime_resize(BongoCatModelRuntime *live2d, int width, int height);
void stub_model_runtime_reshape(BongoCatModelRuntime *live2d, int width, int height);
bool stub_model_runtime_try_reuse_texture_quality(BongoCatModelRuntime *live2d, float quality_percent);
bool stub_model_runtime_texture_refresh_pending(const BongoCatModelRuntime *live2d, bool active);
bool stub_model_runtime_texture_refresh_due(const BongoCatModelRuntime *live2d, bool active, bool allow_start);
bool stub_model_runtime_texture_refresh_busy(const BongoCatModelRuntime *live2d);
void stub_model_runtime_cancel_texture_refresh(BongoCatModelRuntime *live2d);
bool stub_model_runtime_refresh_textures(BongoCatModelRuntime *live2d, bool active, bool allow_start);
bool stub_model_runtime_update(BongoCatModelRuntime *live2d, float delta_seconds);
void stub_model_runtime_draw(BongoCatModelRuntime *live2d);
bool stub_model_runtime_draw_checked(BongoCatModelRuntime *live2d);
void stub_model_runtime_set_mirror(BongoCatModelRuntime *live2d, bool mirror);
void stub_model_runtime_set_vertical_flip(BongoCatModelRuntime *live2d, bool flipped);
void stub_model_runtime_set_render_options(BongoCatModelRuntime *live2d, const BongoCatModelRuntimeRenderOptions *options);
void stub_model_runtime_set_tight_frame(BongoCatModelRuntime *live2d, bool tight);
void stub_model_runtime_set_tight_overlay_rect(BongoCatModelRuntime *live2d, const float *rect);
void stub_model_runtime_set_dragging(BongoCatModelRuntime *live2d, float x, float y);
void stub_model_runtime_set_centered_dragging(BongoCatModelRuntime *live2d, float x, float y);
void stub_model_runtime_prepare_viewer_audit(BongoCatModelRuntime *live2d);
bool stub_model_runtime_prepare_cover_capture(BongoCatModelRuntime *live2d);
bool stub_model_runtime_set_parameter(BongoCatModelRuntime *live2d, const char *id, float value);
bool stub_model_runtime_parameter(BongoCatModelRuntime *live2d, const char *id, BongoCatParameterRange *range);
bool stub_model_runtime_start_motion(BongoCatModelRuntime *live2d, const char *group, int index);
bool stub_model_runtime_restore_motion_state(BongoCatModelRuntime *live2d, const char *group, int index);
bool stub_model_runtime_preview_motion(BongoCatModelRuntime *live2d, const char *group, int index);
bool stub_model_runtime_restore_motion_preview(BongoCatModelRuntime *live2d);
bool stub_model_runtime_commit_motion_preview(BongoCatModelRuntime *live2d, const char *group, int index);
bool stub_model_runtime_motion_selected(const BongoCatModelRuntime *live2d, const char *group, int index);
bool stub_model_runtime_motion_persistent(const BongoCatModelRuntime *live2d, const char *group, int index);
bool stub_model_runtime_motion_visible(const BongoCatModelRuntime *live2d, const char *group, int index);
bool stub_model_runtime_motion_same_toggle(const BongoCatModelRuntime *live2d, const char *left_group, int left_index, const char *right_group, int right_index);
bool stub_model_runtime_set_expression(BongoCatModelRuntime *live2d, int index);
int stub_model_runtime_expression(const BongoCatModelRuntime *live2d);
bool stub_model_runtime_visual_state(const BongoCatModelRuntime *live2d, BongoCatModelRuntimeVisualState *state);

const BongoCatModelPluginOps bongo_cat_model_stub_ops = {
    .create = stub_model_runtime_create,
    .destroy = stub_model_runtime_destroy,
    .set_rhi_info = stub_model_runtime_set_rhi_info,
    .load = stub_model_runtime_load,
    .load_ex = stub_model_runtime_load_ex,
    .ready = stub_model_runtime_ready,
    .canvas_size = stub_model_runtime_canvas_size,
    .frame = stub_model_runtime_frame,
    .measure_frame = stub_model_runtime_measure_frame,
    .set_frame = stub_model_runtime_set_frame,
    .viewport = stub_model_runtime_viewport,
    .overlay_viewport = stub_model_runtime_overlay_viewport,
    .resize = stub_model_runtime_resize,
    .reshape = stub_model_runtime_reshape,
    .try_reuse_texture_quality = stub_model_runtime_try_reuse_texture_quality,
    .texture_refresh_pending = stub_model_runtime_texture_refresh_pending,
    .texture_refresh_due = stub_model_runtime_texture_refresh_due,
    .texture_refresh_busy = stub_model_runtime_texture_refresh_busy,
    .cancel_texture_refresh = stub_model_runtime_cancel_texture_refresh,
    .refresh_textures = stub_model_runtime_refresh_textures,
    .update = stub_model_runtime_update,
    .draw = stub_model_runtime_draw,
    .draw_checked = stub_model_runtime_draw_checked,
    .set_mirror = stub_model_runtime_set_mirror,
    .set_vertical_flip = stub_model_runtime_set_vertical_flip,
    .set_render_options = stub_model_runtime_set_render_options,
    .set_tight_frame = stub_model_runtime_set_tight_frame,
    .set_tight_overlay_rect = stub_model_runtime_set_tight_overlay_rect,
    .set_dragging = stub_model_runtime_set_dragging,
    .set_centered_dragging = stub_model_runtime_set_centered_dragging,
    .prepare_viewer_audit = stub_model_runtime_prepare_viewer_audit,
    .prepare_cover_capture = stub_model_runtime_prepare_cover_capture,
    .set_parameter = stub_model_runtime_set_parameter,
    .parameter = stub_model_runtime_parameter,
    .start_motion = stub_model_runtime_start_motion,
    .restore_motion_state = stub_model_runtime_restore_motion_state,
    .preview_motion = stub_model_runtime_preview_motion,
    .restore_motion_preview = stub_model_runtime_restore_motion_preview,
    .commit_motion_preview = stub_model_runtime_commit_motion_preview,
    .motion_selected = stub_model_runtime_motion_selected,
    .motion_persistent = stub_model_runtime_motion_persistent,
    .motion_visible = stub_model_runtime_motion_visible,
    .motion_same_toggle = stub_model_runtime_motion_same_toggle,
    .set_expression = stub_model_runtime_set_expression,
    .expression = stub_model_runtime_expression,
    .visual_state = stub_model_runtime_visual_state,
};
