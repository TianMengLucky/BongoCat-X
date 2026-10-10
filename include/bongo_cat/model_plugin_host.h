#ifndef BONGO_CAT_MODEL_PLUGIN_HOST_H
#define BONGO_CAT_MODEL_PLUGIN_HOST_H
#include "bongo_cat/model_plugin.h"
#include <SDL3/SDL.h>
#include "bongo_cat/file.h"
#include "bongo_cat/gl_api.h"
#include "bongo_cat/image.h"
#include "bongo_cat/image_texture_job.h"
#include "bongo_cat/json.h"
#include "bongo_cat/model_memory.h"
#include "bongo_cat/memory.h"
#include "bongo_cat/platform.h"
#include "bongo_cat/resource_trace.h"
#include "bongo_cat/safe_ffi.h"
#include "bongo_cat/sha256.h"

/* Services stay owned by the host; plugins never link another SDL runtime.
   Every allocation is released through the service which created it. */
typedef struct BongoCatModelPluginHost {
    uint32_t abi_version, struct_size;
    void *(*core_symbol)(const char *name);
    void *(*gl_proc)(const char *name);
    void (*log)(int priority, const char *message);
    bool (*device_info)(BongoCatRhiDeviceInfo *info);
    bool (*vulkan_frame)(const BongoCatRhi *rhi, BongoCatRhiVulkanFrameInfo *info);
    bool (*metal_frame)(const BongoCatRhi *rhi, BongoCatRhiMetalFrameInfo *info);
    void *(*begin_commands)(const BongoCatRhi *rhi);
    bool (*submit_commands)(const BongoCatRhi *rhi, void *command);
    bool (*wait_idle)(const BongoCatRhi *rhi);
    SDL_Thread * (*SDL_CreateThread)(SDL_ThreadFunction fn, const char *name, void *data);
    void (SDLCALL *SDL_Delay)(Uint32 ms);
    SDL_GLContext (SDLCALL *SDL_GL_GetCurrentContext)(void);
    SDL_Window * (SDLCALL *SDL_GL_GetCurrentWindow)(void);
    const char * (SDLCALL *SDL_GetBasePath)(void);
    bool (SDLCALL *SDL_GetPathInfo)(const char *path, SDL_PathInfo *info);
    SDL_ThreadState (SDLCALL *SDL_GetThreadState)(SDL_Thread *thread);
    Uint64 (SDLCALL *SDL_GetTicksNS)(void);
    void (SDLCALL *SDL_Log)(SDL_PRINTF_FORMAT_STRING const char *fmt, ...);
    void (SDLCALL *SDL_LogDebug)(int category, SDL_PRINTF_FORMAT_STRING const char *fmt, ...);
    void (SDLCALL *SDL_LogError)(int category, SDL_PRINTF_FORMAT_STRING const char *fmt, ...);
    void (SDLCALL *SDL_LogInfo)(int category, SDL_PRINTF_FORMAT_STRING const char *fmt, ...);
    void (SDLCALL *SDL_LogWarn)(int category, SDL_PRINTF_FORMAT_STRING const char *fmt, ...);
    void (SDLCALL *SDL_WaitThread)(SDL_Thread *thread, int *status);
    void (*bongo_cat_error_set)(BongoCatError *error, BongoCatResult code, const char *format, ...);
    bool (*bongo_cat_gl_clear_errors)(void);
    void (*bongo_cat_image_forget_texture_cache)(const char *digest, int max_width, int max_height);
    void (*bongo_cat_image_free)(BongoCatImage *image);
    bool (*bongo_cat_image_info)(const char *path, int *width, int *height);
    void (*bongo_cat_image_make_alpha_mask)(const BongoCatImage *image, BongoCatImageAlphaMask *mask);
    bool (*bongo_cat_image_resize_rgba_take)(BongoCatImage *source, int max_width, int max_height, BongoCatImage *target, BongoCatError *error);
    void (*bongo_cat_image_texture_job_cancel)(BongoCatImageTextureJob *job);
    int (*bongo_cat_image_texture_job_cleanup_poll)(BongoCatImageTextureJob *job, BongoCatError *error);
    void (*bongo_cat_image_texture_job_destroy)(BongoCatImageTextureJob *job);
    bool (*bongo_cat_image_texture_job_needs_poll)(BongoCatImageTextureJob *job);
    int (*bongo_cat_image_texture_job_poll)(BongoCatImageTextureJob *job, unsigned int *texture, int *width, int *height, BongoCatImageAlphaMask *alpha, BongoCatError *error);
    BongoCatImageTextureJob * (*bongo_cat_image_texture_job_start)(const char *path, int max_width, int max_height, BongoCatError *error);
    unsigned int (*bongo_cat_image_texture_model)(const char *path, bool direct_decode, int *width, int *height, BongoCatImageAlphaMask *alpha, BongoCatImageProgress progress, void *userdata, BongoCatError *error);
    unsigned int (*bongo_cat_image_texture_model_scaled_cached)(const char *path, const char *digest, bool direct_decode, int max_width, int max_height, int *width, int *height, BongoCatImageAlphaMask *alpha, BongoCatImageProgress progress, void *userdata, BongoCatError *error);
    BongoJsonDoc * (*bongo_cat_model_json_parse)(const char *data, size_t size, bool *normalized);
    void (*bongo_cat_model_memory_log)(const char *stage, const char *format, ...);
    double (*bongo_cat_model_texture_mib)(int width, int height, bool mipmaps);
    bool (*bongo_cat_platform_memory_usage)(BongoCatMemoryUsage *usage);
    void (*bongo_cat_platform_trim_memory)(void);
    void (*bongo_cat_resource_trace_atlas)(double mib);
    void (*bongo_cat_resource_trace_render)(double mask_mib, unsigned pool_targets, int width, int height);
    bool (*bongo_cat_rhi_active_is_gl)(void);
    void * (*bongo_cat_rhi_begin_commands)(const BongoCatRhi *rhi);
    bool (*bongo_cat_rhi_get_active_device_info)(BongoCatRhiDeviceInfo *info);
    bool (*bongo_cat_rhi_get_metal_frame_info)(const BongoCatRhi *rhi, BongoCatRhiMetalFrameInfo *info);
    bool (*bongo_cat_rhi_get_vulkan_frame_info)(const BongoCatRhi *rhi, BongoCatRhiVulkanFrameInfo *info);
    bool (*bongo_cat_rhi_submit_commands_checked)(const BongoCatRhi *rhi, void *command);
    bool (*bongo_cat_rhi_wait_idle)(const BongoCatRhi *rhi);
    void (*bongo_cat_sha256_bytes)(const void *data, size_t size, char output[65]);
    BongoCatResult (*bongo_cat_sha256_file)(const char *path, char output[65], BongoCatError *error);
    bool (*bongo_safe_expression_parse)(const unsigned char *data, size_t size, BongoSafeExpression **out_expression);
    void (*bongo_safe_free_expression)(BongoSafeExpression *expression);
    void (*bongo_safe_free_pixels)(unsigned char *pixels, size_t byte_count);
    unsigned char * (*bongo_safe_image_decode)(const unsigned char *data, size_t size, int *width, int *height);
    BongoJsonValue * (*bongo_json_arr_get)(const BongoJsonValue *value, size_t index);
    size_t (*bongo_json_arr_size)(const BongoJsonValue *value);
    void (*bongo_json_doc_free)(BongoJsonDoc *doc);
    BongoJsonValue * (*bongo_json_doc_get_root)(const BongoJsonDoc *doc);
    void (*bongo_json_free_text)(char *text);
    int64_t (*bongo_json_get_int)(const BongoJsonValue *value);
    double (*bongo_json_get_num)(const BongoJsonValue *value);
    const char * (*bongo_json_get_str)(const BongoJsonValue *value);
    bool (*bongo_json_is_arr)(const BongoJsonValue *value);
    bool (*bongo_json_is_int)(const BongoJsonValue *value);
    bool (*bongo_json_is_num)(const BongoJsonValue *value);
    bool (*bongo_json_is_obj)(const BongoJsonValue *value);
    bool (*bongo_json_is_str)(const BongoJsonValue *value);
    bool (*bongo_json_is_uint)(const BongoJsonValue *value);
    BongoJsonValue * (*bongo_json_obj_get)(const BongoJsonValue *obj, const char *key);
    BongoJsonValue * (*bongo_json_obj_key_at)(const BongoJsonValue *value, size_t index);
    BongoJsonValue * (*bongo_json_obj_value_at)(const BongoJsonValue *value, size_t index);
    size_t (*bongo_json_obj_size)(const BongoJsonValue *value);
    BongoJsonDoc * (*bongo_json_read)(const char *data, size_t size, BongoJsonReadFlags flags);
    BongoJsonDoc * (*bongo_json_read_opts)(const char *data, size_t size, BongoJsonReadFlags flags, const void *allocator, BongoJsonReadError *error);
    char * (*bongo_json_write)(const BongoJsonDoc *doc, BongoJsonWriteFlags flags, size_t *length);
} BongoCatModelPluginHost;
#ifdef __cplusplus
extern "C" {
#endif
const BongoCatModelPluginHost *bongo_cat_model_plugin_host(void);
#ifdef __cplusplus
}
#endif
#endif
