/* Side of the plugin boundary that lives inside the backend shared library:
   it answers the host calls the Cubism bridge makes, re-exports the bridge
   through the versioned API table, hands the Cubism Core handle to the Core
   shim, and on Windows installs the delay-load failure hook that reports a
   missing Core.

   The bridge calls the project's service functions by their ordinary names
   (bongo_cat_error_set, bongo_cat_image_texture_model, ...). Those functions
   live in the executable, which the shared library cannot resolve at load time,
   so definitions with the same names and signatures forward every call into the
   host table installed by bongo_cat_live2d_backend_init. This translation unit
   is compiled only for the plugin (and the tests), never for the executable. */
#include "bongo_cat/live2d_backend.h"
#include "live2d_backend_table.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#include <delayimp.h>
#else
/* The Cubism Core shim resolves the csm* symbols through this handle, so the
   declaration must match the POSIX platform one. */
extern "C" void *bongo_cat_posix_live2d_core_library(void);
#endif

static BongoCatLive2DBackendHost backend_host;

/* Host forwarders. Every member is resolved at init time; the guards keep a
   partially initialized table from crashing the process, which cannot happen
   with the shipped loader. */

extern "C" void bongo_cat_error_set(BongoCatError *error, BongoCatResult code,
    const char *format, ...) {
    if (!backend_host.error_set) return;
    va_list args;
    va_start(args, format);
    char message[1024];
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    backend_host.error_set(error, code, "%s", message);
}

extern "C" FILE *bongo_cat_file_open(const char *path, const char *mode) {
    if (!backend_host.file_open) return NULL;
    return backend_host.file_open(path, mode);
}

extern "C" bool bongo_cat_gl_clear_errors(void) {
    if (!backend_host.gl_clear_errors) return false;
    return backend_host.gl_clear_errors();
}

extern "C" bool bongo_cat_image_info(const char *path, int *width,
    int *height) {
    if (!backend_host.image_info) return false;
    return backend_host.image_info(path, width, height);
}

extern "C" unsigned int bongo_cat_image_texture_model(const char *path,
    bool direct_decode, int *width, int *height,
    BongoCatImageAlphaMask *alpha, BongoCatImageProgress progress,
    void *userdata, BongoCatError *error) {
    if (!backend_host.image_texture_model) return 0u;
    return backend_host.image_texture_model(path, direct_decode, width, height,
        alpha, progress, userdata, error);
}

extern "C" void bongo_cat_image_forget_texture_cache(const char *digest,
    int max_width, int max_height) {
    if (!backend_host.image_forget_texture_cache) return;
    backend_host.image_forget_texture_cache(digest, max_width, max_height);
}

extern "C" unsigned int bongo_cat_image_texture_model_scaled_cached(
    const char *path, const char *digest, bool direct_decode, int max_width,
    int max_height, int *width, int *height, BongoCatImageAlphaMask *alpha,
    BongoCatImageProgress progress, void *userdata, BongoCatError *error) {
    if (!backend_host.image_texture_model_scaled_cached) return 0u;
    return backend_host.image_texture_model_scaled_cached(path, digest,
        direct_decode, max_width, max_height, width, height, alpha, progress,
        userdata, error);
}

extern "C" BongoCatImageTextureJob *bongo_cat_image_texture_job_start(
    const char *path, int max_width, int max_height, BongoCatError *error) {
    if (!backend_host.image_texture_job_start) return NULL;
    return backend_host.image_texture_job_start(path, max_width, max_height,
        error);
}

extern "C" bool bongo_cat_image_texture_job_needs_poll(
    BongoCatImageTextureJob *job) {
    if (!backend_host.image_texture_job_needs_poll) return false;
    return backend_host.image_texture_job_needs_poll(job);
}

extern "C" int bongo_cat_image_texture_job_poll(BongoCatImageTextureJob *job,
    unsigned int *texture, int *width, int *height,
    BongoCatImageAlphaMask *alpha, BongoCatError *error) {
    if (!backend_host.image_texture_job_poll) return -1;
    return backend_host.image_texture_job_poll(job, texture, width, height,
        alpha, error);
}

extern "C" void bongo_cat_image_texture_job_cancel(
    BongoCatImageTextureJob *job) {
    if (!backend_host.image_texture_job_cancel) return;
    backend_host.image_texture_job_cancel(job);
}

extern "C" int bongo_cat_image_texture_job_cleanup_poll(
    BongoCatImageTextureJob *job, BongoCatError *error) {
    if (!backend_host.image_texture_job_cleanup_poll) return -1;
    return backend_host.image_texture_job_cleanup_poll(job, error);
}

extern "C" void bongo_cat_image_texture_job_destroy(
    BongoCatImageTextureJob *job) {
    if (!backend_host.image_texture_job_destroy) return;
    backend_host.image_texture_job_destroy(job);
}

extern "C" yyjson_doc *bongo_cat_model_json_parse(const char *data,
    size_t size, bool *normalized) {
    if (!backend_host.model_json_parse) return NULL;
    return backend_host.model_json_parse(data, size, normalized);
}

extern "C" void bongo_cat_model_memory_log(const char *stage,
    const char *format, ...) {
    if (!backend_host.model_memory_log) return;
    va_list args;
    va_start(args, format);
    char message[1200];
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    backend_host.model_memory_log(stage, "%s", message);
}

extern "C" double bongo_cat_model_texture_mib(int width, int height,
    bool mipmaps) {
    if (!backend_host.model_texture_mib) return 0.0;
    return backend_host.model_texture_mib(width, height, mipmaps);
}

extern "C" void bongo_cat_platform_trim_memory(void) {
    if (!backend_host.platform_trim_memory) return;
    backend_host.platform_trim_memory();
}

extern "C" bool bongo_cat_platform_memory_usage(BongoCatMemoryUsage *usage) {
    if (!backend_host.platform_memory_usage) return false;
    return backend_host.platform_memory_usage(usage);
}

extern "C" void bongo_cat_resource_trace_atlas(double mib) {
    if (!backend_host.resource_trace_atlas) return;
    backend_host.resource_trace_atlas(mib);
}

extern "C" void bongo_cat_resource_trace_render(double mask_mib,
    unsigned pool_targets, int width, int height) {
    if (!backend_host.resource_trace_render) return;
    backend_host.resource_trace_render(mask_mib, pool_targets, width, height);
}

extern "C" void bongo_cat_sha256_bytes(const void *data, size_t size,
    char output[65]) {
    if (!backend_host.sha256_bytes) return;
    backend_host.sha256_bytes(data, size, output);
}

extern "C" BongoCatResult bongo_cat_sha256_file(const char *path,
    char output[65], BongoCatError *error) {
    if (!backend_host.sha256_file) return BONGO_CAT_ERROR_IO;
    return backend_host.sha256_file(path, output, error);
}

#if !defined(_WIN32)
extern "C" void *bongo_cat_posix_live2d_core_library(void) {
    if (!backend_host.core_library) return NULL;
    return backend_host.core_library();
}
#else
/* Windows resolves the Core by name through the loader; the hook only exists
   to report a missing or incompatible Core DLL instead of failing silently.
   The library deliberately depends on kernel32 alone. */
static FARPROC WINAPI delay_load_failure_hook(unsigned notify,
    PDelayLoadInfo info) {
    if (notify == dliFailLoadLib && info && info->szDll) {
        OutputDebugStringA("BongoCat Live2D backend: Cubism Core missing: ");
        OutputDebugStringA(info->szDll);
    }
    return NULL;
}
extern "C" const PfnDliHook __pfnDliFailureHook2 = delay_load_failure_hook;
#endif

extern "C" uint32_t bongo_cat_live2d_backend_abi(void) {
    return BONGO_CAT_LIVE2D_BACKEND_ABI_VERSION;
}

/* live2d_backend_sdl.h routes the SDL helpers of the Cubism bridge through the
   table installed by init(), so the library itself needs no SDL. */
extern "C" const BongoCatLive2DBackendHost *bongo_cat_live2d_backend_host(
    void) {
    return &backend_host;
}

extern "C" bool bongo_cat_live2d_backend_init(
    const BongoCatLive2DBackendInit *init) {
    if (!init || init->abi_version != BONGO_CAT_LIVE2D_BACKEND_ABI_VERSION
        || !init->host)
        return false;
    backend_host = *init->host;
    return true;
}

extern "C" const BongoCatLive2DBackendApi *bongo_cat_live2d_backend_api(
    void) {
    return &bongo_cat_live2d_backend_table;
}
