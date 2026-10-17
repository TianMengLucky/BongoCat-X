#include "plugin_locale.h"
#include "store_internal.h"
#include "bongo_cat/path.h"
#include "bongo_cat/file.h"
#include "bongo_cat/log.h"
#include "bongo_cat/preferences.h"
#include "runtime.h"
#include "bongo_cat/safe_http.h"
#include <stdio.h>
#include <string.h>
const char *bongo_cat_store_filename(void) {
#ifdef _WIN32
    return "bongo_store.dll";
#elif defined(__APPLE__)
    return "libbongo_store.dylib";
#else
    return "libbongo_store.so";
#endif
}
bool bongo_cat_store_path(BongoCatApp *app, const char *suffix, char *out) {
    char directory[BONGO_CAT_PATH_CAP], name[80];
    snprintf(name, sizeof(name), "%s%s", bongo_cat_store_filename(), suffix);
    return app && bongo_cat_path_join(directory, sizeof(directory), app->data_root, "plugins") &&
        bongo_cat_path_join(out, BONGO_CAT_PATH_CAP, directory, name);
}
static bool resolve(BongoCatApp *app, char *out) {
    const char *override = SDL_getenv("BONGO_CAT_PLUGIN_DIRECTORY");
    if (override && override[0]) return bongo_cat_path_join(out, BONGO_CAT_PATH_CAP,
        override, bongo_cat_store_filename()) && bongo_cat_path_is_file(out);
    if (bongo_cat_store_path(app, "", out) && bongo_cat_path_is_file(out)) return true;
    const char *base = SDL_GetBasePath(); /* Borrowed from SDL3. */
    char directory[BONGO_CAT_PATH_CAP];
    if (!base) return false;
    if (bongo_cat_path_join(directory, sizeof(directory), base, "plugins") &&
        bongo_cat_path_join(out, BONGO_CAT_PATH_CAP, directory, bongo_cat_store_filename()) &&
        bongo_cat_path_is_file(out)) return true;
#ifdef __APPLE__
    if (bongo_cat_path_join(directory, sizeof(directory), base, "../PlugIns") &&
        bongo_cat_path_join(out, BONGO_CAT_PATH_CAP, directory, bongo_cat_store_filename()) &&
        bongo_cat_path_is_file(out)) return true;
#endif
    return false;
}
void bongo_cat_store_info(BongoCatApp *app, BongoCatStoreInfo *info) {
    if (!info) return;
    *info = (BongoCatStoreInfo){0};
    if (!app) return;
    info->installed = resolve(app, info->path);
    char path[BONGO_CAT_PATH_CAP];
    info->enabled = info->installed && bongo_cat_store_path(app, ".disabled", path) &&
        !bongo_cat_path_is_file(path);
    info->managed = info->installed && bongo_cat_store_path(app, "", path) && !strcmp(path, info->path);
    info->active = app->store != NULL;
}
const BongoCatStorePlugin *bongo_cat_store_query(SDL_SharedObject *library) {
    union { SDL_FunctionPointer function; BongoCatStoreQuery query; } entry;
    entry.function = SDL_LoadFunction(library, BONGO_CAT_STORE_SYMBOL);
    const BongoCatStorePlugin *api = entry.function ? entry.query(BONGO_CAT_STORE_ABI) : NULL;
    return api && api->abi_version == BONGO_CAT_STORE_ABI && api->struct_size == sizeof(*api) &&
        api->create && api->destroy && api->parse && api->count && api->entry &&
        bongo_cat_store_transfer_query(library) && bongo_cat_store_models_query(library) &&
        bongo_cat_plugin_events_query(library, "bongo_cat_store_events_query_v1") ? api : NULL;
}
static const BongoCatPluginUiContribution *query_ui(SDL_SharedObject *library) {
    union { SDL_FunctionPointer function; BongoCatPluginUiQuery query; } entry;
    entry.function = SDL_LoadFunction(library, BONGO_CAT_STORE_UI_SYMBOL);
    if (!entry.function) return NULL;
    const BongoCatPluginUiContribution *contribution = entry.query(BONGO_CAT_PLUGIN_UI_ABI);
    return contribution && contribution->abi_version == BONGO_CAT_PLUGIN_UI_ABI &&
        contribution->struct_size == sizeof(*contribution) && contribution->draw ?
        contribution : NULL;
}
static bool work(void *userdata, const BongoCatPluginJob *job, BongoCatError *error) {
    BongoCatStore *v = userdata;
    if (job->kind != 1) return bongo_cat_store_asset_work(v, job, error);
    BongoCatPluginTransferHost host = bongo_cat_store_transfer_host(v);
    unsigned status = v->transfer->catalog(v->instance, v->repository, v->accelerate, &host);
    SDL_LockMutex(v->worker.state_mutex);
    v->status = status;
    if (status == 2) memset(v->preview_state, 0, sizeof(v->preview_state));
    SDL_UnlockMutex(v->worker.state_mutex);
    atomic_store(&v->worker.changed, true);
    if (status != 2) bongo_cat_error_set(error, BONGO_CAT_ERROR_IO, "Store plugin catalog operation failed");
    return status == 2;
}
void bongo_cat_store_stop(BongoCatApp *app) {
    if (!app || !app->store) return;
    BongoCatStore *v = app->store;
    SDL_SetAtomicInt(&v->cancel, 1);
    bongo_cat_plugin_worker_stop(&v->worker);
    app->store = NULL;
    bongo_cat_store_apply_receipt(app, v);
    v->api->destroy(v->instance);
    bongo_cat_i18n_unregister_plugin(app->i18n, "store");
    bongo_cat_plugin_page_stop(&v->page_ui);
    SDL_UnloadObject(v->library); SDL_free(v);
    bongo_cat_preferences_invalidate(app->preferences);
}
void bongo_cat_store_start(BongoCatApp *app) {
    if (!app || app->secondary_pet || app->store) return;
    BongoCatStoreInfo info; bongo_cat_store_info(app, &info);
    if (!info.enabled) return;
    SDL_SharedObject *library = SDL_LoadObject(info.path);
    const BongoCatStorePlugin *api = library ? bongo_cat_store_query(library) : NULL;
    BongoCatStore *v = api ? SDL_calloc(1, sizeof(*v)) : NULL;
    if (v) {
        atomic_init(&v->receipt_ready, false);
        v->library = library; v->api = api;
        v->transfer = bongo_cat_store_transfer_query(library);
        v->models = bongo_cat_store_models_query(library);
        v->events = bongo_cat_plugin_events_query(library, "bongo_cat_store_events_query_v1");
        v->ui = query_ui(library); v->instance = api->create();
    }
    if (!v || !v->instance || !bongo_cat_plugin_worker_start(&v->worker, "store", work, v, false)) {
        if (v && v->instance) api->destroy(v->instance);
        SDL_free(v); if (library) SDL_UnloadObject(library);
        SDL_LogWarn(BONGO_CAT_LOG_LIFECYCLE, "[store] Cannot load store plugin: %s", SDL_GetError()); return;
    }
    if (!bongo_cat_path_join(v->cache, sizeof(v->cache), app->data_root, "cache/store") ||
        !bongo_cat_path_create_directory(v->cache)) {
        app->store = v; bongo_cat_store_stop(app); return;
    }
    SDL_strlcpy(v->models_root, app->models_root, sizeof(v->models_root));
    v->accelerate = app->settings.app.github_download_acceleration;
    app->store = v;
    bongo_cat_plugin_locale_load(app->i18n, library, "store");
    bongo_cat_preferences_plugin_fonts_changed(app->preferences);
    bongo_cat_preferences_invalidate(app->preferences);
}
static bool marker(BongoCatApp *app, bool enabled) {
    char path[BONGO_CAT_PATH_CAP], directory[BONGO_CAT_PATH_CAP];
    if (!bongo_cat_path_join(directory, sizeof(directory), app->data_root, "plugins") ||
        !bongo_cat_path_create_directory(directory) || !bongo_cat_store_path(app, ".disabled", path)) return false;
    if (enabled) return !bongo_cat_path_is_file(path) || bongo_cat_file_remove(path);
    FILE *file = bongo_cat_file_open(path, "wb"); return file && fclose(file) == 0;
}
bool bongo_cat_store_set_enabled(BongoCatApp *app, bool enabled, BongoCatError *error) {
    if (!app || app->secondary_pet) return false;
    if (app->store && (app->store->receipt_ready || bongo_cat_plugin_worker_busy(&app->store->worker))) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_IO, "Store is refreshing; retry when it finishes"); return false;
    }
    if (!marker(app, enabled)) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_IO, "Cannot save store activation state"); return false;
    }
    if (!enabled) { bongo_cat_store_stop(app); return true; }
    bongo_cat_store_start(app);
    if (app->store) return true;
    (void)marker(app, false);
    bongo_cat_error_set(error, BONGO_CAT_ERROR_IO, "Cannot activate store plugin"); return false;
}
bool bongo_cat_store_busy(BongoCatApp *app) {
    return app && app->store && (atomic_load(&app->store->receipt_ready) || bongo_cat_plugin_worker_busy(&app->store->worker));
}
bool bongo_cat_store_active(BongoCatApp *app) { return app && app->store; }
bool bongo_cat_store_refresh(BongoCatApp *app) {
    if (!app || !app->store || app->store->receipt_ready || bongo_cat_plugin_worker_busy(&app->store->worker)) return false;
    BongoCatStore *v = app->store;
    SDL_LockMutex(v->worker.state_mutex); v->status = 1; v->operation = 0;
    v->accelerate = app->settings.app.github_download_acceleration;
    (void)bongo_safe_store_repository_get(app->settings.extensions_json,
        sizeof(app->settings.extensions_json), v->repository, sizeof(v->repository));
    SDL_UnlockMutex(v->worker.state_mutex);
    BongoCatPluginJob job = {0}; job.kind = 1;
    if (bongo_cat_plugin_worker_submit(&v->worker, &job)) return true;
    SDL_LockMutex(v->worker.state_mutex); v->status = 4; SDL_UnlockMutex(v->worker.state_mutex);
    return false;
}
unsigned bongo_cat_store_status(BongoCatApp *app, unsigned *count) {
    *count = 0;
    if (!app || !app->store) return 0;
    BongoCatStore *v = app->store;
    SDL_LockMutex(v->worker.state_mutex);
    unsigned status = v->status;
    *count = status == 2 ? SDL_min(v->api->count(v->instance), BONGO_CAT_STORE_CAP) : 0;
    SDL_UnlockMutex(v->worker.state_mutex); return status;
}
bool bongo_cat_store_entry(BongoCatApp *app, unsigned index, BongoCatStoreEntry *out) {
    if (!app || !app->store || !out || index >= BONGO_CAT_STORE_CAP) return false;
    BongoCatStore *v = app->store;
    SDL_LockMutex(v->worker.state_mutex);
    bool ok = v->transfer->entry(v->instance, index, v->repository, out) != 0;
    SDL_UnlockMutex(v->worker.state_mutex);
    return ok;
}

void bongo_cat_store_tick(BongoCatApp *app) {
    if (!app || !app->store) return;
    bongo_cat_store_apply_receipt(app, app->store);
    if (atomic_exchange(&app->store->worker.changed, false)) {
        app->store->names_pending = true;
        bongo_cat_preferences_invalidate(app->preferences);
    }
    if (app->store->names_pending && !bongo_cat_store_busy(app)) {
        app->store->names_pending = false;
        bongo_cat_store_sync_names(app);
    }
}
const BongoCatPluginUiContribution *bongo_cat_store_ui(BongoCatApp *app) {
    return app && app->store ? app->store->ui : NULL;
}

const BongoCatPluginPageUiContribution *bongo_cat_store_page_ui(BongoCatApp *app, void **instance) {
    if (!app || !app->store) return NULL;
    return bongo_cat_plugin_page_query(&app->store->page_ui, app->store->library,
        "bongo_cat_store_page_ui_query_v1", instance);
}
