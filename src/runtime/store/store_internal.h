#ifndef BONGO_CAT_STORE_INTERNAL_H
#define BONGO_CAT_STORE_INTERNAL_H
#include "plugin_page.h"
#include "bongo_cat/store.h"
#include "plugin_worker.h"
#include "model_import.h"
typedef struct BongoCatStore {
    SDL_SharedObject *library;
    BongoCatPluginPageSession page_ui;
    const BongoCatStorePlugin *api;
    const BongoCatPluginUiContribution *ui;
    void *instance;
    BongoCatPluginWorker worker;
    SDL_AtomicInt cancel;
    unsigned status;
    bool accelerate;
    bool names_pending;
    char repository[512];
    char cache[BONGO_CAT_PATH_CAP], models_root[BONGO_CAT_PATH_CAP];
    unsigned preview_state[BONGO_CAT_STORE_CAP];
    unsigned operation, operation_index;
    uint64_t received, total;
    BongoCatImportReceipt receipt;
    atomic_bool receipt_ready;
    char operation_hash[65];
} BongoCatStore;
bool bongo_cat_store_rebase(BongoCatStore *v, BongoCatStoreEntry *entry);
bool bongo_cat_store_save_receipt(BongoCatStore *v, const char *hash, const BongoCatImportReceipt *receipt);
bool bongo_cat_store_asset_work(BongoCatStore *v, const BongoCatPluginJob *job, BongoCatError *error);
bool bongo_cat_store_cache_path(BongoCatStore *v, const char *key, const char *suffix, char *out);
void bongo_cat_store_sync_names(BongoCatApp *app);
void bongo_cat_store_apply_receipt(BongoCatApp *app, BongoCatStore *value);
bool bongo_cat_store_path(BongoCatApp *app, const char *suffix, char *out);
const BongoCatStorePlugin *bongo_cat_store_query(SDL_SharedObject *library);
#endif
