#ifndef BONGO_CAT_PLUGIN_TRANSFER_H
#define BONGO_CAT_PLUGIN_TRANSFER_H
#include "bongo_cat/safe_http.h"
#include "bongo_cat/store_plugin.h"
#define BONGO_CAT_PLUGIN_TRANSFER_ABI 1u
typedef struct BongoCatImportReceipt BongoCatImportReceipt;
/* Borrowed worker services. Networking, decoding and model installation remain
   reusable host facilities; repository/cache/receipt policy belongs to plugins. */
typedef struct BongoCatPluginTransferHost {
    uint32_t abi_version, struct_size;
    void *context;
    bool (*cancelled)(void *);
    void (*progress)(void *, uint64_t, uint64_t);
    void (*lock)(void *);
    void (*unlock)(void *);
    int (*get)(const char *, bool, size_t, bongo_safe_cancelled, void *,
        unsigned char **, size_t *, char *, size_t);
    void (*free_response)(unsigned char *, size_t);
    int (*download)(const char *, const char *, const char *, bool,
        bongo_safe_cancelled, bongo_safe_http_progress, void *, char *, size_t);
    uint32_t (*install)(void *, const char *, BongoCatImportReceipt *);
    void (*digest)(const void *, size_t, char *);
    uint32_t (*replace)(const char *, const char *);
    uint32_t (*validate_image)(const char *);
} BongoCatPluginTransferHost;
typedef struct BongoCatStoreTransferPlugin {
    uint32_t abi_version, struct_size;
    uint32_t (*catalog)(void *, const char *, bool, const BongoCatPluginTransferHost *);
    uint32_t (*entry)(void *, uint32_t, const char *, BongoCatStoreEntry *);
    uint32_t (*asset)(void *, uint32_t, const char *, const char *, bool, bool,
        const BongoCatPluginTransferHost *, BongoCatImportReceipt *);
    uint32_t (*cache_path)(const char *, const char *, const char *, char *, size_t);
    uint32_t (*receipt)(const char *, const char *, BongoCatImportReceipt *);
    uint32_t (*save_receipt)(const char *, const char *, const BongoCatImportReceipt *,
        const BongoCatPluginTransferHost *);
} BongoCatStoreTransferPlugin;
typedef const BongoCatStoreTransferPlugin *(*BongoCatStoreTransferQuery)(uint32_t);
#endif
