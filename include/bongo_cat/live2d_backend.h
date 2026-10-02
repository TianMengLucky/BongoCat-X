#ifndef BONGO_CAT_LIVE2D_BACKEND_H
#define BONGO_CAT_LIVE2D_BACKEND_H

/* Contract between the BongoCat executable and the optional Live2D backend
   shared library (bongo-cat-live2d-backend). The executable never links the
   Cubism SDK: it locates the library at runtime, hands it the host services
   below, and installs the library's API table into the dispatch layer.
   Without the library the diagnostic stub keeps the pet running, so a missing
   backend degrades Live2D rendering only. */

#include <stdbool.h>
#include <stddef.h>

#include "bongo_cat/common.h"
#include "bongo_cat/file.h"
#include "bongo_cat/gl_api.h"
#include "bongo_cat/image.h"
#include "bongo_cat/image_texture_job.h"
#include "bongo_cat/json.h"
#include "bongo_cat/memory.h"
#include "bongo_cat/model_memory.h"
#include "bongo_cat/model.h"
#include "bongo_cat/resource_trace.h"
#include "bongo_cat/sha256.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Bump whenever a struct member, signature or ownership rule below changes so
   that an older shared library is rejected instead of crashing. */
#define BONGO_CAT_LIVE2D_BACKEND_ABI_VERSION 1

/* Library file names searched next to the executable, in <base>/live2d and in
   <data directory>/live2d. */
#define BONGO_CAT_LIVE2D_BACKEND_FILE_NAME "bongo-cat-live2d-backend"

/* The Live2D ABI the backend implements. Member names mirror the public
   bongo_cat_live2d_* functions in bongo_cat/model.h; adding or changing one
   there means adding or changing the matching member here. The backend never
   sees the BongoCatLive2D internals. */
typedef struct BongoCatLive2DBackendApi {
    BongoCatLive2D *(*create)(const char *asset_root, BongoCatError *error);
    void (*destroy)(BongoCatLive2D *live2d);
    BongoCatResult (*load)(BongoCatLive2D *live2d, const char *model_dir,
        const char *setting_file, bool preset,
        const BongoCatLive2DRenderOptions *render_options,
        BongoCatLive2DLoadProgress progress, void *userdata,
        BongoCatError *error);
    BongoCatResult (*load_ex)(BongoCatLive2D *live2d, const char *model_dir,
        const char *setting_file, bool preset,
        const BongoCatLive2DRenderOptions *render_options,
        const BongoCatLive2DTextureOptions *texture_options,
        BongoCatLive2DLoadProgress progress, void *userdata,
        BongoCatError *error);
    bool (*ready)(const BongoCatLive2D *live2d);
    bool (*canvas_size)(const BongoCatLive2D *live2d, int *width, int *height);
    bool (*frame)(const BongoCatLive2D *live2d, BongoCatLive2DFrame *frame);
    bool (*measure_frame)(BongoCatLive2D *live2d,
        BongoCatLive2DFrame *required);
    void (*set_frame)(BongoCatLive2D *live2d, const BongoCatLive2DFrame *frame);
    bool (*viewport)(const BongoCatLive2D *live2d, int *x, int *y, int *width,
        int *height);
    void (*resize)(BongoCatLive2D *live2d, int width, int height);
    void (*reshape)(BongoCatLive2D *live2d, int width, int height);
    bool (*try_reuse_texture_quality)(BongoCatLive2D *live2d,
        float quality_percent);
    bool (*texture_refresh_pending)(const BongoCatLive2D *live2d, bool active);
    bool (*texture_refresh_due)(const BongoCatLive2D *live2d, bool active,
        bool allow_start);
    bool (*texture_refresh_busy)(const BongoCatLive2D *live2d);
    void (*cancel_texture_refresh)(BongoCatLive2D *live2d);
    bool (*refresh_textures)(BongoCatLive2D *live2d, bool active,
        bool allow_start);
    bool (*update)(BongoCatLive2D *live2d, float delta_seconds);
    void (*draw)(BongoCatLive2D *live2d);
    void (*set_mirror)(BongoCatLive2D *live2d, bool mirror);
    void (*set_vertical_flip)(BongoCatLive2D *live2d, bool flipped);
    void (*set_render_options)(BongoCatLive2D *live2d,
        const BongoCatLive2DRenderOptions *options);
    void (*set_dragging)(BongoCatLive2D *live2d, float x, float y);
    void (*set_centered_dragging)(BongoCatLive2D *live2d, float x, float y);
    void (*prepare_viewer_audit)(BongoCatLive2D *live2d);
    bool (*prepare_cover_capture)(BongoCatLive2D *live2d);
    bool (*set_parameter)(BongoCatLive2D *live2d, const char *id, float value);
    bool (*parameter)(BongoCatLive2D *live2d, const char *id,
        BongoCatParameterRange *range);
    bool (*start_motion)(BongoCatLive2D *live2d, const char *group, int index);
    bool (*restore_motion_state)(BongoCatLive2D *live2d, const char *group,
        int index);
    bool (*preview_motion)(BongoCatLive2D *live2d, const char *group,
        int index);
    bool (*restore_motion_preview)(BongoCatLive2D *live2d);
    bool (*commit_motion_preview)(BongoCatLive2D *live2d, const char *group,
        int index);
    bool (*motion_selected)(const BongoCatLive2D *live2d, const char *group,
        int index);
    bool (*motion_persistent)(const BongoCatLive2D *live2d, const char *group,
        int index);
    bool (*motion_visible)(const BongoCatLive2D *live2d, const char *group,
        int index);
    bool (*motion_same_toggle)(const BongoCatLive2D *live2d,
        const char *left_group, int left_index, const char *right_group,
        int right_index);
    bool (*set_expression)(BongoCatLive2D *live2d, int index);
    int (*expression)(const BongoCatLive2D *live2d);
    bool (*visual_state)(const BongoCatLive2D *live2d,
        BongoCatLive2DVisualState *state);
} BongoCatLive2DBackendApi;

/* The Cubism bridge is written against small SDL helpers (logging, the current
   GL context, threading, timers). A backend library must NOT link its own copy
   of SDL: two copies would not share the application's GL context or log
   state. Those helpers therefore also travel through the host table, and
   src/live2d/live2d_backend_sdl.h redirects the SDL calls to these members. */
/* The values also mirror SDL's own SDL_LogCategory numbering (APPLICATION =
   0, VIDEO = 5, RENDER = 6) so a category can be handed to SDL unchanged,
   while the host still maps it by name. */
enum {
    BONGO_CAT_LIVE2D_LOG_CATEGORY_APPLICATION = 0,
    BONGO_CAT_LIVE2D_LOG_CATEGORY_VIDEO = 5,
    BONGO_CAT_LIVE2D_LOG_CATEGORY_RENDER = 6
};
/* The values mirror SDL's own SDL_LogPriority numbering (SDL_INVALID = 0,
   TRACE = 1, ... CRITICAL = 7): the host passes them straight to
   SDL_LogMessageV, so a mismatch would silently demote every backend message
   by one level. TRACE exists so the mapping stays complete. */
enum {
    BONGO_CAT_LIVE2D_LOG_PRIORITY_TRACE = 1,
    BONGO_CAT_LIVE2D_LOG_PRIORITY_VERBOSE = 2,
    BONGO_CAT_LIVE2D_LOG_PRIORITY_DEBUG = 3,
    BONGO_CAT_LIVE2D_LOG_PRIORITY_INFO = 4,
    BONGO_CAT_LIVE2D_LOG_PRIORITY_WARN = 5,
    BONGO_CAT_LIVE2D_LOG_PRIORITY_ERROR = 6,
    BONGO_CAT_LIVE2D_LOG_PRIORITY_CRITICAL = 7
};
enum {
    BONGO_CAT_LIVE2D_PATH_TYPE_OTHER = 0,
    BONGO_CAT_LIVE2D_PATH_TYPE_FILE = 1,
    BONGO_CAT_LIVE2D_PATH_TYPE_DIRECTORY = 2
};
enum {
    BONGO_CAT_LIVE2D_THREAD_ALIVE = 1
};
typedef struct BongoCatLive2DPathInfo {
    int type;
    unsigned long long size;
    long long modify_time; /* nanoseconds since the epoch */
    long long create_time; /* nanoseconds since the epoch */
} BongoCatLive2DPathInfo;

/* Host services the backend calls back into. The executable fills one and
   passes it to bongo_cat_live2d_backend_init before any other backend call;
   every member must be non-NULL for the whole lifetime of the library.
   model_memory_log is variadic: the backend formats the message first and
   forwards it with a single "%s" argument. */
typedef struct BongoCatLive2DBackendHost {
    void (*error_set)(BongoCatError *error, BongoCatResult code,
        const char *format, ...);
    FILE *(*file_open)(const char *path, const char *mode);
    bool (*gl_clear_errors)(void);
    bool (*image_info)(const char *path, int *width, int *height);
    unsigned int (*image_texture_model)(const char *path, bool direct_decode,
        int *width, int *height, BongoCatImageAlphaMask *alpha,
        BongoCatImageProgress progress, void *userdata, BongoCatError *error);
    void (*image_forget_texture_cache)(const char *digest, int max_width,
        int max_height);
    unsigned int (*image_texture_model_scaled_cached)(const char *path,
        const char *digest, bool direct_decode, int max_width, int max_height,
        int *width, int *height, BongoCatImageAlphaMask *alpha,
        BongoCatImageProgress progress, void *userdata, BongoCatError *error);
    BongoCatImageTextureJob *(*image_texture_job_start)(const char *path,
        int max_width, int max_height, BongoCatError *error);
    bool (*image_texture_job_needs_poll)(BongoCatImageTextureJob *job);
    int (*image_texture_job_poll)(BongoCatImageTextureJob *job,
        unsigned int *texture, int *width, int *height,
        BongoCatImageAlphaMask *alpha, BongoCatError *error);
    void (*image_texture_job_cancel)(BongoCatImageTextureJob *job);
    int (*image_texture_job_cleanup_poll)(BongoCatImageTextureJob *job,
        BongoCatError *error);
    void (*image_texture_job_destroy)(BongoCatImageTextureJob *job);
    yyjson_doc *(*model_json_parse)(const char *data, size_t size,
        bool *normalized);
    void (*model_memory_log)(const char *stage, const char *format, ...);
    double (*model_texture_mib)(int width, int height, bool mipmaps);
    void (*platform_trim_memory)(void);
    bool (*platform_memory_usage)(BongoCatMemoryUsage *usage);
    void (*resource_trace_atlas)(double mib);
    void (*resource_trace_render)(double mask_mib, unsigned pool_targets,
        int width, int height);
    void (*sha256_bytes)(const void *data, size_t size, char output[65]);
    BongoCatResult (*sha256_file)(const char *path, char output[65],
        BongoCatError *error);
    /* Current Cubism Core handle, queried per call so a Core imported after
       the library was loaded still reaches the Cubism Core shim. */
    void *(*core_library)(void);
    /* SDL-compatible services; see the note above this struct. */
    void (*log)(int category, int priority, const char *format, ...);
    void *(*gl_current_context)(void);
    void *(*gl_current_window)(void);
    const char *(*base_path)(void);
    bool (*path_info)(const char *path, BongoCatLive2DPathInfo *info);
    void *(*thread_start)(int (*entry)(void *), const char *name,
        void *userdata);
    int (*thread_state)(void *thread);
    void (*thread_wait)(void *thread, int *result);
    void (*delay_ms)(unsigned int milliseconds);
    unsigned long long (*ticks_ns)(void);
} BongoCatLive2DBackendHost;

typedef struct BongoCatLive2DBackendInit {
    uint32_t abi_version;
    const BongoCatLive2DBackendHost *host;
    /* Already loaded Cubism Core shared library (dlopen/LoadLibrary handle),
       or NULL when no Core is available yet. */
    void *core_library;
} BongoCatLive2DBackendInit;

/* Exported by the backend shared library. abi_version() must equal
   BONGO_CAT_LIVE2D_BACKEND_ABI_VERSION; init() receives the host table and the
   Core handle before any api() member is called. Both pointers stay valid for
   the lifetime of the loaded library. */
uint32_t bongo_cat_live2d_backend_abi(void);
const BongoCatLive2DBackendApi *bongo_cat_live2d_backend_api(void);
bool bongo_cat_live2d_backend_init(const BongoCatLive2DBackendInit *init);

/* Executable side: load the backend shared library for these directories and
   install its API table into the Live2D dispatch layer. Safe to call again
   after a Core import; later calls are no-ops once a library is loaded.
   Returns false when no compatible library is present. */
bool bongo_cat_live2d_backend_load(const char *exe_dir, const char *data_dir);
/* True when the API table is installed and Live2D calls can render. */
bool bongo_cat_live2d_backend_ready(void);

#ifdef __cplusplus
}
#endif
#endif
