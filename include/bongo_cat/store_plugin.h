#ifndef BONGO_CAT_STORE_PLUGIN_H
#define BONGO_CAT_STORE_PLUGIN_H
#include <stddef.h>
#include <stdint.h>
#define BONGO_CAT_STORE_ABI 2u
#define BONGO_CAT_STORE_SYMBOL "bongo_cat_store_query"
#define BONGO_CAT_STORE_CAP 512u
/* Fixed UTF-8 buffers, NUL terminated; URLs are validated HTTPS URLs.
   Calls are serialized by the host. parse replaces the catalog only on success.
   paid is 0 (free) or 1 (paid); purchase_url holds the HTTPS purchase URL of a
   paid model and is empty for free models. */
typedef struct BongoCatStoreEntry {
    char name[256], author[256], author_url[1024];
    char repository_path[1024], folder_path[1024];
    char file_url[2048], folder_url[2048];
    char preview_url[2048], download_url[2048], sha256[65];
    uint32_t paid;
    char purchase_url[1024];
} BongoCatStoreEntry;
typedef struct BongoCatStorePlugin {
    uint32_t abi_version, struct_size;
    void *(*create)(void);
    void (*destroy)(void *instance);
    uint32_t (*parse)(void *instance, const unsigned char *bytes, size_t length);
    uint32_t (*count)(void *instance);
    uint32_t (*entry)(void *instance, uint32_t index, BongoCatStoreEntry *out);
} BongoCatStorePlugin;
typedef const BongoCatStorePlugin *(*BongoCatStoreQuery)(uint32_t abi);
#endif
