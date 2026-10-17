#ifndef BONGO_CAT_PLUGIN_EVENTS_H
#define BONGO_CAT_PLUGIN_EVENTS_H
#include <stdint.h>
#define BONGO_CAT_PLUGIN_EVENTS_ABI 1u
enum BongoCatPluginEvent {
    BONGO_PLUGIN_EVENT_TICK, BONGO_PLUGIN_EVENT_COMPLETED,
    BONGO_PLUGIN_EVENT_EXPORT, BONGO_PLUGIN_EVENT_REQUEST_RESET,
    BONGO_PLUGIN_EVENT_COLLECTION
};
enum BongoCatPluginEventFlags {
    BONGO_PLUGIN_EVENT_INVALIDATE = 1u, BONGO_PLUGIN_EVENT_RESET = 2u
};
/* Strings belong to the plugin and are consumed before the call site returns.
   Host notifications never interpret plugin-specific completion/status codes. */
typedef struct BongoCatPluginNotification {
    uint32_t flags, error;
    const char *key, *fallback;
} BongoCatPluginNotification;
typedef struct BongoCatPluginEvents {
    uint32_t abi_version, struct_size;
    uint32_t (*event)(void *, uint32_t event, uint32_t result,
        BongoCatPluginNotification *);
} BongoCatPluginEvents;
typedef const BongoCatPluginEvents *(*BongoCatPluginEventsQuery)(uint32_t);
#endif
