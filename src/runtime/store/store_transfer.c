#include "store_internal.h"
#include "bongo_cat/file.h"
#include "bongo_cat/image.h"

static bool cancelled(void *context) {
    return SDL_GetAtomicInt(&((BongoCatStore *)context)->cancel) != 0;
}
static void progress(void *context, uint64_t received, uint64_t total) {
    BongoCatStore *v = context;
    SDL_LockMutex(v->worker.state_mutex);
    v->received = received; v->total = total;
    SDL_UnlockMutex(v->worker.state_mutex);
    atomic_store(&v->worker.changed, true);
}
static void lock(void *context) {
    SDL_Mutex *mutex = ((BongoCatStore *)context)->worker.state_mutex;
    if (mutex) SDL_LockMutex(mutex);
}
static void unlock(void *context) {
    SDL_Mutex *mutex = ((BongoCatStore *)context)->worker.state_mutex;
    if (mutex) SDL_UnlockMutex(mutex);
}
static int get(const char *url, bool accelerate, size_t limit,
    bongo_safe_cancelled cancel, void *context, unsigned char **bytes, size_t *length,
    char *message, size_t capacity) {
    bongo_safe_http_client *client = bongo_safe_http_new();
    int result = client ? bongo_safe_http_get(client, url, accelerate, limit, cancel,
        context, bytes, length, message, capacity) : BONGO_SAFE_HTTP_MEMORY;
    bongo_safe_http_free(client);
    return result;
}
static uint32_t install(void *context, const char *path, BongoCatImportReceipt *receipt) {
    BongoCatStore *v = context;
    SDL_LockMutex(v->worker.state_mutex); v->operation = 2;
    SDL_UnlockMutex(v->worker.state_mutex);
    atomic_store(&v->worker.changed, true);
    BongoCatError error = {0};
    return bongo_cat_import_install(path, v->models_root, receipt, &error) == BONGO_CAT_OK;
}
static uint32_t replace(const char *source, const char *target) {
    return bongo_cat_file_replace(source, target, true);
}
static uint32_t validate_image(const char *path) {
    int width = 0, height = 0;
    if (!bongo_cat_image_info(path, &width, &height) || width <= 0 || height <= 0 ||
        width > 8192 || height > 8192 ||
        (uint64_t)width * (uint64_t)height > 16u * 1024u * 1024u) return 0;
    BongoCatImage image = {0}; BongoCatError error = {0};
    bool ok = bongo_cat_image_load(path, &image, &error) == BONGO_CAT_OK;
    bongo_cat_image_free(&image);
    return ok;
}
BongoCatPluginTransferHost bongo_cat_store_transfer_host(BongoCatStore *v) {
    return (BongoCatPluginTransferHost){BONGO_CAT_PLUGIN_TRANSFER_ABI,
        sizeof(BongoCatPluginTransferHost), v, cancelled, progress, lock, unlock,
        get, bongo_safe_free_http_response, bongo_safe_store_download, install,
        bongo_safe_sha256_bytes, replace, validate_image};
}
const BongoCatStoreTransferPlugin *bongo_cat_store_transfer_query(SDL_SharedObject *library) {
    union { SDL_FunctionPointer function; BongoCatStoreTransferQuery query; } entry;
    entry.function = SDL_LoadFunction(library, "bongo_cat_store_transfer_query_v1");
    const BongoCatStoreTransferPlugin *api = entry.function ? entry.query(BONGO_CAT_PLUGIN_TRANSFER_ABI) : NULL;
    return api && api->abi_version == BONGO_CAT_PLUGIN_TRANSFER_ABI &&
        api->struct_size == sizeof(*api) && api->catalog && api->entry && api->asset &&
        api->cache_path && api->receipt && api->save_receipt ? api : NULL;
}
