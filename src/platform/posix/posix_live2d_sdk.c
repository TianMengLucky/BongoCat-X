/* Runtime discovery of the Cubism Core shared library on Linux and macOS.
   Runtime-Core builds keep the proprietary Core out of the executable; the
   binary arrives at render time from the user: a drop-in beside the
   application, a file imported from the settings window, or an official SDK
   zip extracted on first sight. The search runs once per process, before the
   Live2D backend initializes. The Cubism Core symbols themselves are reached
   through the generated dlopen shim (cmake/gen_core_shim.py). */
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

#ifdef BONGO_CAT_LIVE2D_CORE_RUNTIME
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
   platform only, never the static archives or the mobile/embedded trees.
   The archive-relative path of the chosen entry lands in entry_name. */
static int select_core_entry(mz_zip_archive *zip, char *entry_name,
    size_t entry_cap) {
    int chosen = -1;
    unsigned count = mz_zip_reader_get_num_files(zip);
    entry_name[0] = '\0';
    for (unsigned i = 0; i < count; ++i) {
        mz_zip_archive_file_stat stat;
        const char *name;
        size_t length;
        const size_t suffix_length = sizeof(CORE_LIBRARY_NAME) - 1;
        memset(&stat, 0, sizeof(stat));
        if (!mz_zip_reader_file_stat(zip, i, &stat)) continue;
        /* Small floor only: the Core library itself can weigh well under
           1 MB, so the name and platform filters below do the real work. */
        if (stat.m_is_directory || stat.m_uncomp_size < 64 * 1024)
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
        snprintf(entry_name, entry_cap, "%s", name);
        break;
    }
    if (chosen < 0)
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
            "[live2d] No Cubism Core for this platform inside the zip");
    return chosen;
}

/* The whole SDK zip is unpacked below out_dir (live2d/<zip name>/) so the
   user can browse every file; core_target then receives the path of the
   matched Core library, or stays empty when the archive holds none. A
   previous extraction with the Core already in place is reused. */
/* Length of the single top-level folder every entry shares (including its
   separator), or 0 when the entries have no common root. */
static size_t common_top_level(mz_zip_archive *zip) {
    const char *common = NULL;
    size_t common_length = 0;
    unsigned count = mz_zip_reader_get_num_files(zip);
    for (unsigned i = 0; i < count; ++i) {
        mz_zip_archive_file_stat stat;
        const char *separator;
        if (!mz_zip_reader_file_stat(zip, i, &stat) || stat.m_is_directory)
            continue;
        separator = strchr(stat.m_filename, '/');
        if (!separator) return 0;
        if (!common) {
            common = stat.m_filename;
            common_length = (size_t)(separator - common) + 1;
        } else if (strncmp(common, stat.m_filename, common_length) != 0) {
            return 0;
        }
    }
    return common_length;
}

static bool extract_sdk_zip(const char *zip_path, const char *out_dir,
    char *core_target, size_t target_cap) {
    FILE *file = fopen(zip_path, "rb");
    long size;
    uint8_t *bytes;
    mz_zip_archive zip;
    bool extracted = true;
    unsigned written = 0, count;
    size_t strip;
    char core_name[BONGO_CAT_PATH_CAP];
    int core_index;
    core_target[0] = '\0';
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
    if (!mz_zip_reader_init_mem(&zip, bytes, (size_t)size, 0)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
            "[live2d] Cannot read the SDK zip archive");
        free(bytes);
        return false;
    }
    core_index = select_core_entry(&zip, core_name, sizeof(core_name));
    strip = common_top_level(&zip);
    if (core_index >= 0 &&
        bongo_cat_path_join(core_target, target_cap, out_dir,
            core_name + strip) &&
        bongo_cat_path_is_file(core_target)) {
        /* The Core from an earlier run is already in place. */
        mz_zip_reader_end(&zip);
        free(bytes);
        return true;
    }
    core_target[0] = '\0';
    count = mz_zip_reader_get_num_files(&zip);
    for (unsigned i = 0; i < count; ++i) {
        mz_zip_archive_file_stat stat;
        const char *name;
        char target[BONGO_CAT_PATH_CAP];
        char parent[BONGO_CAT_PATH_CAP];
        size_t parent_length;
        size_t file_size = 0;
        void *data;
        FILE *out;
        if (!mz_zip_reader_file_stat(&zip, i, &stat)) continue;
        name = stat.m_filename;
        if (stat.m_is_directory || !name[0] || strlen(name) <= strip)
            continue;
        name += strip;
        /* Zip-slip guard: only plain relative paths land below out_dir. */
        if (name[0] == '/' || strstr(name, "..")) continue;
        if (!bongo_cat_path_join(target, sizeof(target), out_dir, name))
            continue;
        parent_length = strlen(target);
        while (parent_length && target[parent_length - 1] != '/')
            parent_length--;
        if (parent_length >= sizeof(parent)) continue;
        memcpy(parent, target, parent_length);
        parent[parent_length] = '\0';
        if (!bongo_cat_path_create_directory(parent)) continue;
        data = mz_zip_reader_extract_to_heap(&zip, i, &file_size, 0);
        if (!data) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                "[live2d] Cannot extract %s from the SDK zip", name);
            extracted = false;
            continue;
        }
        out = bongo_cat_file_open(target, "wb");
        if (!out || fwrite(data, 1, file_size, out) != file_size ||
            fclose(out) != 0) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                "[live2d] Cannot write %s", target);
            extracted = false;
        } else {
            written++;
        }
        mz_free(data);
    }
    if (core_index >= 0)
        (void)bongo_cat_path_join(core_target, target_cap, out_dir,
            core_name + strip);
    mz_zip_reader_end(&zip);
    free(bytes);
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
        "[live2d] Extracted %u files from the SDK zip below %s", written,
        out_dir);
    return extracted;
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
        char out_dir[BONGO_CAT_PATH_CAP];
        char stem[BONGO_CAT_PATH_CAP];
        snprintf(stem, sizeof(stem), "%s", bongo_cat_path_name(path));
        size_t stem_length = strlen(stem);
        if (stem_length > 4) stem[stem_length - 4] = '\0';
        if (!bongo_cat_path_join(out_dir, sizeof(out_dir), data_dir,
                "live2d") ||
            !bongo_cat_path_join(out_dir, sizeof(out_dir), out_dir, stem) ||
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
