/* Runtime discovery of the Cubism Core shared library on Linux and macOS.
   Runtime-Core builds keep the proprietary Core out of the executable; the
   binary arrives at render time from the user: a drop-in beside the
   application, a file imported from the settings window, or an official SDK
   zip extracted on first sight. The search runs once per process, before the
   Live2D backend initializes. The Cubism Core symbols themselves are reached
   through the generated dlopen shim (cmake/gen_core_shim.py). */
/* d_type and DT_DIR are BSD/POSIX extensions that strict C11 hides. */
#if !defined(_DEFAULT_SOURCE)
#define _DEFAULT_SOURCE 1
#endif

#include "bongo_cat/common.h"
#include "bongo_cat/file.h"
#include "bongo_cat/log.h"
#include "bongo_cat/path.h"
#include "posix_live2d_sdk.h"

#include <SDL3/SDL.h>
#include <dirent.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#ifdef __APPLE__
#define CORE_LIBRARY_NAME "libLive2DCubismCore.dylib"
#else
#define CORE_LIBRARY_NAME "libLive2DCubismCore.so"
#endif

#ifdef BONGO_CAT_LIVE2D_CORE_RUNTIME
#include "bongo_cat/safe_archive.h"

static void *core_handle;
static bool attempted;

static bool try_load(const char *path) {
    unsigned (*version)(void);
    if (core_handle) return true;
    if (!path || !bongo_cat_path_is_file(path)) return false;
    SDL_LogInfo(BONGO_CAT_LOG_LIFECYCLE, "[live2d] Checking Core: %s", path);
    void *handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        SDL_LogWarn(BONGO_CAT_LOG_LIFECYCLE,
            "[live2d] Core candidate rejected: %s (%s)", path, dlerror());
        return false;
    }
    /* csmGetVersion returns the encoded numeric csmVersion (unsigned int),
       not a string (the smoke-test backtrace caught this dereference). */
    version = (unsigned (*)(void))(void *)dlsym(handle, "csmGetVersion");
    if (!version || !dlsym(handle, "csmReviveMocInPlace") ||
        !dlsym(handle, "csmInitializeModelInPlace")) {
        SDL_LogWarn(BONGO_CAT_LOG_LIFECYCLE,
            "[live2d] Core candidate rejected: %s (missing required Core symbols)", path);
        dlclose(handle);
        return false;
    }
    unsigned encoded = version();
    SDL_LogInfo(BONGO_CAT_LOG_LIFECYCLE,
        "[live2d] Cubism Core loaded (version 0x%x)", encoded);
    core_handle = handle;
    return true;
}

static bool extract_sdk_zip(const char *zip_path, const char *out_dir,
    char *core_target, size_t target_cap) {
    return bongo_safe_sdk_extract(zip_path, out_dir, core_target, target_cap);
}

/* One scan directory: accept a bare Core library, then any SDK zips inside. */
static void scan_directory(const char *directory) {
    char candidate[BONGO_CAT_PATH_CAP];
    DIR *dir;
    struct dirent *found;

    if (bongo_cat_path_join(candidate, sizeof(candidate), directory,
            CORE_LIBRARY_NAME) && try_load(candidate))
        return;
    dir = opendir(directory);
    if (!dir) return;
    while ((found = readdir(dir)) != NULL) {
        size_t length = strlen(found->d_name);
        if (found->d_type == DT_DIR || length <= 4 ||
            strcasecmp(found->d_name + length - 4, ".zip") != 0)
            continue;
        {
            char zip_path[BONGO_CAT_PATH_CAP];
            char out_dir[BONGO_CAT_PATH_CAP];
            char core_target[BONGO_CAT_PATH_CAP];
            char stem[BONGO_CAT_PATH_CAP];
            snprintf(stem, sizeof(stem), "%s", found->d_name);
            stem[length - 4] = '\0';
            if (bongo_cat_path_join(zip_path, sizeof(zip_path), directory,
                    found->d_name) &&
                bongo_cat_path_join(out_dir, sizeof(out_dir), directory,
                    stem)) {
                /* The whole SDK unpacks below live2d/<zip name>/; the Core
                   loads straight from that tree. */
                if (bongo_cat_path_create_directory(out_dir) &&
                    extract_sdk_zip(zip_path, out_dir, core_target,
                        sizeof(core_target)) && core_target[0])
                    try_load(core_target);
            }
            if (core_handle) break;
        }
    }
    closedir(dir);
}

void bongo_cat_posix_live2d_sdk_prepare(const char *data_dir) {
    char directories[2][BONGO_CAT_PATH_CAP];
    const char *base;
    unsigned count = 0;

    if (attempted || core_handle) return;
    attempted = true;

    base = SDL_GetBasePath();
    if (base) {
        char bare[BONGO_CAT_PATH_CAP];
        if (bongo_cat_path_join(bare, sizeof(bare), base,
                CORE_LIBRARY_NAME))
            (void)try_load(bare);
        if (!core_handle &&
            bongo_cat_path_join(directories[count],
                sizeof(directories[count]), base, "live2d"))
            count++;
    }
    if (data_dir && data_dir[0] && count < 2 &&
        bongo_cat_path_join(directories[count],
            sizeof(directories[count]), data_dir, "live2d"))
        count++;
    {
        unsigned i;
        for (i = 0; i < count && !core_handle; ++i)
            scan_directory(directories[i]);
    }
    if (!core_handle) {
        /* Nothing loaded: leave the live2d folders behind so the user has a
           visible drop-in place for the Core library or the SDK zip. */
        unsigned i;
        for (i = 0; i < count; ++i)
            (void)bongo_cat_path_create_directory(directories[i]);
    }
    /* SDL3 owns the SDL_GetBasePath() string; freeing it here would make
       every later rescan hit a double free inside SDL_free. */
}

bool bongo_cat_posix_live2d_sdk_ready(void) {
    if (!attempted && !core_handle)
        bongo_cat_posix_live2d_sdk_prepare(NULL);
    return core_handle != NULL;
}

void *bongo_cat_posix_live2d_core_library(void) {
    if (!attempted && !core_handle)
        bongo_cat_posix_live2d_sdk_prepare(NULL);
    return core_handle;
}

bool bongo_cat_posix_live2d_sdk_import(const char *path,
    const char *data_dir, BongoCatError *error) {
    if (core_handle) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_PLATFORM,
            "The Cubism Core is already loaded in this session");
        return false;
    }
    if (!path || !path[0] || !data_dir || !data_dir[0]) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_ARGUMENT,
            "Cannot import the Cubism Core without a file and data directory");
        return false;
    }
    size_t length = strlen(path);
    char target[BONGO_CAT_PATH_CAP];
    if (length > 4 && strcasecmp(path + length - 4, ".zip") == 0) {
        /* Official SDK zip: unpack everything below live2d/<zip name>/, the
           same layout the startup scan uses, so the import survives
           restarts, and load the Core from the extracted tree. */
        char out_dir[BONGO_CAT_PATH_CAP], live2d_dir[BONGO_CAT_PATH_CAP];
        char stem[BONGO_CAT_PATH_CAP];
        snprintf(stem, sizeof(stem), "%s", bongo_cat_path_name(path));
        size_t stem_length = strlen(stem);
        if (stem_length > 4) stem[stem_length - 4] = '\0';
        if (!bongo_cat_path_join(live2d_dir, sizeof(live2d_dir), data_dir,
                "live2d") ||
            !bongo_cat_path_join(out_dir, sizeof(out_dir), live2d_dir, stem) ||
            !bongo_cat_path_create_directory(out_dir)) {
            bongo_cat_error_set(error, BONGO_CAT_ERROR_IO,
                "Cannot prepare the extraction folder for the SDK zip");
            return false;
        }
        if (!extract_sdk_zip(path, out_dir, target, sizeof(target))) {
            bongo_cat_error_set(error, BONGO_CAT_ERROR_FORMAT,
                "Cannot unpack the SDK zip");
            return false;
        }
        if (!target[0]) {
            bongo_cat_error_set(error, BONGO_CAT_ERROR_FORMAT,
                "No usable Cubism Core was found inside the SDK zip");
            return false;
        }
    } else {
        char directory[BONGO_CAT_PATH_CAP];
        if (!bongo_cat_path_join(directory, sizeof(directory), data_dir,
                "live2d") ||
            !bongo_cat_path_create_directory(directory) ||
            !bongo_cat_path_join(target, sizeof(target), directory,
                CORE_LIBRARY_NAME)) {
            bongo_cat_error_set(error, BONGO_CAT_ERROR_IO,
                "Cannot prepare the live2d data folder");
            return false;
        }
        /* Keep a copy in the data directory; skip when the user picked the
           destination file itself. */
        if (strcmp(path, target) != 0 &&
            !bongo_cat_path_copy_file(path, target)) {
            bongo_cat_error_set(error, BONGO_CAT_ERROR_IO,
                "Cannot copy the selected file into the live2d data folder");
            return false;
        }
    }
    if (!try_load(target)) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_FORMAT,
            "The selected file is not a usable Cubism Core library");
        return false;
    }
    return true;
}

bool bongo_cat_posix_live2d_sdk_rescan(const char *data_dir) {
    if (core_handle) return true;
    /* Allow the discovery to run again over the drop-in folders. */
    attempted = false;
    bongo_cat_posix_live2d_sdk_prepare(data_dir);
    return core_handle != NULL;
}
#else
void bongo_cat_posix_live2d_sdk_prepare(const char *data_dir) {
    (void)data_dir;
}
bool bongo_cat_posix_live2d_sdk_ready(void) {
#ifdef BONGO_CAT_HAS_CUBISM
    return true;
#else
    return false;
#endif
}
void *bongo_cat_posix_live2d_core_library(void) { return NULL; }
bool bongo_cat_posix_live2d_sdk_import(const char *path,
    const char *data_dir, BongoCatError *error) {
    (void)path; (void)data_dir;
    bongo_cat_error_set(error, BONGO_CAT_ERROR_PLATFORM,
        "Runtime Cubism Core import requires the runtime-Core build");
    return false;
}
bool bongo_cat_posix_live2d_sdk_rescan(const char *data_dir) {
    (void)data_dir;
#ifdef BONGO_CAT_HAS_CUBISM
    return true;
#else
    return false;
#endif
}
#endif

void *bongo_cat_platform_live2d_core_symbol(const char *name) {
#ifdef BONGO_CAT_LIVE2D_CORE_RUNTIME
    if (!attempted && !core_handle) bongo_cat_posix_live2d_sdk_prepare(NULL);
    void *handle = bongo_cat_posix_live2d_core_library();
    return handle && name ? dlsym(handle, name) : NULL;
#else
    (void)name; return NULL;
#endif
}

bool bongo_cat_platform_live2d_core_available(void) {
    return bongo_cat_posix_live2d_sdk_ready();
}
bool bongo_cat_platform_live2d_core_import_supported(void) {
#ifdef BONGO_CAT_LIVE2D_CORE_RUNTIME
    return true;
#else
    return false;
#endif
}
bool bongo_cat_platform_live2d_core_import(const char *path,
    const char *data_dir, BongoCatError *error) {
    return bongo_cat_posix_live2d_sdk_import(path, data_dir, error);
}
bool bongo_cat_platform_live2d_core_rescan(const char *data_dir) {
    return bongo_cat_posix_live2d_sdk_rescan(data_dir);
}
