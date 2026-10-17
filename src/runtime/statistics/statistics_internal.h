#ifndef BONGO_CAT_STATISTICS_INTERNAL_H
#define BONGO_CAT_STATISTICS_INTERNAL_H
#include "plugin_page.h"
#include "bongo_cat/app.h"
#include "bongo_cat/statistics.h"
#include "bongo_cat/statistics_reset_plugin.h"
#include <SDL3/SDL.h>

struct BongoCatStatistics {
    SDL_SharedObject *library;
    BongoCatPluginPageSession page_ui;
    const BongoCatStatisticsPlugin *plugin;
    const BongoCatPluginUiContribution *ui;
    void *instance;
    uint64_t refresh_ms;
    const BongoCatStatisticsUsagePlugin *usage;
    BongoCatStatisticsPage page;
    BongoCatStatisticsSnapshot cached;
    BongoCatApplicationSnapshot applications[4];
    unsigned export_status, reset_status;
    const BongoCatStatisticsResetPlugin *reset;
    SDL_Thread *retirement;
    atomic_bool retired;
    bool close_ok, operation_ok, restart;
    char install_source[BONGO_CAT_PATH_CAP], install_target[BONGO_CAT_PATH_CAP], remove_path[BONGO_CAT_PATH_CAP];
};
bool bongo_cat_statistics_path(BongoCatApp *app, const char *suffix, char *path);
const BongoCatStatisticsPlugin *bongo_cat_statistics_query(SDL_SharedObject *library);
const BongoCatStatisticsPagePlugin *bongo_cat_statistics_page_api(SDL_SharedObject *library);
bool bongo_cat_statistics_retire(BongoCatApp *app, BongoCatError *error);
#endif
