#ifndef BONGO_CAT_PLUGIN_LOCALE_H
#define BONGO_CAT_PLUGIN_LOCALE_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define BONGO_CAT_PLUGIN_LOCALE_ABI 1u
typedef void (*BongoCatPluginLocaleVisitor)(void *context, const char *text);
/* Optional bongo_cat_<id>_locale_query_v1. The plugin embeds its locale files
   and owns every translated string. Main-thread-only: text pointers remain
   borrowed until the next load/release. Host caches must never retain them.
   load is transactional, uses en-US for an unknown language, and is a no-op
   for the already loaded language. visit lends active strings for font baking.
   release drops translation allocations before the library is unloaded. */
typedef struct BongoCatPluginLocale {
    uint32_t abi_version, struct_size;
    const char *prefix, *extra_prefix;
    uint32_t (*load)(const char *language);
    const char *(*text)(const char *key, const char *fallback);
    void (*visit)(void *context, BongoCatPluginLocaleVisitor visitor);
    void (*release)(void);
} BongoCatPluginLocale;
typedef const BongoCatPluginLocale *(*BongoCatPluginLocaleQuery)(uint32_t abi);
#ifdef __cplusplus
}
#endif
#endif
