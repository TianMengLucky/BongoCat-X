#include "bongo_cat/model_plugins.h"
#include "bongo_cat/path.h"
#include "bongo_cat/platform.h"
#include "model_import_archive.h"
#include <SDL3/SDL.h>
#include <string.h>
#ifndef _WIN32
#include <strings.h>
#endif

typedef struct PluginArchive {
    char plugin[BONGO_CAT_PATH_CAP];
    unsigned count;
    bool core;
} PluginArchive;

static SDL_EnumerationResult SDLCALL scan_plugin(void *userdata,
    const char *directory, const char *name) {
    PluginArchive *scan = userdata;
    char path[BONGO_CAT_PATH_CAP];
    SDL_PathInfo info;
    if (!bongo_cat_path_join(path, sizeof(path), directory, name) ||
        !SDL_GetPathInfo(path, &info)) return SDL_ENUM_FAILURE;
#ifdef _WIN32
    const char *core_name = "Live2DCubismCore.dll";
#elif defined(__APPLE__)
    const char *core_name = "libLive2DCubismCore.dylib";
#else
    const char *core_name = "libLive2DCubismCore.so";
#endif
    if (info.type == SDL_PATHTYPE_FILE && !SDL_strcasecmp(name, core_name))
        scan->core = true;
    if (info.type == SDL_PATHTYPE_DIRECTORY)
        return SDL_EnumerateDirectory(path, scan_plugin, scan) ?
            SDL_ENUM_CONTINUE : SDL_ENUM_FAILURE;
    for (BongoCatModelEngine kind = BONGO_CAT_MODEL_ENGINE_LIVE2D;
        kind <= BONGO_CAT_MODEL_ENGINE_INOX2D; ++kind) {
        if (info.type == SDL_PATHTYPE_FILE &&
            !SDL_strcmp(name, bongo_cat_model_plugin_filename(kind))) {
            SDL_strlcpy(scan->plugin, path, sizeof(scan->plugin));
            scan->count++;
        }
    }
    return SDL_ENUM_CONTINUE;
}

bool bongo_cat_model_plugin_install_archive(const char *source, const char *data_root,
    BongoCatModelEngine *engine, BongoCatError *error) {
    char directory[BONGO_CAT_PATH_CAP], temporary[BONGO_CAT_PATH_CAP];
    if (bongo_cat_import_archive_extract(source, directory, temporary, error) != BONGO_CAT_OK)
        return false;
    PluginArchive scan = {0};
    bool ok = SDL_EnumerateDirectory(directory, scan_plugin, &scan);
    if (!ok || scan.count != 1) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_FORMAT,
            "Plugin ZIP must contain exactly one renderer plugin for this platform.");
        ok = false;
    }
    if (ok) ok = bongo_cat_model_plugin_install(scan.plugin, engine, error);
    /* Reuse the native SDK importer to select the matching Core architecture.
       Core remains external to the renderer's managed plugin directory. */
    if (ok && scan.core && !bongo_cat_platform_live2d_core_available())
        ok = bongo_cat_platform_live2d_core_import(source, data_root, error);
    bongo_cat_import_archive_cleanup(temporary);
    return ok;
}
