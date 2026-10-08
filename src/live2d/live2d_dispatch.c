#include "model_plugin_internal.h"

bool bongo_cat_model_runtime_ready(const BongoCatModelRuntime *live2d) {
    return live2d && live2d->ops->ready ?
        live2d->ops->ready(live2d->instance) : false;
}
bool bongo_cat_model_runtime_canvas_size(const BongoCatModelRuntime *live2d, int *width, int *height) {
    return live2d && live2d->ops->canvas_size ?
        live2d->ops->canvas_size(live2d->instance, width, height) : false;
}
bool bongo_cat_model_runtime_frame(const BongoCatModelRuntime *live2d, BongoCatModelRuntimeFrame *frame) {
    return live2d && live2d->ops->frame ?
        live2d->ops->frame(live2d->instance, frame) : false;
}
bool bongo_cat_model_runtime_measure_frame(BongoCatModelRuntime *live2d, BongoCatModelRuntimeFrame *required) {
    return live2d && live2d->ops->measure_frame ?
        live2d->ops->measure_frame(live2d->instance, required) : false;
}
void bongo_cat_model_runtime_set_frame(BongoCatModelRuntime *live2d, const BongoCatModelRuntimeFrame *frame) {
    if (live2d && live2d->ops->set_frame) live2d->ops->set_frame(live2d->instance, frame);
}
bool bongo_cat_model_runtime_viewport(const BongoCatModelRuntime *live2d, int *x, int *y, int *width, int *height) {
    return live2d && live2d->ops->viewport ?
        live2d->ops->viewport(live2d->instance, x, y, width, height) : false;
}
bool bongo_cat_model_runtime_overlay_viewport(const BongoCatModelRuntime *live2d, int *x, int *y, int *width, int *height) {
    return live2d && live2d->ops->overlay_viewport ?
        live2d->ops->overlay_viewport(live2d->instance, x, y, width, height) : false;
}
void bongo_cat_model_runtime_resize(BongoCatModelRuntime *live2d, int width, int height) {
    if (live2d) { live2d->width = width; live2d->height = height; }
    if (live2d && live2d->ops->resize) live2d->ops->resize(live2d->instance, width, height);
}
void bongo_cat_model_runtime_reshape(BongoCatModelRuntime *live2d, int width, int height) {
    if (live2d) { live2d->width = width; live2d->height = height; }
    if (live2d && live2d->ops->reshape) live2d->ops->reshape(live2d->instance, width, height);
}
bool bongo_cat_model_runtime_try_reuse_texture_quality(BongoCatModelRuntime *live2d, float quality_percent) {
    return live2d && live2d->ops->try_reuse_texture_quality ?
        live2d->ops->try_reuse_texture_quality(live2d->instance, quality_percent) : false;
}
bool bongo_cat_model_runtime_texture_refresh_pending(const BongoCatModelRuntime *live2d, bool active) {
    return live2d && live2d->ops->texture_refresh_pending ?
        live2d->ops->texture_refresh_pending(live2d->instance, active) : false;
}
bool bongo_cat_model_runtime_texture_refresh_due(const BongoCatModelRuntime *live2d, bool active, bool allow_start) {
    return live2d && live2d->ops->texture_refresh_due ?
        live2d->ops->texture_refresh_due(live2d->instance, active, allow_start) : false;
}
bool bongo_cat_model_runtime_texture_refresh_busy(const BongoCatModelRuntime *live2d) {
    return live2d && live2d->ops->texture_refresh_busy ?
        live2d->ops->texture_refresh_busy(live2d->instance) : false;
}
void bongo_cat_model_runtime_cancel_texture_refresh(BongoCatModelRuntime *live2d) {
    if (live2d && live2d->ops->cancel_texture_refresh) live2d->ops->cancel_texture_refresh(live2d->instance);
}
bool bongo_cat_model_runtime_refresh_textures(BongoCatModelRuntime *live2d, bool active, bool allow_start) {
    return live2d && live2d->ops->refresh_textures ?
        live2d->ops->refresh_textures(live2d->instance, active, allow_start) : false;
}
bool bongo_cat_model_runtime_update(BongoCatModelRuntime *live2d, float delta_seconds) {
    return live2d && live2d->ops->update ?
        live2d->ops->update(live2d->instance, delta_seconds) : false;
}
void bongo_cat_model_runtime_draw(BongoCatModelRuntime *live2d) {
    if (live2d && live2d->ops->draw) live2d->ops->draw(live2d->instance);
}
bool bongo_cat_model_runtime_draw_checked(BongoCatModelRuntime *live2d) {
    return live2d && live2d->ops->draw_checked ?
        live2d->ops->draw_checked(live2d->instance) : false;
}
void bongo_cat_model_runtime_set_mirror(BongoCatModelRuntime *live2d, bool mirror) {
    if (live2d && live2d->ops->set_mirror) live2d->ops->set_mirror(live2d->instance, mirror);
}
void bongo_cat_model_runtime_set_vertical_flip(BongoCatModelRuntime *live2d, bool flipped) {
    if (live2d && live2d->ops->set_vertical_flip) live2d->ops->set_vertical_flip(live2d->instance, flipped);
}
void bongo_cat_model_runtime_set_render_options(BongoCatModelRuntime *live2d, const BongoCatModelRuntimeRenderOptions *options) {
    if (live2d && live2d->ops->set_render_options) live2d->ops->set_render_options(live2d->instance, options);
}
void bongo_cat_model_runtime_set_tight_frame(BongoCatModelRuntime *live2d, bool tight) {
    if (live2d && live2d->ops->set_tight_frame) live2d->ops->set_tight_frame(live2d->instance, tight);
}
void bongo_cat_model_runtime_set_tight_overlay_rect(BongoCatModelRuntime *live2d, const float *rect) {
    if (live2d && live2d->ops->set_tight_overlay_rect) live2d->ops->set_tight_overlay_rect(live2d->instance, rect);
}
void bongo_cat_model_runtime_set_dragging(BongoCatModelRuntime *live2d, float x, float y) {
    if (live2d && live2d->ops->set_dragging) live2d->ops->set_dragging(live2d->instance, x, y);
}
void bongo_cat_model_runtime_set_centered_dragging(BongoCatModelRuntime *live2d, float x, float y) {
    if (live2d && live2d->ops->set_centered_dragging) live2d->ops->set_centered_dragging(live2d->instance, x, y);
}
void bongo_cat_model_runtime_prepare_viewer_audit(BongoCatModelRuntime *live2d) {
    if (live2d && live2d->ops->prepare_viewer_audit) live2d->ops->prepare_viewer_audit(live2d->instance);
}
bool bongo_cat_model_runtime_prepare_cover_capture(BongoCatModelRuntime *live2d) {
    return live2d && live2d->ops->prepare_cover_capture ?
        live2d->ops->prepare_cover_capture(live2d->instance) : false;
}
bool bongo_cat_model_runtime_set_parameter(BongoCatModelRuntime *live2d, const char *id, float value) {
    return live2d && live2d->ops->set_parameter ?
        live2d->ops->set_parameter(live2d->instance, id, value) : false;
}
bool bongo_cat_model_runtime_parameter(BongoCatModelRuntime *live2d, const char *id, BongoCatParameterRange *range) {
    return live2d && live2d->ops->parameter ?
        live2d->ops->parameter(live2d->instance, id, range) : false;
}
bool bongo_cat_model_runtime_start_motion(BongoCatModelRuntime *live2d, const char *group, int index) {
    return live2d && live2d->ops->start_motion ?
        live2d->ops->start_motion(live2d->instance, group, index) : false;
}
bool bongo_cat_model_runtime_restore_motion_state(BongoCatModelRuntime *live2d, const char *group, int index) {
    return live2d && live2d->ops->restore_motion_state ?
        live2d->ops->restore_motion_state(live2d->instance, group, index) : false;
}
bool bongo_cat_model_runtime_preview_motion(BongoCatModelRuntime *live2d, const char *group, int index) {
    return live2d && live2d->ops->preview_motion ?
        live2d->ops->preview_motion(live2d->instance, group, index) : false;
}
bool bongo_cat_model_runtime_restore_motion_preview(BongoCatModelRuntime *live2d) {
    return live2d && live2d->ops->restore_motion_preview ?
        live2d->ops->restore_motion_preview(live2d->instance) : false;
}
bool bongo_cat_model_runtime_commit_motion_preview(BongoCatModelRuntime *live2d, const char *group, int index) {
    return live2d && live2d->ops->commit_motion_preview ?
        live2d->ops->commit_motion_preview(live2d->instance, group, index) : false;
}
bool bongo_cat_model_runtime_motion_selected(const BongoCatModelRuntime *live2d, const char *group, int index) {
    return live2d && live2d->ops->motion_selected ?
        live2d->ops->motion_selected(live2d->instance, group, index) : false;
}
bool bongo_cat_model_runtime_motion_persistent(const BongoCatModelRuntime *live2d, const char *group, int index) {
    return live2d && live2d->ops->motion_persistent ?
        live2d->ops->motion_persistent(live2d->instance, group, index) : false;
}
bool bongo_cat_model_runtime_motion_visible(const BongoCatModelRuntime *live2d, const char *group, int index) {
    return live2d && live2d->ops->motion_visible ?
        live2d->ops->motion_visible(live2d->instance, group, index) : false;
}
bool bongo_cat_model_runtime_motion_same_toggle(const BongoCatModelRuntime *live2d, const char *left_group, int left_index, const char *right_group, int right_index) {
    return live2d && live2d->ops->motion_same_toggle ?
        live2d->ops->motion_same_toggle(live2d->instance, left_group, left_index, right_group, right_index) : false;
}
bool bongo_cat_model_runtime_set_expression(BongoCatModelRuntime *live2d, int index) {
    return live2d && live2d->ops->set_expression ?
        live2d->ops->set_expression(live2d->instance, index) : false;
}
int bongo_cat_model_runtime_expression(const BongoCatModelRuntime *live2d) {
    return live2d && live2d->ops->expression ?
        live2d->ops->expression(live2d->instance) : -1;
}
bool bongo_cat_model_runtime_visual_state(const BongoCatModelRuntime *live2d, BongoCatModelRuntimeVisualState *state) {
    return live2d && live2d->ops->visual_state ?
        live2d->ops->visual_state(live2d->instance, state) : false;
}
