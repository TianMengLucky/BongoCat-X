#include "model_import.h"
#include "model_import_lock.h"
#include "model_storage.h"
#include "runtime.h"
#include "bongo_cat/path.h"

#include <stdio.h>

#ifdef BONGO_CAT_HAS_MODEL_PLUGINS
static void remove_receipt(const char *models_root,
    const BongoCatImportReceipt *receipt) {
    char path[BONGO_CAT_PATH_CAP];
    if (!receipt || !models_root) return;
    bongo_cat_import_storage_lock();
    for (size_t i = 0; i < receipt->count; ++i)
        if (receipt->installed[i] &&
            bongo_cat_path_join(path, sizeof(path), models_root, receipt->ids[i]))
            bongo_cat_model_remove_tree(path, NULL);
    bongo_cat_import_storage_unlock();
}
#endif

BongoCatResult bongo_cat_app_import_model(BongoCatApp *app, const char *source,
    BongoCatError *error) {
    if (!app || !source) return BONGO_CAT_ERROR_ARGUMENT;
    BongoCatImportReceipt receipt;
    BongoCatResult result = bongo_cat_import_install(source, app->models_root,
        &receipt, error);
    if (result != BONGO_CAT_OK) {
        if (error && !error->message[0]) bongo_cat_error_set(error, result,
            "Model import failed while installing: %s", source);
        return result;
    }
    char previous[BONGO_CAT_PATH_CAP];
    snprintf(previous, sizeof(previous), "%s", app->session.active_model_id);
    size_t preferred = 0;
    for (size_t i = 0; i < receipt.count; ++i)
        if (receipt.installed[i]) { preferred = i; break; }
    const char *imported_id = receipt.count ? receipt.ids[preferred] : NULL;
    if (receipt.count)
        bongo_cat_settings_restore_model_package(&app->settings,
            receipt.ids[0]);
    bongo_cat_app_refresh_installed_models(app);
    for (size_t i = 0; i < receipt.count; ++i)
        if (receipt.installed[i])
            bongo_cat_app_forget_behavior_state(app, receipt.ids[i]);
    if (imported_id && bongo_cat_app_select_model(app, imported_id))
        return BONGO_CAT_OK;
#ifndef BONGO_CAT_HAS_MODEL_PLUGINS
    const BongoCatModelEntry *entry = imported_id ?
        bongo_cat_models_find(&app->models, imported_id) : NULL;
    if (entry) {
        snprintf(app->session.active_model_id, sizeof(app->session.active_model_id),
            "%s", imported_id);
        app->loaded_mode = entry->mode;
    }
    return BONGO_CAT_OK;
#else
    remove_receipt(app->models_root, &receipt);
    bongo_cat_app_refresh_installed_models(app);
    if (previous[0]) bongo_cat_app_select_model(app, previous);
    bongo_cat_error_set(error, BONGO_CAT_ERROR_CUBISM,
        "Model import was rolled back because the model renderer could not load it");
    return BONGO_CAT_ERROR_CUBISM;
#endif
}
