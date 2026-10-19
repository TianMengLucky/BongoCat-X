#include "preferences_live2d_setup.h"
#include "preferences_state.h"
#include "bongo_cat/statistics.h"
#include "bongo_cat/items.h"
#include "bongo_cat/sounds.h"
#include "bongo_cat/store.h"
#include "bongo_cat/companion.h"
#include "bongo_cat/lan.h"
#include "preferences_widgets.h"
#include "preferences_controls.h"
#include "preferences_notice.h"
#include "plugin_ui_internal.h"
#include "bongo_cat/model_plugins.h"
#include "bongo_cat/i18n.h"
#include "bongo_cat/preferences.h"
#include "ui_paint.h"
#include "bongo_cat/path.h"
#include "bongo_cat/platform.h"
#include <stdio.h>
static const char *tr(BongoCatApp *app, const char *key, const char *fallback) {
    return bongo_cat_i18n_get(app->i18n, key, fallback);
}
static void result_notice(BongoCatApp *app, bool ok, BongoCatError *error) {
    char message[1024];
    snprintf(message, sizeof(message), "%s%s%s", tr(app,
        ok ? "pages.preference.plugins.saved" : "pages.preference.plugins.failed",
        ok ? "Plugin settings updated" : "Unable to update plugin"),
        ok ? "" : "\n", ok ? "" : error->message);
    bongo_cat_preferences_notice_show(app, message, !ok);
    bongo_cat_preferences_invalidate(app->preferences);
}
void bongo_cat_preferences_plugin_import(BongoCatApp *app, const char *path) {
    BongoCatError error = {0};
    BongoCatModelEngine engine = 0;
    bool matched = false;
    bool ok = bongo_cat_lan_import(app, path, &matched, &error);
    if (!matched) ok = bongo_cat_companion_import(app, path, &matched, &error);
    if (!matched) ok = bongo_cat_store_import(app, path, &matched, &error);
    if (!matched) ok = bongo_cat_sounds_import_plugin(app, path, &matched, &error);
    if (!matched) ok = bongo_cat_items_import_plugin(app, path, &matched, &error);
    if (!matched) ok = bongo_cat_statistics_import(app, path, &matched, &error);
    if (!matched) ok = bongo_cat_model_plugin_install(path, &engine, &error);
    if (ok && app->session.active_model_id[0] && app->model_runtime && bongo_cat_model_runtime_engine(app->model_runtime) == engine &&
        !bongo_cat_model_runtime_rendering(app->model_runtime))
        ok = bongo_cat_app_reload_model_with_error(app, &error);
    result_notice(app, ok, &error);
    if (ok && !matched && engine == BONGO_CAT_MODEL_ENGINE_LIVE2D &&
        !bongo_cat_platform_live2d_core_available())
        bongo_cat_preferences_live2d_setup_request(app->preferences);
}
static void plugin_card(BongoCatPreferences *value, struct nk_context *context,
    BongoCatModelEngine engine, bool core_available) {
    BongoCatApp *app = value->app;
    BongoCatModelPluginInfo info;
    bongo_cat_model_plugin_info(engine, &info);
    bool live = engine == BONGO_CAT_MODEL_ENGINE_LIVE2D;
    BongoCatUIPalette palette = bongo_cat_ui_palette(bongo_cat_ui_dark(context));
    BongoCatPluginUi ui = {.context = context, .i18n = app->i18n, .preferences = value};
    const BongoCatPluginUiHost *widgets = bongo_cat_preferences_plugin_widgets();
    const BongoCatPluginPageUiHost *canvas = bongo_cat_preferences_plugin_canvas();
    if (!widgets->card_begin(&ui, live ? "live2d-plugin" : "inox-plugin")) return;
    widgets->label(&ui, live ? "Live2D Cubism" : "Inochi2D / Inox2D");
    widgets->label_wrapped(&ui, tr(app, live ? "pages.preference.plugins.liveDescription" :
        "pages.preference.plugins.inoxDescription", live ?
        "Live2D models require Cubism Core. Missing Core can be imported with the ZIP." :
        "Inochi2D models in INP / INX containers."), 76);
    struct nk_color text = context->style.text.color;
    context->style.text.color = info.active ? palette.accent : palette.muted;
    widgets->label(&ui, tr(app, info.active ? "pages.preference.plugins.active" :
        !info.installed ? "pages.preference.plugins.missing" :
        !info.enabled ? "pages.preference.plugins.disabled" : "pages.preference.plugins.installed",
        info.active ? "In use" : !info.installed ? "Not installed" :
        !info.enabled ? "Disabled" : "Installed"));
    context->style.text.color = text;
    uint32_t enabled = info.enabled;
    if (widgets->toggle_available(&ui, live ? "enable-live2d-plugin" : "enable-inox-plugin",
        tr(app, "pages.preference.plugins.enabled", "Enabled"), &enabled, info.installed && !info.active)) {
        BongoCatError error = {0};
        bool ok = bongo_cat_model_plugin_set_enabled(engine, enabled != 0, &error);
        result_notice(app, ok, &error);
        if (ok && live && enabled && !core_available)
            bongo_cat_preferences_live2d_setup_request(value);
    }
    if (info.managed && !info.active) {
        widgets->row(&ui, 1, 30);
        if (canvas->button(&ui, "remove-renderer-plugin",
            tr(app, "pages.preference.plugins.remove", "Remove plugin"), BONGO_UI_BUTTON_DANGER)) {
            BongoCatError error = {0};
            result_notice(app, bongo_cat_model_plugin_remove(engine, &error), &error);
        }
    }
    if (live && !core_available) {
        widgets->row(&ui, 1, 30);
        if (canvas->button(&ui, "import-live2d-core",
            tr(app, "native.live2dCoreImport", "Import Cubism Core"), BONGO_UI_BUTTON_SECONDARY))
            bongo_cat_preferences_request_sdk_import(value);
        if (info.enabled) {
            widgets->row(&ui, 1, 36);
            if (canvas->button(&ui, "download-live2d-sdk",
                tr(app, "pages.preference.live2dSetup.getSdk", "Download Live2D SDK"), BONGO_UI_BUTTON))
                bongo_cat_preferences_live2d_setup_request(value);
        }
    }
    widgets->card_end(&ui);
}
static void import_card(BongoCatPreferences *value, struct nk_context *context) {
    const char *title = tr(value->app, "pages.preference.plugins.drop", "Click or drop a plugin ZIP here");
    const char *hint = tr(value->app, "pages.preference.plugins.dropHint",
        "ZIP or plugin library; renderer packages can also supply missing Core.");
    BongoCatPluginUi ui = {.context = context, .i18n = value->app->i18n, .preferences = value};
    const BongoCatPluginUiHost *widgets = bongo_cat_preferences_plugin_widgets();
    const float padding = 20.0f;
    float width = NK_MAX(1.0f, nk_window_get_content_region(context).w - padding * 2);
    float title_height = bongo_cat_preferences_plugin_text_height(&ui, title, width, 30);
    float hint_height = bongo_cat_preferences_plugin_text_height(&ui, hint, width, 26);
    nk_layout_row_dynamic(context, padding * 2 + title_height + hint_height +
        context->style.window.spacing.y, 1);
    struct nk_rect bounds = nk_widget_bounds(context);
    bool hover = !context->current->widgets_disabled &&
        !(context->current->layout->flags & NK_WINDOW_ROM) &&
        nk_input_is_mouse_hovering_rect(&context->input, bounds) &&
        nk_input_is_mouse_hovering_rect(&context->input, nk_window_get_canvas(context)->clip);
    bool highlighted = value->import_drop_active || hover;
    BongoCatUIPalette p = bongo_cat_ui_palette(bongo_cat_ui_dark(context));
    struct nk_style_window saved = context->style.window;
    context->style.window.fixed_background = nk_style_item_color(highlighted ? p.hover_pink : p.hover);
    context->style.window.background = highlighted ? p.hover_pink : p.hover;
    context->style.window.group_padding = nk_vec2(padding, padding);
    context->style.window.rounding = 14;
    if (nk_group_begin(context, "plugin-import", NK_WINDOW_NO_SCROLLBAR)) {
        struct nk_color text = context->style.text.color;
        context->style.text.color = p.accent;
        widgets->label_wrapped(&ui, title, title_height);
        context->style.text.color = p.muted;
        widgets->label_wrapped(&ui, hint, hint_height);
        context->style.text.color = text;
        nk_group_end(context);
    }
    context->style.window = saved;
    bongo_cat_ui_paint_dashed_rounded(context, bounds, 14, 2, 5, 5, highlighted ? p.pink : p.accent);
    if (hover) bongo_cat_ui_cursor_hover_rect(context, bounds, BONGO_CAT_UI_CURSOR_POINTER);
    if (hover && nk_input_is_mouse_click_in_rect(&context->input, NK_BUTTON_LEFT, bounds))
        (void)bongo_cat_preferences_import_plugin_open(value->import_dialog, value->window);
}
static void settings_card(BongoCatPreferences *value, struct nk_context *context, bool companion) {
    BongoCatPluginUi ui = {.context = context, .i18n = value->app->i18n, .preferences = value};
    const BongoCatPluginUiHost *widgets = bongo_cat_preferences_plugin_widgets();
    if (!widgets->card_begin(&ui, companion ? "companion-plugin-settings" : "lan-plugin-settings")) return;
    widgets->label(&ui, tr(value->app, companion ?
        "pages.preference.companion.title" : "pages.preference.lan.title",
        companion ? "AI companion (experimental)" : "Friends / LAN"));
    widgets->label_wrapped(&ui, tr(value->app, companion ?
        "pages.preference.companion.description" : "pages.preference.lan.description",
        companion ? "Experimental AI companion. Disabled by default." :
        "Connect with friends using a room code."), 76);
    widgets->row(&ui, 1, 36);
    if (bongo_cat_preferences_plugin_canvas()->button(&ui, "open-plugin-settings",
        tr(value->app, "pages.preference.plugins.configure", "Open settings"), BONGO_UI_BUTTON_SECONDARY)) {
        value->page = companion ? 11 : 12;
        value->render_dirty = true;
    }
    widgets->card_end(&ui);
}
void bongo_cat_preferences_page_plugins(BongoCatPreferences *value, struct nk_context *context) {
    bongo_cat_pref_intro(context, tr(value->app, "pages.preference.plugins.description",
        "Manage renderers and extensions. Model type selects its renderer automatically."));
    const char *folder = tr(value->app, "pages.preference.plugins.openFolder", "Open folder");
    const struct nk_user_font *font = context->style.font;
    float width = NK_MAX(124.0f, font->width(font->userdata, font->height, folder, nk_strlen(folder)) + 28.0f);
    nk_layout_row_static(context, 36,
        (int)NK_MIN(width, nk_window_get_content_region(context).w), 1);
    BongoCatPluginUi ui = {.context = context, .i18n = value->app->i18n, .preferences = value};
    if (bongo_cat_preferences_plugin_canvas()->button(&ui, "open-plugin-folder", folder,
        BONGO_UI_BUTTON_SECONDARY)) {
        char directory[BONGO_CAT_PATH_CAP];
        BongoCatError error = {0};
        bool ok = bongo_cat_path_join(directory, sizeof(directory), value->app->data_root, "plugins") &&
            bongo_cat_path_create_directory(directory) && bongo_cat_platform_open_directory(directory);
        if (!ok) {
            bongo_cat_error_set(&error, BONGO_CAT_ERROR_IO, "%s", SDL_GetError());
            result_notice(value->app, false, &error);
        }
    }
    import_card(value, context);
    nk_layout_row_dynamic(context, 16, 1); nk_spacing(context, 1);
    const float minimum_card_width = 340.0f;
    float gap = context->style.window.spacing.x;
    int columns = NK_MAX(1, (int)((nk_window_get_content_region(context).w + gap) /
        (minimum_card_width + gap)));
    bool core_available = bongo_cat_platform_live2d_core_available();
    bongo_cat_pref_section(context, tr(value->app,
        "pages.preference.plugins.renderers", "Model renderers"));
    nk_layout_row_dynamic(context, core_available ? 360.0f : 440.0f, NK_MIN(columns, 2));
    plugin_card(value, context, BONGO_CAT_MODEL_ENGINE_LIVE2D, core_available);
    if (!bongo_cat_preferences_plugin_card(value, context, BONGO_CAT_PLUGIN_UI_INOX2D))
        plugin_card(value, context, BONGO_CAT_MODEL_ENGINE_INOX2D, core_available);
    nk_layout_row_dynamic(context, 12, 1); nk_spacing(context, 1);
    bongo_cat_preferences_plugin_widgets()->label(&ui, tr(value->app,
        "pages.preference.plugins.activeHint",
        "Click a selected model to deselect it before changing its active plugin."));
    bongo_cat_pref_section(context, tr(value->app,
        "pages.preference.plugins.extensions", "Extensions"));
    static const enum BongoCatPluginUiKind extensions[] = {
        BONGO_CAT_PLUGIN_UI_STATISTICS, BONGO_CAT_PLUGIN_UI_ITEMS,
        BONGO_CAT_PLUGIN_UI_SOUNDS, BONGO_CAT_PLUGIN_UI_STORE
    };
    bool companion = bongo_cat_companion_active(value->app);
    bool lan = value->app->lan != NULL;
    int count = (int)SDL_arraysize(extensions) + (int)companion + (int)lan;
    nk_layout_row_dynamic(context, 360, NK_MIN(columns, count));
    for (size_t i = 0; i < SDL_arraysize(extensions); ++i)
        (void)bongo_cat_preferences_plugin_card(value, context, extensions[i]);
    if (companion) settings_card(value, context, true);
    if (lan) settings_card(value, context, false);
}
