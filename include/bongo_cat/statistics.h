#ifndef BONGO_CAT_STATISTICS_H
#define BONGO_CAT_STATISTICS_H

#include "bongo_cat/common.h"
#include "bongo_cat/statistics_plugin.h"
#include "bongo_cat/statistics_page_plugin.h"
#include "bongo_cat/statistics_usage_plugin.h"

typedef struct BongoCatApp BongoCatApp;
typedef struct BongoCatStatistics BongoCatStatistics;
typedef struct BongoCatStatisticsInfo {
    bool installed, enabled, active, managed;
    char path[BONGO_CAT_PATH_CAP];
} BongoCatStatisticsInfo;

const char *bongo_cat_statistics_filename(void);
void bongo_cat_statistics_start(BongoCatApp *app);
void bongo_cat_statistics_stop(BongoCatApp *app);
void bongo_cat_statistics_tick(BongoCatApp *app);
void bongo_cat_statistics_info(BongoCatApp *app, BongoCatStatisticsInfo *info);
/* Borrowed host copy; NULL when the plugin has no active registration. */
const BongoCatStatisticsPage *bongo_cat_statistics_page(const BongoCatApp *app);
bool bongo_cat_statistics_set_enabled(BongoCatApp *app, bool enabled, BongoCatError *error);
/* matched distinguishes a statistics import from a renderer import. */
bool bongo_cat_statistics_import(BongoCatApp *app, const char *source,
    bool *matched, BongoCatError *error);
bool bongo_cat_statistics_install(BongoCatApp *app, const char *source, BongoCatError *error);
bool bongo_cat_statistics_remove(BongoCatApp *app, BongoCatError *error);
bool bongo_cat_statistics_snapshot(BongoCatApp *app, BongoCatStatisticsSnapshot *snapshot);
bool bongo_cat_statistics_export(BongoCatApp *app, BongoCatError *error);

bool bongo_cat_statistics_usage_snapshot(BongoCatApp *app, unsigned period, BongoCatApplicationSnapshot *snapshot);
bool bongo_cat_statistics_usage_enable(BongoCatApp *app, bool enabled);
bool bongo_cat_statistics_usage_export(BongoCatApp *app);
#endif
