#ifndef BONGO_CAT_MODEL_H
#define BONGO_CAT_MODEL_H

#include "bongo_cat/config.h"
#include "bongo_cat/rhi.h"

typedef enum BongoCatModelEngine {
    BONGO_CAT_MODEL_ENGINE_LIVE2D = 1,
    BONGO_CAT_MODEL_ENGINE_INOX2D = 2
} BongoCatModelEngine;

typedef enum BongoCatModelSourceFormat {
    BONGO_CAT_MODEL_SOURCE_UNKNOWN,
    BONGO_CAT_MODEL_SOURCE_BUILTIN,
    BONGO_CAT_MODEL_SOURCE_TAURI,
    BONGO_CAT_MODEL_SOURCE_MVER,
    BONGO_CAT_MODEL_SOURCE_MVER_PATCH,
    BONGO_CAT_MODEL_SOURCE_INOCHI2D
} BongoCatModelSourceFormat;

typedef enum BongoCatModelCapability {
    BONGO_CAT_MODEL_CAPABILITY_LIVE2D = 1u << 0,
    BONGO_CAT_MODEL_CAPABILITY_PREVIEW = 1u << 1,
    BONGO_CAT_MODEL_CAPABILITY_RUNTIME_ADAPTER = 1u << 2,
    BONGO_CAT_MODEL_CAPABILITY_INPUT_IMAGES = 1u << 3,
    BONGO_CAT_MODEL_CAPABILITY_KEYBOARD_INPUT = 1u << 4,
    BONGO_CAT_MODEL_CAPABILITY_GAMEPAD_INPUT = 1u << 5,
    BONGO_CAT_MODEL_CAPABILITY_BEHAVIORS = 1u << 6,
    BONGO_CAT_MODEL_CAPABILITY_AUDIO = 1u << 7,
    BONGO_CAT_MODEL_CAPABILITY_EFFECTS = 1u << 8,
    BONGO_CAT_MODEL_CAPABILITY_MVER_PROJECTION = 1u << 9,
    BONGO_CAT_MODEL_CAPABILITY_POINTER_OVERLAY = 1u << 10,
    BONGO_CAT_MODEL_CAPABILITY_IMAGE_PATCH = 1u << 11,
    BONGO_CAT_MODEL_CAPABILITY_INOCHI2D = 1u << 12
} BongoCatModelCapability;

typedef struct BongoCatModelEntry {
    char id[BONGO_CAT_ID_CAP];
    char package_id[BONGO_CAT_ID_CAP];
    char content_digest[65];
    char family_id[BONGO_CAT_ID_CAP];
    char display_name[BONGO_CAT_ID_CAP];
    char directory[BONGO_CAT_PATH_CAP];
    char adapter_directory[BONGO_CAT_PATH_CAP];
    char storage_directory[BONGO_CAT_PATH_CAP];
    char setting_file[BONGO_CAT_PATH_CAP];
    BongoCatModelMode mode;
    BongoCatModelSourceFormat source_format;
    uint32_t capabilities;
    int adapter_schema;
    int adapter_generator;
    bool preset;
    bool managed;
} BongoCatModelEntry;

typedef struct BongoCatModelCatalog {
    BongoCatModelEntry entries[BONGO_CAT_MODEL_CAP];
    size_t count;
} BongoCatModelCatalog;

typedef enum BongoCatBehaviorKind {
    BONGO_CAT_BEHAVIOR_MOTION,
    BONGO_CAT_BEHAVIOR_EXPRESSION,
    BONGO_CAT_BEHAVIOR_SOUND,
    BONGO_CAT_BEHAVIOR_EFFECT
} BongoCatBehaviorKind;

typedef struct BongoCatBehaviorEntry {
    char id[BONGO_CAT_PATH_CAP];
    char label[BONGO_CAT_ID_CAP];
    char group[BONGO_CAT_ID_CAP];
    char sound[BONGO_CAT_PATH_CAP];
    char effect[BONGO_CAT_PATH_CAP];
    int index;
    BongoCatBehaviorKind kind;
    bool momentary;
    bool sound_overlap;
    bool sound_clear;
    bool shortcut_active, audio_playing; /* Runtime state owned by this entry. */
} BongoCatBehaviorEntry;

typedef struct BongoCatBehaviorCatalog {
    BongoCatBehaviorEntry *entries;
    size_t count, capacity;
} BongoCatBehaviorCatalog;

#ifdef __cplusplus
extern "C" {
#endif

void bongo_cat_models_init(BongoCatModelCatalog *catalog);
BongoCatResult bongo_cat_models_scan(BongoCatModelCatalog *catalog, const char *root,
    bool preset, BongoCatError *error);
const BongoCatModelEntry *bongo_cat_models_find(const BongoCatModelCatalog *catalog,
    const char *id);
bool bongo_cat_model_adapter_metadata_path(const char *directory,
    char *path, size_t capacity);
const char *bongo_cat_model_default_name(const BongoCatModelEntry *entry);
const char *bongo_cat_model_name(const BongoCatSettings *settings,
    const BongoCatModelEntry *entry);
/* Zero-initialize catalogs before first use. Copy/move preserve ownership. */
bool bongo_cat_behaviors_reserve(BongoCatBehaviorCatalog *catalog, size_t capacity,
    BongoCatError *error);
void bongo_cat_behaviors_clear(BongoCatBehaviorCatalog *catalog);
bool bongo_cat_behaviors_copy(BongoCatBehaviorCatalog *target,
    const BongoCatBehaviorCatalog *source, BongoCatError *error);
void bongo_cat_behaviors_move(BongoCatBehaviorCatalog *target, BongoCatBehaviorCatalog *source);
BongoCatResult bongo_cat_behaviors_load(BongoCatBehaviorCatalog *catalog,
    const BongoCatModelEntry *model, BongoCatError *error);

typedef struct BongoCatModelRuntime BongoCatModelRuntime;
BongoCatModelEngine bongo_cat_model_engine(const BongoCatModelEntry *entry);
BongoCatModelEngine bongo_cat_model_runtime_engine(const BongoCatModelRuntime *runtime);
bool bongo_cat_model_runtime_rendering(const BongoCatModelRuntime *runtime);
typedef struct BongoCatParameterRange { float minimum, maximum, value; } BongoCatParameterRange;
typedef void (*BongoCatModelRuntimeLoadProgress)(void *userdata, float progress);
typedef struct BongoCatModelRuntimeVisualState {
    float fit_scale, fit_translate_x, fit_translate_y;
    float visible_min_x, visible_min_y, visible_max_x, visible_max_y;
    int drawable_count, drawable_visible, drawable_vertex_changed;
    int offscreen_count, offscreen_positive, part_count, part_positive;
    bool fitted, visible, mver_projection;
} BongoCatModelRuntimeVisualState;

/* Extra transparent space around the authored model canvas, expressed as a
   fraction of that canvas dimension.  For example, top=0.5 reserves half a
   canvas height above the normal composition. */
typedef struct BongoCatModelRuntimeFrame {
    float left, top, right, bottom;
} BongoCatModelRuntimeFrame;

typedef struct BongoCatModelRuntimeRenderOptions {
    bool mver_projection;
    bool auto_frame;
    bool source_mirror;
    bool custom_pointer_bounds;
    bool pointer_left_handed;
    bool mouse_force_move;
    float mouse_speed;
    float projection_scale;
    float offset_x;
    float offset_y;
    int reference_width;
    int reference_height;
    int pointer_left;
    int pointer_top;
    int pointer_right;
    int pointer_bottom;
} BongoCatModelRuntimeRenderOptions;

/* Runtime texture policy.  This is deliberately separate from model package
   rendering metadata: it is a user preference and can be changed without
   modifying an imported model. */
typedef bool (*BongoCatModelRuntimeTextureDisplaySize)(void *userdata,
    const BongoCatModelRuntimeRenderOptions *options, int canvas_width,
    int canvas_height, int *display_width, int *display_height);
typedef struct BongoCatModelRuntimeTextureOptions {
    bool dynamic_resolution;
    /* Approximate texture-memory budget: 0.1, 1, then 10 to 100 percent. */
    float render_quality_percent;
    /* Synchronous planner, called after reading the incoming canvas and before
       allocating its atlases. Returns content pixels, excluding transparent
       frame padding. It must not change the live model or window. */
    BongoCatModelRuntimeTextureDisplaySize display_size;
    void *display_size_userdata;
} BongoCatModelRuntimeTextureOptions;

BongoCatModelRuntime *bongo_cat_model_runtime_create(const char *asset_root, BongoCatError *error);
void bongo_cat_model_runtime_destroy(BongoCatModelRuntime *live2d);
/* Attaches the active frame-backend device handles (rhi.h) so the bridge
   can drive the Cubism Vulkan/Metal renderers. Call after create and
   whenever the render backend is hot-swapped; no-op on the stub. */
void bongo_cat_model_runtime_set_rhi_info(BongoCatModelRuntime *live2d,
    const BongoCatRhiDeviceInfo *info);
BongoCatResult bongo_cat_model_runtime_load(BongoCatModelRuntime *live2d, const char *model_dir,
    const char *setting_file, bool preset,
    const BongoCatModelRuntimeRenderOptions *render_options,
    BongoCatModelRuntimeLoadProgress progress, void *userdata,
    BongoCatError *error);
BongoCatResult bongo_cat_model_runtime_load_ex(BongoCatModelRuntime *live2d,
    const char *model_dir, const char *setting_file, bool preset,
    const BongoCatModelRuntimeRenderOptions *render_options,
    const BongoCatModelRuntimeTextureOptions *texture_options,
    BongoCatModelRuntimeLoadProgress progress, void *userdata,
    BongoCatError *error);
bool bongo_cat_model_runtime_ready(const BongoCatModelRuntime *live2d);
/* Returns the authored pixel canvas size of the loaded model. */
bool bongo_cat_model_runtime_canvas_size(const BongoCatModelRuntime *live2d,
    int *width, int *height);
bool bongo_cat_model_runtime_frame(const BongoCatModelRuntime *live2d,
    BongoCatModelRuntimeFrame *frame);
/* Inspect current CPU geometry before drawing. Updates the retained envelope
   and fits it inside the allocated frame until the window can grow. No GL
   readback, animation advancement, or native window operations. */
bool bongo_cat_model_runtime_measure_frame(BongoCatModelRuntime *live2d,
    BongoCatModelRuntimeFrame *required);
/* Commit only the transparent margins that the native window can allocate. */
void bongo_cat_model_runtime_set_frame(BongoCatModelRuntime *live2d,
    const BongoCatModelRuntimeFrame *frame);
bool bongo_cat_model_runtime_viewport(const BongoCatModelRuntime *live2d,
    int *x, int *y, int *width, int *height);
/* Rect the 2D overlay layers draw into: the canvas mapped into window
   pixels with the model's own transform (tight mode crops it, so the rect
   may extend past the window; GL clips that). */
bool bongo_cat_model_runtime_overlay_viewport(const BongoCatModelRuntime *live2d,
    int *x, int *y, int *width, int *height);
void bongo_cat_model_runtime_resize(BongoCatModelRuntime *live2d, int width, int height);
void bongo_cat_model_runtime_reshape(BongoCatModelRuntime *live2d, int width, int height);
/* Main-thread fast path with no GL calls. Commit the new quality only when
   every current atlas already has the required pixels. False leaves the
   model unchanged; the caller can use the normal reload/rollback path. */
bool bongo_cat_model_runtime_try_reuse_texture_quality(BongoCatModelRuntime *live2d,
    float quality_percent);
bool bongo_cat_model_runtime_texture_refresh_pending(const BongoCatModelRuntime *live2d, bool active);
/* Main-thread query without GL calls. Unlike pending, excludes debounce,
   cancellation cooldown and paused jobs that do not yet need retirement. */
bool bongo_cat_model_runtime_texture_refresh_due(const BongoCatModelRuntime *live2d,
    bool active, bool allow_start);
/* True only while upload/cleanup can make progress, so low playback FPS does
   not delay reclamation. Paused jobs do not request frequent wakeups. */
bool bongo_cat_model_runtime_texture_refresh_busy(const BongoCatModelRuntime *live2d);
/* Request cancellation without joining the worker. Continue polling cleanup.
   The displayed atlas remains valid; pending resolution work resumes on show. */
void bongo_cat_model_runtime_cancel_texture_refresh(BongoCatModelRuntime *live2d);
/* Call with the model's GL context current. False active pauses uploads during
   gestures/temporary hiding; obsolete work is still cleaned up. True permits
   one upload batch. Long pauses release the unfinished replacement.
   False allow_start services existing work and settled reclamation only. */
bool bongo_cat_model_runtime_refresh_textures(BongoCatModelRuntime *live2d,
    bool active, bool allow_start);
bool bongo_cat_model_runtime_update(BongoCatModelRuntime *live2d, float delta_seconds);
void bongo_cat_model_runtime_draw(BongoCatModelRuntime *live2d);
bool bongo_cat_model_runtime_draw_checked(BongoCatModelRuntime *live2d);
void bongo_cat_model_runtime_set_mirror(BongoCatModelRuntime *live2d, bool mirror);
void bongo_cat_model_runtime_set_vertical_flip(BongoCatModelRuntime *live2d, bool flipped);
void bongo_cat_model_runtime_set_render_options(BongoCatModelRuntime *live2d,
    const BongoCatModelRuntimeRenderOptions *options);
void bongo_cat_model_runtime_set_tight_frame(BongoCatModelRuntime *live2d, bool tight);
/* Static 2D overlay art bounds in canvas NDC {min_x, min_y, max_x, max_y};
   the tight-window envelope must cover them so the crop never slices the
   desk art. NULL or an inverted rect clears the constraint. */
void bongo_cat_model_runtime_set_tight_overlay_rect(BongoCatModelRuntime *live2d,
    const float *rect);
void bongo_cat_model_runtime_set_dragging(BongoCatModelRuntime *live2d, float x, float y);
void bongo_cat_model_runtime_set_centered_dragging(BongoCatModelRuntime *live2d,
    float x, float y);
void bongo_cat_model_runtime_prepare_viewer_audit(BongoCatModelRuntime *live2d);
/* Synchronize drawables from the current state before a cover-only frame. */
bool bongo_cat_model_runtime_prepare_cover_capture(BongoCatModelRuntime *live2d);
bool bongo_cat_model_runtime_set_parameter(BongoCatModelRuntime *live2d, const char *id, float value);
bool bongo_cat_model_runtime_parameter(BongoCatModelRuntime *live2d, const char *id,
    BongoCatParameterRange *range);
bool bongo_cat_model_runtime_start_motion(BongoCatModelRuntime *live2d, const char *group, int index);
bool bongo_cat_model_runtime_restore_motion_state(BongoCatModelRuntime *live2d,
    const char *group, int index);
bool bongo_cat_model_runtime_preview_motion(BongoCatModelRuntime *live2d,
    const char *group, int index);
bool bongo_cat_model_runtime_restore_motion_preview(BongoCatModelRuntime *live2d);
bool bongo_cat_model_runtime_commit_motion_preview(BongoCatModelRuntime *live2d,
    const char *group, int index);
bool bongo_cat_model_runtime_motion_selected(const BongoCatModelRuntime *live2d,
    const char *group, int index);
bool bongo_cat_model_runtime_motion_persistent(const BongoCatModelRuntime *live2d,
    const char *group, int index);
bool bongo_cat_model_runtime_motion_visible(const BongoCatModelRuntime *live2d,
    const char *group, int index);
bool bongo_cat_model_runtime_motion_same_toggle(const BongoCatModelRuntime *live2d,
    const char *left_group, int left_index,
    const char *right_group, int right_index);
bool bongo_cat_model_runtime_set_expression(BongoCatModelRuntime *live2d, int index);
int bongo_cat_model_runtime_expression(const BongoCatModelRuntime *live2d);
bool bongo_cat_model_runtime_visual_state(const BongoCatModelRuntime *live2d,
    BongoCatModelRuntimeVisualState *state);

#ifdef __cplusplus
}
#endif

#endif
