#include "preferences_state.h"
#include "preferences_controls.h"
#include "preferences_widgets.h"
#include "preferences_notice.h"
#include "preferences_model_glyphs.h"
#include "bongo_cat/store.h"
#include "bongo_cat/i18n.h"
#include "bongo_cat/preferences.h"
#include <stdio.h>
static const char *label(BongoCatApp *app, const char *key, const char *fallback) {
    char full[128]; snprintf(full, sizeof(full), "pages.preference.store.%s", key);
    return bongo_cat_i18n_get(app->i18n, full, fallback);
}
static void failure(BongoCatApp *app) {
    bongo_cat_preferences_notice_show(app, label(app, "failed", "Unable to update the store"), true);
}
static void open_link(BongoCatApp *app, const char *url) {
    if (SDL_strncmp(url, "https://", 8) || !SDL_OpenURL(url)) failure(app);
}
void bongo_cat_preferences_page_store(BongoCatPreferences *value, struct nk_context *context) {
    BongoCatApp *app = value->app; unsigned count = 0;
    unsigned status = bongo_cat_store_status(app, &count);
    if (!status) {
        (void)bongo_cat_store_refresh(app);
        status = bongo_cat_store_status(app, &count);
    }
    bongo_cat_pref_intro(context, label(app, "help", "Browse community models and author links. Download community models after SHA-256 verification, or uninstall them here."));
    const char *refresh = label(app, "refresh", "Refresh");
    const char *repository = label(app, "repository", "Community repository");
    const struct nk_user_font *font = context->style.font;
    float refresh_width = NK_MAX(88.0f, font->width(font->userdata, font->height,
        refresh, nk_strlen(refresh)) + 28.0f);
    float repository_width = font->width(font->userdata, font->height,
        repository, nk_strlen(repository)) + 28.0f;
    float available = nk_window_get_content_region(context).w;
    bool fitted = refresh_width + repository_width + context->style.window.spacing.x <= available;
    if (fitted) {
        nk_layout_row_begin(context, NK_STATIC, 36, 2);
        nk_layout_row_push(context, refresh_width);
    } else nk_layout_row_dynamic(context, 36, 1);

    if (status == 1) nk_label(context, label(app, "loading", "Loading catalog..."), NK_TEXT_LEFT);
    else if (nk_button_label(context, refresh)) {
        if (!bongo_cat_store_refresh(app)) failure(app);
        value->render_dirty = true;
    }
    if (fitted) nk_layout_row_push(context, repository_width);
    if (nk_button_label(context, repository))
        open_link(app, "https://github.com/TianMengLucky/Awesome-BongoCat");
    if (fitted) nk_layout_row_end(context);
    if (status == 3 || status == 4 || (status == 2 && !count)) {
        nk_layout_row_dynamic(context, 60, 1);
        nk_label_wrap(context, label(app, status == 3 ? "unpublished" : status == 4 ? "networkError" : "empty",
            status == 3 ? "The community catalog has not been published yet." : status == 4 ?
            "Unable to load the catalog. Check your connection or catalog format and retry." : "No models are listed yet."));
    }
    if (status != 1) bongo_cat_preferences_store_cards(value, context, count);

}
void bongo_cat_preferences_store_plugin_card(BongoCatPreferences *value, struct nk_context *context) {
    BongoCatApp *app = value->app; BongoCatStoreInfo info; bongo_cat_store_info(app, &info);
    struct nk_style_window saved;
    if (!bongo_cat_pref_card_begin(context, "store-plugin", &saved)) return;
    nk_layout_row_dynamic(context, 30, 1); nk_label(context, label(app, "title", "Store"), NK_TEXT_LEFT);
    nk_layout_row_dynamic(context, 76, 1);
    nk_label_wrap(context, label(app, "pluginHint", "Rust plugin for the community model catalog. The Store page appears while the plugin is enabled."));
    nk_layout_row_dynamic(context, 25, 1);
    nk_label(context, bongo_cat_i18n_get(app->i18n, info.active ? "pages.preference.plugins.active" :
        !info.installed ? "pages.preference.plugins.missing" : "pages.preference.plugins.disabled",
        info.active ? "In use" : !info.installed ? "Not installed" : "Disabled"), NK_TEXT_LEFT);
    bool busy = bongo_cat_store_busy(app);
    nk_layout_row_dynamic(context, 30, 2);
    nk_label(context, bongo_cat_i18n_get(app->i18n, "pages.preference.plugins.enabled", "Enabled"), NK_TEXT_LEFT);
    bool enabled = info.enabled;
    if (bongo_cat_pref_control_toggle_available(context, "enable-store-plugin", &enabled, info.installed && !busy)) {
        BongoCatError error = {0}; if (!bongo_cat_store_set_enabled(app, enabled, &error)) failure(app);
        bongo_cat_preferences_invalidate(app->preferences);
    }
    if (info.managed && !busy) {
        nk_layout_row_dynamic(context, 30, 1);
        if (nk_button_label(context, bongo_cat_i18n_get(app->i18n, "pages.preference.plugins.remove", "Remove plugin"))) {
            BongoCatError error = {0}; if (!bongo_cat_store_remove(app, &error)) failure(app);
        }
    }
    bongo_cat_pref_card_end(context, &saved);
}
