#include "statistics_internal.h"
#include "bongo_cat/path.h"
#include "bongo_cat/safe_ffi.h"
#include <string.h>
#include <stdio.h>

bool bongo_cat_statistics_usage_snapshot(BongoCatApp *app, unsigned period, BongoCatApplicationSnapshot *snapshot) {
    if (!app || !app->statistics || !snapshot || period > 3) return false;
    BongoCatStatistics *stats = app->statistics;
    BongoCatApplicationSnapshot updated = {0}; updated.struct_size = sizeof(updated);
    if (stats->usage->snapshot(stats->instance, period, &updated)) stats->applications[period] = updated;
    *snapshot = stats->applications[period];
    return true;
}
bool bongo_cat_statistics_usage_enable(BongoCatApp *app, bool enabled) {
    return app && app->statistics && bongo_cat_plugin_event(app, app->statistics->events,
        app->statistics->instance, BONGO_PLUGIN_EVENT_COLLECTION, enabled) != 0;
}
bool bongo_cat_statistics_usage_export(BongoCatApp *app) {
    return app && app->statistics && bongo_cat_plugin_event(app, app->statistics->events,
        app->statistics->instance, BONGO_PLUGIN_EVENT_EXPORT, 1) != 0;
}

bool bongo_cat_statistics_keyboard_snapshot(BongoCatApp *app, unsigned period, uint64_t counts[256]) {
    if (!app || !app->statistics || !app->statistics->keyboard || period > 3 || !counts) return false;
    BongoCatStatistics *stats = app->statistics;
    stats->keyboard->snapshot(stats->instance, period, stats->key_counts[period]);
    memcpy(counts, stats->key_counts[period], sizeof(stats->key_counts[period]));
    return true;
}
bool bongo_cat_statistics_icon_path(const char *root, const char *id, char *path) {
    char digest[65], name[72], directory[BONGO_CAT_PATH_CAP];
    if (!root || !id || !id[0] || !path) return false;
    bongo_safe_sha256_bytes(id, strlen(id), digest);
    snprintf(name, sizeof(name), "%s.bmp", digest);
    return bongo_cat_path_join(directory, sizeof(directory), root, "application-icons") &&
        bongo_cat_path_join(path, BONGO_CAT_PATH_CAP, directory, name);
}

bool bongo_cat_statistics_usage_icon(BongoCatApp *app, unsigned period, unsigned index, char *path) {
    if (!app || !app->statistics || period > 3 || index >= BONGO_CAT_STATISTICS_APPS) return false;
    const BongoCatApplicationSnapshot *snapshot = &app->statistics->applications[period];
    const char *id = snapshot->apps[index].id;
    return index < snapshot->count && memchr(id, 0, BONGO_CAT_STATISTICS_APP_CAP) &&
        bongo_cat_statistics_icon_path(app->data_root, id, path) && bongo_cat_path_is_file(path);
}
