#include "plugin_locale.h"
#include "statistics_internal.h"
#include "bongo_cat/file.h"
#include "bongo_cat/i18n.h"
#include "bongo_cat/log.h"
#include "bongo_cat/path.h"
#include "bongo_cat/preferences.h"
#include "preferences_notice.h"
#include <stdio.h>
#include <string.h>

const char *bongo_cat_statistics_filename(void) {
#ifdef _WIN32
    return "bongo_stats.dll";
#elif defined(__APPLE__)
    return "libbongo_stats.dylib";
#else
    return "libbongo_stats.so";
#endif
}
bool bongo_cat_statistics_path(BongoCatApp *app, const char *suffix, char *path) {
    char directory[BONGO_CAT_PATH_CAP], name[80];
    snprintf(name, sizeof(name), "%s%s", bongo_cat_statistics_filename(), suffix);
    return app && bongo_cat_path_join(directory, sizeof(directory), app->data_root, "plugins") &&
        bongo_cat_path_join(path, BONGO_CAT_PATH_CAP, directory, name);
}
static bool resolve(BongoCatApp *app, char *path) {
    const char *name = bongo_cat_statistics_filename();
    const char *override = SDL_getenv("BONGO_CAT_PLUGIN_DIRECTORY");
    if (override && override[0])
        return bongo_cat_path_join(path, BONGO_CAT_PATH_CAP, override, name) && bongo_cat_path_is_file(path);
    if (bongo_cat_statistics_path(app, "", path) && bongo_cat_path_is_file(path)) return true;
    const char *base = SDL_GetBasePath(); /* SDL-owned. */
    char directory[BONGO_CAT_PATH_CAP];
    if (!base) return false;
    if (bongo_cat_path_join(directory, sizeof(directory), base, "plugins") &&
        bongo_cat_path_join(path, BONGO_CAT_PATH_CAP, directory, name) && bongo_cat_path_is_file(path)) return true;
#ifdef __APPLE__
    if (bongo_cat_path_join(directory, sizeof(directory), base, "../PlugIns") &&
        bongo_cat_path_join(path, BONGO_CAT_PATH_CAP, directory, name) && bongo_cat_path_is_file(path)) return true;
#endif
    return false;
}
void bongo_cat_statistics_info(BongoCatApp *app, BongoCatStatisticsInfo *info) {
    if (!info) return;
    *info = (BongoCatStatisticsInfo){0};
    if (!app) return;
    info->installed = resolve(app, info->path);
    char path[BONGO_CAT_PATH_CAP];
    info->enabled = info->installed && bongo_cat_statistics_path(app, ".disabled", path) && !bongo_cat_path_is_file(path);
    info->managed = info->installed && bongo_cat_statistics_path(app, "", path) && !SDL_strcmp(path, info->path);
    info->active = app->statistics != NULL;
}
static const BongoCatStatisticsUsagePlugin *usage_query(SDL_SharedObject *library);
const BongoCatStatisticsPlugin *bongo_cat_statistics_query(SDL_SharedObject *library) {
    union { SDL_FunctionPointer function; BongoCatStatisticsQuery query; } entry;
    entry.function = SDL_LoadFunction(library, BONGO_CAT_STATISTICS_SYMBOL);
    const BongoCatStatisticsPlugin *plugin = entry.function ? entry.query(BONGO_CAT_STATISTICS_ABI) : NULL;
    return plugin && plugin->abi_version == BONGO_CAT_STATISTICS_ABI && plugin->struct_size == sizeof(*plugin) &&
        plugin->create && plugin->destroy && plugin->ingest && plugin->snapshot && plugin->flush && plugin->export_csv &&
        usage_query(library) && bongo_cat_statistics_page_api(library) ? plugin : NULL;
}
static void poll_metrics(void *context, BongoCatStatisticsMetrics *metrics) {
    bongo_cat_input_metrics_take(context, metrics);
}
static void poll_application(void *context, BongoCatApplicationSample *sample) {
    BongoCatInputState *input = context;
    *sample = (BongoCatApplicationSample){0};
    if (bongo_cat_input_metrics_enabled(input))
        bongo_cat_statistics_application_poll(context, sample);
}
static const BongoCatStatisticsUsagePlugin *usage_query(SDL_SharedObject *library) {
    union { SDL_FunctionPointer function; BongoCatStatisticsUsageQuery query; } entry;
    entry.function = SDL_LoadFunction(library, BONGO_CAT_STATISTICS_USAGE_SYMBOL);
    const BongoCatStatisticsUsagePlugin *p = entry.function ? entry.query(1) : NULL;
    return p && p->abi_version == 1 && p->struct_size == sizeof(*p) && p->configure &&
        p->snapshot && p->set_enabled && p->export_csv && p->export_status ? p : NULL;
}
static const BongoCatStatisticsResetPlugin *reset_query(SDL_SharedObject *library) {
    union { SDL_FunctionPointer function; BongoCatStatisticsResetQuery query; } entry;
    entry.function = SDL_LoadFunction(library, BONGO_CAT_STATISTICS_RESET_SYMBOL);
    const BongoCatStatisticsResetPlugin *p = entry.function ? entry.query(BONGO_CAT_STATISTICS_RESET_ABI) : NULL;
    if (!entry.function) SDL_ClearError(); /* Older plugins can still count. */
    return p && p->abi_version == BONGO_CAT_STATISTICS_RESET_ABI &&
        p->struct_size == sizeof(*p) && p->reset && p->status ? p : NULL;
}
static const BongoCatPluginUiContribution *query_ui(SDL_SharedObject *library) {
    union { SDL_FunctionPointer function; BongoCatPluginUiQuery query; } entry;
    entry.function = SDL_LoadFunction(library, BONGO_CAT_STATISTICS_UI_SYMBOL);
    if (!entry.function) return NULL;
    const BongoCatPluginUiContribution *contribution = entry.query(BONGO_CAT_PLUGIN_UI_ABI);
    return contribution && contribution->abi_version == BONGO_CAT_PLUGIN_UI_ABI &&
        contribution->struct_size == sizeof(*contribution) && contribution->draw ?
        contribution : NULL;
}
static bool start(BongoCatApp *app, BongoCatError *error) {
    if (!app || app->secondary_pet || app->smoke_ignore_global_input) return false;
    if (app->statistics) return true;
    if (app->statistics_retiring) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_IO, "Statistics plugin is finishing a background operation");
        return false;
    }
    BongoCatStatisticsInfo info;
    bongo_cat_statistics_info(app, &info);
    if (!info.enabled) return false;
    SDL_SharedObject *library = SDL_LoadObject(info.path);
    const BongoCatStatisticsPlugin *plugin = library ? bongo_cat_statistics_query(library) : NULL;
    char directory[BONGO_CAT_PATH_CAP];
    void *instance = plugin && bongo_cat_path_join(directory, sizeof(directory), app->data_root, "statistics") ?
        plugin->create(directory) : NULL;
    BongoCatStatistics *stats = instance ? SDL_calloc(1, sizeof(*stats)) : NULL;
    if (!stats) {
        if (instance) plugin->destroy(instance);
        if (library) SDL_UnloadObject(library);
        bongo_cat_error_set(error, BONGO_CAT_ERROR_IO, "Statistics plugin could not open its local database or has an incompatible ABI");
        return false;
    }
    stats->library = library; stats->plugin = plugin; stats->instance = instance;
    stats->usage = usage_query(library);
    stats->reset = reset_query(library);
    stats->ui = query_ui(library);
    const BongoCatStatisticsPagePlugin *page = bongo_cat_statistics_page_api(library);
    stats->page.struct_size = sizeof(stats->page);
    if (!page || !page->page(instance, &stats->page) ||
        stats->page.struct_size != sizeof(stats->page) ||
        !memchr(stats->page.id, 0, sizeof(stats->page.id)) ||
        !memchr(stats->page.title_key, 0, sizeof(stats->page.title_key)) ||
        !memchr(stats->page.title, 0, sizeof(stats->page.title)) ||
        SDL_strcmp(stats->page.id, "statistics") || !stats->page.title_key[0] ||
        !stats->page.title[0] || stats->page.icon != BONGO_CAT_STATISTICS_PAGE_ICON_ACTIVITY) {
        plugin->destroy(instance); SDL_UnloadObject(library); SDL_free(stats);
        bongo_cat_error_set(error, BONGO_CAT_ERROR_FORMAT, "Statistics plugin cannot register its page");
        return false;
    }
    stats->cached.struct_size = sizeof(stats->cached);
    for (unsigned i = 0; i < 4; ++i) {
        stats->applications[i].struct_size = sizeof(stats->applications[i]);
        stats->applications[i].enabled = 1;
    }
    if (!stats->usage || !stats->usage->configure(instance, &app->input, poll_metrics,
        poll_application)) {
        plugin->destroy(instance); SDL_UnloadObject(library); SDL_free(stats);
        bongo_cat_error_set(error, BONGO_CAT_ERROR_FORMAT, "Statistics plugin requires the independent worker extension");
        return false;
    }
    app->statistics = stats;
    bongo_cat_plugin_locale_load(app->i18n, library, "statistics");
    bongo_cat_preferences_plugin_fonts_changed(app->preferences);
    bongo_cat_input_metrics_enable(&app->input, true);
    bongo_cat_preferences_invalidate(app->preferences);
    SDL_LogInfo(BONGO_CAT_LOG_LIFECYCLE, "[statistics] Rust plugin enabled");
    return true;
}
void bongo_cat_statistics_start(BongoCatApp *app) {
    BongoCatError error = {0};
    if (!start(app, &error) && error.message[0])
        SDL_LogWarn(BONGO_CAT_LOG_LIFECYCLE, "[statistics] %s", error.message);
}
void bongo_cat_statistics_tick(BongoCatApp *app) {
    if (!app) return;
    BongoCatStatistics *retiring = app->statistics_retiring;
    if (retiring && atomic_load_explicit(&retiring->retired, memory_order_acquire)) {
        SDL_WaitThread(retiring->retirement, NULL); /* Already completed. */
        bool restart = retiring->restart;
        if (!retiring->close_ok || !retiring->operation_ok)
            bongo_cat_preferences_notice_show(app, bongo_cat_i18n_get(app->i18n,
                "pages.preference.statistics.failed", "Statistics background operation failed"), true);
        SDL_free(retiring); app->statistics_retiring = NULL;
        if (restart) (void)bongo_cat_statistics_set_enabled(app, true, NULL);
        bongo_cat_preferences_invalidate(app->preferences);
    }
    if (!app->statistics) return;
    uint64_t now = SDL_GetTicks();
    if (now - app->statistics->refresh_ms >= 1000) {
        app->statistics->refresh_ms = now;
        unsigned status = app->statistics->usage->export_status(app->statistics->instance);
        if (status != app->statistics->export_status && status >= 2) {
            const char *key = status == 2 ? "pages.preference.statistics.exported" : "pages.preference.statistics.failed";
            bongo_cat_preferences_notice_show(app, bongo_cat_i18n_get(app->i18n, key,
                status == 2 ? "CSV saved in the data folder" : "Statistics export failed"), status != 2);
        }
        app->statistics->export_status = status;
        BongoCatStatistics *stats = app->statistics;
        unsigned reset = stats->reset ? stats->reset->status(stats->instance) : 0;
        if (stats->reset_status == 1 && reset >= 2) {
            bongo_cat_preferences_notice_show(app, bongo_cat_i18n_get(app->i18n,
                reset == 2 ? "pages.preference.statistics.resetDone" : "pages.preference.statistics.failed",
                reset == 2 ? "Statistics reset" : "Unable to reset statistics"), reset != 2);
            if (reset == 2) {
                (void)bongo_cat_statistics_snapshot(app, &stats->cached);
                bongo_cat_statistics_overlay_destroy(app);
            }
        }
        stats->reset_status = reset;
        bongo_cat_preferences_statistics_invalidate(app->preferences);
    }
}
void bongo_cat_statistics_stop(BongoCatApp *app) {
    if (!app) return;
    if (app->statistics_retiring) {
        SDL_WaitThread(app->statistics_retiring->retirement, NULL);
        SDL_free(app->statistics_retiring); app->statistics_retiring = NULL;
    }
    if (!app->statistics) return;
    bongo_cat_input_metrics_enable(&app->input, false);
    BongoCatStatistics *stats = app->statistics;
    /* Collection stops before draining, so no producer can refill the batch. */
    if (!stats->plugin->destroy(stats->instance))
        SDL_LogError(BONGO_CAT_LOG_LIFECYCLE, "[statistics] Final database flush failed");
    bongo_cat_i18n_unregister_plugin(app->i18n, "statistics");
    bongo_cat_plugin_page_stop(&stats->page_ui);
    SDL_UnloadObject(stats->library);
    SDL_free(stats); app->statistics = NULL;
}
static bool marker(BongoCatApp *app, bool enabled) {
    char directory[BONGO_CAT_PATH_CAP], path[BONGO_CAT_PATH_CAP];
    if (!bongo_cat_path_join(directory, sizeof(directory), app->data_root, "plugins") ||
        !bongo_cat_path_create_directory(directory) || !bongo_cat_statistics_path(app, ".disabled", path)) return false;
    if (enabled) return !bongo_cat_path_is_file(path) || bongo_cat_file_remove(path);
    FILE *file = bongo_cat_file_open(path, "wb");
    return file && fclose(file) == 0;
}
bool bongo_cat_statistics_set_enabled(BongoCatApp *app, bool enabled, BongoCatError *error) {
    if (!app || app->secondary_pet) return false;
    if (app->statistics_retiring) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_IO, "Statistics plugin is finishing a background operation");
        return false;
    }
    if (!marker(app, enabled)) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_IO, "Cannot save statistics plugin activation state"); return false;
    }
    if (!enabled) {
        if (!app->statistics || bongo_cat_statistics_retire(app, error)) return true;
        (void)marker(app, true); return false;
    }
    if (start(app, error)) return true;
    (void)marker(app, false);
    return false;
}
bool bongo_cat_statistics_snapshot(BongoCatApp *app, BongoCatStatisticsSnapshot *snapshot) {
    if (!app || !app->statistics || !snapshot) return false;
    BongoCatStatistics *stats = app->statistics;
    BongoCatStatisticsSnapshot updated = {0}; updated.struct_size = sizeof(updated);
    if (stats->plugin->snapshot(stats->instance, &updated)) stats->cached = updated;
    *snapshot = stats->cached;
    return true;
}
bool bongo_cat_statistics_export(BongoCatApp *app, BongoCatError *error) {
    char path[BONGO_CAT_PATH_CAP];
    if (!app || !app->statistics || !bongo_cat_path_join(path, sizeof(path), app->data_root, "statistics-export.csv")) return false;
    if (app->statistics->plugin->export_csv(app->statistics->instance, path)) { app->statistics->export_status = 1; return true; }
    bongo_cat_error_set(error, BONGO_CAT_ERROR_IO, "Cannot export statistics CSV"); return false;
}

bool bongo_cat_statistics_reset_available(const BongoCatApp *app) {
    return app && app->statistics && app->statistics->reset;
}
bool bongo_cat_statistics_reset_busy(const BongoCatApp *app) {
    return app && app->statistics && app->statistics->reset_status == 1;
}
bool bongo_cat_statistics_reset(BongoCatApp *app, BongoCatError *error) {
    if (!bongo_cat_statistics_reset_available(app) || bongo_cat_statistics_reset_busy(app) ||
        !app->statistics->reset->reset(app->statistics->instance)) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_IO, "Cannot queue statistics reset"); return false;
    }
    app->statistics->reset_status = 1;
    bongo_cat_preferences_invalidate(app->preferences);
    return true;
}
const BongoCatPluginUiContribution *bongo_cat_statistics_ui(BongoCatApp *app) {
    return app && app->statistics ? app->statistics->ui : NULL;
}

const BongoCatPluginPageUiContribution *bongo_cat_statistics_page_ui(BongoCatApp *app, void **instance) {
    if (!app || !app->statistics) return NULL;
    return bongo_cat_plugin_page_query(&app->statistics->page_ui, app->statistics->library,
        "bongo_cat_statistics_page_ui_query_v1", instance);
}
