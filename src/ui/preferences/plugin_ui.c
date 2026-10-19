#include "plugin_ui_internal.h"
#include "preferences_widgets.h"
#include "preferences_controls.h"
#include "preferences_notice.h"
#include "bongo_cat/app.h"
#include "bongo_cat/preferences.h"
#include "bongo_cat/sounds.h"
#include "bongo_cat/items.h"
#include "bongo_cat/statistics.h"
#include "bongo_cat/store.h"
#include "bongo_cat/model_plugins.h"
#include "plugin_locale.h"
#include <stdio.h>

/* Host side of the generic plugin UI ABI (include/bongo_cat/plugin_ui.h).
   This wraps the Nuklear context behind an opaque BongoCatPluginUi * so no
   UI toolkit type ever reaches a plugin, and lends each plugin the host
   services it needs to render its own management card. */

/* Per-plugin handle behind BongoCatPluginUiServices.context. #enable (-1 when
   untouched) and #remove record what the plugin requested so the host can apply
   it only after the draw returns. */
typedef struct PluginUiBinding {
    BongoCatApp *app;
    unsigned kind;
    int enable;
    bool remove;
} PluginUiBinding;

static const char *ui_text(BongoCatPluginUi *ui, const char *key, const char *fallback) {
    return bongo_cat_i18n_get(ui->i18n, key, fallback);
}

static void ui_intro(BongoCatPluginUi *ui, const char *text) {
    bongo_cat_pref_intro(ui->context, text);
}

static uint32_t ui_card_begin(BongoCatPluginUi *ui, const char *id) {
    if (ui->card_open) return 1;
    ui->card_open = bongo_cat_pref_card_begin(ui->context, id, &ui->saved);
    if (ui->card_open) ui->context->current->layout->flags &= ~(nk_flags)NK_WINDOW_NO_SCROLLBAR;
    return ui->card_open ? 1u : 0u;
}

static void ui_card_end(BongoCatPluginUi *ui) {
    if (!ui->card_open) return;
    bongo_cat_pref_card_end(ui->context, &ui->saved);
    ui->card_open = false;
}

/* Wrap whole words when possible and split long UTF-8 words at glyph boundaries. */
float bongo_cat_preferences_plugin_text_height(BongoCatPluginUi *ui,
    const char *text, float width, float minimum) {
    const struct nk_user_font *font = ui->context->style.font;
    float line = 0; unsigned lines = 1;
    width = NK_MAX(1.0f, width - ui->context->style.text.padding.x * 2);
    int remaining = text ? nk_strlen(text) : 0;
    while (remaining > 0) {
        if (*text == '\n') { lines++; line = 0; text++; remaining--; continue; }
        int length = 1;
        if (*text != ' ' && *text != '\t')
            while (length < remaining && text[length] != ' ' && text[length] != '\t' && text[length] != '\n') length++;
        float word = font->width(font->userdata, font->height, text, length);
        if (line > 0 && line + word > width) {lines++; line = 0;}
        if (word <= width) {line += word; text += length; remaining -= length; continue;}
        while (length > 0) {
            nk_rune rune;
            int bytes = nk_utf_decode(text, &rune, length);
            if (bytes <= 0) bytes = 1;
            float glyph = font->width(font->userdata, font->height, text, bytes);
            if (line > 0 && line + glyph > width) {lines++; line = 0;}
            line += glyph; text += bytes; remaining -= bytes; length -= bytes;
        }
    }
    return NK_MAX(minimum, lines * (font->height + 2.0f) + ui->context->style.text.padding.y * 2);
}

static void ui_label(BongoCatPluginUi *ui, const char *text) {
    float height = bongo_cat_preferences_plugin_text_height(ui, text,
        nk_window_get_content_region(ui->context).w, 30);
    nk_layout_row_dynamic(ui->context, height, 1);
    nk_label_wrap(ui->context, text);
}

static void ui_label_wrapped(BongoCatPluginUi *ui, const char *text, float height) {
    height = bongo_cat_preferences_plugin_text_height(ui, text,
        nk_window_get_content_region(ui->context).w, height > 0 ? height : 30);
    nk_layout_row_dynamic(ui->context, height, 1);
    nk_label_wrap(ui->context, text);
}

static void ui_spacer(BongoCatPluginUi *ui, float height) {
    nk_layout_row_dynamic(ui->context, height > 0.0f ? height : 8.0f, 1);
    nk_spacing(ui->context, 1);
}

static void ui_row(BongoCatPluginUi *ui, uint32_t columns, float height) {
    nk_layout_row_dynamic(ui->context, height > 0.0f ? height : 30.0f,
        columns > 0 ? (int)columns : 1);
}

static uint32_t ui_button(BongoCatPluginUi *ui, const char *id, const char *text) {
    return bongo_cat_preferences_plugin_canvas()->button(ui, id, text,
        id && SDL_strncmp(id, "remove-", 7) == 0 ? BONGO_UI_BUTTON_DANGER : BONGO_UI_BUTTON);
}

static uint32_t ui_toggle_available(BongoCatPluginUi *ui, const char *id,
    const char *text, uint32_t *value, uint32_t available) {
    if (!value) return 0;
    struct nk_context *ctx = ui->context;
    float width = nk_window_get_content_region(ctx).w;
    float control = NK_MIN(72.0f, width * .5f);
    float label = NK_MAX(1.0f, width - control - ctx->style.window.spacing.x);
    float height = bongo_cat_preferences_plugin_text_height(ui, text, label, 36);
    nk_layout_row_begin(ctx, NK_STATIC, height, 2);
    nk_layout_row_push(ctx, label); nk_label_wrap(ctx, text);
    nk_layout_row_push(ctx, control);
    bool on = *value != 0;
    bool changed = bongo_cat_pref_control_toggle_available(ctx, id, &on, available != 0 &&
        !ctx->current->widgets_disabled && !(ctx->current->layout->flags & NK_WINDOW_ROM));
    nk_layout_row_end(ctx);
    *value = on ? 1u : 0u;
    return changed ? 1u : 0u;
}
static uint32_t ui_toggle(BongoCatPluginUi *ui, const char *id, const char *text,
    uint32_t *value) {
    return ui_toggle_available(ui, id, text, value, 1);
}

static uint32_t ui_slider(BongoCatPluginUi *ui, const char *id, const char *title,
    const char *description, float *value, float minimum, float maximum,
    float step, float default_value) {
    if (!value) return 0;
    bool changed = bongo_cat_pref_slider(ui->context, id, title, description,
        minimum, value, maximum, step, default_value);
    return changed ? 1u : 0u;
}

static const BongoCatPluginUiHost UI_HOST = {
    BONGO_CAT_PLUGIN_UI_ABI,
    (uint32_t)sizeof(BongoCatPluginUiHost),
    ui_text,
    ui_intro,
    ui_card_begin,
    ui_card_end,
    ui_label,
    ui_label_wrapped,
    ui_spacer,
    ui_row,
    ui_button,
    ui_toggle,
    ui_toggle_available,
    ui_slider
};

const BongoCatPluginUiHost *bongo_cat_preferences_plugin_widgets(void) { return &UI_HOST; }

static void service_card_state(void *context, BongoCatPluginCardState *out) {
    const PluginUiBinding *binding = context;
    if (!binding || !out || out->struct_size != sizeof(*out)) return;
    *out = (BongoCatPluginCardState){0};
    out->struct_size = sizeof(*out);
    switch (binding->kind) {
    case BONGO_CAT_PLUGIN_UI_SOUNDS: {
        BongoCatSoundsInfo info;
        bongo_cat_sounds_info(binding->app, &info);
        out->installed = info.installed; out->enabled = info.enabled;
        out->active = info.active; out->managed = info.managed;
        out->busy = bongo_cat_sounds_busy(binding->app);
        break;
    }
    case BONGO_CAT_PLUGIN_UI_ITEMS: {
        BongoCatItemsInfo info;
        bongo_cat_items_info(binding->app, &info);
        out->installed = info.installed; out->enabled = info.enabled;
        out->active = info.active; out->managed = info.managed;
        out->busy = bongo_cat_items_busy(binding->app);
        break;
    }
    case BONGO_CAT_PLUGIN_UI_STATISTICS: {
        BongoCatStatisticsInfo info;
        bongo_cat_statistics_info(binding->app, &info);
        out->installed = info.installed; out->enabled = info.enabled;
        out->active = info.active; out->managed = info.managed;
        out->busy = 0;
        break;
    }
    case BONGO_CAT_PLUGIN_UI_STORE: {
        BongoCatStoreInfo info;
        bongo_cat_store_info(binding->app, &info);
        out->installed = info.installed; out->enabled = info.enabled;
        out->active = info.active; out->managed = info.managed;
        out->busy = bongo_cat_store_busy(binding->app);
        break;
    }
    case BONGO_CAT_PLUGIN_UI_INOX2D: {
        BongoCatModelPluginInfo info;
        bongo_cat_model_plugin_info(BONGO_CAT_MODEL_ENGINE_INOX2D, &info);
        out->installed = info.installed; out->enabled = info.enabled;
        out->active = info.active; out->managed = info.managed;
        break;
    }
    default: break;
    }
}

static void service_set_enabled(void *context, uint32_t enabled) {
    PluginUiBinding *binding = context;
    if (binding) binding->enable = enabled != 0 ? 1 : 0;
}

static void service_remove(void *context) {
    PluginUiBinding *binding = context;
    if (binding) binding->remove = true;
}

/* Runs after the plugin stopped drawing: it may unload the plugin library. */
static bool apply_request(PluginUiBinding *binding, BongoCatError *error) {
    switch (binding->kind) {
    case BONGO_CAT_PLUGIN_UI_SOUNDS:
        return binding->remove ?
            bongo_cat_sounds_remove_plugin(binding->app, error) :
            bongo_cat_sounds_set_enabled(binding->app, binding->enable != 0, error);
    case BONGO_CAT_PLUGIN_UI_ITEMS:
        return binding->remove ?
            bongo_cat_items_remove_plugin(binding->app, error) :
            bongo_cat_items_set_enabled(binding->app, binding->enable != 0, error);
    case BONGO_CAT_PLUGIN_UI_STATISTICS:
        return binding->remove ?
            bongo_cat_statistics_remove(binding->app, error) :
            bongo_cat_statistics_set_enabled(binding->app, binding->enable != 0, error);
    case BONGO_CAT_PLUGIN_UI_STORE:
        return binding->remove ?
            bongo_cat_store_remove(binding->app, error) :
            bongo_cat_store_set_enabled(binding->app, binding->enable != 0, error);
    case BONGO_CAT_PLUGIN_UI_INOX2D:
        return binding->remove ? bongo_cat_model_plugin_remove(BONGO_CAT_MODEL_ENGINE_INOX2D, error) :
            bongo_cat_model_plugin_set_enabled(BONGO_CAT_MODEL_ENGINE_INOX2D, binding->enable != 0, error);
    default:
        return false;
    }
}

static void report(PluginUiBinding *binding, const BongoCatError *error) {
    const char *key, *fallback;
    switch (binding->kind) {
    case BONGO_CAT_PLUGIN_UI_SOUNDS:
        key = "pages.preference.sounds.failed"; fallback = "Unable to update sound effects"; break;
    case BONGO_CAT_PLUGIN_UI_ITEMS:
        key = "pages.preference.items.failed"; fallback = "Unable to update items"; break;
    case BONGO_CAT_PLUGIN_UI_STATISTICS:
        key = "pages.preference.statistics.failed"; fallback = "Unable to update statistics"; break;
    case BONGO_CAT_PLUGIN_UI_INOX2D:
        key = "pages.preference.plugins.failed"; fallback = "Unable to update plugin"; break;
    default:
        key = "pages.preference.store.failed"; fallback = "Unable to update the store"; break;
    }
    const char *text = bongo_cat_i18n_get(binding->app->i18n, key, fallback);
    char message[1024];
    snprintf(message, sizeof(message), "%s%s%s", text,
        error && error->message[0] ? "\n" : "", error ? error->message : "");
    bongo_cat_preferences_notice_show(binding->app, message, true);
}

/* Provisioning controls remain available when no plugin code can draw them. */
static void fallback_card(BongoCatPluginUi *ui, PluginUiBinding *binding) {
    static const char *const ids[] = {"sounds", "items", "statistics", "store"};
    static const char *const titles[] = {"Sound effects", "Items", "Statistics", "Store"};
    unsigned kind = binding->kind;
    if (kind >= SDL_arraysize(ids)) return;
    char id[64], key[128];
    snprintf(id, sizeof(id), "%s-plugin", ids[kind]);
    if (!ui_card_begin(ui, id)) return;
    snprintf(key, sizeof(key), "pages.preference.%s.title", ids[kind]);
    ui_label(ui, ui_text(ui, key, titles[kind]));
    BongoCatPluginCardState state = {.struct_size = sizeof(state)};
    service_card_state(binding, &state);
    const char *status = state.active ? "active" : !state.installed ? "missing" :
        !state.enabled ? "disabled" : "installed";
    snprintf(key, sizeof(key), "pages.preference.plugins.%s", status);
    ui_label(ui, ui_text(ui, key, status));
    uint32_t enabled = state.enabled;
    snprintf(id, sizeof(id), "enable-%s-plugin", ids[kind]);
    if (ui_toggle_available(ui, id, ui_text(ui, "pages.preference.plugins.enabled", "Enabled"),
            &enabled, state.installed && !state.busy)) service_set_enabled(binding, enabled);
    if (state.managed && !state.busy) {
        ui_row(ui, 1, 30);
        if (ui_button(ui, "remove-plugin", ui_text(ui, "pages.preference.plugins.remove", "Remove plugin")))
            service_remove(binding);
    }
    ui_card_end(ui);
}

bool bongo_cat_preferences_plugin_card(BongoCatPreferences *value,
    struct nk_context *context, enum BongoCatPluginUiKind kind) {
    if (!value || !context) return false;
    BongoCatApp *app = value->app;
    const BongoCatPluginUiContribution *contribution = NULL;
    SDL_SharedObject *temporary = NULL;
    switch (kind) {
    case BONGO_CAT_PLUGIN_UI_SOUNDS: contribution = bongo_cat_sounds_ui(app); break;
    case BONGO_CAT_PLUGIN_UI_ITEMS: contribution = bongo_cat_items_ui(app); break;
    case BONGO_CAT_PLUGIN_UI_STATISTICS: contribution = bongo_cat_statistics_ui(app); break;
    case BONGO_CAT_PLUGIN_UI_STORE: contribution = bongo_cat_store_ui(app); break;
    case BONGO_CAT_PLUGIN_UI_INOX2D: {
        BongoCatModelPluginInfo info;
        bongo_cat_model_plugin_info(BONGO_CAT_MODEL_ENGINE_INOX2D, &info);
        /* The active renderer already holds this library. Borrow another OS
           reference for the draw, then release before processing requests. */
        if (info.active) temporary = SDL_LoadObject(info.path);
        if (temporary) {
            union { SDL_FunctionPointer function; BongoCatPluginUiQuery query; } entry;
            entry.function = SDL_LoadFunction(temporary, "bongo_cat_inox2d_ui_query_v1");
            if (entry.function) contribution = entry.query(BONGO_CAT_PLUGIN_UI_ABI);
        }
        break;
    }
    default: return false;
    }
    if (contribution && (contribution->abi_version != BONGO_CAT_PLUGIN_UI_ABI ||
            contribution->struct_size != sizeof(*contribution) || !contribution->draw)) contribution = NULL;
    if (!contribution && kind == BONGO_CAT_PLUGIN_UI_INOX2D) {
        if (temporary) SDL_UnloadObject(temporary);
        return false;
    }
    if (temporary) bongo_cat_plugin_locale_load(app->i18n, temporary, "inox2d");
    PluginUiBinding binding = {0};
    binding.app = app;
    binding.kind = (unsigned)kind;
    binding.enable = -1;
    BongoCatPluginUiServices services = {
        BONGO_CAT_PLUGIN_UI_ABI, (uint32_t)sizeof(BongoCatPluginUiServices), &binding,
        service_card_state, service_set_enabled, service_remove
    };
    BongoCatPluginUi ui = {0};
    ui.context = context;
    ui.i18n = app->i18n;
    ui.preferences = value;
    bool drawn = contribution && contribution->draw(NULL, &ui, &UI_HOST, &services) != 0;
    ui_card_end(&ui);
    if (!drawn) fallback_card(&ui, &binding);
    if (temporary) {
        bongo_cat_i18n_unregister_plugin(app->i18n, "inox2d");
        SDL_UnloadObject(temporary);
    }
    /* Nothing may call into the plugin after this point: applying the request
       can unload its library. */
    if (binding.remove || binding.enable >= 0) {
        BongoCatError error = {0};
        if (!apply_request(&binding, &error)) report(&binding, &error);
        bongo_cat_preferences_invalidate(app->preferences);
    }
    return true;
}
