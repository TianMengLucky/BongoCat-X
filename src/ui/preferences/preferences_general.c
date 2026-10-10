#include "preferences_internal.h"
#include "preferences_state.h"
#include "preferences_theme.h"
#include "preferences_controls.h"
#include "preferences_widgets.h"
#include "preferences_notice.h"
#include "ui_tooltip.h"
#include "bongo_cat/audio.h"
#include "bongo_cat/i18n.h"
#include "bongo_cat/log.h"
#include "bongo_cat/preferences.h"
#include "bongo_cat/tray.h"
#include "portable_mode.h"
#include "runtime.h"
#ifdef _WIN32
#include "windows_game_compatibility.h"
#endif

#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

static const char *tr(BongoCatApp *app, const char *key, const char *fallback) {
    return bongo_cat_i18n_get(app->i18n, key, fallback);
}

static void section_gap(struct nk_context *context, float pixels) {
    context->current->layout->at_y += pixels;
}

static void update_autostart(BongoCatApp *app, bool old_value) {
    BongoCatError error = {0};
    if (bongo_cat_platform_set_autostart(app->settings.app.autostart,
        false, &error) == BONGO_CAT_OK) return;
    app->settings.app.autostart = old_value;
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", error.message);
    char message[1024];
    snprintf(message, sizeof(message), "%s\n%s", tr(app,
        "pages.preference.general.hints.autostartFailed",
        "Unable to update launch-on-startup settings"), error.message);
    bongo_cat_preferences_notice_show(app, message, true);
}

#ifdef __APPLE__
/* System Settings keeps Input Monitoring under Privacy & Security. */
#define BONGO_CAT_INPUT_MONITORING_SETTINGS_URI \
    "x-apple.systempreferences:com.apple.preference.security?Privacy_ListenEvent"

void bongo_cat_preferences_input_monitoring_refresh(BongoCatPreferences *value) {
    if (!value) return;
    /* The refresh points are user actions, not frames: one read here. */
    bool authorized = bongo_cat_platform_input_monitoring_authorized();
    /* A button drawn from the old reading stays on screen until something
       repaints, so a reading that changed - or arrives for the first time -
       schedules the next frame; an unchanged one must not, or every refresh
       point would keep the window rendering. */
    if (value->input_monitoring_valid &&
        value->input_monitoring_authorized == authorized) return;
    value->input_monitoring_authorized = authorized;
    value->input_monitoring_valid = true;
    value->render_dirty = true;
}

static void enable_input_monitoring(BongoCatApp *app) {
    BongoCatPreferences *value = app->preferences;
    if (!value) return;
    /* The permission can change while the page is open, so the button reads it
       once more before acting: an already granted permission hides the button
       instead of asking again or sending the user to System Settings. */
    bongo_cat_preferences_input_monitoring_refresh(value);
    if (value->input_monitoring_authorized) return;
    /* The system prompt belongs to this button and to a missing permission; a
       refusal is an answer, not a failure. */
    (void)bongo_cat_platform_input_monitoring_request();
    /* SDL_OpenURL is the whole entry point, so a failure is where the path has
       to be spelled out: the item itself only says when to restart. */
    if (!SDL_OpenURL(BONGO_CAT_INPUT_MONITORING_SETTINGS_URI))
        bongo_cat_preferences_notice_show(app, tr(app,
            "pages.preference.general.status.openSettingsFailed",
            "Cannot open System Settings. Open Privacy & Security → Input Monitoring."), true);
    bongo_cat_preferences_input_monitoring_refresh(value);
}

static void input_monitoring_section(BongoCatApp *app,
    struct nk_context *context) {
    /* The page is drawn through the preferences that hold the cached reading; a
       page drawn before the first refresh initializes it once, never per frame. */
    BongoCatPreferences *value = app->preferences;
    if (!value) return;
    if (!value->input_monitoring_valid)
        bongo_cat_preferences_input_monitoring_refresh(value);
    bongo_cat_pref_section_icon(context, tr(app,
        "pages.preference.general.labels.permissionsSettings",
        "Permissions Settings"), BONGO_CAT_PREF_ICON_SECTION_APPLICATION);
    const char *title = tr(app,
        "pages.preference.general.labels.inputMonitoringPermission",
        "Input Monitoring Permission");
    /* Granted is a reading without an action; only a missing permission offers
       the button, and nothing here asks for authorization twice. */
    if (value->input_monitoring_authorized) {
        bongo_cat_pref_status(context, "input-monitoring", title, tr(app,
            "pages.preference.general.status.authorized", "Authorized"));
        return;
    }
    if (bongo_cat_pref_button(context, "input-monitoring", title, tr(app,
        "pages.preference.general.hints.inputMonitoringPermission",
        "Allow keyboard input from other apps in macOS Input Monitoring, then quit and restart BongoCat."), tr(app,
        "pages.preference.general.status.authorize", "Go to Enable")))
        enable_input_monitoring(app);
}
#endif

void bongo_cat_preferences_page_general(BongoCatApp *app, struct nk_context *context) {
    BongoCatApplicationPreferences *options = &app->settings.app;
    // Keep each option in its own native language so the list is recognizable
    // regardless of the language currently used by the settings window. The
    // names come from the i18n core so the font glyph ranges cover them.
    const char *ui_languages[] = {
        bongo_cat_ui_language_name(BONGO_CAT_LANG_ZH_CN),
        bongo_cat_ui_language_name(BONGO_CAT_LANG_ZH_HANT),
        bongo_cat_ui_language_name(BONGO_CAT_LANG_EN_US),
        bongo_cat_ui_language_name(BONGO_CAT_LANG_FR_FR),
        bongo_cat_ui_language_name(BONGO_CAT_LANG_DE_DE),
        bongo_cat_ui_language_name(BONGO_CAT_LANG_JA_JP),
        bongo_cat_ui_language_name(BONGO_CAT_LANG_KO_KR),
        bongo_cat_ui_language_name(BONGO_CAT_LANG_PT_BR),
        bongo_cat_ui_language_name(BONGO_CAT_LANG_RU_RU),
        bongo_cat_ui_language_name(BONGO_CAT_LANG_ES_ES)};
    bongo_cat_pref_section_icon(context, tr(app,
        "pages.preference.general.labels.appSettings", "Application"),
        BONGO_CAT_PREF_ICON_SECTION_APPLICATION);
    bool old_autostart = options->autostart;
    bongo_cat_pref_row_icon(context, BONGO_CAT_PREF_ICON_AUTOSTART);
    if (bongo_cat_pref_toggle(context, "autostart", tr(app,
        "pages.preference.general.labels.launchOnStartup", "Launch on Startup"), tr(app, "pages.preference.general.hints.launchOnStartup",
            "Start BongoCat automatically when you sign in."),
        &options->autostart)) update_autostart(app, old_autostart);
#ifdef _WIN32
    bongo_cat_pref_row_icon(context, BONGO_CAT_PREF_ICON_ADMINISTRATOR);
    if (bongo_cat_pref_toggle_help(context, "run-as-admin",
        tr(app, "pages.preference.general.labels.runAsAdmin",
            "Run as Administrator"),
        tr(app, "pages.preference.general.hints.runAsAdmin",
            "Improve input capture in elevated Windows games. Enabling requests permission and restarts the app."),
        tr(app, "pages.preference.general.hints.runAsAdminHelp",
            "Running as administrator lets the app capture some system-level keys and input events more reliably, and stay responsive in games that run with administrator privileges.\n\nWhen enabled: the app asks for administrator permission and restarts itself; every later launch (including launch-on-startup) also runs as administrator. With launch-on-startup enabled, Windows shows a User Account Control confirmation once at each sign-in.\n\nWhen disabled: normal privileges are restored from the next launch on; the running app is not affected."),
        &options->run_as_admin)) {
        BongoCatError admin_error = {0};
        if (!bongo_cat_windows_game_compatibility_set(app,
                options->run_as_admin, &admin_error)) {
            char message[1024];
            snprintf(message, sizeof(message), "%s\n%s", tr(app,
                "pages.preference.general.hints.runAsAdminFailed",
                "Unable to change the run-as-administrator setting"),
                admin_error.message);
            bongo_cat_preferences_notice_show(app, message, true);
        }
    }
#endif
    /* Enabling portable storage copies existing data beside the executable;
       the selected location is used from the next launch on. */
    bool portable = bongo_cat_portable_mode_active();
    bongo_cat_pref_row_icon(context, BONGO_CAT_PREF_ICON_SECTION_APPLICATION);
    if (bongo_cat_pref_toggle_help(context, "portable-mode",
        tr(app, "pages.preference.general.labels.portableMode",
            "Portable Mode"), tr(app, "pages.preference.general.hints.portableModeSummary",
            "Store data beside the executable. Enabling copies existing data; restart to use the selected location."),
        tr(app, "pages.preference.general.hints.portableMode",
            "Store settings, models and data in the application folder instead of the system profile. Enabling copies your existing config, data (including the live2d core), models and state folders next to the executable (files already there are kept; originals are not deleted) and takes effect after a restart. The switch itself is stored in BongoCat.ini beside the executable."),
        &portable)) {
        BongoCatError portable_error = {0};
        if (bongo_cat_portable_mode_set(portable, app, &portable_error))
            bongo_cat_preferences_notice_show(app, tr(app, portable ?
                "pages.preference.general.hints.portableModeEnabled" :
                "pages.preference.general.hints.portableModeDisabled",
                portable ?
                "Portable mode will be used after a restart" : "The system data location will be used after a restart"),
                false);
        else {
            char message[1024];
            snprintf(message, sizeof(message), "%s\n%s", tr(app,
                "pages.preference.general.hints.portableModeFailed", "Unable to change the portable mode setting"),
                portable_error.message[0] ? portable_error.message :
                "unknown error");
            bongo_cat_preferences_notice_show(app, message, true);
        }
    }
    bongo_cat_preferences_render_backend_row(app, options, context);
    bongo_cat_preferences_large_render_row(app, options, context);
    section_gap(context, 7);
    bongo_cat_pref_section_icon(context, tr(app,
        "pages.preference.general.labels.appearanceSettings", "Appearance"),
        BONGO_CAT_PREF_ICON_SECTION_APPEARANCE);
    section_gap(context, 6);
    const int language_to_ui[] = {2, 0, 1, 3, 4, 5, 6, 7, 8, 9};
    const BongoCatLanguage ui_to_language[] = {
        BONGO_CAT_LANG_ZH_CN, BONGO_CAT_LANG_ZH_HANT, BONGO_CAT_LANG_EN_US, BONGO_CAT_LANG_FR_FR,
        BONGO_CAT_LANG_DE_DE, BONGO_CAT_LANG_JA_JP, BONGO_CAT_LANG_KO_KR, BONGO_CAT_LANG_PT_BR,
        BONGO_CAT_LANG_RU_RU, BONGO_CAT_LANG_ES_ES};
    bongo_cat_pref_row_icon(context, BONGO_CAT_PREF_ICON_LANGUAGE);
    int selected = bongo_cat_pref_combo(context,
        "language", tr(app, "pages.preference.general.labels.language",
        "Language"), tr(app, "pages.preference.general.hints.language",
            "Change the interface language immediately; model names are kept as authored."), ui_languages, BONGO_CAT_LANG_COUNT,
        language_to_ui[options->language]);
    options->language = ui_to_language[selected];
    const char *themes[] = {
        tr(app, "pages.preference.general.options.auto", "System"),
        tr(app, "pages.preference.general.options.lightMode", "Light"),
        tr(app, "pages.preference.general.options.darkMode", "Dark")};
    bongo_cat_pref_row_icon(context, BONGO_CAT_PREF_ICON_THEME);
    options->theme = (BongoCatTheme)bongo_cat_pref_theme(context,
        "theme", tr(app, "pages.preference.general.labels.themeMode",
        "Theme"), tr(app, "pages.preference.general.hints.themeMode",
            "Follow the system appearance or choose a light or dark settings window."), themes, options->theme);
#ifdef __APPLE__
    section_gap(context, 7);
    input_monitoring_section(app, context);
#endif
}
