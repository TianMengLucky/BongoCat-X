#include "preferences_state.h"
#include "preferences_widgets.h"
#include "preferences_notice.h"
#include "bongo_cat/image.h"
#include "bongo_cat/path.h"
#include "bongo_cat/sha256.h"
#include "bongo_cat/log.h"
#include "runtime.h"
#include <stdio.h>
#include <string.h>

static const char *tr(BongoCatApp *app, const char *key, const char *fallback) {
    return bongo_cat_i18n_get(app->i18n, key, fallback);
}

void bongo_cat_preferences_background_row(BongoCatPreferences *value,
    struct nk_context *context) {
    BongoCatApp *app = value->app;
    BongoCatWindowPreferences *window = &app->settings.window;
    bongo_cat_pref_row_icon(context, BONGO_CAT_PREF_ICON_SOLID_BACKGROUND);
    if (bongo_cat_pref_toggle(context, "custom-background", tr(app,
        "pages.preference.cat.background.title", "Custom Image Background"), tr(app,
        "pages.preference.cat.background.help",
        "Fill the window with an image. Mutually exclusive with Solid Background."),
        &window->custom_background)) {
        if (window->custom_background && !window->custom_background_path[0]) {
            window->custom_background = false;
            bongo_cat_preferences_import_background_open(value->import_dialog, value->window);
        } else {
            if (window->custom_background) window->obs_background = false;
            app->dirty = true;
            bongo_cat_window_mark_hit_dirty(app);
        }
    }
    bongo_cat_pref_row_icon(context, BONGO_CAT_PREF_ICON_SOLID_BACKGROUND);
    if (bongo_cat_pref_button(context, "choose-background", tr(app,
        "pages.preference.cat.background.image", "Background Image"), tr(app,
        window->custom_background_path[0] ? "pages.preference.cat.background.selected"
            : "pages.preference.cat.background.empty",
        window->custom_background_path[0] ? "An image has been imported" : "No image selected"),
        tr(app, "pages.preference.cat.background.choose", "Choose Image")))
        bongo_cat_preferences_import_background_open(value->import_dialog, value->window);
}

void bongo_cat_preferences_background_import(BongoCatApp *app, const char *path) {
    BongoCatError error = {0};
    char digest[65], name[80], directory[BONGO_CAT_PATH_CAP];
    char target[BONGO_CAT_PATH_CAP], relative[BONGO_CAT_PATH_CAP];
    uint64_t size = 0;
    int width = 0, height = 0;
    BongoCatImage image = {0};
    bool created = false;
    if (!app || !path) return;
    if (!bongo_cat_path_file_size(path, &size) || !size || size > 64 * 1024 * 1024 ||
        !bongo_cat_image_info(path, &width, &height) || width <= 0 || height <= 0 ||
        (uint64_t)width * height > 16777216) goto failed;
    if (bongo_cat_sha256_file(path, digest, &error) != BONGO_CAT_OK) goto failed;
    snprintf(name, sizeof(name), "%s.img", digest);
    if (!bongo_cat_path_join(directory, sizeof(directory), app->data_root, "backgrounds") ||
        !bongo_cat_path_create_directory(directory) ||
        !bongo_cat_path_join(target, sizeof(target), directory, name) ||
        !bongo_cat_path_join(relative, sizeof(relative), "backgrounds", name)) goto failed;
    if (!bongo_cat_path_is_file(target)) {
        created = true;
        if (!bongo_cat_path_copy_file(path, target)) goto failed;
    }
    /* Validate the managed copy, not a source that may change during copying. */
    if (!bongo_cat_path_file_size(target, &size) || size > 64 * 1024 * 1024 ||
        !bongo_cat_image_info(target, &width, &height) || width <= 0 || height <= 0 ||
        (uint64_t)width * height > 16777216 ||
        bongo_cat_image_load(target, &image, &error) != BONGO_CAT_OK) goto failed;
    bongo_cat_image_free(&image);
    snprintf(app->settings.window.custom_background_path,
        sizeof(app->settings.window.custom_background_path), "%s", relative);
    app->settings.window.custom_background = true;
    app->settings.window.obs_background = false;
    app->dirty = true;
    bongo_cat_window_mark_hit_dirty(app);
    bongo_cat_preferences_notice_show(app, tr(app,
        "pages.preference.cat.background.imported", "Background image imported"), false);
    return;
failed:
    bongo_cat_image_free(&image);
    if (created) bongo_cat_path_remove(target);
    SDL_LogWarn(BONGO_CAT_LOG_LIFECYCLE, "Background image import failed: %s", error.message);
    bongo_cat_preferences_notice_show(app, tr(app,
        "pages.preference.cat.background.failed",
        "Cannot load this image. Choose a supported image up to 64 MB and 16 megapixels."), true);
}
