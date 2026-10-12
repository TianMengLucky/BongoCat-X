#include "preferences_state.h"
#include "ui_paint.h"
#include "bongo_cat/tray.h"

#include <SDL3/SDL.h>

bool bongo_cat_preferences_reload_language(BongoCatPreferences *value) {
    if (!value || value->font_language == value->app->settings.app.language)
        return false;
    BongoCatLanguage previous = value->font_language;
    BongoCatLanguage requested = value->app->settings.app.language;
    BongoCatError error = {0};
    if (!value->app->i18n || bongo_cat_i18n_reload(value->app->i18n,
        requested, &error) != BONGO_CAT_OK) {
        value->app->settings.app.language = previous;
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", error.message);
        return false;
    }
    value->font_language = requested;
    if (!bongo_cat_preferences_reload_fonts(value)) {
        BongoCatError restore = {0};
        value->app->settings.app.language = previous;
        value->font_language = previous;
        bongo_cat_i18n_reload(value->app->i18n, previous, &restore);
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
            "Preferences font resources could not be rebuilt");
        return false;
    }
    bongo_cat_ui_paint_destroy(&value->ui);
    if (value->window) SDL_SetWindowTitle(value->window,
        bongo_cat_i18n_get(value->app->i18n,
            "native.preferencesWindowTitle", "BongoCat-X - Settings"));
    if (value->app->tray) bongo_cat_tray_sync(value->app->tray);
    value->render_dirty = true;
    return true;
}
