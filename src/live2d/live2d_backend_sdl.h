/* Redirects the SDL helpers the Cubism bridge uses to host services.
 *
 * The backend library must stay linkable without SDL: a second copy of SDL
 * inside the process would not see the application's GL context, log output or
 * thread state. Every SDL call the bridge makes is therefore served from
 * BongoCatLive2DBackendHost (see include/bongo_cat/live2d_backend.h), and the
 * bridge translation units include this header instead of <SDL3/...>.
 *
 * Only the subset the bridge actually uses is provided; names intentionally
 * match SDL so the bridge sources stay untouched apart from the include. */
#pragma once

#include "bongo_cat/live2d_backend.h"
#include "live2d_backend_table.h"

#include <stdarg.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void *SDL_GLContext;
typedef void *SDL_Thread;

/* SDL spells thread entry points with its calling convention; the backend host
   table passes plain C function pointers instead. */
#ifdef _WIN32
#define SDLCALL __cdecl
#else
#define SDLCALL
#endif

/* Values must match the host's log category and priority mapping. */
#define SDL_LOG_CATEGORY_APPLICATION \
    BONGO_CAT_LIVE2D_LOG_CATEGORY_APPLICATION
#define SDL_LOG_CATEGORY_RENDER BONGO_CAT_LIVE2D_LOG_CATEGORY_RENDER
#define SDL_LOG_CATEGORY_VIDEO BONGO_CAT_LIVE2D_LOG_CATEGORY_VIDEO
#define SDL_LOG_PRIORITY_VERBOSE BONGO_CAT_LIVE2D_LOG_PRIORITY_VERBOSE
#define SDL_LOG_PRIORITY_DEBUG BONGO_CAT_LIVE2D_LOG_PRIORITY_DEBUG
#define SDL_LOG_PRIORITY_INFO BONGO_CAT_LIVE2D_LOG_PRIORITY_INFO
#define SDL_LOG_PRIORITY_WARN BONGO_CAT_LIVE2D_LOG_PRIORITY_WARN
#define SDL_LOG_PRIORITY_ERROR BONGO_CAT_LIVE2D_LOG_PRIORITY_ERROR
#define SDL_LOG_PRIORITY_CRITICAL BONGO_CAT_LIVE2D_LOG_PRIORITY_CRITICAL
/* SDL_GetThreadState reports BONGO_CAT_LIVE2D_THREAD_ALIVE while running. */
#define SDL_THREAD_ALIVE BONGO_CAT_LIVE2D_THREAD_ALIVE

/* SDL_PathInfo fields the bridge reads; the host fills the same numbers SDL
   would report, so the file-change detection stays byte-identical. */
typedef BongoCatLive2DPathInfo SDL_PathInfo;
#define SDL_PATHTYPE_OTHER BONGO_CAT_LIVE2D_PATH_TYPE_OTHER
#define SDL_PATHTYPE_FILE BONGO_CAT_LIVE2D_PATH_TYPE_FILE
#define SDL_PATHTYPE_DIRECTORY BONGO_CAT_LIVE2D_PATH_TYPE_DIRECTORY

static inline void bongo_cat_live2d_host_log(int category, int priority,
    const char *format, ...) {
    const BongoCatLive2DBackendHost *host = bongo_cat_live2d_backend_host();
    if (!host || !host->log) return;
    va_list args;
    va_start(args, format);
    char message[1024];
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    host->log(category, priority, "%s", message);
}

#define BONGO_CAT_LIVE2D_LOG(category, priority, fmt, ...)                    \
    bongo_cat_live2d_host_log(category, priority, fmt, ##__VA_ARGS__)
#define SDL_Log(fmt, ...)                                                     \
    BONGO_CAT_LIVE2D_LOG(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO,  \
        fmt, ##__VA_ARGS__)
#define SDL_LogInfo(category, fmt, ...)                                       \
    BONGO_CAT_LIVE2D_LOG(category, SDL_LOG_PRIORITY_INFO, fmt, ##__VA_ARGS__)
#define SDL_LogWarn(category, fmt, ...)                                       \
    BONGO_CAT_LIVE2D_LOG(category, SDL_LOG_PRIORITY_WARN, fmt, ##__VA_ARGS__)
#define SDL_LogError(category, fmt, ...)                                      \
    BONGO_CAT_LIVE2D_LOG(category, SDL_LOG_PRIORITY_ERROR, fmt, ##__VA_ARGS__)
#define SDL_LogDebug(category, fmt, ...)                                      \
    BONGO_CAT_LIVE2D_LOG(category, SDL_LOG_PRIORITY_DEBUG, fmt, ##__VA_ARGS__)
#define SDL_LogVerbose(category, fmt, ...)                                    \
    BONGO_CAT_LIVE2D_LOG(category, SDL_LOG_PRIORITY_VERBOSE, fmt,             \
        ##__VA_ARGS__)
#define SDL_LogCritical(category, fmt, ...)                                   \
    BONGO_CAT_LIVE2D_LOG(category, SDL_LOG_PRIORITY_CRITICAL, fmt,            \
        ##__VA_ARGS__)

static inline void *SDL_GL_GetCurrentContext(void) {
    const BongoCatLive2DBackendHost *host = bongo_cat_live2d_backend_host();
    return host && host->gl_current_context ? host->gl_current_context() : NULL;
}

static inline void *SDL_GL_GetCurrentWindow(void) {
    const BongoCatLive2DBackendHost *host = bongo_cat_live2d_backend_host();
    return host && host->gl_current_window ? host->gl_current_window() : NULL;
}

static inline const char *SDL_GetBasePath(void) {
    const BongoCatLive2DBackendHost *host = bongo_cat_live2d_backend_host();
    return host && host->base_path ? host->base_path() : NULL;
}

static inline bool SDL_GetPathInfo(const char *path, SDL_PathInfo *info) {
    const BongoCatLive2DBackendHost *host = bongo_cat_live2d_backend_host();
    if (!host || !host->path_info) return false;
    return host->path_info(path, info);
}

static inline SDL_Thread *SDL_CreateThread(int (*entry)(void *),
    const char *name, void *userdata) {
    const BongoCatLive2DBackendHost *host = bongo_cat_live2d_backend_host();
    if (!host || !host->thread_start) return NULL;
    return (SDL_Thread *)host->thread_start(entry, name, userdata);
}

static inline int SDL_GetThreadState(SDL_Thread *thread) {
    const BongoCatLive2DBackendHost *host = bongo_cat_live2d_backend_host();
    if (!host || !host->thread_state) return 0;
    return host->thread_state(thread);
}

static inline void SDL_WaitThread(SDL_Thread *thread, int *result) {
    const BongoCatLive2DBackendHost *host = bongo_cat_live2d_backend_host();
    if (!host || !host->thread_wait) return;
    host->thread_wait(thread, result);
}

static inline void SDL_Delay(unsigned int milliseconds) {
    const BongoCatLive2DBackendHost *host = bongo_cat_live2d_backend_host();
    if (!host || !host->delay_ms) return;
    host->delay_ms(milliseconds);
}

static inline unsigned long long SDL_GetTicksNS(void) {
    const BongoCatLive2DBackendHost *host = bongo_cat_live2d_backend_host();
    if (!host || !host->ticks_ns) return 0;
    return host->ticks_ns();
}

#ifdef __cplusplus
}
#endif
