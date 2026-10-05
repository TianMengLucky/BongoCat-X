/* Runtime selection between the Cubism bridge and the diagnostic stub.
   In the runtime-Core configuration neither implementation owns the plain
   ABI names: the bridge is compiled as bridge_live2d_*, the stub as
   stub_live2d_*, and this file owns the ABI. The Core DLL (user-supplied at
   runtime) is only touched through the bridge; create routes every handle
   to the backend that produced it, so calls on a stub fallback never enter
   bridge code (and vice versa). The runtime owns a single Live2D instance,
   which is what the process-wide marker tracks.

   The prototypes and forwarders are generated from model.h; regenerate
   (build-tests/gen_dispatch.py) whenever the ABI changes. */
#include "bongo_cat/model.h"
#include "bongo_cat/platform.h"

/* Prefixed implementations of the same signatures. */
BongoCatLive2D * bridge_live2d_create(const char *asset_root, BongoCatError *error);
void bridge_live2d_destroy(BongoCatLive2D *live2d);
void bridge_live2d_set_rhi_info(BongoCatLive2D *live2d, const BongoCatRhiDeviceInfo *info);
BongoCatResult bridge_live2d_load(BongoCatLive2D *live2d, const char *model_dir, const char *setting_file, bool preset, const BongoCatLive2DRenderOptions *render_options, BongoCatLive2DLoadProgress progress, void *userdata, BongoCatError *error);
BongoCatResult bridge_live2d_load_ex(BongoCatLive2D *live2d, const char *model_dir, const char *setting_file, bool preset, const BongoCatLive2DRenderOptions *render_options, const BongoCatLive2DTextureOptions *texture_options, BongoCatLive2DLoadProgress progress, void *userdata, BongoCatError *error);
bool bridge_live2d_ready(const BongoCatLive2D *live2d);
bool bridge_live2d_canvas_size(const BongoCatLive2D *live2d, int *width, int *height);
bool bridge_live2d_frame(const BongoCatLive2D *live2d, BongoCatLive2DFrame *frame);
bool bridge_live2d_measure_frame(BongoCatLive2D *live2d, BongoCatLive2DFrame *required);
void bridge_live2d_set_frame(BongoCatLive2D *live2d, const BongoCatLive2DFrame *frame);
bool bridge_live2d_viewport(const BongoCatLive2D *live2d, int *x, int *y, int *width, int *height);
bool bridge_live2d_overlay_viewport(const BongoCatLive2D *live2d, int *x, int *y, int *width, int *height);
void bridge_live2d_resize(BongoCatLive2D *live2d, int width, int height);
void bridge_live2d_reshape(BongoCatLive2D *live2d, int width, int height);
bool bridge_live2d_try_reuse_texture_quality(BongoCatLive2D *live2d, float quality_percent);
bool bridge_live2d_texture_refresh_pending(const BongoCatLive2D *live2d, bool active);
bool bridge_live2d_texture_refresh_due(const BongoCatLive2D *live2d, bool active, bool allow_start);
bool bridge_live2d_texture_refresh_busy(const BongoCatLive2D *live2d);
void bridge_live2d_cancel_texture_refresh(BongoCatLive2D *live2d);
bool bridge_live2d_refresh_textures(BongoCatLive2D *live2d, bool active, bool allow_start);
bool bridge_live2d_update(BongoCatLive2D *live2d, float delta_seconds);
void bridge_live2d_draw(BongoCatLive2D *live2d);
void bridge_live2d_set_mirror(BongoCatLive2D *live2d, bool mirror);
void bridge_live2d_set_vertical_flip(BongoCatLive2D *live2d, bool flipped);
void bridge_live2d_set_render_options(BongoCatLive2D *live2d, const BongoCatLive2DRenderOptions *options);
void bridge_live2d_set_tight_frame(BongoCatLive2D *live2d, bool tight);
void bridge_live2d_set_tight_overlay_rect(BongoCatLive2D *live2d, const float *rect);
void bridge_live2d_set_dragging(BongoCatLive2D *live2d, float x, float y);
void bridge_live2d_set_centered_dragging(BongoCatLive2D *live2d, float x, float y);
void bridge_live2d_prepare_viewer_audit(BongoCatLive2D *live2d);
bool bridge_live2d_prepare_cover_capture(BongoCatLive2D *live2d);
bool bridge_live2d_set_parameter(BongoCatLive2D *live2d, const char *id, float value);
bool bridge_live2d_parameter(BongoCatLive2D *live2d, const char *id, BongoCatParameterRange *range);
bool bridge_live2d_start_motion(BongoCatLive2D *live2d, const char *group, int index);
bool bridge_live2d_restore_motion_state(BongoCatLive2D *live2d, const char *group, int index);
bool bridge_live2d_preview_motion(BongoCatLive2D *live2d, const char *group, int index);
bool bridge_live2d_restore_motion_preview(BongoCatLive2D *live2d);
bool bridge_live2d_commit_motion_preview(BongoCatLive2D *live2d, const char *group, int index);
bool bridge_live2d_motion_selected(const BongoCatLive2D *live2d, const char *group, int index);
bool bridge_live2d_motion_persistent(const BongoCatLive2D *live2d, const char *group, int index);
bool bridge_live2d_motion_visible(const BongoCatLive2D *live2d, const char *group, int index);
bool bridge_live2d_motion_same_toggle(const BongoCatLive2D *live2d, const char *left_group, int left_index, const char *right_group, int right_index);
bool bridge_live2d_set_expression(BongoCatLive2D *live2d, int index);
int bridge_live2d_expression(const BongoCatLive2D *live2d);
bool bridge_live2d_visual_state(const BongoCatLive2D *live2d, BongoCatLive2DVisualState *state);
BongoCatLive2D * stub_live2d_create(const char *asset_root, BongoCatError *error);
void stub_live2d_destroy(BongoCatLive2D *live2d);
void stub_live2d_set_rhi_info(BongoCatLive2D *live2d, const BongoCatRhiDeviceInfo *info);
BongoCatResult stub_live2d_load(BongoCatLive2D *live2d, const char *model_dir, const char *setting_file, bool preset, const BongoCatLive2DRenderOptions *render_options, BongoCatLive2DLoadProgress progress, void *userdata, BongoCatError *error);
BongoCatResult stub_live2d_load_ex(BongoCatLive2D *live2d, const char *model_dir, const char *setting_file, bool preset, const BongoCatLive2DRenderOptions *render_options, const BongoCatLive2DTextureOptions *texture_options, BongoCatLive2DLoadProgress progress, void *userdata, BongoCatError *error);
bool stub_live2d_ready(const BongoCatLive2D *live2d);
bool stub_live2d_canvas_size(const BongoCatLive2D *live2d, int *width, int *height);
bool stub_live2d_frame(const BongoCatLive2D *live2d, BongoCatLive2DFrame *frame);
bool stub_live2d_measure_frame(BongoCatLive2D *live2d, BongoCatLive2DFrame *required);
void stub_live2d_set_frame(BongoCatLive2D *live2d, const BongoCatLive2DFrame *frame);
bool stub_live2d_viewport(const BongoCatLive2D *live2d, int *x, int *y, int *width, int *height);
bool stub_live2d_overlay_viewport(const BongoCatLive2D *live2d, int *x, int *y, int *width, int *height);
void stub_live2d_resize(BongoCatLive2D *live2d, int width, int height);
void stub_live2d_reshape(BongoCatLive2D *live2d, int width, int height);
bool stub_live2d_try_reuse_texture_quality(BongoCatLive2D *live2d, float quality_percent);
bool stub_live2d_texture_refresh_pending(const BongoCatLive2D *live2d, bool active);
bool stub_live2d_texture_refresh_due(const BongoCatLive2D *live2d, bool active, bool allow_start);
bool stub_live2d_texture_refresh_busy(const BongoCatLive2D *live2d);
void stub_live2d_cancel_texture_refresh(BongoCatLive2D *live2d);
bool stub_live2d_refresh_textures(BongoCatLive2D *live2d, bool active, bool allow_start);
bool stub_live2d_update(BongoCatLive2D *live2d, float delta_seconds);
void stub_live2d_draw(BongoCatLive2D *live2d);
void stub_live2d_set_mirror(BongoCatLive2D *live2d, bool mirror);
void stub_live2d_set_vertical_flip(BongoCatLive2D *live2d, bool flipped);
void stub_live2d_set_render_options(BongoCatLive2D *live2d, const BongoCatLive2DRenderOptions *options);
void stub_live2d_set_tight_frame(BongoCatLive2D *live2d, bool tight);
void stub_live2d_set_tight_overlay_rect(BongoCatLive2D *live2d, const float *rect);
void stub_live2d_set_dragging(BongoCatLive2D *live2d, float x, float y);
void stub_live2d_set_centered_dragging(BongoCatLive2D *live2d, float x, float y);
void stub_live2d_prepare_viewer_audit(BongoCatLive2D *live2d);
bool stub_live2d_prepare_cover_capture(BongoCatLive2D *live2d);
bool stub_live2d_set_parameter(BongoCatLive2D *live2d, const char *id, float value);
bool stub_live2d_parameter(BongoCatLive2D *live2d, const char *id, BongoCatParameterRange *range);
bool stub_live2d_start_motion(BongoCatLive2D *live2d, const char *group, int index);
bool stub_live2d_restore_motion_state(BongoCatLive2D *live2d, const char *group, int index);
bool stub_live2d_preview_motion(BongoCatLive2D *live2d, const char *group, int index);
bool stub_live2d_restore_motion_preview(BongoCatLive2D *live2d);
bool stub_live2d_commit_motion_preview(BongoCatLive2D *live2d, const char *group, int index);
bool stub_live2d_motion_selected(const BongoCatLive2D *live2d, const char *group, int index);
bool stub_live2d_motion_persistent(const BongoCatLive2D *live2d, const char *group, int index);
bool stub_live2d_motion_visible(const BongoCatLive2D *live2d, const char *group, int index);
bool stub_live2d_motion_same_toggle(const BongoCatLive2D *live2d, const char *left_group, int left_index, const char *right_group, int right_index);
bool stub_live2d_set_expression(BongoCatLive2D *live2d, int index);
int stub_live2d_expression(const BongoCatLive2D *live2d);
bool stub_live2d_visual_state(const BongoCatLive2D *live2d, BongoCatLive2DVisualState *state);

/* Backend that produced the live instance; set on every create. */
static bool bridge_instance;

BongoCatLive2D *bongo_cat_live2d_create(const char *asset_root,
    BongoCatError *error) {
    if (bongo_cat_platform_live2d_core_available()) {
        BongoCatLive2D *live2d = bridge_live2d_create(asset_root, error);
        if (live2d) {
            bridge_instance = true;
            return live2d;
        }
        /* The Core DLL loaded but the backend failed to start; fall back to
           the stub so the pet still runs, without the rendering. */
    }
    bridge_instance = false;
    return stub_live2d_create(asset_root, error);
}

void bongo_cat_live2d_destroy(BongoCatLive2D *live2d) {
    if (bridge_instance) bridge_live2d_destroy(live2d);
    else stub_live2d_destroy(live2d);
}

BongoCatResult bongo_cat_live2d_load(BongoCatLive2D *live2d, const char *model_dir, const char *setting_file, bool preset, const BongoCatLive2DRenderOptions *render_options, BongoCatLive2DLoadProgress progress, void *userdata, BongoCatError *error) {
    if (!live2d) return BONGO_CAT_ERROR_ARGUMENT;
    if (bridge_instance) return bridge_live2d_load(live2d, model_dir, setting_file, preset, render_options, progress, userdata, error);
    return stub_live2d_load(live2d, model_dir, setting_file, preset, render_options, progress, userdata, error);
}

BongoCatResult bongo_cat_live2d_load_ex(BongoCatLive2D *live2d, const char *model_dir, const char *setting_file, bool preset, const BongoCatLive2DRenderOptions *render_options, const BongoCatLive2DTextureOptions *texture_options, BongoCatLive2DLoadProgress progress, void *userdata, BongoCatError *error) {
    if (!live2d) return BONGO_CAT_ERROR_ARGUMENT;
    if (bridge_instance) return bridge_live2d_load_ex(live2d, model_dir, setting_file, preset, render_options, texture_options, progress, userdata, error);
    return stub_live2d_load_ex(live2d, model_dir, setting_file, preset, render_options, texture_options, progress, userdata, error);
}

bool bongo_cat_live2d_ready(const BongoCatLive2D *live2d) {
    if (!live2d) return false;
    if (bridge_instance) return bridge_live2d_ready(live2d);
    return stub_live2d_ready(live2d);
}

bool bongo_cat_live2d_canvas_size(const BongoCatLive2D *live2d, int *width, int *height) {
    if (!live2d) return false;
    if (bridge_instance) return bridge_live2d_canvas_size(live2d, width, height);
    return stub_live2d_canvas_size(live2d, width, height);
}

bool bongo_cat_live2d_frame(const BongoCatLive2D *live2d, BongoCatLive2DFrame *frame) {
    if (!live2d) return false;
    if (bridge_instance) return bridge_live2d_frame(live2d, frame);
    return stub_live2d_frame(live2d, frame);
}

bool bongo_cat_live2d_measure_frame(BongoCatLive2D *live2d, BongoCatLive2DFrame *required) {
    if (!live2d) return false;
    if (bridge_instance) return bridge_live2d_measure_frame(live2d, required);
    return stub_live2d_measure_frame(live2d, required);
}

void bongo_cat_live2d_set_frame(BongoCatLive2D *live2d, const BongoCatLive2DFrame *frame) {
    if (!live2d) return;
    if (bridge_instance) bridge_live2d_set_frame(live2d, frame);
    else stub_live2d_set_frame(live2d, frame);
}

bool bongo_cat_live2d_viewport(const BongoCatLive2D *live2d, int *x, int *y, int *width, int *height) {
    if (!live2d) return false;
    if (bridge_instance) return bridge_live2d_viewport(live2d, x, y, width, height);
    return stub_live2d_viewport(live2d, x, y, width, height);
}

bool bongo_cat_live2d_overlay_viewport(const BongoCatLive2D *live2d, int *x, int *y, int *width, int *height) {
    if (bridge_instance) return bridge_live2d_overlay_viewport(live2d, x, y, width, height);
    return stub_live2d_overlay_viewport(live2d, x, y, width, height);
}

void bongo_cat_live2d_resize(BongoCatLive2D *live2d, int width, int height) {
    if (!live2d) return;
    if (bridge_instance) bridge_live2d_resize(live2d, width, height);
    else stub_live2d_resize(live2d, width, height);
}

void bongo_cat_live2d_reshape(BongoCatLive2D *live2d, int width, int height) {
    if (!live2d) return;
    if (bridge_instance) bridge_live2d_reshape(live2d, width, height);
    else stub_live2d_reshape(live2d, width, height);
}

bool bongo_cat_live2d_try_reuse_texture_quality(BongoCatLive2D *live2d, float quality_percent) {
    if (!live2d) return false;
    if (bridge_instance) return bridge_live2d_try_reuse_texture_quality(live2d, quality_percent);
    return stub_live2d_try_reuse_texture_quality(live2d, quality_percent);
}

bool bongo_cat_live2d_texture_refresh_pending(const BongoCatLive2D *live2d, bool active) {
    if (!live2d) return false;
    if (bridge_instance) return bridge_live2d_texture_refresh_pending(live2d, active);
    return stub_live2d_texture_refresh_pending(live2d, active);
}

bool bongo_cat_live2d_texture_refresh_due(const BongoCatLive2D *live2d, bool active, bool allow_start) {
    if (!live2d) return false;
    if (bridge_instance) return bridge_live2d_texture_refresh_due(live2d, active, allow_start);
    return stub_live2d_texture_refresh_due(live2d, active, allow_start);
}

bool bongo_cat_live2d_texture_refresh_busy(const BongoCatLive2D *live2d) {
    if (!live2d) return false;
    if (bridge_instance) return bridge_live2d_texture_refresh_busy(live2d);
    return stub_live2d_texture_refresh_busy(live2d);
}

void bongo_cat_live2d_cancel_texture_refresh(BongoCatLive2D *live2d) {
    if (!live2d) return;
    if (bridge_instance) bridge_live2d_cancel_texture_refresh(live2d);
    else stub_live2d_cancel_texture_refresh(live2d);
}

bool bongo_cat_live2d_refresh_textures(BongoCatLive2D *live2d, bool active, bool allow_start) {
    if (!live2d) return false;
    if (bridge_instance) return bridge_live2d_refresh_textures(live2d, active, allow_start);
    return stub_live2d_refresh_textures(live2d, active, allow_start);
}

bool bongo_cat_live2d_update(BongoCatLive2D *live2d, float delta_seconds) {
    if (!live2d) return false;
    if (bridge_instance) return bridge_live2d_update(live2d, delta_seconds);
    return stub_live2d_update(live2d, delta_seconds);
}

void bongo_cat_live2d_draw(BongoCatLive2D *live2d) {
    if (!live2d) return;
    if (bridge_instance) bridge_live2d_draw(live2d);
    else stub_live2d_draw(live2d);
}

void bongo_cat_live2d_set_rhi_info(BongoCatLive2D *live2d,
    const BongoCatRhiDeviceInfo *info) {
    if (!live2d) return;
    if (bridge_instance) bridge_live2d_set_rhi_info(live2d, info);
    else stub_live2d_set_rhi_info(live2d, info);
}

void bongo_cat_live2d_set_mirror(BongoCatLive2D *live2d, bool mirror) {
    if (!live2d) return;
    if (bridge_instance) bridge_live2d_set_mirror(live2d, mirror);
    else stub_live2d_set_mirror(live2d, mirror);
}

void bongo_cat_live2d_set_vertical_flip(BongoCatLive2D *live2d, bool flipped) {
    if (!live2d) return;
    if (bridge_instance) bridge_live2d_set_vertical_flip(live2d, flipped);
    else stub_live2d_set_vertical_flip(live2d, flipped);
}

void bongo_cat_live2d_set_render_options(BongoCatLive2D *live2d, const BongoCatLive2DRenderOptions *options) {
    if (!live2d) return;
    if (bridge_instance) bridge_live2d_set_render_options(live2d, options);
    else stub_live2d_set_render_options(live2d, options);
}

void bongo_cat_live2d_set_tight_frame(BongoCatLive2D *live2d, bool tight) {
    if (!live2d) return;
    if (bridge_instance) bridge_live2d_set_tight_frame(live2d, tight);
    else stub_live2d_set_tight_frame(live2d, tight);
}

void bongo_cat_live2d_set_tight_overlay_rect(BongoCatLive2D *live2d, const float *rect) {
    if (!live2d) return;
    if (bridge_instance) bridge_live2d_set_tight_overlay_rect(live2d, rect);
    else stub_live2d_set_tight_overlay_rect(live2d, rect);
}

void bongo_cat_live2d_set_dragging(BongoCatLive2D *live2d, float x, float y) {
    if (!live2d) return;
    if (bridge_instance) bridge_live2d_set_dragging(live2d, x, y);
    else stub_live2d_set_dragging(live2d, x, y);
}

void bongo_cat_live2d_set_centered_dragging(BongoCatLive2D *live2d, float x, float y) {
    if (!live2d) return;
    if (bridge_instance) bridge_live2d_set_centered_dragging(live2d, x, y);
    else stub_live2d_set_centered_dragging(live2d, x, y);
}

void bongo_cat_live2d_prepare_viewer_audit(BongoCatLive2D *live2d) {
    if (!live2d) return;
    if (bridge_instance) bridge_live2d_prepare_viewer_audit(live2d);
    else stub_live2d_prepare_viewer_audit(live2d);
}

bool bongo_cat_live2d_prepare_cover_capture(BongoCatLive2D *live2d) {
    if (!live2d) return false;
    if (bridge_instance) return bridge_live2d_prepare_cover_capture(live2d);
    return stub_live2d_prepare_cover_capture(live2d);
}

bool bongo_cat_live2d_set_parameter(BongoCatLive2D *live2d, const char *id, float value) {
    if (!live2d) return false;
    if (bridge_instance) return bridge_live2d_set_parameter(live2d, id, value);
    return stub_live2d_set_parameter(live2d, id, value);
}

bool bongo_cat_live2d_parameter(BongoCatLive2D *live2d, const char *id, BongoCatParameterRange *range) {
    if (!live2d) return false;
    if (bridge_instance) return bridge_live2d_parameter(live2d, id, range);
    return stub_live2d_parameter(live2d, id, range);
}

bool bongo_cat_live2d_start_motion(BongoCatLive2D *live2d, const char *group, int index) {
    if (!live2d) return false;
    if (bridge_instance) return bridge_live2d_start_motion(live2d, group, index);
    return stub_live2d_start_motion(live2d, group, index);
}

bool bongo_cat_live2d_restore_motion_state(BongoCatLive2D *live2d, const char *group, int index) {
    if (!live2d) return false;
    if (bridge_instance) return bridge_live2d_restore_motion_state(live2d, group, index);
    return stub_live2d_restore_motion_state(live2d, group, index);
}

bool bongo_cat_live2d_preview_motion(BongoCatLive2D *live2d, const char *group, int index) {
    if (!live2d) return false;
    if (bridge_instance) return bridge_live2d_preview_motion(live2d, group, index);
    return stub_live2d_preview_motion(live2d, group, index);
}

bool bongo_cat_live2d_restore_motion_preview(BongoCatLive2D *live2d) {
    if (!live2d) return false;
    if (bridge_instance) return bridge_live2d_restore_motion_preview(live2d);
    return stub_live2d_restore_motion_preview(live2d);
}

bool bongo_cat_live2d_commit_motion_preview(BongoCatLive2D *live2d, const char *group, int index) {
    if (!live2d) return false;
    if (bridge_instance) return bridge_live2d_commit_motion_preview(live2d, group, index);
    return stub_live2d_commit_motion_preview(live2d, group, index);
}

bool bongo_cat_live2d_motion_selected(const BongoCatLive2D *live2d, const char *group, int index) {
    if (!live2d) return false;
    if (bridge_instance) return bridge_live2d_motion_selected(live2d, group, index);
    return stub_live2d_motion_selected(live2d, group, index);
}

bool bongo_cat_live2d_motion_persistent(const BongoCatLive2D *live2d, const char *group, int index) {
    if (!live2d) return false;
    if (bridge_instance) return bridge_live2d_motion_persistent(live2d, group, index);
    return stub_live2d_motion_persistent(live2d, group, index);
}

bool bongo_cat_live2d_motion_visible(const BongoCatLive2D *live2d, const char *group, int index) {
    if (!live2d) return false;
    if (bridge_instance) return bridge_live2d_motion_visible(live2d, group, index);
    return stub_live2d_motion_visible(live2d, group, index);
}

bool bongo_cat_live2d_motion_same_toggle(const BongoCatLive2D *live2d, const char *left_group, int left_index, const char *right_group, int right_index) {
    if (!live2d) return false;
    if (bridge_instance) return bridge_live2d_motion_same_toggle(live2d, left_group, left_index, right_group, right_index);
    return stub_live2d_motion_same_toggle(live2d, left_group, left_index, right_group, right_index);
}

bool bongo_cat_live2d_set_expression(BongoCatLive2D *live2d, int index) {
    if (!live2d) return false;
    if (bridge_instance) return bridge_live2d_set_expression(live2d, index);
    return stub_live2d_set_expression(live2d, index);
}

int bongo_cat_live2d_expression(const BongoCatLive2D *live2d) {
    if (!live2d) return 0;
    if (bridge_instance) return bridge_live2d_expression(live2d);
    return stub_live2d_expression(live2d);
}

bool bongo_cat_live2d_visual_state(const BongoCatLive2D *live2d, BongoCatLive2DVisualState *state) {
    if (!live2d) return false;
    if (bridge_instance) return bridge_live2d_visual_state(live2d, state);
    return stub_live2d_visual_state(live2d, state);
}

