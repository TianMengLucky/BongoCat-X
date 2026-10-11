#include "statistics_internal.h"
#include "bongo_cat/path.h"

bool bongo_cat_statistics_usage_snapshot(BongoCatApp *app, unsigned period, BongoCatApplicationSnapshot *snapshot) {
    if (!app || !app->statistics || !snapshot || period > 3) return false;
    BongoCatStatistics *stats = app->statistics;
    BongoCatApplicationSnapshot updated = {0}; updated.struct_size = sizeof(updated);
    if (stats->usage->snapshot(stats->instance, period, &updated)) stats->applications[period] = updated;
    *snapshot = stats->applications[period];
    return true;
}
bool bongo_cat_statistics_usage_enable(BongoCatApp *app, bool enabled) {
    return app && app->statistics && app->statistics->usage->set_enabled(app->statistics->instance, enabled) != 0;
}
bool bongo_cat_statistics_usage_export(BongoCatApp *app) {
    char path[BONGO_CAT_PATH_CAP];
    if (!app || !app->statistics || !bongo_cat_path_join(path, sizeof(path), app->data_root, "application-usage.csv")) return false;
    if (!app->statistics->usage->export_csv(app->statistics->instance, path)) return false;
    app->statistics->export_status = 1;
    return true;
}
