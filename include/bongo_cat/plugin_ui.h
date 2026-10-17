#ifndef BONGO_CAT_PLUGIN_UI_H
#define BONGO_CAT_PLUGIN_UI_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Generic plugin UI ABI.
   A plugin builds its own page from these primitives instead of the host
   hard-coding one card per plugin. No Nuklear, SDL or graphics handle crosses
   this boundary: the host lends an opaque context (BongoCatPluginUi *) plus a
   versioned service table. All strings are NUL-terminated UTF-8. text()
   resolves an i18n key through the host so every plugin stays localized.
   Widget return values report interaction only (1 = clicked/changed). */

#define BONGO_CAT_PLUGIN_UI_ABI 1u

/* Opaque host drawing context; only the host dereferences it. */
typedef struct BongoCatPluginUi BongoCatPluginUi;

typedef struct BongoCatPluginUiHost {
    uint32_t abi_version, struct_size;
    const char *(*text)(BongoCatPluginUi *ui, const char *key, const char *fallback);
    void (*intro)(BongoCatPluginUi *ui, const char *text);
    /* Opens a card; returns 0 when the host could not place it, in which case
       the plugin must not draw content and must not call card_end. */
    uint32_t (*card_begin)(BongoCatPluginUi *ui, const char *id);
    void (*card_end)(BongoCatPluginUi *ui);
    void (*label)(BongoCatPluginUi *ui, const char *text);
    void (*label_wrapped)(BongoCatPluginUi *ui, const char *text, float height);
    void (*spacer)(BongoCatPluginUi *ui, float height);
    void (*row)(BongoCatPluginUi *ui, uint32_t columns, float height);
    uint32_t (*button)(BongoCatPluginUi *ui, const char *id, const char *text);
    uint32_t (*toggle)(BongoCatPluginUi *ui, const char *id, const char *text, uint32_t *value);
    uint32_t (*toggle_available)(BongoCatPluginUi *ui, const char *id, const char *text,
        uint32_t *value, uint32_t available);
    uint32_t (*slider)(BongoCatPluginUi *ui, const char *id, const char *title,
        const char *description, float *value, float minimum, float maximum,
        float step, float default_value);
} BongoCatPluginUiHost;

/* What the host knows about one plugin, so the plugin can render its own
   management card without reaching into host configuration itself. */
typedef struct BongoCatPluginCardState {
    uint32_t struct_size;
    uint32_t installed, enabled, active, managed, busy;
} BongoCatPluginCardState;

/* Host operations bound to a single plugin. #context is a host-owned opaque
   handle the plugin only forwards back to these callbacks. Built per draw, so
   the plugin must not retain it. */
typedef struct BongoCatPluginUiServices {
    uint32_t abi_version, struct_size;
    void *context;
    void (*card_state)(void *context, BongoCatPluginCardState *out);
    /* Both only record the request; the host applies it once the plugin has
       stopped drawing. Disabling or removing a plugin unloads its library, so
       running that inside the library's own draw would free live code. */
    void (*set_enabled)(void *context, uint32_t enabled);
    void (*remove)(void *context);
} BongoCatPluginUiServices;

/* Input event routed to the plugin while its page is displayed; the plugin
   returns 1 to consume it before the host handles it. */
typedef struct BongoCatPluginUiEvent {
    uint32_t struct_size, kind;
    float x, y;
    uint32_t button;
} BongoCatPluginUiEvent;

/* Optional UI contribution a plugin exports under its own symbol
   (for example "bongo_cat_sounds_ui_query_v1"). draw runs every frame while
   the plugin's page or card is shown; event is optional. Additive extension:
   hosts that do not know the symbol fall back to their built-in UI. */
typedef struct BongoCatPluginUiContribution {
    uint32_t abi_version, struct_size;
    uint32_t (*draw)(void *instance, BongoCatPluginUi *ui,
        const BongoCatPluginUiHost *host, const BongoCatPluginUiServices *services);
    uint32_t (*event)(void *instance, const BongoCatPluginUiEvent *event);
} BongoCatPluginUiContribution;

typedef const BongoCatPluginUiContribution *(*BongoCatPluginUiQuery)(uint32_t abi);

#ifdef __cplusplus
}
#endif

#endif
