/* Versioned export table of the Cubism bridge. This file is compiled together
   with the bridge sources and shares their build-time renames
   (bongo_cat_live2d_* -> bridge_live2d_*), so every initializer resolves to the
   bridge implementation while the plain public names stay free for the
   dispatch layer in the executable. Splitting the table out of the plugin glue
   keeps the entry-point translation unit free of those renames. */
#include "bongo_cat/live2d_backend.h"
#include "live2d_backend_table.h"

const BongoCatLive2DBackendApi bongo_cat_live2d_backend_table = {
    /* .create = */ bongo_cat_live2d_create,
    /* .destroy = */ bongo_cat_live2d_destroy,
    /* .load = */ bongo_cat_live2d_load,
    /* .load_ex = */ bongo_cat_live2d_load_ex,
    /* .ready = */ bongo_cat_live2d_ready,
    /* .canvas_size = */ bongo_cat_live2d_canvas_size,
    /* .frame = */ bongo_cat_live2d_frame,
    /* .measure_frame = */ bongo_cat_live2d_measure_frame,
    /* .set_frame = */ bongo_cat_live2d_set_frame,
    /* .viewport = */ bongo_cat_live2d_viewport,
    /* .resize = */ bongo_cat_live2d_resize,
    /* .reshape = */ bongo_cat_live2d_reshape,
    /* .try_reuse_texture_quality = */ bongo_cat_live2d_try_reuse_texture_quality,
    /* .texture_refresh_pending = */ bongo_cat_live2d_texture_refresh_pending,
    /* .texture_refresh_due = */ bongo_cat_live2d_texture_refresh_due,
    /* .texture_refresh_busy = */ bongo_cat_live2d_texture_refresh_busy,
    /* .cancel_texture_refresh = */ bongo_cat_live2d_cancel_texture_refresh,
    /* .refresh_textures = */ bongo_cat_live2d_refresh_textures,
    /* .update = */ bongo_cat_live2d_update,
    /* .draw = */ bongo_cat_live2d_draw,
    /* .set_mirror = */ bongo_cat_live2d_set_mirror,
    /* .set_vertical_flip = */ bongo_cat_live2d_set_vertical_flip,
    /* .set_render_options = */ bongo_cat_live2d_set_render_options,
    /* .set_dragging = */ bongo_cat_live2d_set_dragging,
    /* .set_centered_dragging = */ bongo_cat_live2d_set_centered_dragging,
    /* .prepare_viewer_audit = */ bongo_cat_live2d_prepare_viewer_audit,
    /* .prepare_cover_capture = */ bongo_cat_live2d_prepare_cover_capture,
    /* .set_parameter = */ bongo_cat_live2d_set_parameter,
    /* .parameter = */ bongo_cat_live2d_parameter,
    /* .start_motion = */ bongo_cat_live2d_start_motion,
    /* .restore_motion_state = */ bongo_cat_live2d_restore_motion_state,
    /* .preview_motion = */ bongo_cat_live2d_preview_motion,
    /* .restore_motion_preview = */ bongo_cat_live2d_restore_motion_preview,
    /* .commit_motion_preview = */ bongo_cat_live2d_commit_motion_preview,
    /* .motion_selected = */ bongo_cat_live2d_motion_selected,
    /* .motion_persistent = */ bongo_cat_live2d_motion_persistent,
    /* .motion_visible = */ bongo_cat_live2d_motion_visible,
    /* .motion_same_toggle = */ bongo_cat_live2d_motion_same_toggle,
    /* .set_expression = */ bongo_cat_live2d_set_expression,
    /* .expression = */ bongo_cat_live2d_expression,
    /* .visual_state = */ bongo_cat_live2d_visual_state
};
