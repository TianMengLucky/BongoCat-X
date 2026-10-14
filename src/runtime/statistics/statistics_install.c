#include "statistics_internal.h"
#include "bongo_cat/file.h"
#include "bongo_cat/model_plugins.h"
#include "bongo_cat/items.h"
#include "bongo_cat/store.h"
#include "bongo_cat/sounds.h"
#include "bongo_cat/path.h"
#include "model_import_archive.h"
#include <stdio.h>

bool bongo_cat_statistics_install(BongoCatApp *app, const char *source, BongoCatError *error) {
    uint64_t size = 0;
    if (!app || !source || !bongo_cat_path_file_size(source, &size) || !size || size > 256u * 1024u * 1024u) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_ARGUMENT, "Invalid statistics plugin library"); return false;
    }
    if (app->statistics_retiring) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_IO, "Statistics plugin is finishing a background operation"); return false;
    }
    SDL_SharedObject *library = SDL_LoadObject(source);
    bool valid = library && bongo_cat_statistics_query(library);
    if (library) SDL_UnloadObject(library);
    if (!valid) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_FORMAT, "Statistics plugin ABI is incompatible"); return false;
    }
    char directory[BONGO_CAT_PATH_CAP], temporary[BONGO_CAT_PATH_CAP] = {0}, target[BONGO_CAT_PATH_CAP];
    if (!bongo_cat_path_join(directory, sizeof(directory), app->data_root, "plugins") ||
        !bongo_cat_path_create_directory(directory) || !bongo_cat_statistics_path(app, ".new", temporary) ||
        !bongo_cat_statistics_path(app, "", target) || !bongo_cat_path_copy_file(source, temporary)) {
        if (temporary[0]) (void)bongo_cat_file_remove(temporary);
        bongo_cat_error_set(error, BONGO_CAT_ERROR_IO, "Cannot stage the statistics plugin"); return false;
    }
    if (app->statistics_retiring) {
        (void)bongo_cat_file_remove(temporary);
        bongo_cat_error_set(error, BONGO_CAT_ERROR_IO, "Statistics plugin is finishing a background operation"); return false;
    }
    bool was_active = app->statistics != NULL;
    if (was_active) {
        BongoCatStatistics *stats = app->statistics;
        SDL_strlcpy(stats->install_source, temporary, sizeof(stats->install_source));
        SDL_strlcpy(stats->install_target, target, sizeof(stats->install_target)); stats->restart = true;
        if (bongo_cat_statistics_set_enabled(app, false, error)) return true;
        stats->install_source[0] = stats->install_target[0] = 0; stats->restart = false;
        (void)bongo_cat_file_remove(temporary); return false;
    }
    if (!bongo_cat_statistics_set_enabled(app, false, error)) { (void)bongo_cat_file_remove(temporary); return false; }
    bool ok = bongo_cat_file_replace(temporary, target, true);
    (void)bongo_cat_file_remove(temporary);
    if (!ok) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_IO, "Cannot install the statistics plugin"); return false;
    }
    return bongo_cat_statistics_set_enabled(app, true, error);
}
bool bongo_cat_statistics_remove(BongoCatApp *app, BongoCatError *error) {
    char path[BONGO_CAT_PATH_CAP];
    if (!app || !bongo_cat_statistics_path(app, "", path)) return false;
    if (app->statistics && bongo_cat_path_is_file(path)) {
        BongoCatStatistics *stats = app->statistics;
        SDL_strlcpy(stats->remove_path, path, sizeof(stats->remove_path));
        if (bongo_cat_statistics_set_enabled(app, false, error)) return true;
        stats->remove_path[0] = 0; return false;
    }
    if (!bongo_cat_statistics_set_enabled(app, false, error)) return false;
    if (!bongo_cat_path_is_file(path) || bongo_cat_file_remove(path)) return true;
    bongo_cat_error_set(error, BONGO_CAT_ERROR_IO, "Cannot remove the statistics plugin"); return false;
}

typedef struct StatisticsArchive {
    unsigned count, statistics;
    char path[BONGO_CAT_PATH_CAP];
} StatisticsArchive;
static SDL_EnumerationResult SDLCALL scan(void *userdata, const char *directory, const char *name) {
    StatisticsArchive *value = userdata;
    char path[BONGO_CAT_PATH_CAP]; SDL_PathInfo info;
    if (!bongo_cat_path_join(path, sizeof(path), directory, name) || !SDL_GetPathInfo(path, &info)) return SDL_ENUM_FAILURE;
    if (info.type == SDL_PATHTYPE_DIRECTORY)
        return SDL_EnumerateDirectory(path, scan, value) ? SDL_ENUM_CONTINUE : SDL_ENUM_FAILURE;
    if (info.type != SDL_PATHTYPE_FILE) return SDL_ENUM_CONTINUE;
    if (!SDL_strcmp(name, bongo_cat_statistics_filename())) {
        ++value->count; ++value->statistics;
        SDL_strlcpy(value->path, path, sizeof(value->path));
    }
    if (!SDL_strcmp(name, bongo_cat_items_filename()) || !SDL_strcmp(name, bongo_cat_sounds_filename())) ++value->count;
    if (!SDL_strcmp(name, bongo_cat_store_filename())) ++value->count;
    for (BongoCatModelEngine engine = BONGO_CAT_MODEL_ENGINE_LIVE2D; engine <= BONGO_CAT_MODEL_ENGINE_INOX2D; ++engine)
        if (!SDL_strcmp(name, bongo_cat_model_plugin_filename(engine))) ++value->count;
    return SDL_ENUM_CONTINUE;
}
bool bongo_cat_statistics_import(BongoCatApp *app, const char *source, bool *matched, BongoCatError *error) {
    *matched = false;
    if (bongo_cat_import_is_archive(source)) {
        char directory[BONGO_CAT_PATH_CAP], temporary[BONGO_CAT_PATH_CAP];
        if (bongo_cat_import_archive_extract(source, directory, temporary, error) != BONGO_CAT_OK) { *matched = true; return false; }
        StatisticsArchive value = {0};
        bool ok = SDL_EnumerateDirectory(directory, scan, &value);
        *matched = !ok || value.statistics > 0;
        if (*matched) {
            if (!ok || value.count != 1) {
                bongo_cat_error_set(error, BONGO_CAT_ERROR_FORMAT, "Statistics ZIP must contain exactly one plugin for this platform"); ok = false;
            } else ok = bongo_cat_statistics_install(app, value.path, error);
        }
        bongo_cat_import_archive_cleanup(temporary);
        return *matched && ok;
    }
    if (app->statistics_retiring) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_IO, "Statistics plugin is finishing a background operation"); return false;
    }
    SDL_SharedObject *library = SDL_LoadObject(source);
    if (library) {
        *matched = SDL_LoadFunction(library, BONGO_CAT_STATISTICS_SYMBOL) != NULL;
        SDL_UnloadObject(library);
        if (!*matched) SDL_ClearError();
    }
    return *matched && bongo_cat_statistics_install(app, source, error);
}
