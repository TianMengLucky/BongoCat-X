#include "preferences_state.h"
#include "preferences_widgets.h"
#include "preferences_controls.h"
#include "preferences_notice.h"
#include "bongo_cat/statistics.h"
#include "bongo_cat/preferences.h"
#include "bongo_cat/path.h"
#include "ui_paint.h"
#include <stdio.h>

static const char *tr(BongoCatApp *app, const char *key, const char *fallback) {
    return bongo_cat_i18n_get(app->i18n, key, fallback);
}
static const char *label(BongoCatApp *app, const char *key, const char *fallback) {
    char full[128];
    snprintf(full, sizeof(full), "pages.preference.statistics.%s", key);
    return tr(app, full, fallback);
}
static void notice(BongoCatApp *app, bool ok, const char *success, BongoCatError *error) {
    char message[1024];
    snprintf(message, sizeof(message), "%s%s%s", ok ? success : label(app, "failed", "Unable to update statistics"),
        ok ? "" : "\n", ok ? "" : error->message);
    bongo_cat_preferences_notice_show(app, message, !ok);
    bongo_cat_preferences_invalidate(app->preferences);
}
void bongo_cat_preferences_statistics_invalidate(BongoCatPreferences *value) {
    if (value && value->visible && value->page == 4) value->render_dirty = true;
}
static uint64_t clicks(const BongoCatStatisticsMetrics *metrics) {
    uint64_t total = 0;
    for (unsigned i = 0; i < BONGO_CAT_STATISTICS_BUTTONS; ++i) {
        if (UINT64_MAX - total < metrics->buttons[i]) return UINT64_MAX;
        total += metrics->buttons[i];
    }
    return total;
}
static void card(struct nk_context *context, const char *id, const char *title, const char *number) {
    BongoCatUIPalette p = bongo_cat_ui_palette(bongo_cat_ui_dark(context));
    struct nk_style_window saved = context->style.window;
    context->style.window.fixed_background = nk_style_item_color(p.surface);
    context->style.window.background = p.surface;
    context->style.window.group_border = 1;
    context->style.window.group_border_color = p.border_subtle;
    context->style.window.group_padding = nk_vec2(16, 12);
    if (nk_group_begin(context, id, NK_WINDOW_BORDER | NK_WINDOW_NO_SCROLLBAR)) {
        nk_layout_row_dynamic(context, 44, 1);
        nk_label_wrap(context, title);
        nk_layout_row_dynamic(context, 30, 1);
        nk_label_colored(context, number, NK_TEXT_LEFT, p.accent);
        nk_group_end(context);
    }
    context->style.window = saved;
}
static void plot(BongoCatApp *app, struct nk_context *context, const BongoCatStatisticsSnapshot *data, bool hourly) {
    bongo_cat_pref_section(context, label(app, hourly ? "hourly" : "daily",
        hourly ? "Today by hour" : "Last 30 days"));
    int count = hourly ? 24 : (int)BONGO_CAT_STATISTICS_DAYS;
    double maximum = 1.0;
    for (int i = 0; i < count; ++i) {
        const BongoCatStatisticsMetrics *m = hourly ? &data->hours[i] : &data->days[i].metrics;
        double activity = (double)m->keys + (double)clicks(m);
        if (activity > maximum) maximum = activity;
    }
    struct nk_rect bounds;
    nk_layout_row_dynamic(context, 160, 1);
    if (nk_widget(&bounds, context) == NK_WIDGET_INVALID) return;
    BongoCatUIPalette p = bongo_cat_ui_palette(bongo_cat_ui_dark(context));
    struct nk_command_buffer *canvas = nk_window_get_canvas(context);
    nk_fill_rect(canvas, bounds, 8, p.surface);
    float width = (bounds.w - 16) / count;
    float bottom = bounds.y + bounds.h - 28;
    float available = bounds.h - 44;
    const struct nk_user_font *font = context->style.font;
    for (int i = 0; i < count; ++i) {
        const BongoCatStatisticsMetrics *m = hourly ? &data->hours[i] : &data->days[i].metrics;
        double activity = (double)m->keys + (double)clicks(m);
        float height = (float)(activity / maximum) * available;
        struct nk_rect bar = nk_rect(bounds.x + 8 + i * width, bottom - height,
            NK_MAX(1.0f, width - 3), height);
        if (height > 0) nk_fill_rect(canvas, bar, 2, i == count - 1 ? p.pink : p.accent);
        char text[32];
        if (hourly) snprintf(text, sizeof(text), "%02d", i);
        else snprintf(text, sizeof(text), "%02u/%02u", data->days[i].date / 100 % 100, data->days[i].date % 100);
        if (i % (hourly ? 6 : 7) == 0)
            nk_draw_text(canvas, nk_rect(bar.x, bottom + 5, width * (hourly ? 4 : 6), 20),
                text, nk_strlen(text), font, nk_rgba(0, 0, 0, 0), p.muted);
        struct nk_rect hover = nk_rect(bar.x, bounds.y, width, bounds.h);
        if (nk_input_is_mouse_hovering_rect(&context->input, hover)) {
            char tooltip[256];
            snprintf(tooltip, sizeof(tooltip), "%s: %s %llu / %s %llu", text,
                label(app, "keys", "Key presses"), (unsigned long long)m->keys,
                label(app, "clicks", "Mouse clicks"), (unsigned long long)clicks(m));
            nk_tooltip(context, tooltip);
        }
    }
    nk_layout_row_dynamic(context, 42, 1);
    nk_label_wrap(context, label(app, "activity", "Bar height: key presses + mouse clicks"));
}
void bongo_cat_preferences_page_statistics(BongoCatPreferences *value, struct nk_context *context) {
    BongoCatApp *app = value->app;
    nk_layout_row_dynamic(context, 68, 1);
    nk_label_wrap(context, label(app, "privacy", "Local input totals, app names and durations. No typed text, key order, coordinates, window titles, URLs or screenshots."));
    BongoCatStatisticsSnapshot data;
    if (!bongo_cat_statistics_snapshot(app, &data)) {
        nk_layout_row_dynamic(context, 64, 1);
        nk_label_wrap(context, label(app, "inactive", "Install or enable the Statistics plugin on the Plugins page to start counting."));
        return;
    }
    if (data.storage_error) {
        nk_layout_row_dynamic(context, 48, 1);
        nk_label_wrap(context, label(app, "storageError", "Statistics could not be saved. Check disk space and data directory permissions."));
    }
    const char *periods[] = {label(app, "today", "Today"), label(app, "week", "7 days"),
        label(app, "month", "30 days"), label(app, "lifetime", "All time")};
    nk_layout_row_dynamic(context, 34, 4);
    for (int i = 0; i < 4; ++i)
        if (nk_option_label(context, periods[i], value->statistics_period == i)) value->statistics_period = i;
    const BongoCatStatisticsMetrics *metrics[] = {&data.today, &data.week, &data.month, &data.lifetime};
    const BongoCatStatisticsMetrics *m = metrics[NK_CLAMP(0, value->statistics_period, 3)];
    char keys[40], mouse[40], movement[48], scroll[48];
    snprintf(keys, sizeof(keys), "%llu", (unsigned long long)m->keys);
    snprintf(mouse, sizeof(mouse), "%llu", (unsigned long long)clicks(m));
    snprintf(movement, sizeof(movement), "%.0f", m->movement);
    snprintf(scroll, sizeof(scroll), "%.1f", m->scroll);
    int columns = nk_window_get_content_region(context).w >= 540 ? 2 : 1;
    nk_layout_row_dynamic(context, 116, columns);
    card(context, "stats-keys", label(app, "keys", "Key presses"), keys);
    card(context, "stats-clicks", label(app, "clicks", "Mouse clicks"), mouse);
    card(context, "stats-movement", label(app, "movement", "Pointer travel (input units)"), movement);
    card(context, "stats-scroll", label(app, "scroll", "Scroll (wheel units)"), scroll);
    static const char *button_keys[] = {"left", "right", "middle", "back", "forward"};
    static const char *button_names[] = {"Left button", "Right button", "Middle button", "Back button", "Forward button"};
    for (unsigned i = 0; i < BONGO_CAT_STATISTICS_BUTTONS; ++i) {
        char number[40]; snprintf(number, sizeof(number), "%llu", (unsigned long long)m->buttons[i]);
        bongo_cat_pref_status(context, button_keys[i], label(app, button_keys[i], button_names[i]), number);
    }
    nk_layout_row_dynamic(context, 96, 1);
    nk_label_wrap(context, label(app, "unitsHint", "Movement uses desktop pixels/points, or raw device units with Linux evdev. Wheel units vary by device; neither value is a physical distance."));
    bongo_cat_preferences_statistics_usage(value, context);
    plot(app, context, &data, false);
    plot(app, context, &data, true);
    nk_layout_row_dynamic(context, 36, 2);
    if (nk_button_label(context, label(app, "export", "Export CSV"))) {
        BongoCatError error = {0};
        bool ok = bongo_cat_statistics_export(app, &error);
        notice(app, ok, label(app, "exportQueued", "Export queued in the background"), &error);
    }
    if (nk_button_label(context, label(app, "openFolder", "Open data folder")))
        (void)bongo_cat_platform_open_directory(app->data_root);
}
void bongo_cat_preferences_statistics_plugin_card(BongoCatPreferences *value, struct nk_context *context) {
    BongoCatApp *app = value->app;
    BongoCatStatisticsInfo info;
    bongo_cat_statistics_info(app, &info);
    BongoCatUIPalette p = bongo_cat_ui_palette(bongo_cat_ui_dark(context));
    struct nk_style_window saved = context->style.window;
    context->style.window.fixed_background = nk_style_item_color(p.surface);
    context->style.window.background = p.surface;
    context->style.window.group_border = 1;
    context->style.window.group_border_color = p.border_subtle;
    context->style.window.group_padding = nk_vec2(18, 14);
    if (nk_group_begin(context, "statistics-plugin", NK_WINDOW_BORDER | NK_WINDOW_NO_SCROLLBAR)) {
        nk_layout_row_dynamic(context, 30, 1);
        nk_label(context, label(app, "title", "Statistics"), NK_TEXT_LEFT);
        nk_layout_row_dynamic(context, 25, 1);
        nk_label(context, tr(app, info.active ? "pages.preference.plugins.active" :
            !info.installed ? "pages.preference.plugins.missing" :
            !info.enabled ? "pages.preference.plugins.disabled" : "pages.preference.plugins.installed",
            info.active ? "In use" : !info.installed ? "Not installed" : !info.enabled ? "Disabled" : "Installed"), NK_TEXT_LEFT);
        nk_layout_row_dynamic(context, 76, 1);
        nk_label_wrap(context, label(app, "description", "Input counts, application time, daily/hourly charts, local storage and CSV export. Disabling keeps history."));
        nk_layout_row_dynamic(context, 30, 2);
        nk_label(context, tr(app, "pages.preference.plugins.enabled", "Enabled"), NK_TEXT_LEFT);
        bool enabled = info.active;
        if (bongo_cat_pref_control_toggle_available(context, "enable-statistics-plugin", &enabled, info.installed)) {
            BongoCatError error = {0};
            bool ok = bongo_cat_statistics_set_enabled(app, enabled, &error);
            notice(app, ok, tr(app, "pages.preference.plugins.saved", "Plugin settings updated"), &error);
        }
        if (info.managed) {
            nk_layout_row_dynamic(context, 30, 1);
            if (nk_button_label(context, tr(app, "pages.preference.plugins.remove", "Remove plugin"))) {
                BongoCatError error = {0};
                bool ok = bongo_cat_statistics_remove(app, &error);
                notice(app, ok, tr(app, "pages.preference.plugins.saved", "Plugin settings updated"), &error);
            }
        }
        nk_group_end(context);
    }
    context->style.window = saved;
}
