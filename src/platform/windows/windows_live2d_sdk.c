/* Runtime discovery of the Cubism Core DLL. The executable never embeds the
   proprietary Core: the binary arrives at render time from the user - a
   registry value pointing at it, a drop-in next to the application, or an
   official SDK zip extracted on first sight. The search runs once per process,
   before the Live2D backend initializes. */
#include "bongo_cat/common.h"
#include "bongo_cat/file.h"
#include "bongo_cat/path.h"
#include "windows_live2d_sdk.h"

#include <SDL3/SDL.h>
#include <windows.h>

#include <delayimp.h>
#include <miniz.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#define REGISTRY_KEY L"Software\\BongoCat\\Live2D"
#define REGISTRY_VALUE L"CoreDll"

static HMODULE core_module;
static bool attempted;

static wchar_t *wide_from_utf8(const char *text) {
    int size = MultiByteToWideChar(CP_UTF8, 0, text, -1, NULL, 0);
    wchar_t *wide;
    if (size <= 0) return NULL;
    wide = (wchar_t *)malloc(sizeof(wchar_t) * (size_t)size);
    if (wide && MultiByteToWideChar(CP_UTF8, 0, text, -1, wide, size) == 0) {
        free(wide);
        return NULL;
    }
    return wide;
}

static char *utf8_from_wide(const wchar_t *text) {
    int size = WideCharToMultiByte(CP_UTF8, 0, text, -1, NULL, 0, NULL, NULL);
    char *narrow;
    if (size <= 0) return NULL;
    narrow = (char *)malloc((size_t)size);
    if (narrow && WideCharToMultiByte(CP_UTF8, 0, text, -1, narrow, size,
            NULL, NULL) == 0) {
        free(narrow);
        return NULL;
    }
    return narrow;
}

/* A delay-load failure means the located DLL does not match the Core the
   framework was compiled against; log the cause instead of crashing on an
   unhandled exception. */
static FARPROC WINAPI delay_load_failure_hook(unsigned notify,
    PDelayLoadInfo info) {
    if (notify == dliFailLoadLib && info)
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
            "[live2d] Cannot load %s (error %lu)", info->szDll,
            (unsigned long)info->dwLastError);
    else if (notify == dliFailGetProc && info)
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
            "[live2d] Core DLL misses %s", info->dlp.szProcName);
    return NULL;
}
const PfnDliHook __pfnDliFailureHook2 = delay_load_failure_hook;

static bool try_load(const wchar_t *path) {
    const char *(WINAPI *version)(void);
    const char *text;
    if (core_module) return true;
    if (!path) return false;
    core_module = LoadLibraryW(path);
    if (!core_module) {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
            "[live2d] Core candidate rejected (error %lu)",
            (unsigned long)GetLastError());
        return false;
    }
    version = (const char *(WINAPI *)(void))(void *)GetProcAddress(
        core_module, "csmVersion");
    text = version ? version() : NULL;
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
        "[live2d] Cubism Core loaded (%s)", text ? text : "version unknown");
    return true;
}

/* Writing the confirmed path back saves the whole scan on later launches. */
static void cache_path(const wchar_t *path) {
    HKEY key = NULL;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, REGISTRY_KEY, 0, NULL,
            REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, NULL, &key,
            NULL) != ERROR_SUCCESS) return;
    RegSetValueExW(key, REGISTRY_VALUE, 0, REG_SZ, (const BYTE *)path,
        (DWORD)((wcslen(path) + 1) * sizeof(wchar_t)));
    RegCloseKey(key);
}

static wchar_t *registry_path(void) {
    wchar_t buffer[BONGO_CAT_PATH_CAP];
    wchar_t *copy;
    DWORD size = sizeof(buffer);
    if (RegGetValueW(HKEY_CURRENT_USER, REGISTRY_KEY, REGISTRY_VALUE,
            RRF_RT_REG_SZ, NULL, buffer, &size) != ERROR_SUCCESS)
        return NULL;
    copy = (wchar_t *)malloc(size + sizeof(wchar_t));
    if (copy) wcscpy(copy, buffer);
    return copy;
}

/* Pick the Core DLL entry inside an SDK zip: matching architecture only. */
static int select_core_entry(mz_zip_archive *zip) {
    bool want_x64 = sizeof(void *) >= 8;
    int chosen = -1;
    unsigned count = mz_zip_reader_get_num_files(zip);
    unsigned i;
    for (i = 0; i < count; ++i) {
        mz_zip_archive_file_stat stat;
        const char *name;
        size_t length;
        bool x64, x86;
        memset(&stat, 0, sizeof(stat));
        if (!mz_zip_reader_file_stat(zip, i, &stat)) continue;
        if (stat.m_is_directory) continue;
        if (stat.m_uncompressed_size < 1024 * 1024) continue;
        name = stat.m_filename;
        length = strlen(name);
        if (length < sizeof("live2dcubismcore.dll") - 1 ||
            _stricmp(name + length - (sizeof("live2dcubismcore.dll") - 1),
                "live2dcubismcore.dll") != 0) continue;
        x64 = strstr(name, "x86_64") != NULL;
        x86 = strstr(name, "x86") != NULL && !x64;
        if (strstr(name, "arm") || (x64 && !want_x64) || (x86 && want_x64))
            continue;
        if (x64 || chosen < 0) {
            chosen = (int)i;
            if (x64 == want_x64) break;
        }
    }
    return chosen;
}

/* miniz reads narrow paths; stream the archive through memory so non-ASCII
   user folders stay supported. */
static bool extract_core_dll(const wchar_t *zip_path, const char *target) {
    FILE *file = _wfopen(zip_path, L"rb");
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
            size_t dll_size = 0;
            void *dll = mz_zip_reader_extract_to_heap(&zip,
                (mz_uint)index, &dll_size, 0);
            FILE *out = dll ? bongo_cat_file_open(target, "wb") : NULL;
            extracted = out && fwrite(dll, 1, dll_size, out) == dll_size &&
                fclose(out) == 0;
            mz_free(dll);
            if (extracted)
                SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "[live2d] Extracted Core DLL from an SDK zip");
        }
        mz_zip_reader_end(&zip);
    }
    free(bytes);
    return extracted;
}

/* One scan directory: accept a bare Core DLL, then any SDK zips inside. */
static void scan_directory(const char *directory, const char *work_dir) {
    char candidate[BONGO_CAT_PATH_CAP];
    wchar_t *wide;
    WIN32_FIND_DATAW found;
    HANDLE handle;

    if (bongo_cat_path_join(candidate, sizeof(candidate), directory,
            "Live2DCubismCore.dll")) {
        wide = wide_from_utf8(candidate);
        if (wide) {
            bool loaded = try_load(wide);
            free(wide);
            if (loaded) return;
        }
    }
    if (!bongo_cat_path_join(candidate, sizeof(candidate), directory,
            "*.zip")) return;
    wide = wide_from_utf8(candidate);
    if (!wide) return;
    memset(&found, 0, sizeof(found));
    handle = FindFirstFileW(wide, &found);
    free(wide);
    if (handle == INVALID_HANDLE_VALUE) return;
    for (;;) {
        char *name;
        if (!(found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
            (name = utf8_from_wide(found.cFileName)) != NULL) {
            char zip_path[BONGO_CAT_PATH_CAP];
            char zip_stem[BONGO_CAT_PATH_CAP];
            char dll_target[BONGO_CAT_PATH_CAP];
            char *stem = name;
            size_t length = strlen(name);
            bool usable;
            if (length > 4 && _stricmp(name + length - 4, ".zip") == 0)
                name[length - 4] = '\0';
            usable = bongo_cat_path_join(zip_path, sizeof(zip_path),
                    directory, name) &&
                bongo_cat_path_join(zip_stem, sizeof(zip_stem), work_dir,
                    stem) &&
                bongo_cat_path_join(dll_target, sizeof(dll_target), zip_stem,
                    "Live2DCubismCore.dll");
            if (usable) {
                wchar_t *zip_wide = wide_from_utf8(zip_path);
                /* The extraction target lives below live2d/core/<stem>/;
                   nothing else creates those directories. */
                if (bongo_cat_path_create_directory(zip_stem) &&
                    (bongo_cat_path_is_file(dll_target) ||
                    (zip_wide && extract_core_dll(zip_wide, dll_target)))) {
                    wchar_t *dll_wide = wide_from_utf8(dll_target);
                    if (dll_wide && try_load(dll_wide)) cache_path(dll_wide);
                    free(dll_wide);
                }
                free(zip_wide);
            }
            free(name);
            if (core_module) break;
        }
        if (!FindNextFileW(handle, &found)) break;
    }
    FindClose(handle);
}

void bongo_cat_windows_live2d_sdk_prepare(const char *data_dir) {
    char directories[2][BONGO_CAT_PATH_CAP];
    char work_dirs[2][BONGO_CAT_PATH_CAP];
    const char *base;
    unsigned count = 0;
    wchar_t *cached;

    if (attempted || core_module) return;
    attempted = true;

    cached = registry_path();
    if (cached && try_load(cached)) {
        free(cached);
        return;
    }
    free(cached);

    base = SDL_GetBasePath();
    if (base) {
        char bare[BONGO_CAT_PATH_CAP];
        wchar_t *wide;
        if (bongo_cat_path_join(bare, sizeof(bare), base,
                "Live2DCubismCore.dll")) {
            wide = wide_from_utf8(bare);
            if (wide) {
                (void)try_load(wide);
                free(wide);
            }
        }
        if (!core_module &&
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
        for (i = 0; i < count && !core_module; ++i)
            scan_directory(directories[i], work_dirs[i]);
    }
    if (!core_module) {
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

bool bongo_cat_windows_live2d_sdk_ready(void) {
    if (!attempted) bongo_cat_windows_live2d_sdk_prepare(NULL);
    return core_module != NULL;
}

HMODULE bongo_cat_windows_live2d_core_library(void) {
    if (!attempted) bongo_cat_windows_live2d_sdk_prepare(NULL);
    return core_module;
}

bool bongo_cat_windows_live2d_sdk_import(const char *path,
    const char *data_dir, BongoCatError *error) {
    if (core_module) {
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
    if (length > 4 && _stricmp(path + length - 4, ".zip") == 0) {
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
                "Live2DCubismCore.dll")) {
            bongo_cat_error_set(error, BONGO_CAT_ERROR_IO,
                "Cannot prepare the extraction folder for the SDK zip");
            return false;
        }
        if (!bongo_cat_path_is_file(target)) {
            wchar_t *wide = wide_from_utf8(path);
            bool extracted = wide && extract_core_dll(wide, target);
            free(wide);
            if (!extracted) {
                bongo_cat_error_set(error, BONGO_CAT_ERROR_FORMAT,
                    "No usable Cubism Core was found inside the SDK zip");
                return false;
            }
        }
    } else {
        char directory[BONGO_CAT_PATH_CAP];
        if (!bongo_cat_path_join(directory, sizeof(directory), data_dir,
                "live2d") ||
            !bongo_cat_path_create_directory(directory) ||
            !bongo_cat_path_join(target, sizeof(target), directory,
                "Live2DCubismCore.dll")) {
            bongo_cat_error_set(error, BONGO_CAT_ERROR_IO,
                "Cannot prepare the live2d data folder");
            return false;
        }
        /* Keep a copy in the data directory; skip when the user picked the
           destination file itself. */
        if (_stricmp(path, target) != 0 &&
            !bongo_cat_path_copy_file(path, target)) {
            bongo_cat_error_set(error, BONGO_CAT_ERROR_IO,
                "Cannot copy the selected file into the live2d data folder");
            return false;
        }
    }
    wchar_t *wide = wide_from_utf8(target);
    bool loaded = wide && try_load(wide);
    if (loaded) cache_path(wide);
    free(wide);
    if (!loaded) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_FORMAT,
            "The selected file is not a usable Cubism Core DLL");
        return false;
    }
    return true;
}
