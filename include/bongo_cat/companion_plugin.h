#ifndef BONGO_CAT_COMPANION_PLUGIN_H
#define BONGO_CAT_COMPANION_PLUGIN_H
#include "bongo_cat/plugin_ui_page.h"
#include <stdbool.h>
#include <stddef.h>
#define BONGO_CAT_COMPANION_ABI 1u
#define BONGO_CAT_COMPANION_SYMBOL "bongo_cat_companion_query_v1"
/* No key names, typed text, application titles or paths cross this interface. */
enum BongoCatCompanionEvent {
    BONGO_COMPANION_TICK, BONGO_COMPANION_STARTUP, BONGO_COMPANION_KEYBOARD,
    BONGO_COMPANION_MOUSE, BONGO_COMPANION_BEHAVIOR
};
typedef bool (*BongoCatCompanionCancelled)(void *);
typedef struct BongoCatCompanionHost {
    uint32_t abi_version, struct_size;
    int (*post)(const char *, const char *, const char *, BongoCatCompanionCancelled,
        void *, unsigned char **, size_t *);
    void (*free_response)(unsigned char *, size_t);
    uint32_t (*replace)(const char *, const char *);
} BongoCatCompanionHost;
typedef struct BongoCatCompanionSnapshot {
    uint32_t struct_size, visible, busy, sequence, status;
    char text[4096];
} BongoCatCompanionSnapshot;
/* Calls except network callbacks run on the main thread. destroy cancels and
   joins the worker before returning. Host callbacks live until destroy returns. */
typedef struct BongoCatCompanionPlugin {
    uint32_t abi_version, struct_size;
    void *(*create)(const char *, const BongoCatCompanionHost *);
    void (*destroy)(void *);
    void (*event)(void *, uint32_t, uint64_t);
    uint32_t (*snapshot)(void *, BongoCatCompanionSnapshot *);
    uint32_t (*settings)(void *, BongoCatPluginUi *, const BongoCatPluginPageUiHost *,
        const BongoCatPluginPageUiServices *);
} BongoCatCompanionPlugin;
typedef const BongoCatCompanionPlugin *(*BongoCatCompanionQuery)(uint32_t);
#endif
