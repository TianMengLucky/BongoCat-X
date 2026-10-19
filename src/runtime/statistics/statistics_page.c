#include "statistics_internal.h"

const BongoCatStatisticsPagePlugin *bongo_cat_statistics_page_api(SDL_SharedObject *library) {
    union { SDL_FunctionPointer function; BongoCatStatisticsPageQuery query; } entry;
    entry.function = SDL_LoadFunction(library, BONGO_CAT_STATISTICS_PAGE_SYMBOL);
    const BongoCatStatisticsPagePlugin *plugin = entry.function ?
        entry.query(BONGO_CAT_STATISTICS_PAGE_ABI) : NULL;
    return plugin && plugin->abi_version == BONGO_CAT_STATISTICS_PAGE_ABI &&
        plugin->struct_size == sizeof(*plugin) && plugin->page ? plugin : NULL;
}

const BongoCatStatisticsPage *bongo_cat_statistics_page(const BongoCatApp *app) {
    return app && app->statistics ? &app->statistics->page : NULL;
}

bool bongo_cat_statistics_words_snapshot(BongoCatApp *app, unsigned period,
    BongoCatStatisticsWordsSnapshot *snapshot) {
    if (!app || !app->statistics || !snapshot || period >= 4) return false;
    BongoCatStatistics *stats = app->statistics;
    union { SDL_FunctionPointer function; BongoCatStatisticsWordsQuery query; } entry;
    entry.function = SDL_LoadFunction(stats->library, BONGO_CAT_STATISTICS_WORDS_SYMBOL);
    if (!entry.function) { SDL_ClearError(); return false; }
    const BongoCatStatisticsWordsPlugin *plugin = entry.query(1);
    return plugin && plugin->abi_version == 1 && plugin->struct_size == sizeof(*plugin) &&
        plugin->snapshot && plugin->snapshot(stats->instance, period, snapshot);
}

static const BongoCatStatisticsTimerPlugin *timer_api(BongoCatStatistics *stats) {
    union { SDL_FunctionPointer function; BongoCatStatisticsTimerQuery query; } entry;
    entry.function = SDL_LoadFunction(stats->library, BONGO_CAT_STATISTICS_TIMER_SYMBOL);
    if (!entry.function) { SDL_ClearError(); return NULL; }
    const BongoCatStatisticsTimerPlugin *plugin = entry.query(1);
    return plugin && plugin->abi_version == 1 && plugin->struct_size == sizeof(*plugin) &&
        plugin->snapshot && plugin->enable ? plugin : NULL;
}

bool bongo_cat_statistics_timer_snapshot(BongoCatApp *app, BongoCatStatisticsTimerSnapshot *snapshot) {
    if (!app || !app->statistics || !snapshot) return false;
    const BongoCatStatisticsTimerPlugin *plugin = timer_api(app->statistics);
    return plugin && plugin->snapshot(app->statistics->instance, snapshot);
}

bool bongo_cat_statistics_timer_enable(BongoCatApp *app, bool enabled) {
    if (!app || !app->statistics) return false;
    const BongoCatStatisticsTimerPlugin *plugin = timer_api(app->statistics);
    return plugin && plugin->enable(app->statistics->instance, enabled);
}

static const BongoCatStatisticsBackspacePlugin *backspace_api(BongoCatStatistics *stats) {
    union { SDL_FunctionPointer function; BongoCatStatisticsBackspaceQuery query; } entry;
    entry.function = SDL_LoadFunction(stats->library, BONGO_CAT_STATISTICS_BACKSPACE_SYMBOL);
    if (!entry.function) { SDL_ClearError(); return NULL; }
    const BongoCatStatisticsBackspacePlugin *plugin = entry.query(1);
    return plugin && plugin->abi_version == 1 && plugin->struct_size == sizeof(*plugin) &&
        plugin->snapshot && plugin->enable ? plugin : NULL;
}
bool bongo_cat_statistics_backspace_snapshot(BongoCatApp *app, uint32_t *enabled) {
    if (!app || !app->statistics || !enabled) return false;
    const BongoCatStatisticsBackspacePlugin *plugin = backspace_api(app->statistics);
    return plugin && plugin->snapshot(app->statistics->instance, enabled);
}
bool bongo_cat_statistics_backspace_enable(BongoCatApp *app, bool enabled) {
    if (!app || !app->statistics) return false;
    const BongoCatStatisticsBackspacePlugin *plugin = backspace_api(app->statistics);
    return plugin && plugin->enable(app->statistics->instance, enabled);
}
