#include "store_internal.h"
#include "bongo_cat/path.h"
#include "bongo_cat/file.h"
#include "bongo_cat/i18n.h"
#include "bongo_cat/preferences.h"
#include "preferences_notice.h"
#include "runtime.h"
#include <stdio.h>
#include <string.h>
bool bongo_cat_store_save_receipt(BongoCatStore *v, const char *hash, const BongoCatImportReceipt *receipt) {
    char path[BONGO_CAT_PATH_CAP], temporary[BONGO_CAT_PATH_CAP] = {0};
    if (!bongo_cat_store_cache_path(v, hash, ".installed", path) ||
        !bongo_cat_store_cache_path(v, hash, ".installed.new", temporary)) return false;
    FILE *file = bongo_cat_file_open(temporary, "wb"); bool saved = file != NULL;
    for (size_t i = 0; saved && i < receipt->count; ++i)
        if (fprintf(file, "%s\n", receipt->ids[i]) < 0) saved = false;
    if (file && fclose(file) != 0) saved = false;
    if (saved) saved = bongo_cat_file_replace(temporary, path, true);
    (void)bongo_cat_file_remove(temporary); return saved;
}
static bool read_receipt(BongoCatStore *v, const char *hash, BongoCatImportReceipt *receipt) {
    char path[BONGO_CAT_PATH_CAP]; *receipt = (BongoCatImportReceipt){0};
    if (!bongo_cat_store_cache_path(v, hash, ".installed", path)) return false;
    FILE *file = bongo_cat_file_open(path, "rb"); if (!file) return false;
    char line[BONGO_CAT_ID_CAP + 4]; bool ok = true;
    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\r\n")] = 0;
        if (receipt->count >= BONGO_CAT_IMPORT_CANDIDATE_CAP || !line[0] ||
            strlen(line) >= BONGO_CAT_ID_CAP || strpbrk(line, "/\\") || !strcmp(line, "..")) { ok = false; break; }
        SDL_strlcpy(receipt->ids[receipt->count++], line, BONGO_CAT_ID_CAP);
    }
    if (ferror(file)) ok = false;
    fclose(file); return ok && receipt->count;
}
static bool matches(const BongoCatModelEntry *model, const BongoCatImportReceipt *receipt) {
    for (size_t i = 0; i < receipt->count; ++i)
        if (!strcmp(model->id, receipt->ids[i])) return !model->managed && model->storage_directory[0];
    return false;
}
bool bongo_cat_store_installed(BongoCatApp *app, const BongoCatStoreEntry *entry) {
    if (!app || !app->store) return false;
    BongoCatImportReceipt receipt;
    if (!read_receipt(app->store, entry->sha256, &receipt)) return false;
    for (size_t i = 0; i < app->models.count; ++i)
        if (matches(&app->models.entries[i], &receipt) &&
            !bongo_cat_settings_model_removed(&app->settings, app->models.entries[i].id)) return true;
    return false;
}
bool bongo_cat_store_uninstall(BongoCatApp *app, const BongoCatStoreEntry *entry, BongoCatError *error) {
    if (!app || !app->store || atomic_load(&app->store->receipt_ready)) return false;
    unsigned index; uint64_t received, total;
    unsigned operation = bongo_cat_store_operation(app, &index, &received, &total);
    if (operation == 1 || operation == 2) return false;
    BongoCatImportReceipt receipt;
    if (!read_receipt(app->store, entry->sha256, &receipt)) return false;
    for (;;) {
        char id[BONGO_CAT_ID_CAP] = {0};
        for (size_t i = 0; i < app->models.count; ++i) {
            if (matches(&app->models.entries[i], &receipt) &&
                !bongo_cat_settings_model_removed(&app->settings, app->models.entries[i].id)) {
                SDL_strlcpy(id, app->models.entries[i].id, sizeof(id)); break;
            }
        }
        if (!id[0]) break;
        if (bongo_cat_app_remove_model(app, id, error) != BONGO_CAT_OK) return false;
    }
    char path[BONGO_CAT_PATH_CAP];
    if (bongo_cat_store_cache_path(app->store, entry->sha256, ".installed", path)) (void)bongo_cat_file_remove(path);
    bongo_cat_preferences_models_changed(app->preferences); return true;
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
    bongo_cat_preferences_notice_show(app, bongo_cat_i18n_get(app->i18n,
        saved ? "pages.preference.store.installed" : "pages.preference.store.failed",
        saved ? "Model installed and SHA-256 verified" : "Unable to update the store"), !saved);
}

/* Receipts also let an existing installation recover its title after the
   store catalog loads. Save only changed titles, then update the local cards. */
void bongo_cat_store_sync_names(BongoCatApp *app) {
    unsigned count = 0;
    if (bongo_cat_store_busy(app) || bongo_cat_store_status(app, &count) != 2) return;
    bool changed = false;
    for (unsigned index = 0; index < count; ++index) {
        BongoCatStoreEntry item;
        BongoCatImportReceipt receipt;
        if (!bongo_cat_store_entry(app, index, &item) || !item.name[0] ||
            strlen(item.name) >= BONGO_CAT_ID_CAP ||
            !read_receipt(app->store, item.sha256, &receipt)) continue;
        for (size_t i = 0; i < app->models.count; ++i) {
            BongoCatModelEntry *model = &app->models.entries[i];
            if (!matches(model, &receipt)) continue;
            char saved[BONGO_CAT_ID_CAP];
            if (!bongo_cat_import_store_name_read(model->storage_directory,
                    saved, sizeof(saved)) || strcmp(saved, item.name)) {
                if (!bongo_cat_import_store_name_write(model->storage_directory,
                        item.name)) continue;
            }
            if (strcmp(model->display_name, item.name)) {
                SDL_strlcpy(model->display_name, item.name, sizeof(model->display_name));
                changed = true;
            }
        }
    }
    if (changed) bongo_cat_preferences_models_changed(app->preferences);
}
