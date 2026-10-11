#include "preferences_state.h"
#include "preferences_widgets.h"
#include "preferences_controls.h"
#include "preferences_notice.h"
#include "preferences_model_glyphs.h"
#include "bongo_cat/statistics.h"
#include <stdio.h>

static const char *label(BongoCatApp *app, const char *key, const char *fallback) {
    char full[128];
    snprintf(full, sizeof(full), "pages.preference.statistics.%s", key);
    return bongo_cat_i18n_get(app->i18n, full, fallback);
}
static void duration(char *text, size_t capacity, uint64_t milliseconds) {
    uint64_t seconds = milliseconds / 1000;
    snprintf(text, capacity, "%llu:%02llu:%02llu", (unsigned long long)(seconds / 3600),
        (unsigned long long)(seconds / 60 % 60), (unsigned long long)(seconds % 60));
}
void bongo_cat_preferences_statistics_usage(BongoCatPreferences *value, struct nk_context *context) {
    BongoCatApp *app = value->app;
    BongoCatApplicationSnapshot data;
    if (!bongo_cat_statistics_usage_snapshot(app, (unsigned)NK_CLAMP(0, value->statistics_period, 3), &data)) return;
    bongo_cat_pref_section(context, label(app, "applications", "Application usage"));
    bool enabled = data.enabled != 0;
    if (bongo_cat_pref_toggle(context, "application-statistics-enabled",
        label(app, "trackApplications", "Track foreground application time"),
        label(app, "usageHint", "Samples each second; pauses after 5 idle minutes, on lock or sleep. App names and durations stay local."), &enabled)) {
        if (!bongo_cat_statistics_usage_enable(app, enabled))
            bongo_cat_preferences_notice_show(app, label(app, "failed", "Unable to update statistics"), true);
    }
    if (data.enabled && !data.available) {
        nk_layout_row_dynamic(context, 54, 1);
        nk_label_wrap(context, label(app, "usageUnavailable", "Foreground app tracking is unavailable in this desktop session. Wayland is not supported; X11 requires xcb and xcb-screensaver."));
    }
    char total[64]; duration(total, sizeof(total), data.total_ms);
    bongo_cat_pref_status(context, "application-duration", label(app, "usageTotal", "Total active time (h:mm:ss)"), total);
    if (!data.count) {
        nk_layout_row_dynamic(context, 38, 1);
        nk_label_wrap(context, label(app, "usageEmpty", "No application usage recorded for this period yet."));
    }
    unsigned count = data.count > BONGO_CAT_STATISTICS_APPS ? BONGO_CAT_STATISTICS_APPS : data.count;
    uint64_t glyph_hash = UINT64_C(14695981039346656037);
    bool missing_glyphs = false;
    for (unsigned i = 0; i < count; ++i) {
        BongoCatApplicationTotal *entry = &data.apps[i];
        entry->name[sizeof(entry->name) - 1] = 0;
        if (!bongo_cat_preferences_model_glyphs_ready(value, entry->name)) missing_glyphs = true;
        for (const unsigned char *p = (const unsigned char *)entry->name; *p; ++p)
            glyph_hash = (glyph_hash ^ *p) * UINT64_C(1099511628211);
        char elapsed[80], id[40]; duration(elapsed, sizeof(elapsed), entry->milliseconds);
        snprintf(id, sizeof(id), "application-%u", i);
        bongo_cat_pref_status(context, id, entry->name, elapsed);
        nk_size percent = data.total_ms ? (nk_size)((double)entry->milliseconds / (double)data.total_ms * 1000.0) : 0;
        nk_layout_row_dynamic(context, 8, 1);
        nk_progress(context, &percent, 1000, NK_FIXED);
    }
    /* At most one font request per changed set of names, including atlas limits. */
    if (missing_glyphs && glyph_hash != value->statistics_glyph_hash) {
        value->statistics_glyph_hash = glyph_hash;
        value->font_reload_pending = true;
    }
    nk_layout_row_dynamic(context, 30, 1);
    nk_label(context, label(app, "usageHourly", "Today's application time by hour (minutes)"), NK_TEXT_LEFT);
    float maximum = 1.0f;
    for (unsigned i = 0; i < 24; ++i) {
        float minutes = (float)((double)data.hours[i] / 60000.0);
        if (minutes > maximum) maximum = minutes;
    }
    nk_layout_row_dynamic(context, 140, 1);
    if (nk_chart_begin(context, NK_CHART_COLUMN, 24, 0, maximum)) {
        for (unsigned i = 0; i < 24; ++i) {
            nk_flags event = nk_chart_push(context, (float)((double)data.hours[i] / 60000.0));
            if (event & NK_CHART_HOVERING) {
                char tooltip[80], elapsed[64]; duration(elapsed, sizeof(elapsed), data.hours[i]);
                snprintf(tooltip, sizeof(tooltip), "%02u:00  %s", i, elapsed);
                nk_tooltip(context, tooltip);
            }
        }
        nk_chart_end(context);
    }
    nk_layout_row_dynamic(context, 22, 3);
    nk_label(context, "00:00", NK_TEXT_LEFT); nk_label(context, "12:00", NK_TEXT_CENTERED); nk_label(context, "23:00", NK_TEXT_RIGHT);
    nk_layout_row_dynamic(context, 36, 1);
    if (nk_button_label(context, label(app, "usageExport", "Export application CSV"))) {
        bool ok = bongo_cat_statistics_usage_export(app);
        bongo_cat_preferences_notice_show(app, label(app, ok ? "exportQueued" : "failed",
            ok ? "Export queued in the background" : "Unable to export statistics"), !ok);
    }
}
