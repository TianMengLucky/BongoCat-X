/* Runtime discovery of the Cubism Core shared library on Linux and macOS.
   The executable never embeds the proprietary Core: the binary arrives at
   render time from the user - a drop-in beside the application, a file
   imported from the settings window, or an official SDK zip extracted on
   first sight. The search runs once per process, before the Live2D backend
   initializes. The Cubism Core symbols themselves are reached through the
   generated dlopen shim (cmake/gen_core_shim.py). */
/* Strict ISO mode (-std=c11) hides the dirent d_type constants; this build
   needs them to tell directory entries apart from zip candidates. */
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE 1
#endif
#include "bongo_cat/common.h"
#include "bongo_cat/file.h"
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

#include <miniz.h>

static void *core_handle;
static bool attempted;

static bool try_load(const char *path) {
    const char *(*version)(void);
    const char *text;
    if (core_handle) return true;
    if (!path) return false;
    void *handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
            "[live2d] Core candidate rejected (%s)", dlerror());
        return false;
    }
    version = (const char *(*)(void))(void *)dlsym(handle, "csmGetVersion");
    text = version ? version() : NULL;
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
        "[live2d] Cubism Core loaded (%s)", text ? text : "version unknown");
    core_handle = handle;
    return true;
}

/* Pick the Core library entry inside an SDK zip: the shared binary of this
   platform only, never the static archives or the mobile/embedded trees. */
static int select_core_entry(mz_zip_archive *zip) {
    int chosen = -1;
    unsigned count = mz_zip_reader_get_num_files(zip);
    for (unsigned i = 0; i < count; ++i) {
        mz_zip_archive_file_stat stat;
        const char *name;
        size_t length;
        const size_t suffix_length = sizeof(CORE_LIBRARY_NAME) - 1;
        memset(&stat, 0, sizeof(stat));
        if (!mz_zip_reader_file_stat(zip, i, &stat)) continue;
        if (stat.m_is_directory || stat.m_uncomp_size < 1024 * 1024)
            continue;
        name = stat.m_filename;
        length = strlen(name);
        if (length <= suffix_length ||
            strcasecmp(name + length - suffix_length, CORE_LIBRARY_NAME) != 0)
            continue;
        if (strstr(name, "ios") || strstr(name, "catalyst") ||
            strstr(name, "android") || strstr(name, "harmonyos") ||
            strstr(name, "uwp") || strstr(name, "emscripten"))
            continue;
#ifdef __APPLE__
        /* The macOS Core dylib is universal; only the macos tree matches. */
        if (!strstr(name, "macos")) continue;
#else
        if (strstr(name, "rpi")) continue;
#if defined(__aarch64__) || defined(__arm__)
        if (!strstr(name, "arm") && !strstr(name, "aarch64")) continue;
#else
        /* Desktop Linux releases target x86_64 (see cmake/Cubism.cmake). */
        if (strstr(name, "arm") || strstr(name, "aarch64") ||
            !strstr(name, "x86_64"))
            continue;
#endif
#endif
        chosen = (int)i;
        break;
    }
    return chosen;
}

static bool extract_core_library(const char *zip_path, const char *target) {
    FILE *file = fopen(zip_path, "rb");
    long size;
    uint8_t *bytes;
    mz_zip_archive zip;
    bool extracted = false;
    if (!file) return false;
    fseek(file, 0, SEEK_END);
    size = ftell(file);
    fseek(file, 0, SEEK_SET);
    if (size <= 0) {
        fclose(file);
        return false;
    }
    bytes = (uint8_t *)malloc((size_t)size);
    if (!bytes || fread(bytes, 1, (size_t)size, file) != (size_t)size) {
        free(bytes);
        fclose(file);
        return false;
    }
    fclose(file);
    memset(&zip, 0, sizeof(zip));
    if (mz_zip_reader_init_mem(&zip, bytes, (size_t)size, 0)) {
        int index = select_core_entry(&zip);
        if (index >= 0) {
            size_t core_size = 0;
            void *core = mz_zip_reader_extract_to_heap(&zip,
                (mz_uint)index, &core_size, 0);
            FILE *out = core ? bongo_cat_file_open(target, "wb") : NULL;
            extracted = out && fwrite(core, 1, core_size, out) == core_size &&
                fclose(out) == 0;
            mz_free(core);
            if (extracted)
                SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "[live2d] Extracted the Cubism Core from an SDK zip");
        }
        mz_zip_reader_end(&zip);
    }
    free(bytes);
    return extracted;
}

/* One scan directory: accept a bare Core library, then any SDK zips inside. */
static void scan_directory(const char *directory, const char *work_dir) {
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
            char zip_stem[BONGO_CAT_PATH_CAP];
            char core_target[BONGO_CAT_PATH_CAP];
            char stem[BONGO_CAT_PATH_CAP];
            snprintf(stem, sizeof(stem), "%s", found->d_name);
            stem[length - 4] = '\0';
            if (bongo_cat_path_join(zip_path, sizeof(zip_path), directory,
                    found->d_name) &&
                bongo_cat_path_join(zip_stem, sizeof(zip_stem), work_dir,
                    stem) &&
                bongo_cat_path_join(core_target, sizeof(core_target),
                    zip_stem, CORE_LIBRARY_NAME)) {
                /* The extraction target lives below live2d/core/<stem>/;
                   nothing else creates those directories. */
                if (bongo_cat_path_create_directory(zip_stem) &&
                    (bongo_cat_path_is_file(core_target) ||
                    extract_core_library(zip_path, core_target)))
                    try_load(core_target);
            }
            if (core_handle) break;
        }
    }
    closedir(dir);
}

void bongo_cat_posix_live2d_sdk_prepare(const char *data_dir) {
    char directories[2][BONGO_CAT_PATH_CAP];
    char work_dirs[2][BONGO_CAT_PATH_CAP];
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
                sizeof(directories[count]), base, "live2d") &&
            bongo_cat_path_join(work_dirs[count], sizeof(work_dirs[count]),
                directories[count], "core"))
            count++;
    }
    if (data_dir && data_dir[0] && count < 2 &&
        bongo_cat_path_join(directories[count], sizeof(directories[count]),
            data_dir, "live2d") &&
        bongo_cat_path_join(work_dirs[count], sizeof(work_dirs[count]),
            directories[count], "core"))
        count++;
    {
        unsigned i;
        for (i = 0; i < count && !core_handle; ++i)
            scan_directory(directories[i], work_dirs[i]);
    }
    if (!core_handle) {
        /* Nothing loaded: leave the live2d folders behind so the user has a
           visible drop-in place for the Core library or the SDK zip. */
        unsigned i;
        for (i = 0; i < count; ++i)
            (void)bongo_cat_path_create_directory(directories[i]);
    }
    /* SDL_GetBasePath() returns SDL's own cached string and must not be
       freed: later callers (the asset locator, the backend loader) would be
       left with a dangling pointer. */
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
        /* Official SDK zip: extract the matching Core into the same layout
           the startup scan uses, so the import survives restarts. */
        char directory[BONGO_CAT_PATH_CAP];
        char stem[BONGO_CAT_PATH_CAP];
        snprintf(stem, sizeof(stem), "%s", bongo_cat_path_name(path));
        size_t stem_length = strlen(stem);
        if (stem_length > 4) stem[stem_length - 4] = '\0';
        if (!bongo_cat_path_join(directory, sizeof(directory), data_dir,
                "live2d") ||
            !bongo_cat_path_join(directory, sizeof(directory), directory,
                "core") ||
            !bongo_cat_path_join(directory, sizeof(directory), directory,
                stem) ||
            !bongo_cat_path_create_directory(directory) ||
            !bongo_cat_path_join(target, sizeof(target), directory,
                CORE_LIBRARY_NAME)) {
            bongo_cat_error_set(error, BONGO_CAT_ERROR_IO,
                "Cannot prepare the extraction folder for the SDK zip");
            return false;
        }
        if (!bongo_cat_path_is_file(target) &&
            !extract_core_library(path, target)) {
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

bool bongo_cat_platform_live2d_core_available(void) {
    return bongo_cat_posix_live2d_sdk_ready();
}
void *bongo_cat_platform_live2d_core_library(void) {
    return bongo_cat_posix_live2d_core_library();
}
bool bongo_cat_platform_live2d_core_import_supported(void) { return true; }
bool bongo_cat_platform_live2d_core_import(const char *path,
    const char *data_dir, BongoCatError *error) {
    return bongo_cat_posix_live2d_sdk_import(path, data_dir, error);
}
