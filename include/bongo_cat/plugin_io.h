#ifndef BONGO_CAT_PLUGIN_IO_H
#define BONGO_CAT_PLUGIN_IO_H

#include <stdint.h>

/* Optional companion to the stable scene ABIs. The plugin owns file formats,
   imports and playback policy. Host callbacks provide only platform services.
   Calls are serialized on the plugin worker; lock protects short scene copies.
   All pointers are borrowed until execute returns. No callback retains them. */
#define BONGO_CAT_PLUGIN_IO_ABI 1u
enum BongoCatPluginIoOperation {
    BONGO_PLUGIN_IO_LOAD, BONGO_PLUGIN_IO_SAVE, BONGO_PLUGIN_IO_IMPORT,
    BONGO_PLUGIN_IO_CLEAR, BONGO_PLUGIN_IO_PREVIEW, BONGO_PLUGIN_IO_INPUT,
    BONGO_PLUGIN_IO_RELEASE
};
enum BongoCatPluginIoMedia { BONGO_PLUGIN_IO_IMAGE, BONGO_PLUGIN_IO_AUDIO };
typedef struct BongoCatPluginIoHost {
    uint32_t abi_version, struct_size;
    void *context;
    void (*lock)(void *);
    void (*unlock)(void *);
    uint32_t (*sha256)(const char *, char *);
    uint32_t (*replace)(const char *, const char *);
    uint32_t (*validate)(const char *, uint32_t);
    uint32_t (*play)(void *, const char *);
    void (*stop)(void *, const char *);
} BongoCatPluginIoHost;
typedef struct BongoCatPluginIo {
    uint32_t abi_version, struct_size;
    uint32_t (*execute)(void *instance, const char *directory,
        const BongoCatPluginIoHost *host, uint32_t operation, uint32_t index,
        const char *key, const char *path, uint32_t flags);
} BongoCatPluginIo;
typedef const BongoCatPluginIo *(*BongoCatPluginIoQuery)(uint32_t);

#endif
