#include "items_internal.h"
#include "bongo_cat/path.h"
#include "bongo_cat/file.h"
#include "bongo_cat/model_plugins.h"
#include "bongo_cat/statistics.h"
#include "bongo_cat/sounds.h"
#include "bongo_cat/store.h"
#include "model_import_archive.h"
#include <string.h>
static bool install(BongoCatApp *app, const char *source, BongoCatError *error) {
    uint64_t size = 0;
    if (!source || !bongo_cat_path_file_size(source, &size) || !size || size > 256u * 1024u * 1024u) goto failed;
    SDL_SharedObject *library = SDL_LoadObject(source);
    bool valid = library && bongo_cat_items_query(library);
    if (library) SDL_UnloadObject(library);
    if (!valid) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_FORMAT, "Items plugin ABI is incompatible"); return false;
    }
    char target[BONGO_CAT_PATH_CAP], temporary[BONGO_CAT_PATH_CAP] = {0}, directory[BONGO_CAT_PATH_CAP];
    bool ok = bongo_cat_path_join(directory, sizeof(directory), app->data_root, "plugins") &&
        bongo_cat_path_create_directory(directory) && bongo_cat_items_managed_path(app, "", target) &&
        bongo_cat_items_managed_path(app, ".new", temporary) && bongo_cat_path_copy_file(source, temporary);
    if (ok) ok = bongo_cat_items_set_enabled(app, false, error);
    if (ok) ok = bongo_cat_file_replace(temporary, target, true);
    if (temporary[0]) (void)bongo_cat_file_remove(temporary);
    if (ok) return bongo_cat_items_set_enabled(app, true, error);
failed:
    if (!error || !error->message[0]) bongo_cat_error_set(error, BONGO_CAT_ERROR_IO, "Cannot install items plugin");
    return false;
}
bool bongo_cat_items_remove_plugin(BongoCatApp *app, BongoCatError *error) {
    BongoCatItemsInfo info; bongo_cat_items_info(app, &info);
    if (!info.managed) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_ARGUMENT, "Only a managed items plugin can be removed"); return false;
    }
    if (!bongo_cat_items_set_enabled(app, false, error)) return false;
    if (bongo_cat_file_remove(info.path)) return true;
    bongo_cat_error_set(error, BONGO_CAT_ERROR_IO, "Cannot remove items plugin"); return false;
}
typedef struct ItemsArchive { unsigned plugins, items; char path[BONGO_CAT_PATH_CAP]; } ItemsArchive;
static SDL_EnumerationResult SDLCALL scan(void *userdata, const char *directory, const char *name) {
    ItemsArchive *v = userdata; char path[BONGO_CAT_PATH_CAP]; SDL_PathInfo info;
    if (!bongo_cat_path_join(path, sizeof(path), directory, name) || !SDL_GetPathInfo(path, &info)) return SDL_ENUM_FAILURE;
    if (info.type == SDL_PATHTYPE_DIRECTORY)
        return SDL_EnumerateDirectory(path, scan, v) ? SDL_ENUM_CONTINUE : SDL_ENUM_FAILURE;
    if (info.type != SDL_PATHTYPE_FILE) return SDL_ENUM_CONTINUE;
    if (!strcmp(name, bongo_cat_items_filename())) {
        ++v->plugins; ++v->items; SDL_strlcpy(v->path, path, sizeof(v->path));
    }
    if (!strcmp(name, bongo_cat_statistics_filename()) || !strcmp(name, bongo_cat_sounds_filename())) ++v->plugins;
    if (!SDL_strcmp(name, bongo_cat_store_filename())) ++v->plugins;
    for (BongoCatModelEngine engine = BONGO_CAT_MODEL_ENGINE_LIVE2D; engine <= BONGO_CAT_MODEL_ENGINE_INOX2D; ++engine)
        if (!strcmp(name, bongo_cat_model_plugin_filename(engine))) ++v->plugins;
    return SDL_ENUM_CONTINUE;
}
bool bongo_cat_items_import_plugin(BongoCatApp *app, const char *source, bool *matched, BongoCatError *error) {
    if (!matched) return false;
    *matched = false;
    if (!app || !source) return false;
    if (bongo_cat_import_is_archive(source)) {
        char directory[BONGO_CAT_PATH_CAP], temporary[BONGO_CAT_PATH_CAP];
        if (bongo_cat_import_archive_extract(source, directory, temporary, error) != BONGO_CAT_OK) {
            *matched = true; return false;
        }
        ItemsArchive v = {0}; bool ok = SDL_EnumerateDirectory(directory, scan, &v);
        *matched = !ok || v.items > 0;
        if (*matched) {
            if (!ok || v.plugins != 1) {
                bongo_cat_error_set(error, BONGO_CAT_ERROR_FORMAT, "Items ZIP must contain exactly one plugin for this platform"); ok = false;
            } else ok = install(app, v.path, error);
        }
        bongo_cat_import_archive_cleanup(temporary);
        return *matched && ok;
    }
    SDL_SharedObject *library = SDL_LoadObject(source);
    if (library) {
        *matched = SDL_LoadFunction(library, BONGO_CAT_ITEMS_SYMBOL) != NULL;
        SDL_UnloadObject(library);
    }
    if (!*matched) SDL_ClearError();
    return *matched && install(app, source, error);
}
