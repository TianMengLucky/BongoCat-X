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

/* A delay-load failure means the located DLL does not match the Core the
   framework was compiled against; log it instead of crashing. */
static FARPROC WINAPI delay_load_failure_hook(unsigned notify,
    PDelayLoadInfo info) {
    if (notify == dliFailLoadLib && info)
        SDL_LogError(SDL_LOG_CATEGORY_CUSTOM,
            "[live2d] Cannot load %s (error %lu)", info->szDll,
            (unsigned long)info->dwLastError);
    else if (notify == dliFailGetProc && info)
        SDL_LogError(SDL_LOG_CATEGORY_CUSTOM,
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
        SDL_LogInfo(SDL_LOG_CATEGORY_CUSTOM,
            "[live2d] Core candidate rejected (error %lu)",
            (unsigned long)GetLastError());
        return false;
    }
    version = (const char *(WINAPI *)(void))(void *)
        GetProcAddress(core_module, "csmVersion");
    text = version ? version() : NULL;
    SDL_LogInfo(SDL_LOG_CATEGORY_CUSTOM, "[live2d] Cubism Core loaded (%s)",
        text ? text : "version unknown");
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
    return copy ? wcscpy(copy, buffer), copy : NULL;
}

/* Pick the Core DLL entry inside an SDK zip: matching architecture only;
   the archive-relative path of the choice lands in entry_name. */
static int select_core_entry(mz_zip_archive *zip, char *entry_name,
    size_t entry_cap) {
    bool want_x64 = sizeof(void *) >= 8;
    int chosen = -1;
    unsigned count = mz_zip_reader_get_num_files(zip);
    unsigned i;
    entry_name[0] = '\0';
    for (i = 0; i < count; ++i) {
        mz_zip_archive_file_stat stat;
        const char *name;
        size_t length;
        bool x64, x86;
        memset(&stat, 0, sizeof(stat));
        if (!mz_zip_reader_file_stat(zip, i, &stat)) continue;
        if (stat.m_is_directory) continue;
        /* Small floor only: the Core DLL itself can weigh well under 1 MB. */
        if (stat.m_uncomp_size < 64 * 1024) continue;
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
            snprintf(entry_name, entry_cap, "%s", name);
            if (x64 == want_x64) break;
        }
    }
    if (chosen < 0)
        SDL_LogWarn(SDL_LOG_CATEGORY_CUSTOM,
            "[live2d] No Cubism Core for this architecture inside the zip");
    return chosen;
}

/* Length of the single top-level folder every entry shares (including its
   separator), or 0 when the entries have no common root. Unpacking the
   contents of that shared folder straight into out_dir (live2d/<zip name>/)
   keeps the SDK browsable without nesting a duplicate directory. */
static size_t common_top_level(mz_zip_archive *zip) {
    const char *common = NULL;
    size_t common_length = 0;
    unsigned count = mz_zip_reader_get_num_files(zip);
    unsigned i;
    for (i = 0; i < count; ++i) {
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

static bool extract_sdk_zip(const wchar_t *zip_path, const char *out_dir,
    char *dll_target, size_t target_cap) {
    FILE *file = _wfopen(zip_path, L"rb");
    long size;
    uint8_t *bytes;
    mz_zip_archive zip;
    bool extracted = true;
    unsigned written = 0, count, i;
    size_t strip;
    char core_name[BONGO_CAT_PATH_CAP];
    int core_index;
    dll_target[0] = '\0';
    if (!file) return false;
    fseek(file, 0, SEEK_END);
    size = ftell(file);
    fseek(file, 0, SEEK_SET);
    if (size <= 0) { fclose(file); return false; }
    bytes = (uint8_t *)malloc((size_t)size);
    if (!bytes || fread(bytes, 1, (size_t)size, file) != (size_t)size) {
        free(bytes);
        fclose(file);
        return false;
    }
    fclose(file);
    memset(&zip, 0, sizeof(zip));
    if (!mz_zip_reader_init_mem(&zip, bytes, (size_t)size, 0)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_CUSTOM,
            "[live2d] Cannot read the SDK zip archive");
        free(bytes);
        return false;
    }
    core_index = select_core_entry(&zip, core_name, sizeof(core_name));
    /* Entries usually share one top-level folder named after the zip; unpack its contents straight into out_dir. */
    strip = common_top_level(&zip);
    {
        const char *core_relative = core_name + strip;
        if (core_index >= 0 &&
            bongo_cat_path_join(dll_target, target_cap, out_dir,
                core_relative) &&
            bongo_cat_path_is_file(dll_target)) {
            /* The Core from an earlier run is already in place. */
            mz_zip_reader_end(&zip);
            free(bytes);
            return true;
        }
    }
    dll_target[0] = '\0';
    count = mz_zip_reader_get_num_files(&zip);
    for (i = 0; i < count; ++i) {
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
        if (stat.m_is_directory || !name[0] || strlen(name) <= strip) continue;
        name += strip;
        /* Zip-slip guard: only plain relative paths land below out_dir. */
        if (name[0] == '/' || name[0] == '\\' || name[1] == ':' ||
            strstr(name, "..")) continue;
        if (!bongo_cat_path_join(target, sizeof(target), out_dir, name))
            continue;
        parent_length = strlen(target);
        while (parent_length && target[parent_length - 1] != '/' &&
            target[parent_length - 1] != '\\')
            parent_length--;
        if (parent_length >= sizeof(parent)) continue;
        memcpy(parent, target, parent_length);
        parent[parent_length] = '\0';
        if (!bongo_cat_path_create_directory(parent)) continue;
        data = mz_zip_reader_extract_to_heap(&zip, i, &file_size, 0);
        if (!data) {
            SDL_LogWarn(SDL_LOG_CATEGORY_CUSTOM,
                "[live2d] Cannot extract %s from the SDK zip", name);
            extracted = false;
            continue;
        }
        out = bongo_cat_file_open(target, "wb");
        if (!out || fwrite(data, 1, file_size, out) != file_size ||
            fclose(out) != 0) {
            SDL_LogWarn(SDL_LOG_CATEGORY_CUSTOM,
                "[live2d] Cannot write %s", target);
            extracted = false;
        } else {
            written++;
        }
        mz_free(data);
    }
    if (core_index >= 0)
        (void)bongo_cat_path_join(dll_target, target_cap, out_dir,
            core_name + strip);
    mz_zip_reader_end(&zip);
    free(bytes);
    SDL_LogInfo(SDL_LOG_CATEGORY_CUSTOM,
        "[live2d] Extracted %u files from the SDK zip below %s", written,
        out_dir);
    return extracted;
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
        char out_dir[BONGO_CAT_PATH_CAP];
        char stem[BONGO_CAT_PATH_CAP];
        snprintf(stem, sizeof(stem), "%s", bongo_cat_path_name(path));
        size_t stem_length = strlen(stem);
        if (stem_length > 4) stem[stem_length - 4] = '\0';
        if (!bongo_cat_path_join(out_dir, sizeof(out_dir), data_dir, "live2d") ||
            !bongo_cat_path_join(out_dir, sizeof(out_dir), out_dir, stem) ||
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
bool bongo_cat_windows_live2d_sdk_ready(void) {
#ifdef BONGO_CAT_HAS_CUBISM
    return true;
#else
    return false;
#endif
}
#endif
