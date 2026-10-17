#ifndef BONGO_CAT_PLUGIN_MODELS_H
#define BONGO_CAT_PLUGIN_MODELS_H
#include <stdint.h>
/* Main-thread model catalog services, shared by add-on plugins. query returns
   0=unavailable/protected, 1=installed, 2=removed. Strings are borrowed per call. */
typedef struct BongoCatPluginModelsHost {
    uint32_t abi_version, struct_size;
    void *context;
    uint32_t (*query)(void *, const char *);
    uint32_t (*remove)(void *, const char *);
    uint32_t (*rename)(void *, const char *, const char *);
} BongoCatPluginModelsHost;
typedef struct BongoCatStoreModelsPlugin {
    uint32_t abi_version, struct_size;
    uint32_t (*installed)(const char *, const char *, const BongoCatPluginModelsHost *);
    uint32_t (*uninstall)(const char *, const char *, const BongoCatPluginModelsHost *);
    uint32_t (*sync_names)(void *, const char *, const BongoCatPluginModelsHost *);
} BongoCatStoreModelsPlugin;
typedef const BongoCatStoreModelsPlugin *(*BongoCatStoreModelsQuery)(uint32_t);
#endif
