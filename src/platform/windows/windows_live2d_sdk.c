/* Runtime discovery of the Cubism Core DLL. Runtime-Core builds keep the
   proprietary Core out of the executable; the binary arrives at render time
   from the user: a registry value pointing at it, a drop-in next to the
   application, or an official SDK zip extracted on first sight. */
#include "bongo_cat/common.h"
#include "bongo_cat/file.h"
#include "bongo_cat/path.h"
#include "windows_live2d_sdk.h"
#include <SDL3/SDL.h>
#include <windows.h>
#ifdef BONGO_CAT_LIVE2D_CORE_RUNTIME
#include "bongo_cat/safe_archive.h"
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
    if (size <= 0) return NULL;
    wchar_t *wide = (wchar_t *)malloc(sizeof(wchar_t) * (size_t)size);
    if (wide && MultiByteToWideChar(CP_UTF8, 0, text, -1, wide, size) == 0)
        free(wide), wide = NULL;
    return wide;
}

static char *utf8_from_wide(const wchar_t *text) {
    int size = WideCharToMultiByte(CP_UTF8, 0, text, -1, NULL, 0, NULL, NULL);
    if (size <= 0) return NULL;
    char *narrow = (char *)malloc((size_t)size);
    if (narrow && WideCharToMultiByte(CP_UTF8, 0, text, -1, narrow, size,
            NULL, NULL) == 0)
        free(narrow), narrow = NULL;
    return narrow;
}

static bool try_load(const wchar_t *path) {
    unsigned int (__cdecl *version)(void);
    if (core_module) return true;
    if (!path) return false;
    core_module = LoadLibraryW(path);
    if (!core_module) {
        SDL_LogInfo(SDL_LOG_CATEGORY_CUSTOM,
            "[live2d] Core candidate rejected (error %lu)",
            (unsigned long)GetLastError());
        return false;
    }
    version = (unsigned int (__cdecl *)(void))(void *)GetProcAddress(core_module, "csmGetVersion");
    if (!version || !GetProcAddress(core_module, "csmReviveMocInPlace") ||
        !GetProcAddress(core_module, "csmInitializeModelInPlace")) {
        FreeLibrary(core_module); core_module = NULL;
        return false;
    }
    SDL_LogInfo(SDL_LOG_CATEGORY_CUSTOM, "[live2d] Cubism Core loaded (version %u)", version());
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
    DWORD size = sizeof(buffer);
    if (RegGetValueW(HKEY_CURRENT_USER, REGISTRY_KEY, REGISTRY_VALUE,
            RRF_RT_REG_SZ, NULL, buffer, &size) != ERROR_SUCCESS)
        return NULL;
    wchar_t *copy = (wchar_t *)malloc(size + sizeof(wchar_t));
    if (copy) memcpy(copy, buffer, size); /* size includes the terminator */
    return copy;
}

static bool extract_sdk_zip(const wchar_t *zip_path, const char *out_dir,
    char *dll_target, size_t target_cap) {
    char *source = utf8_from_wide(zip_path);
    bool result = source && bongo_safe_sdk_extract(source, out_dir, dll_target, target_cap);
    free(source);
    return result;
}

/* Keep a copy of the confirmed Core inside the data directory and point the
   registry at it, so later launches find it even if the drop-in folder or
   the SDK zip disappears. Best effort: a failed copy keeps working in place. */
static void stash_core(const char *loaded, const char *data_dir) {
    char directory[BONGO_CAT_PATH_CAP], target[BONGO_CAT_PATH_CAP];
    if (!loaded || !loaded[0] || !data_dir || !data_dir[0]) return;
    if (!bongo_cat_path_join(directory, sizeof(directory), data_dir, "live2d") ||
        !bongo_cat_path_join(target, sizeof(target), directory, "Live2DCubismCore.dll"))
        return;
    if (_stricmp(loaded, target) != 0 &&
        (!bongo_cat_path_create_directory(directory) || !bongo_cat_path_copy_file(loaded, target)))
        return;
    wchar_t *wide = wide_from_utf8(target);
    if (wide) cache_path(wide);
    free(wide);
}

/* One scan directory: accept a bare Core DLL, then any SDK zips inside. */
static void scan_directory(const char *directory, const char *data_dir) {
    char candidate[BONGO_CAT_PATH_CAP];
    wchar_t *wide;
    WIN32_FIND_DATAW found;
    HANDLE handle;
    if (bongo_cat_path_join(candidate, sizeof(candidate), directory,
            "Live2DCubismCore.dll")) {
        wide = wide_from_utf8(candidate);
        if (wide && try_load(wide)) {
            free(wide); stash_core(candidate, data_dir); return;
        }
        free(wide);
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
            char zip_path[BONGO_CAT_PATH_CAP], out_dir[BONGO_CAT_PATH_CAP],
                dll_target[BONGO_CAT_PATH_CAP];
            size_t length = strlen(name);
            /* zip_path keeps the .zip extension; only the folder drops it. */
            if (length > 4 && _stricmp(name + length - 4, ".zip") == 0 &&
                bongo_cat_path_join(zip_path, sizeof(zip_path),
                    directory, name) &&
                bongo_cat_path_join(out_dir, sizeof(out_dir), directory,
                    name)) {
                out_dir[strlen(out_dir) - 4] = '\0';
                /* The SDK unpacks below live2d/<zip name>/; the Core loads
                   straight from that tree. */
                wchar_t *zip_wide = wide_from_utf8(zip_path);
                if (zip_wide && bongo_cat_path_create_directory(out_dir) &&
                    extract_sdk_zip(zip_wide, out_dir, dll_target,
                        sizeof(dll_target)) && dll_target[0]) {
                    wchar_t *dll_wide = wide_from_utf8(dll_target);
                    if (dll_wide && try_load(dll_wide))
                        stash_core(dll_target, data_dir);
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
        char bare[BONGO_CAT_PATH_CAP]; wchar_t *wide;
        if (bongo_cat_path_join(bare, sizeof(bare), base, "Live2DCubismCore.dll")) {
            wide = wide_from_utf8(bare);
            if (wide && try_load(wide)) stash_core(bare, data_dir);
            free(wide);
        }
        if (!core_module &&
            bongo_cat_path_join(directories[count],
                sizeof(directories[count]), base, "live2d"))
            count++;
    }
    if (data_dir && data_dir[0] && count < 2 &&
        bongo_cat_path_join(directories[count],
            sizeof(directories[count]), data_dir, "live2d"))
        count++;
    for (unsigned i = 0; i < count && !core_module; ++i)
        scan_directory(directories[i], data_dir);
    for (unsigned i = 0; i < count; ++i)
        (void)bongo_cat_path_create_directory(directories[i]);
    /* Nothing loaded: leave the live2d folders behind as a visible drop-in
       place for the Core library or the SDK zip.
       SDL3 owns the SDL_GetBasePath() string; freeing it here made every
       later rescan hit a double free inside SDL_free. */
}

void *bongo_cat_platform_live2d_core_symbol(const char *name) {
    if (!attempted && !core_module) bongo_cat_windows_live2d_sdk_prepare(NULL);
    return core_module && name ? (void *)(uintptr_t)GetProcAddress(core_module, name) : NULL;
}
bool bongo_cat_windows_live2d_sdk_ready(void) {
    if (!attempted) bongo_cat_windows_live2d_sdk_prepare(NULL);
    return core_module != NULL;
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
        /* Official SDK zip: unpack below live2d/<zip name>/ — the layout the
           startup scan uses — and load the Core from there. */
        char out_dir[BONGO_CAT_PATH_CAP], live2d_dir[BONGO_CAT_PATH_CAP];
        char stem[BONGO_CAT_PATH_CAP];
        snprintf(stem, sizeof(stem), "%s", bongo_cat_path_name(path));
        size_t stem_length = strlen(stem);
        if (stem_length > 4) stem[stem_length - 4] = '\0';
        if (!bongo_cat_path_join(live2d_dir, sizeof(live2d_dir), data_dir, "live2d") ||
            !bongo_cat_path_join(out_dir, sizeof(out_dir), live2d_dir, stem) ||
            !bongo_cat_path_create_directory(out_dir)) {
            bongo_cat_error_set(error, BONGO_CAT_ERROR_IO,
                "Cannot prepare the extraction folder for the SDK zip");
            return false;
        }
        {
            wchar_t *wide = wide_from_utf8(path);
            bool extracted = wide && extract_sdk_zip(wide, out_dir, target,
                sizeof(target));
            free(wide);
            if (!extracted) {
                bongo_cat_error_set(error, BONGO_CAT_ERROR_FORMAT,
                    "Cannot unpack the SDK zip");
                return false;
            }
            if (!target[0]) {
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
        /* Keep a copy in the data directory; skip when the user picked it. */
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
    if (loaded) return true;
    bongo_cat_error_set(error, BONGO_CAT_ERROR_FORMAT,
        "The selected file is not a usable Cubism Core DLL");
    return false;
}

bool bongo_cat_windows_live2d_sdk_rescan(const char *data_dir) {
    if (core_module) return true;
    attempted = false; /* allow the discovery to run again */
    bongo_cat_windows_live2d_sdk_prepare(data_dir);
    return core_module != NULL;
}
#else
void bongo_cat_windows_live2d_sdk_prepare(const char *data_dir) {
    (void)data_dir;
}
bool bongo_cat_windows_live2d_sdk_import(const char *path,
    const char *data_dir, BongoCatError *error) {
    (void)path; (void)data_dir;
    bongo_cat_error_set(error, BONGO_CAT_ERROR_PLATFORM,
        "Runtime Cubism Core import requires the runtime-Core build");
    return false;
}
bool bongo_cat_windows_live2d_sdk_rescan(const char *data_dir) {
    (void)data_dir;
#ifdef BONGO_CAT_HAS_CUBISM
    return true;
#else
    return false;
#endif
}
void *bongo_cat_platform_live2d_core_symbol(const char *name) { (void)name; return NULL; }
bool bongo_cat_windows_live2d_sdk_ready(void) {
#ifdef BONGO_CAT_HAS_CUBISM
    return true;
#else
    return false;
#endif
}
#endif
