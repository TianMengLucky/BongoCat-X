#include "windows_game_compatibility.h"
#include "bongo_cat/app.h"

bool bongo_cat_windows_game_compatibility_startup(BongoCatApp *app,
    bool *restarting, BongoCatError *error) {
    *restarting = false;
    if (app->smoke || app->secondary_pet || !app->settings.app.run_as_admin)
        return true;
    bool administrator = false;
    if (!bongo_cat_windows_game_compatibility_elevated(&administrator, error))
        return false;
    if (administrator) return true;
    if (!bongo_cat_windows_game_compatibility_launch(true, error)) return false;
    *restarting = true;
    return true;
}

bool bongo_cat_windows_game_compatibility_set(BongoCatApp *app,
    bool enabled, BongoCatError *error) {
    if (!app || app->secondary_pet || app->smoke || app->settings_store_blocked) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_PLATFORM,
            "Run-as-administrator settings are not writable in this instance");
        return false;
    }
    bool administrator = false;
    if (!bongo_cat_windows_game_compatibility_elevated(&administrator, error))
        return false;
    BongoCatApplicationPreferences previous = app->settings.app;

    app->settings.app.run_as_admin = enabled;
    if (bongo_cat_settings_save(app->settings_path, &app->settings, error) != BONGO_CAT_OK) {
        app->settings.app = previous;
        return false;
    }

    /* An elevated process already has the required access. Disabling only
       changes future launches; it must not restart the running app. */
    bool needs_restart = enabled && !administrator;
    if (needs_restart && !bongo_cat_windows_game_compatibility_launch(true, error)) {
        app->settings.app = previous;
        BongoCatError rollback = {0};
        (void)bongo_cat_settings_save(app->settings_path, &app->settings, &rollback);
        return false;
    }
    if (needs_restart) app->running = false;
    return true;
}
