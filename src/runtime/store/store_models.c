#include "store_internal.h"
#include "bongo_cat/path.h"
#include "bongo_cat/file.h"
#include "bongo_cat/i18n.h"
#include "bongo_cat/preferences.h"
#include "preferences_notice.h"
#include "runtime.h"
#include "plugin_models.h"
#include <stdio.h>
#include <string.h>
bool bongo_cat_store_save_receipt(BongoCatStore *v, const char *hash, const BongoCatImportReceipt *receipt) {
    BongoCatPluginTransferHost host = bongo_cat_store_transfer_host(v);
    return v->transfer->save_receipt(v->cache, hash, receipt, &host) != 0;
}
const BongoCatStoreModelsPlugin *bongo_cat_store_models_query(SDL_SharedObject *library) {
    union {SDL_FunctionPointer function; BongoCatStoreModelsQuery query;} entry;
    entry.function = SDL_LoadFunction(library, "bongo_cat_store_models_query_v1");
    const BongoCatStoreModelsPlugin *api = entry.function ? entry.query(1) : NULL;
    return api && api->abi_version == 1 && api->struct_size == sizeof(*api) &&
        api->installed && api->uninstall && api->sync_names ? api : NULL;
}
bool bongo_cat_store_installed(BongoCatApp *app, const BongoCatStoreEntry *entry) {
    if (!app || !app->store || !entry) return false;
    BongoCatPluginModelsContext context = {app, NULL};
    BongoCatPluginModelsHost host = bongo_cat_plugin_models_host(&context);
    return app->store->models->installed(app->store->cache, entry->sha256, &host) != 0;
}
bool bongo_cat_store_uninstall(BongoCatApp *app, const BongoCatStoreEntry *entry, BongoCatError *error) {
    if (!app || !app->store || !entry || atomic_load(&app->store->receipt_ready)) return false;
    unsigned index; uint64_t received, total;
    unsigned operation = bongo_cat_store_operation(app, &index, &received, &total);
    if (operation == 1 || operation == 2) return false;
    BongoCatPluginModelsContext context = {app, error};
    BongoCatPluginModelsHost host = bongo_cat_plugin_models_host(&context);
    bool ok = app->store->models->uninstall(app->store->cache, entry->sha256, &host) != 0;
    bongo_cat_preferences_models_changed(app->preferences);
    return ok;
}
void bongo_cat_store_apply_receipt(BongoCatApp *app, BongoCatStore *v) {
    if (!atomic_load(&v->receipt_ready) || bongo_cat_plugin_worker_busy(&v->worker)) return;
    if (v->worker.state_mutex) SDL_LockMutex(v->worker.state_mutex);
    BongoCatImportReceipt receipt = v->receipt; char hash[65]; SDL_strlcpy(hash, v->operation_hash, sizeof(hash));
    if (v->worker.state_mutex) SDL_UnlockMutex(v->worker.state_mutex);
    for (size_t i = 0; i < receipt.count; ++i) {
        char base[BONGO_CAT_ID_CAP];
        if (bongo_cat_import_package_base_id(base, sizeof(base), receipt.ids[i]))
            bongo_cat_settings_restore_model_package(&app->settings, base);
        if (receipt.installed[i]) bongo_cat_app_forget_behavior_state(app, receipt.ids[i]);
    }
    bongo_cat_app_refresh_installed_models(app);
    bool saved = bongo_cat_store_save_receipt(v, hash, &receipt);
    if (v->worker.state_mutex) SDL_LockMutex(v->worker.state_mutex);
    v->operation = saved ? 3 : 4;
    atomic_store(&v->receipt_ready, false);
    if (v->worker.state_mutex) SDL_UnlockMutex(v->worker.state_mutex);
    bongo_cat_preferences_models_changed(app->preferences);
    (void)bongo_cat_plugin_event(app, v->events, v->instance, BONGO_PLUGIN_EVENT_COMPLETED, saved);
}

void bongo_cat_store_sync_names(BongoCatApp *app) {
    unsigned count = 0;
    if (bongo_cat_store_busy(app) || bongo_cat_store_status(app, &count) != 2) return;
    BongoCatPluginModelsContext context = {app, NULL};
    BongoCatPluginModelsHost host = bongo_cat_plugin_models_host(&context);
    if (app->store->models->sync_names(app->store->instance, app->store->cache, &host))
        bongo_cat_preferences_models_changed(app->preferences);
}
