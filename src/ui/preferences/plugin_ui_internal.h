#ifndef BONGO_CAT_PREFERENCES_PLUGIN_UI_INTERNAL_H
#define BONGO_CAT_PREFERENCES_PLUGIN_UI_INTERNAL_H

#include "bongo_cat/plugin_ui.h"
#include "bongo_cat/plugin_ui_page.h"
#include "bongo_cat/i18n.h"
#include "preferences_state.h"
#include "nuklear_config.h"
#include <stdbool.h>
#include <SDL3/SDL.h>

/* Add-on plugins whose UI can be contributed by the plugin itself. Keep in
   step with the runtime accessors in plugin_ui.c. */
enum BongoCatPluginUiKind {
    BONGO_CAT_PLUGIN_UI_SOUNDS = 0,
    BONGO_CAT_PLUGIN_UI_ITEMS = 1,
    BONGO_CAT_PLUGIN_UI_STATISTICS = 2,
    BONGO_CAT_PLUGIN_UI_STORE = 3,
    BONGO_CAT_PLUGIN_UI_INOX2D = 4,
    BONGO_CAT_PLUGIN_UI_COMPANION = 5,
    BONGO_CAT_PLUGIN_UI_LAN = 6
};

struct BongoCatPluginUi {
    struct nk_context *context;
    const BongoCatI18n *i18n;
    struct nk_style_window saved;
    bool card_open, disabled_was_rom, disabled_was_active;
    uint32_t disabled_depth;
    struct nk_panel *disabled_panel;
    BongoCatPreferences *preferences;
};

typedef struct BongoCatPluginPageBinding {
    BongoCatPreferences *preferences;
    enum BongoCatPluginUiKind kind;
} BongoCatPluginPageBinding;

float bongo_cat_preferences_plugin_text_height(BongoCatPluginUi *, const char *, float, float);
const BongoCatPluginUiHost *bongo_cat_preferences_plugin_widgets(void);
const BongoCatPluginPageUiHost *bongo_cat_preferences_plugin_canvas(void);
bool bongo_cat_preferences_plugin_page(BongoCatPreferences *value,
    struct nk_context *context, enum BongoCatPluginUiKind kind);
uint32_t bongo_cat_preferences_plugin_read(void *context, uint32_t operation,
    uint32_t index, void *output, uint32_t size);
uint32_t bongo_cat_preferences_plugin_action(void *context, uint32_t operation,
    uint32_t index, const void *data, uint32_t size);

/* Draw a plugin's contributed card on the Plugins page. Returns false when the
   plugin exports no usable UI contribution, so the caller falls back to the
   host's built-in card. */
bool bongo_cat_preferences_plugin_card(BongoCatPreferences *value,
    struct nk_context *context, enum BongoCatPluginUiKind kind);

#endif
