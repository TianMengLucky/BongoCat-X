#include "store_internal.h"
#include "bongo_cat/safe_http.h"
#include "bongo_cat/file.h"
#include "bongo_cat/path.h"
#include <stdio.h>
#include <string.h>
static bool cancelled(void *userdata) {
    return SDL_GetAtomicInt(&((BongoCatStore *)userdata)->cancel) != 0;
}
bool bongo_cat_store_cache_path(BongoCatStore *v, const char *key, const char *suffix, char *out) {
    if (!key || strlen(key) != 64 || strspn(key, "0123456789abcdef") != 64) return false;
    char name[96]; snprintf(name, sizeof(name), "%s%s", key, suffix);
    return bongo_cat_path_join(out, BONGO_CAT_PATH_CAP, v->cache, name);
}
static void progress(void *userdata, uint64_t received, uint64_t total) {
    BongoCatStore *v = userdata;
    SDL_LockMutex(v->worker.state_mutex); v->received = received; v->total = total;
    SDL_UnlockMutex(v->worker.state_mutex); atomic_store(&v->worker.changed, true);
}
static bool preview(BongoCatStore *v, unsigned index, BongoCatStoreEntry *entry) {
    char key[65], path[BONGO_CAT_PATH_CAP];
    bongo_safe_sha256_bytes(entry->preview_url, strlen(entry->preview_url), key);
    bool ok = bongo_cat_store_cache_path(v, key, ".preview", path);
    if (ok && !bongo_cat_path_is_file(path)) {
        bongo_safe_http_client *client = bongo_safe_http_new();
        unsigned char *bytes = NULL; size_t length = 0; char message[256];
        ok = client && bongo_safe_http_get(client, entry->preview_url, v->accelerate,
            4u * 1024u * 1024u, cancelled, v, &bytes, &length, message, sizeof(message)) == BONGO_SAFE_HTTP_OK;
        bongo_safe_http_free(client);
        int width = 0, height = 0;
        if (ok) ok = bongo_safe_image_info(bytes, length, &width, &height) &&
            width > 0 && height > 0 && width <= 8192 && height <= 8192 &&
            (uint64_t)width * (uint64_t)height <= 16u * 1024u * 1024u;
        if (ok && !cancelled(v)) {
            unsigned char *pixels = bongo_safe_image_decode(bytes, length, &width, &height);
            ok = pixels != NULL;
            if (pixels) bongo_safe_free_pixels(pixels, (size_t)width * (size_t)height * 4);
        }
        if (ok && !cancelled(v)) {
            char temporary[BONGO_CAT_PATH_CAP] = {0};
            ok = bongo_cat_store_cache_path(v, key, ".preview.new", temporary);
            FILE *file = ok ? bongo_cat_file_open(temporary, "wb") : NULL;
            ok = file && fwrite(bytes, 1, length, file) == length;
            if (file && fclose(file) != 0) ok = false;
            if (ok) ok = bongo_cat_file_replace(temporary, path, true);
            if (temporary[0]) (void)bongo_cat_file_remove(temporary);
        } else ok = false;
        bongo_safe_free_http_response(bytes, length);
    }
    SDL_LockMutex(v->worker.state_mutex); v->preview_state[index] = ok ? 2 : 3;
    SDL_UnlockMutex(v->worker.state_mutex); return ok;
}
bool bongo_cat_store_asset_work(BongoCatStore *v, const BongoCatPluginJob *job, BongoCatError *error) {
    if (job->kind != 2 && job->kind != 3) return true;
    if (cancelled(v)) return true;
    BongoCatStoreEntry entry;
    SDL_LockMutex(v->worker.state_mutex);
    bool ok = v->api->entry(v->instance, job->index, &entry) != 0;
    SDL_UnlockMutex(v->worker.state_mutex);
    if (!ok || job->index >= BONGO_CAT_STORE_CAP ||
        !memchr(entry.preview_url, 0, sizeof(entry.preview_url)) ||
        !memchr(entry.download_url, 0, sizeof(entry.download_url)) ||
        !memchr(entry.sha256, 0, sizeof(entry.sha256)) ||
        !memchr(entry.repository_path, 0, sizeof(entry.repository_path))) return false;
    if (job->kind == 2) { (void)preview(v, job->index, &entry); atomic_store(&v->worker.changed, true); return true; }
    char path[BONGO_CAT_PATH_CAP] = {0}, directory[BONGO_CAT_PATH_CAP] = {0};
    ok = bongo_cat_store_cache_path(v, entry.sha256, ".download", directory) &&
        bongo_cat_path_create_directory(directory) &&
        bongo_cat_path_join(path, sizeof(path), directory, bongo_cat_path_name(entry.repository_path));
    char message[512] = {0};
    if (ok) ok = bongo_safe_store_download(entry.download_url, entry.sha256, path, v->accelerate,
        cancelled, progress, v, message, sizeof(message)) == BONGO_SAFE_HTTP_OK;
    BongoCatImportReceipt receipt = {0};
    if (ok && !cancelled(v)) {
        SDL_LockMutex(v->worker.state_mutex); v->operation = 2; SDL_UnlockMutex(v->worker.state_mutex);
        atomic_store(&v->worker.changed, true);
        ok = bongo_cat_import_install(path, v->models_root, &receipt, error) == BONGO_CAT_OK && receipt.count;
    } else ok = false;
    if (path[0]) (void)bongo_cat_file_remove(path);
    if (directory[0]) (void)SDL_RemovePath(directory);
    bool imported = ok;
    if (ok) (void)bongo_cat_store_save_receipt(v, entry.sha256, &receipt);
    SDL_LockMutex(v->worker.state_mutex);
    if (imported) {
        v->receipt = receipt; SDL_strlcpy(v->operation_hash, entry.sha256, sizeof(v->operation_hash));
        atomic_store(&v->receipt_ready, true);
    } else { v->operation = strstr(message, "SHA-256") ? 5 : 4; }
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
