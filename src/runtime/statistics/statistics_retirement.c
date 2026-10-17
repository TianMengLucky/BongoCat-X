#include "plugin_locale.h"
#include "statistics_internal.h"
#include "bongo_cat/file.h"
#include "bongo_cat/preferences.h"

static int SDLCALL retire_worker(void *context) {
    BongoCatStatistics *stats = context;
    stats->close_ok = stats->plugin->destroy(stats->instance) != 0;
    SDL_UnloadObject(stats->library);
    stats->operation_ok = true;
    if (stats->install_source[0]) {
        stats->operation_ok = bongo_cat_file_replace(stats->install_source, stats->install_target, true);
        (void)bongo_cat_file_remove(stats->install_source);
    }
    if (stats->remove_path[0]) stats->operation_ok = bongo_cat_file_remove(stats->remove_path);
    atomic_store_explicit(&stats->retired, true, memory_order_release);
    return 0;
}
bool bongo_cat_statistics_retire(BongoCatApp *app, BongoCatError *error) {
    if (!app || !app->statistics || app->statistics_retiring) return false;
    BongoCatStatistics *stats = app->statistics;
    bongo_cat_statistics_overlay_destroy(app);
    atomic_init(&stats->retired, false);
    bongo_cat_input_metrics_enable(&app->input, false);
    bongo_cat_i18n_unregister_plugin(app->i18n, "statistics");
    bongo_cat_plugin_page_stop(&stats->page_ui);
    stats->retirement = SDL_CreateThread(retire_worker, "statistics-stop", stats);
    if (!stats->retirement) {
        bongo_cat_plugin_locale_load(app->i18n, stats->library, "statistics");
        bongo_cat_input_metrics_enable(&app->input, true);
        bongo_cat_error_set(error, BONGO_CAT_ERROR_IO, "Cannot start statistics cleanup worker");
        return false;
    }
    app->statistics = NULL;
    app->statistics_retiring = stats;
    bongo_cat_preferences_invalidate(app->preferences);
    return true;
}
