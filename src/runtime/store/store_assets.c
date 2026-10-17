#include "store_internal.h"
#include "bongo_cat/path.h"
#include <string.h>
bool bongo_cat_store_cache_path(BongoCatStore *v, const char *key, const char *suffix, char *out) {
    return v && v->transfer->cache_path(v->cache, key, suffix, out, BONGO_CAT_PATH_CAP) != 0;
}
bool bongo_cat_store_asset_work(BongoCatStore *v, const BongoCatPluginJob *job, BongoCatError *error) {
    (void)error;
    if (job->kind != 2 && job->kind != 3) return true;
    BongoCatImportReceipt receipt = {0};
    BongoCatPluginTransferHost host = bongo_cat_store_transfer_host(v);
    unsigned result = v->transfer->asset(v->instance, job->index, v->repository,
        v->cache, v->accelerate, job->kind == 2, &host, &receipt);
    SDL_LockMutex(v->worker.state_mutex);
    if (job->kind == 2) {
        if (job->index < BONGO_CAT_STORE_CAP) v->preview_state[job->index] = result == 2 ? 2 : 3;
    } else if (result == 3) {
        BongoCatStoreEntry entry;
        if (v->api->entry(v->instance, job->index, &entry)) {
            v->receipt = receipt;
            SDL_strlcpy(v->operation_hash, entry.sha256, sizeof(v->operation_hash));
            atomic_store(&v->receipt_ready, true);
        } else v->operation = 4;
    } else v->operation = result;
    SDL_UnlockMutex(v->worker.state_mutex);
    atomic_store(&v->worker.changed, true);
    return true;
}
bool bongo_cat_store_preview(BongoCatApp *app, unsigned index, char *path) {
    if (!app || !app->store || index >= BONGO_CAT_STORE_CAP) return false;
    BongoCatStoreEntry entry;
    if (!bongo_cat_store_entry(app, index, &entry)) return false;
    BongoCatStore *v = app->store; char key[65];
    bongo_safe_sha256_bytes(entry.preview_url, strlen(entry.preview_url), key);
    if (!bongo_cat_store_cache_path(v, key, ".preview", path)) return false;
    SDL_LockMutex(v->worker.state_mutex); unsigned state = v->preview_state[index];
    bool idle = v->status == 2 && (v->operation == 0 || v->operation >= 3);
    if (!state && idle) v->preview_state[index] = 1;
    SDL_UnlockMutex(v->worker.state_mutex);
    if (!state && idle) {
        BongoCatPluginJob job = {0}; job.kind = 2; job.index = index;
        if (!bongo_cat_plugin_worker_submit(&v->worker, &job)) {
            SDL_LockMutex(v->worker.state_mutex); v->preview_state[index] = 0; SDL_UnlockMutex(v->worker.state_mutex);
        }
    }
    return state == 2;
}
bool bongo_cat_store_download(BongoCatApp *app, unsigned index) {
    if (!app || !app->store || atomic_load(&app->store->receipt_ready)) return false;
    BongoCatStore *v = app->store; unsigned current; uint64_t received, total;
    unsigned operation = bongo_cat_store_operation(app, &current, &received, &total);
    if (operation == 1 || operation == 2) return false;
    BongoCatStoreEntry entry;
    if (!bongo_cat_store_entry(app, index, &entry)) return false;
    SDL_LockMutex(v->worker.state_mutex);
    if (v->status != 2) { SDL_UnlockMutex(v->worker.state_mutex); return false; }
    v->operation = 1; v->operation_index = index; v->received = v->total = 0;
    SDL_UnlockMutex(v->worker.state_mutex);
    BongoCatPluginJob job = {0}; job.kind = 3; job.index = index;
    if (bongo_cat_plugin_worker_submit(&v->worker, &job)) return true;
    SDL_LockMutex(v->worker.state_mutex); v->operation = 4; SDL_UnlockMutex(v->worker.state_mutex);
    return false;
}
unsigned bongo_cat_store_operation(BongoCatApp *app, unsigned *index, uint64_t *received, uint64_t *total) {
    *index = 0; *received = *total = 0;
    if (!app || !app->store) return 0;
    BongoCatStore *v = app->store; SDL_LockMutex(v->worker.state_mutex);
    unsigned operation = v->operation; *index = v->operation_index; *received = v->received; *total = v->total;
    SDL_UnlockMutex(v->worker.state_mutex); return operation;
}
