/* Runtime discovery of the Live2D backend shared library. The executable never
   links the Cubism SDK: Live2D-capable builds ship bongo-cat-live2d-backend
   next to the executable, and a user can drop the same library into a live2d
   folder next to the application or inside the data directory. Loading is best
   effort — without the library the diagnostic stub keeps the pet running and
   the settings window reports which part is missing. */
#include "bongo_cat/live2d_backend.h"
#include "bongo_cat/log.h"
#include "live2d_dispatch.h"
#include "bongo_cat/path.h"
#include "bongo_cat/platform.h"

#include <SDL3/SDL.h>

#include <stddef.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#if defined(__APPLE__)
#define BACKEND_LIBRARY_FILE "libbongo-cat-live2d-backend.dylib"
#elif defined(_WIN32)
#define BACKEND_LIBRARY_FILE "bongo-cat-live2d-backend.dll"
#else
#define BACKEND_LIBRARY_FILE "libbongo-cat-live2d-backend.so"
#endif

typedef uint32_t (*BackendAbiFunction)(void);
typedef const BongoCatLive2DBackendApi *(*BackendApiFunction)(void);
typedef bool (*BackendInitFunction)(const BongoCatLive2DBackendInit *);

static BongoCatLive2DBackendHost backend_host;

/* SDL-backed service wrappers (defined below) so the backend library does not
   have to link its own copy of SDL. */
static void host_log(int category, int priority, const char *format, ...);
static void *host_gl_context(void);
static void *host_gl_window(void);
static const char *host_base_path(void);
static bool host_path_info(const char *path, BongoCatLive2DPathInfo *info);
static void *host_thread_start(int (*entry)(void *), const char *name,
    void *userdata);
static int host_thread_state(void *thread);
static void host_thread_wait(void *thread, int *result);
static void host_delay_ms(unsigned int milliseconds);
static unsigned long long host_ticks_ns(void);

static void fill_host(BongoCatLive2DBackendHost *host) {
    host->error_set = bongo_cat_error_set;
    host->file_open = bongo_cat_file_open;
    host->gl_clear_errors = bongo_cat_gl_clear_errors;
    host->image_info = bongo_cat_image_info;
    host->image_texture_model = bongo_cat_image_texture_model;
    host->image_forget_texture_cache = bongo_cat_image_forget_texture_cache;
    host->image_texture_model_scaled_cached =
        bongo_cat_image_texture_model_scaled_cached;
    host->image_texture_job_start = bongo_cat_image_texture_job_start;
    host->image_texture_job_needs_poll = bongo_cat_image_texture_job_needs_poll;
    host->image_texture_job_poll = bongo_cat_image_texture_job_poll;
    host->image_texture_job_cancel = bongo_cat_image_texture_job_cancel;
    host->image_texture_job_cleanup_poll =
        bongo_cat_image_texture_job_cleanup_poll;
    host->image_texture_job_destroy = bongo_cat_image_texture_job_destroy;
    host->model_json_parse = bongo_cat_model_json_parse;
    host->model_memory_log = bongo_cat_model_memory_log;
    host->model_texture_mib = bongo_cat_model_texture_mib;
    host->platform_trim_memory = bongo_cat_platform_trim_memory;
    host->platform_memory_usage = bongo_cat_platform_memory_usage;
    host->resource_trace_atlas = bongo_cat_resource_trace_atlas;
    host->resource_trace_render = bongo_cat_resource_trace_render;
    host->sha256_bytes = bongo_cat_sha256_bytes;
    host->sha256_file = bongo_cat_sha256_file;
    /* The plugin asks for the Core on every call, so a core imported after
       the library was loaded still reaches the Cubism Core shim. */
    host->core_library = bongo_cat_platform_live2d_core_library;
    /* SDL services: the backend library links no copy of SDL, so these come
       from the process that owns the GL context and the log. */
    host->log = host_log;
    host->gl_current_context = host_gl_context;
    host->gl_current_window = host_gl_window;
    host->base_path = host_base_path;
    host->path_info = host_path_info;
    host->thread_start = host_thread_start;
    host->thread_state = host_thread_state;
    host->thread_wait = host_thread_wait;
    host->delay_ms = host_delay_ms;
    host->ticks_ns = host_ticks_ns;
}

/* SDL log categories and priorities are ints in SDL; the backend uses the
   portable constants of include/bongo_cat/live2d_backend.h so the mapping
   stays in one place. */
static void host_log(int category, int priority, const char *format, ...) {
    int sdl_category = SDL_LOG_CATEGORY_APPLICATION;
    if (category == BONGO_CAT_LIVE2D_LOG_CATEGORY_RENDER) {
        sdl_category = SDL_LOG_CATEGORY_RENDER;
    } else if (category == BONGO_CAT_LIVE2D_LOG_CATEGORY_VIDEO) {
        sdl_category = SDL_LOG_CATEGORY_VIDEO;
    }
    SDL_LogPriority sdl_priority = (SDL_LogPriority)priority;
    va_list args;
    va_start(args, format);
    SDL_LogMessageV(sdl_category, sdl_priority, format, args);
    va_end(args);
}

static void *host_gl_context(void) {
    return (void *)SDL_GL_GetCurrentContext();
}

static void *host_gl_window(void) {
    return (void *)SDL_GL_GetCurrentWindow();
}

static const char *host_base_path(void) {
    return SDL_GetBasePath();
}

static bool host_path_info(const char *path, BongoCatLive2DPathInfo *info) {
    SDL_PathInfo details;
    if (!path || !info || !SDL_GetPathInfo(path, &details)) return false;
    info->size = (unsigned long long)details.size;
    info->modify_time = (long long)details.modify_time;
    info->create_time = (long long)details.create_time;
    switch (details.type) {
    case SDL_PATHTYPE_FILE: info->type = BONGO_CAT_LIVE2D_PATH_TYPE_FILE; break;
    case SDL_PATHTYPE_DIRECTORY:
        info->type = BONGO_CAT_LIVE2D_PATH_TYPE_DIRECTORY;
        break;
    default: info->type = BONGO_CAT_LIVE2D_PATH_TYPE_OTHER; break;
    }
    return true;
}

static void *host_thread_start(int (*entry)(void *), const char *name,
    void *userdata) {
    /* SDL spells its thread entry pointer with its own calling convention; the
       host signature stays portable so the ABI header needs no SDL types. */
#ifdef _WIN32
    return (void *)SDL_CreateThread((SDL_ThreadFunction)entry, name, userdata);
#else
    return (void *)SDL_CreateThread(entry, name, userdata);
#endif
}

static int host_thread_state(void *thread) {
    SDL_ThreadState state = SDL_GetThreadState((SDL_Thread *)thread);
    return state == SDL_THREAD_ALIVE ? BONGO_CAT_LIVE2D_THREAD_ALIVE : 0;
}

static void host_thread_wait(void *thread, int *result) {
    SDL_WaitThread((SDL_Thread *)thread, result);
}

static void host_delay_ms(unsigned int milliseconds) {
    SDL_Delay(milliseconds);
}

static unsigned long long host_ticks_ns(void) {
    return SDL_GetTicksNS();
}

static void *open_library(const char *path) {
#ifdef _WIN32
    wchar_t wide[BONGO_CAT_PATH_CAP];
    int converted = MultiByteToWideChar(CP_UTF8, 0, path, -1, wide,
        (int)SDL_arraysize(wide));
    if (converted <= 0) {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
            "Live2D backend path is too long to load: %s", path);
        return NULL;
    }
    return (void *)LoadLibraryW(wide);
#else
    return dlopen(path, RTLD_NOW | RTLD_LOCAL);
#endif
}

static void close_library(void *library) {
#ifdef _WIN32
    FreeLibrary((HMODULE)library);
#else
    dlclose(library);
#endif
}

static const char *load_error_message(void) {
#ifdef _WIN32
    static char message[192];
    DWORD code = GetLastError();
    SDL_snprintf(message, sizeof(message), "win32 error %lu", code);
    return message;
#else
    const char *message = dlerror();
    return message ? message : "unknown loader error";
#endif
}

static void *resolve_symbol(void *library, const char *name) {
#ifdef _WIN32
    return (void *)GetProcAddress((HMODULE)library, name);
#else
    return dlsym(library, name);
#endif
}

static bool library_path(char *path, size_t size, const char *directory) {
    return bongo_cat_path_join(path, size, directory, BACKEND_LIBRARY_FILE);
}

static bool load_library(const char *path) {
    void *library = open_library(path);
    if (!library) {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
            "Live2D backend candidate could not be loaded: %s (%s)", path,
            load_error_message());
        return false;
    }
    BackendAbiFunction abi = (BackendAbiFunction)resolve_symbol(library,
        "bongo_cat_live2d_backend_abi");
    BackendApiFunction api = (BackendApiFunction)resolve_symbol(library,
        "bongo_cat_live2d_backend_api");
    BackendInitFunction init = (BackendInitFunction)resolve_symbol(library,
        "bongo_cat_live2d_backend_init");
    if (!abi || !api || !init) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
            "Live2D backend is incompatible: %s is missing an entry point",
            path);
        close_library(library);
        return false;
    }
    if (abi() != BONGO_CAT_LIVE2D_BACKEND_ABI_VERSION) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
            "Live2D backend is incompatible: %s reports ABI %" SDL_PRIu32
            ", this build speaks ABI %d", path, abi(),
            BONGO_CAT_LIVE2D_BACKEND_ABI_VERSION);
        close_library(library);
        return false;
    }
    BongoCatLive2DBackendInit init_info = {0};
    init_info.abi_version = BONGO_CAT_LIVE2D_BACKEND_ABI_VERSION;
    init_info.host = &backend_host;
    /* Passed once for convenience; the plugin keeps asking the host member so a
       Core imported later still reaches the Cubism Core shim. */
    init_info.core_library = backend_host.core_library
        ? backend_host.core_library()
        : NULL;
    if (!init(&init_info)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
            "Live2D backend rejected the host table: %s", path);
        close_library(library);
        return false;
    }
    bongo_cat_live2d_dispatch_install(api());
    /* The default log threshold hides INFO on the application category, so the
       renderer handover is reported on the lifecycle channel instead: an
       SDK-built install shows one line proving the renderer is live. */
    SDL_LogInfo(BONGO_CAT_LOG_LIFECYCLE, "Live2D backend loaded: %s", path);
    /* The library is intentionally never unloaded: the Cubism Framework keeps
       process-wide state for the lifetime of the renderer. */
    return true;
}

static bool load_candidate(const char *directory) {
    char path[BONGO_CAT_PATH_CAP];
    if (!directory || !directory[0]) return false;
    if (!library_path(path, sizeof(path), directory)) return false;
    if (!bongo_cat_path_is_file(path)) return false;
    return load_library(path);
}

static void resolve_base(const char *exe_dir, char *base, size_t size,
    const char **directory) {
    *directory = exe_dir;
    if (directory && *directory && (*directory)[0]) return;
    /* SDL_GetBasePath() returns SDL's own cached string: copy it, never free
       it — releasing it would leave later callers with a dangling pointer. */
    const char *sdl_base = SDL_GetBasePath();
    if (!sdl_base) return;
    SDL_strlcpy(base, sdl_base, size);
    *directory = base;
}

bool bongo_cat_live2d_backend_load(const char *exe_dir, const char *data_dir) {
    if (bongo_cat_live2d_dispatch_installed()) return true;
    fill_host(&backend_host);
    char base[BONGO_CAT_PATH_CAP] = "";
    const char *directory = NULL;
    resolve_base(exe_dir, base, sizeof(base), &directory);
    char folder[BONGO_CAT_PATH_CAP];
    char data_folder[BONGO_CAT_PATH_CAP];
    if (load_candidate(directory)) return true;
    if (directory && directory[0]
        && bongo_cat_path_join(folder, sizeof(folder), directory, "live2d")
        && load_candidate(folder))
        return true;
    if (data_dir && data_dir[0]
        && bongo_cat_path_join(data_folder, sizeof(data_folder), data_dir,
            "live2d")
        && load_candidate(data_folder))
        return true;
    return false;
}

bool bongo_cat_live2d_backend_ready(void) {
    return bongo_cat_live2d_dispatch_installed();
}
