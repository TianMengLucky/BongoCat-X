#include "plugin_locale.h"
#include "sounds_internal.h"
#include "bongo_cat/path.h"
#include "bongo_cat/file.h"
#include "bongo_cat/log.h"
#include "bongo_cat/preferences.h"
#include "runtime.h"
#include <stdio.h>
#include <string.h>
const char *bongo_cat_sounds_filename(void) {
#ifdef _WIN32
    return "bongo_sounds.dll";
#elif defined(__APPLE__)
    return "libbongo_sounds.dylib";
#else
    return "libbongo_sounds.so";
#endif
}
bool bongo_cat_sounds_managed_path(BongoCatApp *app, const char *suffix, char *out) {
    char directory[BONGO_CAT_PATH_CAP], name[80];
    snprintf(name, sizeof(name), "%s%s", bongo_cat_sounds_filename(), suffix);
    return app && bongo_cat_path_join(directory, sizeof(directory), app->data_root, "plugins") &&
        bongo_cat_path_join(out, BONGO_CAT_PATH_CAP, directory, name);
}
static bool resolve(BongoCatApp *app, char *out) {
    const char *override = SDL_getenv("BONGO_CAT_PLUGIN_DIRECTORY");
    if (override && override[0]) return bongo_cat_path_join(out, BONGO_CAT_PATH_CAP,
        override, bongo_cat_sounds_filename()) && bongo_cat_path_is_file(out);
    if (bongo_cat_sounds_managed_path(app, "", out) && bongo_cat_path_is_file(out)) return true;
    const char *base = SDL_GetBasePath(); /* Borrowed from SDL3. */
    char directory[BONGO_CAT_PATH_CAP];
    if (!base) return false;
    if (bongo_cat_path_join(directory, sizeof(directory), base, "plugins") &&
        bongo_cat_path_join(out, BONGO_CAT_PATH_CAP, directory, bongo_cat_sounds_filename()) &&
        bongo_cat_path_is_file(out)) return true;
#ifdef __APPLE__
    if (bongo_cat_path_join(directory, sizeof(directory), base, "../PlugIns") &&
        bongo_cat_path_join(out, BONGO_CAT_PATH_CAP, directory, bongo_cat_sounds_filename()) &&
        bongo_cat_path_is_file(out)) return true;
#endif
    return false;
}
void bongo_cat_sounds_info(BongoCatApp *app, BongoCatSoundsInfo *info) {
    if (!info) return;
    *info = (BongoCatSoundsInfo){0};
    if (!app) return;
    info->installed = resolve(app, info->path);
    char path[BONGO_CAT_PATH_CAP];
    info->enabled = info->installed && bongo_cat_sounds_managed_path(app, ".disabled", path) &&
        !bongo_cat_path_is_file(path);
    info->managed = info->installed && bongo_cat_sounds_managed_path(app, "", path) &&
        !strcmp(path, info->path);
    info->active = app->sounds != NULL;
}
const BongoCatSoundsPlugin *bongo_cat_sounds_query(SDL_SharedObject *library) {
    union { SDL_FunctionPointer function; BongoCatSoundsQuery query; } entry;
    entry.function = SDL_LoadFunction(library, BONGO_CAT_SOUNDS_SYMBOL);
    const BongoCatSoundsPlugin *api = entry.function ? entry.query(BONGO_CAT_SOUNDS_ABI) : NULL;
    return api && api->abi_version == BONGO_CAT_SOUNDS_ABI && api->struct_size == sizeof(*api) &&
        api->create && api->destroy && api->page && api->snapshot && api->restore &&
        api->key_count && api->key && api->bind && api->press && api->key_count() > 0 && api->key_count() <= BONGO_CAT_SOUNDS_CAP &&
        bongo_cat_plugin_io_query(library, "bongo_cat_sounds_io_query_v1") &&
        bongo_cat_plugin_events_query(library, "bongo_cat_sounds_events_query_v1") ? api : NULL;
}
static const BongoCatPluginUiContribution *query_ui(SDL_SharedObject *library) {
    union { SDL_FunctionPointer function; BongoCatPluginUiQuery query; } entry;
    entry.function = SDL_LoadFunction(library, BONGO_CAT_SOUNDS_UI_SYMBOL);
    if (!entry.function) return NULL;
    const BongoCatPluginUiContribution *contribution = entry.query(BONGO_CAT_PLUGIN_UI_ABI);
    return contribution && contribution->abi_version == BONGO_CAT_PLUGIN_UI_ABI &&
        contribution->struct_size == sizeof(*contribution) && contribution->draw ?
        contribution : NULL;
}
static void detach(BongoCatApp *app) {
    if (!app->sounds) return;
    bongo_cat_plugin_worker_stop(&app->sounds->worker);
    bongo_cat_audio_destroy(app->sounds->audio);
    BongoCatSounds *v = app->sounds;
    app->sounds = NULL; /* Page disappears before library unload. */
    v->api->destroy(v->instance);
    bongo_cat_i18n_unregister_plugin(app->i18n, "sounds");
    bongo_cat_plugin_page_stop(&v->page_ui);
    SDL_UnloadObject(v->library);
    SDL_free(v);
    bongo_cat_preferences_invalidate(app->preferences);
    app->dirty = true;
}
static bool start(BongoCatApp *app, BongoCatError *error) {
    if (!app || app->secondary_pet) return false;
    if (app->sounds) return true;
    BongoCatSoundsInfo info;
    bongo_cat_sounds_info(app, &info);
    if (!info.enabled) return false;
    SDL_SharedObject *library = SDL_LoadObject(info.path);
    const BongoCatSoundsPlugin *api = library ? bongo_cat_sounds_query(library) : NULL;
    BongoCatSounds *v = api ? SDL_calloc(1, sizeof(*v)) : NULL;
    if (v) {
        atomic_init(&v->loaded, false);
        v->library = library; v->api = api; v->ui = query_ui(library);
        v->io = bongo_cat_plugin_io_query(library, "bongo_cat_sounds_io_query_v1");
        v->events = bongo_cat_plugin_events_query(library, "bongo_cat_sounds_events_query_v1");
        v->instance = api->create();
        v->page.struct_size = sizeof(v->page);
    }
    bool valid = v && v->instance && api->page(v->instance, &v->page) &&
        v->page.struct_size == sizeof(v->page) &&
        memchr(v->page.id, 0, sizeof(v->page.id)) &&
        memchr(v->page.title_key, 0, sizeof(v->page.title_key)) &&
        memchr(v->page.title, 0, sizeof(v->page.title)) &&
        !strcmp(v->page.id, "sounds") && v->page.title_key[0] && v->page.title[0];
    if (!valid) {
        if (v && v->instance) api->destroy(v->instance);
        SDL_free(v);
        if (library) SDL_UnloadObject(library);
        bongo_cat_error_set(error, BONGO_CAT_ERROR_FORMAT, "Sounds plugin ABI or page is incompatible");
        return false;
    }
    app->sounds = v;
    v->audio = bongo_cat_audio_create(error);
    if (!v->audio || !bongo_cat_sounds_worker_start(app)) { detach(app); return false; }
    bongo_cat_plugin_locale_load(app->i18n, library, "sounds");
    bongo_cat_preferences_plugin_fonts_changed(app->preferences);
    bongo_cat_preferences_invalidate(app->preferences);
    return true;
}
void bongo_cat_sounds_start(BongoCatApp *app) {
    BongoCatError error = {0};
    if (!start(app, &error) && error.message[0])
        SDL_LogWarn(BONGO_CAT_LOG_LIFECYCLE, "[sounds] %s", error.message);
}
void bongo_cat_sounds_stop(BongoCatApp *app) {
    if (!app || !app->sounds) return;
    detach(app);
}
static bool marker(BongoCatApp *app, bool enabled) {
    char path[BONGO_CAT_PATH_CAP], directory[BONGO_CAT_PATH_CAP];
    if (!bongo_cat_path_join(directory, sizeof(directory), app->data_root, "plugins") ||
        !bongo_cat_path_create_directory(directory) ||
        !bongo_cat_sounds_managed_path(app, ".disabled", path)) return false;
    if (enabled) return !bongo_cat_path_is_file(path) || bongo_cat_file_remove(path);
    FILE *file = bongo_cat_file_open(path, "wb");
    return file && fclose(file) == 0;
}
bool bongo_cat_sounds_set_enabled(BongoCatApp *app, bool enabled, BongoCatError *error) {
    if (!app || app->secondary_pet) return false;
    if (!enabled && bongo_cat_sounds_busy(app)) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_IO, "Sound effects worker is busy; retry when the operation finishes"); return false;
    }
    if (!marker(app, enabled)) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_IO, "Cannot save sounds plugin activation state");
        return false;
    }
    if (!enabled) { detach(app); return true; }
    if (start(app, error)) return true;
    (void)marker(app, false);
    return false;
}
const BongoCatSoundsPage *bongo_cat_sounds_page(BongoCatApp *app) {
    return app && app->sounds ? &app->sounds->page : NULL;
}
const BongoCatPluginUiContribution *bongo_cat_sounds_ui(BongoCatApp *app) {
    return app && app->sounds ? app->sounds->ui : NULL;
}
bool bongo_cat_sounds_snapshot(BongoCatApp *app, BongoCatSoundScene *scene) {
    if (!app || !app->sounds || !scene) return false;
    if (!atomic_load_explicit(&app->sounds->loaded, memory_order_acquire) &&
        app->sounds->worker.core && !bongo_cat_plugin_worker_current(&app->sounds->worker)) return false;
    *scene = (BongoCatSoundScene){0}; scene->struct_size = sizeof(*scene);
    if (app->sounds->worker.state_mutex) SDL_LockMutex(app->sounds->worker.state_mutex);
    bool ok = app->sounds->api->snapshot(app->sounds->instance, scene) != 0 &&
        scene->struct_size == sizeof(*scene) && scene->count == app->sounds->api->key_count();
    if (app->sounds->worker.state_mutex) SDL_UnlockMutex(app->sounds->worker.state_mutex);
    return ok;
}

const BongoCatPluginPageUiContribution *bongo_cat_sounds_page_ui(BongoCatApp *app, void **instance) {
    if (!app || !app->sounds) return NULL;
    return bongo_cat_plugin_page_query(&app->sounds->page_ui, app->sounds->library,
        "bongo_cat_sounds_page_ui_query_v1", instance);
}
