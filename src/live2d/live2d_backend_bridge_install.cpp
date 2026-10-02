/* Test-side bridge installation. Live2D tests link the Cubism bridge objects
   directly instead of opening the shared library, so the versioned API table is
   installed from a load-time initializer, with the host services taken from the
   test binary itself. Only linked into the Live2D tests, never into the
   executable. */
#include "live2d_dispatch.h"
#include "live2d_backend_table.h"

#include "bongo_cat/platform.h"

#include <SDL3/SDL.h>

/* These mirror the loader in live2d_backend_load.c: the executable serves the
   project services and the SDL helpers the bridge reaches through
   live2d_backend_sdl.h from its own SDL, so the tests do the same. */

static void test_host_log(int category, int priority, const char *format, ...) {
    int sdl_category = SDL_LOG_CATEGORY_APPLICATION;
    if (category == BONGO_CAT_LIVE2D_LOG_CATEGORY_RENDER) {
        sdl_category = SDL_LOG_CATEGORY_RENDER;
    } else if (category == BONGO_CAT_LIVE2D_LOG_CATEGORY_VIDEO) {
        sdl_category = SDL_LOG_CATEGORY_VIDEO;
    }
    va_list args;
    va_start(args, format);
    SDL_LogMessageV(sdl_category, (SDL_LogPriority)priority, format, args);
    va_end(args);
}

static void *test_host_gl_context(void) {
    return (void *)SDL_GL_GetCurrentContext();
}

static void *test_host_gl_window(void) {
    return (void *)SDL_GL_GetCurrentWindow();
}

static const char *test_host_base_path(void) {
    return SDL_GetBasePath();
}

static bool test_host_path_info(const char *path,
    BongoCatLive2DPathInfo *info) {
    SDL_PathInfo details;
    if (!path || !info || !SDL_GetPathInfo(path, &details)) return false;
    info->size = (unsigned long long)details.size;
    info->modify_time = (long long)details.modify_time;
    info->create_time = (long long)details.create_time;
    switch (details.type) {
    case SDL_PATHTYPE_FILE:
        info->type = BONGO_CAT_LIVE2D_PATH_TYPE_FILE;
        break;
    case SDL_PATHTYPE_DIRECTORY:
        info->type = BONGO_CAT_LIVE2D_PATH_TYPE_DIRECTORY;
        break;
    default:
        info->type = BONGO_CAT_LIVE2D_PATH_TYPE_OTHER;
        break;
    }
    return true;
}

static void *test_host_thread_start(int (*entry)(void *), const char *name,
    void *userdata) {
#ifdef _WIN32
    return (void *)SDL_CreateThread((SDL_ThreadFunction)entry, name, userdata);
#else
    return (void *)SDL_CreateThread(entry, name, userdata);
#endif
}

static int test_host_thread_state(void *thread) {
    return SDL_GetThreadState((SDL_Thread *)thread) == SDL_THREAD_ALIVE
        ? BONGO_CAT_LIVE2D_THREAD_ALIVE
        : 0;
}

static void test_host_thread_wait(void *thread, int *result) {
    SDL_WaitThread((SDL_Thread *)thread, result);
}

static void test_host_delay_ms(unsigned int milliseconds) {
    SDL_Delay(milliseconds);
}

static unsigned long long test_host_ticks_ns(void) {
    return SDL_GetTicksNS();
}

static bool install_bridge_table(void) {
    BongoCatLive2DBackendHost host;
    host.error_set = bongo_cat_error_set;
    host.file_open = bongo_cat_file_open;
    host.gl_clear_errors = bongo_cat_gl_clear_errors;
    host.image_info = bongo_cat_image_info;
    host.image_texture_model = bongo_cat_image_texture_model;
    host.image_forget_texture_cache = bongo_cat_image_forget_texture_cache;
    host.image_texture_model_scaled_cached =
        bongo_cat_image_texture_model_scaled_cached;
    host.image_texture_job_start = bongo_cat_image_texture_job_start;
    host.image_texture_job_needs_poll = bongo_cat_image_texture_job_needs_poll;
    host.image_texture_job_poll = bongo_cat_image_texture_job_poll;
    host.image_texture_job_cancel = bongo_cat_image_texture_job_cancel;
    host.image_texture_job_cleanup_poll =
        bongo_cat_image_texture_job_cleanup_poll;
    host.image_texture_job_destroy = bongo_cat_image_texture_job_destroy;
    host.model_json_parse = bongo_cat_model_json_parse;
    host.model_memory_log = bongo_cat_model_memory_log;
    host.model_texture_mib = bongo_cat_model_texture_mib;
    host.platform_trim_memory = bongo_cat_platform_trim_memory;
    host.platform_memory_usage = bongo_cat_platform_memory_usage;
    host.resource_trace_atlas = bongo_cat_resource_trace_atlas;
    host.resource_trace_render = bongo_cat_resource_trace_render;
    host.sha256_bytes = bongo_cat_sha256_bytes;
    host.sha256_file = bongo_cat_sha256_file;
    host.core_library = bongo_cat_platform_live2d_core_library;
    host.log = test_host_log;
    host.gl_current_context = test_host_gl_context;
    host.gl_current_window = test_host_gl_window;
    host.base_path = test_host_base_path;
    host.path_info = test_host_path_info;
    host.thread_start = test_host_thread_start;
    host.thread_state = test_host_thread_state;
    host.thread_wait = test_host_thread_wait;
    host.delay_ms = test_host_delay_ms;
    host.ticks_ns = test_host_ticks_ns;
    BongoCatLive2DBackendInit init;
    init.abi_version = BONGO_CAT_LIVE2D_BACKEND_ABI_VERSION;
    init.host = &host;
    init.core_library = host.core_library ? host.core_library() : NULL;
    if (!bongo_cat_live2d_backend_init(&init)) return false;
    bongo_cat_live2d_dispatch_install(bongo_cat_live2d_backend_api());
    return true;
}

static bool bridge_table_installed = install_bridge_table();

/* Keeps the initializer object referenced so it is never dropped, and gives
   the Live2D tests a way to assert the bridge really was installed. */
extern "C" bool bongo_cat_live2d_bridge_table_installed(void) {
    return bridge_table_installed;
}
