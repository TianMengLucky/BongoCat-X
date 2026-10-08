#include "preferences_state.h"
#include "preferences_widgets.h"
#include "preferences_controls.h"
#include "preferences_notice.h"
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
    bool ok = bongo_cat_model_plugin_install(path, &engine, &error);
    if (ok && app->session.active_model_id[0] && app->model_runtime && bongo_cat_model_runtime_engine(app->model_runtime) == engine &&
        !bongo_cat_model_runtime_rendering(app->model_runtime))
        ok = bongo_cat_app_reload_model_with_error(app, &error);
    result_notice(app, ok, &error);
}
static void plugin_card(BongoCatPreferences *value, struct nk_context *context,
    BongoCatModelEngine engine) {
    BongoCatApp *app = value->app;
    BongoCatModelPluginInfo info;
    bongo_cat_model_plugin_info(engine, &info);
    bool live = engine == BONGO_CAT_MODEL_ENGINE_LIVE2D;
    BongoCatUIPalette palette = bongo_cat_ui_palette(bongo_cat_ui_dark(context));
    struct nk_style_window saved = context->style.window;
    context->style.window.fixed_background = nk_style_item_color(palette.surface);
    context->style.window.background = palette.surface;
    context->style.window.group_border = 1;
    context->style.window.group_border_color = palette.border_subtle;
    context->style.window.group_padding = nk_vec2(18, 14);
    context->style.window.spacing = nk_vec2(8, 8);
    if (!nk_group_begin(context, live ? "live2d-plugin" : "inox-plugin",
        NK_WINDOW_BORDER | NK_WINDOW_NO_SCROLLBAR)) {
        context->style.window = saved;
        return;
    }
    nk_layout_row_dynamic(context, 30, 1);
    nk_label(context, live ? "Live2D Cubism" : "Inochi2D / Inox2D", NK_TEXT_LEFT);
    nk_layout_row_dynamic(context, 25, 1);
    nk_label(context, tr(app, info.active ? "pages.preference.plugins.active" :
        !info.installed ? "pages.preference.plugins.missing" :
        !info.enabled ? "pages.preference.plugins.disabled" : "pages.preference.plugins.installed",
        info.active ? "In use" : !info.installed ? "Not installed" :
        !info.enabled ? "Disabled" : "Installed"), NK_TEXT_LEFT);
    nk_layout_row_dynamic(context, 38, 1);
    nk_label_wrap(context, tr(app, live ? "pages.preference.plugins.liveDescription" :
        "pages.preference.plugins.inoxDescription", live ?
        "Live2D models require Cubism Core. Missing Core can be imported with the ZIP." : "Inochi2D models in INP / INX containers."));
    nk_layout_row_dynamic(context, 30, 2);
    nk_label(context, tr(app, "pages.preference.plugins.enabled", "Enabled"), NK_TEXT_LEFT);
    bool enabled = info.enabled;
    if (bongo_cat_pref_control_toggle_available(context, live ? "enable-live2d-plugin" : "enable-inox-plugin",
        &enabled, info.installed && !info.active)) {
        BongoCatError error = {0};
        result_notice(app, bongo_cat_model_plugin_set_enabled(engine, enabled, &error), &error);
    }
    nk_layout_row_dynamic(context, 30, 1);
    if (info.managed && !info.active && nk_button_label(context, tr(app,
        "pages.preference.plugins.remove", "Remove plugin"))) {
        BongoCatError error = {0};
        result_notice(app, bongo_cat_model_plugin_remove(engine, &error), &error);
    }
    if (live && !bongo_cat_platform_live2d_core_available()) {
        nk_layout_row_dynamic(context, 30, 1);
        if (nk_button_label(context, tr(app, "native.live2dCoreImport", "Import Cubism Core")))
            bongo_cat_preferences_request_sdk_import(value);
    }
    nk_group_end(context);
    context->style.window = saved;
}
static void import_card(BongoCatPreferences *value, struct nk_context *context) {
    struct nk_rect bounds;
    nk_layout_row_dynamic(context, 104, 1);
    if (nk_widget(&bounds, context) == NK_WIDGET_INVALID) return;
    bool hover = value->import_drop_active || nk_input_is_mouse_hovering_rect(&context->input, bounds);
    BongoCatUIPalette p = bongo_cat_ui_palette(bongo_cat_ui_dark(context));
    struct nk_command_buffer *canvas = nk_window_get_canvas(context);
    nk_fill_rect(canvas, bounds, 14, hover ? p.hover_pink : p.hover);
    bongo_cat_ui_paint_dashed_rounded(context, bounds, 14, 2, 5, 5, hover ? p.pink : p.accent);
    const char *title = tr(value->app, "pages.preference.plugins.drop", "Click or drop a plugin ZIP here");
    const char *hint = tr(value->app, "pages.preference.plugins.dropHint", "ZIP or renderer library; missing Core is imported from the package.");
    const struct nk_user_font *font = context->style.font;
    nk_draw_text(canvas, nk_rect(bounds.x + 20, bounds.y + 22, bounds.w - 40, 30),
        title, nk_strlen(title), font, nk_rgba(0, 0, 0, 0), p.accent);
    nk_draw_text(canvas, nk_rect(bounds.x + 20, bounds.y + 57, bounds.w - 40, 26),
        hint, nk_strlen(hint), font, nk_rgba(0, 0, 0, 0), p.muted);
    if (nk_input_is_mouse_click_in_rect(&context->input, NK_BUTTON_LEFT, bounds))
        (void)bongo_cat_preferences_import_plugin_open(value->import_dialog, value->window);
}
void bongo_cat_preferences_page_plugins(BongoCatPreferences *value, struct nk_context *context) {
    float width = nk_window_get_content_region(context).w;
    nk_layout_row_begin(context, NK_STATIC, 42, 2);
    nk_layout_row_push(context, NK_MAX(120.0f, width - 134));
    nk_label_wrap(context, tr(value->app, "pages.preference.plugins.description",
        "Install renderer plugins. The model type selects its renderer automatically."));
    nk_layout_row_push(context, 124);
    if (nk_button_label(context, tr(value->app,
        "pages.preference.plugins.openFolder", "Open folder"))) {
        char directory[BONGO_CAT_PATH_CAP];
        BongoCatError error = {0};
        bool ok = bongo_cat_path_join(directory, sizeof(directory), value->app->data_root, "plugins") &&
            bongo_cat_path_create_directory(directory) && bongo_cat_platform_open_directory(directory);
        if (!ok) {
            bongo_cat_error_set(&error, BONGO_CAT_ERROR_IO, "%s", SDL_GetError());
            result_notice(value->app, false, &error);
        }
    }
    nk_layout_row_end(context);
    import_card(value, context);
    nk_layout_row_dynamic(context, 16, 1); nk_spacing(context, 1);
    int columns = nk_window_get_content_region(context).w >= 620 ? 2 : 1;
    nk_layout_row_dynamic(context, bongo_cat_platform_live2d_core_available() ? 220.0f : 260.0f, columns);
    plugin_card(value, context, BONGO_CAT_MODEL_ENGINE_LIVE2D);
    plugin_card(value, context, BONGO_CAT_MODEL_ENGINE_INOX2D);
    nk_layout_row_dynamic(context, 54, 1);
    nk_label_wrap(context, tr(value->app, "pages.preference.plugins.activeHint",
        "Click a selected model to deselect it before changing its active plugin."));
}
