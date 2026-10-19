#ifndef BONGO_CAT_STATISTICS_H
#define BONGO_CAT_STATISTICS_H

#include "bongo_cat/common.h"
#include "bongo_cat/statistics_plugin.h"
#include "bongo_cat/statistics_words_plugin.h"
#include "bongo_cat/statistics_page_plugin.h"
#include "bongo_cat/statistics_usage_plugin.h"
#include "bongo_cat/plugin_ui.h"
#include "bongo_cat/plugin_ui_page.h"
/* Optional UI contribution the plugin may export so it can draw its own
   management card. */
#define BONGO_CAT_STATISTICS_UI_SYMBOL "bongo_cat_statistics_ui_query_v1"

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
union SDL_Event;
bool bongo_cat_statistics_overlay_event(BongoCatApp *app, const union SDL_Event *event);
void bongo_cat_statistics_overlay_update(BongoCatApp *app);
void bongo_cat_statistics_overlay_destroy(BongoCatApp *app);
void bongo_cat_statistics_info(BongoCatApp *app, BongoCatStatisticsInfo *info);
/* Borrowed host copy; NULL when the plugin has no active registration. */
const BongoCatStatisticsPage *bongo_cat_statistics_page(const BongoCatApp *app);
/* The active plugin's optional UI contribution, or NULL when it exports none.
   Call it after every load/unload instead of caching the pointer. */
const BongoCatPluginUiContribution *bongo_cat_statistics_ui(BongoCatApp *app);
const BongoCatPluginPageUiContribution *bongo_cat_statistics_page_ui(BongoCatApp *app, void **instance);
bool bongo_cat_statistics_set_enabled(BongoCatApp *app, bool enabled, BongoCatError *error);
/* matched distinguishes a statistics import from a renderer import. */
bool bongo_cat_statistics_import(BongoCatApp *app, const char *source,
    bool *matched, BongoCatError *error);
bool bongo_cat_statistics_install(BongoCatApp *app, const char *source, BongoCatError *error);
bool bongo_cat_statistics_remove(BongoCatApp *app, BongoCatError *error);
bool bongo_cat_statistics_snapshot(BongoCatApp *app, BongoCatStatisticsSnapshot *snapshot);
bool bongo_cat_statistics_export(BongoCatApp *app, BongoCatError *error);
bool bongo_cat_statistics_reset_available(const BongoCatApp *app);
bool bongo_cat_statistics_reset_busy(const BongoCatApp *app);
bool bongo_cat_statistics_reset(BongoCatApp *app, BongoCatError *error);

bool bongo_cat_statistics_timer_snapshot(BongoCatApp *app, BongoCatStatisticsTimerSnapshot *snapshot);
bool bongo_cat_statistics_backspace_snapshot(BongoCatApp *app, uint32_t *enabled);
bool bongo_cat_statistics_backspace_enable(BongoCatApp *app, bool enabled);
bool bongo_cat_statistics_timer_enable(BongoCatApp *app, bool enabled);
bool bongo_cat_statistics_words_snapshot(BongoCatApp *app, unsigned period, BongoCatStatisticsWordsSnapshot *snapshot);
bool bongo_cat_statistics_keyboard_snapshot(BongoCatApp *app, unsigned period, uint64_t counts[256]);
bool bongo_cat_statistics_usage_icon(BongoCatApp *app, unsigned period, unsigned index, char *path);
bool bongo_cat_statistics_icon_path(const char *root, const char *id, char *path);
bool bongo_cat_statistics_usage_snapshot(BongoCatApp *app, unsigned period, BongoCatApplicationSnapshot *snapshot);
bool bongo_cat_statistics_usage_enable(BongoCatApp *app, bool enabled);
bool bongo_cat_statistics_usage_export(BongoCatApp *app);
#endif
