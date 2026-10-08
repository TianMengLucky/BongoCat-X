#include "bongo_cat/model.h"
#include "model_import_manifest.h"
#include "bongo_cat/json.h"
#include "bongo_cat/path.h"

#include <SDL3/SDL_log.h>
#include <stdlib.h>

struct BongoCatModelRuntime {
    int width;
    int height;
    bool loaded;
};

static BongoCatModelRuntime *create_runtime(const char *asset_root,
    BongoCatError *error) {
    (void)asset_root;
    BongoCatModelRuntime *value = calloc(1, sizeof(*value));
    if (!value) bongo_cat_error_set(error, BONGO_CAT_ERROR_MEMORY, "Cannot allocate Live2D runtime");
    return value;
}

BongoCatModelRuntime *bongo_cat_model_runtime_create(const char *asset_root,
    BongoCatError *error) {
    return create_runtime(asset_root, error);
}

void bongo_cat_model_runtime_destroy(BongoCatModelRuntime *live2d) { free(live2d); }

BongoCatResult bongo_cat_model_runtime_load_ex(BongoCatModelRuntime *live2d,
    const char *model_dir,
    const char *setting_file, bool preset,
    const BongoCatModelRuntimeRenderOptions *render_options,
    const BongoCatModelRuntimeTextureOptions *texture_options,
    BongoCatModelRuntimeLoadProgress progress, void *userdata,
    BongoCatError *error) {
    (void)preset; (void)render_options; (void)texture_options;
    if (!live2d || !model_dir || !setting_file) return BONGO_CAT_ERROR_ARGUMENT;
    if (progress) progress(userdata, 0.1f);
    char path[BONGO_CAT_PATH_CAP];
    BongoJsonDoc *document = bongo_cat_path_join(path, sizeof(path), model_dir,
        setting_file) ? bongo_cat_model_json_read(path, NULL) : NULL;
    bool valid = bongo_cat_import_manifest_document_valid(model_dir, document, true);
    bongo_json_doc_free(document);
    if (!valid) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_FORMAT,
            "Model manifest or required assets are invalid: %s", setting_file);
        return BONGO_CAT_ERROR_FORMAT;
    }
    live2d->loaded = true;
    if (progress) progress(userdata, 1.0f);
    return BONGO_CAT_OK;
}

BongoCatResult bongo_cat_model_runtime_load(BongoCatModelRuntime *live2d, const char *model_dir,
    const char *setting_file, bool preset,
    const BongoCatModelRuntimeRenderOptions *render_options,
    BongoCatModelRuntimeLoadProgress progress, void *userdata,
    BongoCatError *error) {
    return bongo_cat_model_runtime_load_ex(live2d, model_dir, setting_file, preset,
        render_options, NULL, progress, userdata, error);
}

bool bongo_cat_model_runtime_ready(const BongoCatModelRuntime *live2d) {
    return live2d && live2d->loaded;
}

bool bongo_cat_model_runtime_canvas_size(const BongoCatModelRuntime *live2d,
    int *width, int *height) {
    (void)live2d; (void)width; (void)height; return false;
}

bool bongo_cat_model_runtime_frame(const BongoCatModelRuntime *live2d,
    BongoCatModelRuntimeFrame *frame) {
    if (!live2d || !frame) return false;
    *frame = (BongoCatModelRuntimeFrame){0};
    return true;
}

bool bongo_cat_model_runtime_viewport(const BongoCatModelRuntime *live2d,
    int *x, int *y, int *width, int *height) {
    if (!live2d || !x || !y || !width || !height) return false;
    *x = 0;
    *y = 0;
    *width = live2d->width;
    *height = live2d->height;
    return true;
}

bool bongo_cat_model_runtime_overlay_viewport(const BongoCatModelRuntime *live2d,
    int *x, int *y, int *width, int *height) {
    return bongo_cat_model_runtime_viewport(live2d, x, y, width, height);
}

void bongo_cat_model_runtime_resize(BongoCatModelRuntime *live2d, int width, int height) {
    if (!live2d) return;
    live2d->width = width;
    live2d->height = height;
}
void bongo_cat_model_runtime_reshape(BongoCatModelRuntime *live2d, int width, int height) {
    bongo_cat_model_runtime_resize(live2d, width, height);
}

bool bongo_cat_model_runtime_update(BongoCatModelRuntime *live2d, float delta_seconds) {
    (void)live2d; (void)delta_seconds; return false;
}
bool bongo_cat_model_runtime_texture_refresh_pending(const BongoCatModelRuntime *live2d, bool active) {
    (void)live2d; (void)active; return false;
}
bool bongo_cat_model_runtime_texture_refresh_due(const BongoCatModelRuntime *live2d,
    bool active, bool allow_start) {
    (void)live2d; (void)active; (void)allow_start; return false;
}
bool bongo_cat_model_runtime_try_reuse_texture_quality(BongoCatModelRuntime *live2d,
    float quality_percent) {
    (void)live2d; (void)quality_percent; return false;
}

bool bongo_cat_model_runtime_measure_frame(BongoCatModelRuntime *live2d,
    BongoCatModelRuntimeFrame *required) {
    (void)live2d; (void)required;
    return false;
}

void bongo_cat_model_runtime_set_frame(BongoCatModelRuntime *live2d,
    const BongoCatModelRuntimeFrame *frame) {
    (void)live2d; (void)frame;
}
bool bongo_cat_model_runtime_refresh_textures(BongoCatModelRuntime *live2d,
    bool active, bool allow_start) {
    (void)live2d; (void)active; (void)allow_start; return false;
}
bool bongo_cat_model_runtime_texture_refresh_busy(const BongoCatModelRuntime *live2d) {
    (void)live2d; return false;
}
void bongo_cat_model_runtime_cancel_texture_refresh(BongoCatModelRuntime *live2d) {
    (void)live2d;
}
bool bongo_cat_model_runtime_draw_checked(BongoCatModelRuntime *live2d) {
    (void)live2d; return false;
}
void bongo_cat_model_runtime_draw(BongoCatModelRuntime *live2d) { (void)live2d; }
void stub_model_runtime_set_rhi_info(BongoCatModelRuntime *live2d,
    const BongoCatRhiDeviceInfo *info) {
    (void)live2d; (void)info;
}
void bongo_cat_model_runtime_set_vertical_flip(BongoCatModelRuntime *live2d, bool flipped) {
    (void)live2d; (void)flipped;
}

void bongo_cat_model_runtime_set_mirror(BongoCatModelRuntime *live2d, bool mirror) {
    (void)live2d; (void)mirror;
}
void bongo_cat_model_runtime_set_render_options(BongoCatModelRuntime *live2d,
    const BongoCatModelRuntimeRenderOptions *options) {
    (void)live2d; (void)options;
}
void bongo_cat_model_runtime_set_tight_frame(BongoCatModelRuntime *live2d, bool tight) {
    (void)live2d; (void)tight;
}
void bongo_cat_model_runtime_set_tight_overlay_rect(BongoCatModelRuntime *live2d,
    const float *rect) {
    (void)live2d; (void)rect;
}
void bongo_cat_model_runtime_set_dragging(BongoCatModelRuntime *live2d, float x, float y) {
    (void)live2d; (void)x; (void)y;
}
void bongo_cat_model_runtime_set_centered_dragging(BongoCatModelRuntime *live2d,
    float x, float y) { (void)live2d; (void)x; (void)y; }
void bongo_cat_model_runtime_prepare_viewer_audit(BongoCatModelRuntime *live2d) { (void)live2d; }
bool bongo_cat_model_runtime_prepare_cover_capture(BongoCatModelRuntime *live2d) {
    return live2d && live2d->loaded;
}
bool bongo_cat_model_runtime_set_parameter(BongoCatModelRuntime *live2d, const char *id, float value) {
    (void)live2d; (void)id; (void)value; return false;
}
bool bongo_cat_model_runtime_parameter(BongoCatModelRuntime *value, const char *id, BongoCatParameterRange *range) {
    (void)value; (void)id; (void)range; return false;
}
bool bongo_cat_model_runtime_start_motion(BongoCatModelRuntime *value, const char *group, int index) {
    (void)value; (void)group; (void)index; return false;
}
bool bongo_cat_model_runtime_restore_motion_state(BongoCatModelRuntime *value,
    const char *group, int index) {
    (void)value; (void)group; (void)index; return false;
}
bool bongo_cat_model_runtime_preview_motion(BongoCatModelRuntime *value,
    const char *group, int index) {
    (void)value; (void)group; (void)index; return false;
}
bool bongo_cat_model_runtime_restore_motion_preview(BongoCatModelRuntime *value) {
    (void)value; return false;
}
bool bongo_cat_model_runtime_commit_motion_preview(BongoCatModelRuntime *value,
    const char *group, int index) {
    (void)value; (void)group; (void)index; return false;
}
bool bongo_cat_model_runtime_motion_selected(const BongoCatModelRuntime *value,
    const char *group, int index) {
    (void)value; (void)group; (void)index; return false;
}
bool bongo_cat_model_runtime_motion_persistent(const BongoCatModelRuntime *value,
    const char *group, int index) {
    (void)value; (void)group; (void)index; return false;
}
bool bongo_cat_model_runtime_motion_visible(const BongoCatModelRuntime *value,
    const char *group, int index) {
    (void)value; (void)group; (void)index; return true;
}
bool bongo_cat_model_runtime_motion_same_toggle(const BongoCatModelRuntime *value,
    const char *left_group, int left_index,
    const char *right_group, int right_index) {
    (void)value; (void)left_group; (void)left_index;
    (void)right_group; (void)right_index; return false;
}
bool bongo_cat_model_runtime_set_expression(BongoCatModelRuntime *value, int index) {
    (void)value; (void)index; return false;
}
int bongo_cat_model_runtime_expression(const BongoCatModelRuntime *value) {
    (void)value; return -1;
}
bool bongo_cat_model_runtime_visual_state(const BongoCatModelRuntime *value,
    BongoCatModelRuntimeVisualState *state) {
    (void)value; (void)state; return false;
}
