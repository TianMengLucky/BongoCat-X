#include "preferences_state.h"
#include "preferences_overlay.h"
#include "preferences_controls.h"
#include "preferences_widgets.h"
#include "runtime.h"
#include "ui_backend.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

static const char *tr(BongoCatPreferences *value, const char *key,
    const char *fallback) {
    return bongo_cat_i18n_get(value->app->i18n, key, fallback);
}

static struct nk_color alpha(struct nk_color color, float amount) {
    return bongo_cat_preferences_overlay_alpha(color, amount);
}

static bool hit(struct nk_context *context, struct nk_rect bounds, bool enabled) {
    return enabled && nk_input_is_mouse_hovering_rect(&context->input, bounds) &&
        nk_input_is_mouse_click_in_rect(&context->input, NK_BUTTON_LEFT, bounds);
}

static bool candidate(BongoCatApp *app, const BongoCatBehaviorEntry *entry,
    BongoCatBehaviorKind kind) {
    return entry->kind == kind && (kind != BONGO_CAT_BEHAVIOR_MOTION ||
        !app->live2d ||
        bongo_cat_live2d_motion_visible(app->live2d, entry->group, entry->index));
}

static size_t candidate_count(BongoCatApp *app, BongoCatBehaviorKind kind) {
    size_t count = 0;
    for (size_t i = 0; i < app->behaviors.count; ++i)
        if (candidate(app, &app->behaviors.entries[i], kind)) count++;
    return count;
}

static size_t model_candidate_count(BongoCatApp *app) {
    size_t count = 0;
    for (size_t i = 0; i < app->models.count; ++i)
        if (!bongo_cat_settings_model_hidden(&app->settings,
            app->models.entries[i].id)) count++;
    return count;
}

bool bongo_cat_preferences_random_dialog_active(
    const BongoCatPreferences *value) {
    return value && value->random_dialog;
}

void bongo_cat_preferences_sequential_model_dialog_open(
    BongoCatPreferences *value) {
    if (!value) return;
    bongo_cat_preferences_shortcut_cancel(value);
    value->sequential_model_dialog = true;
    value->random_dialog_scroll = 0;
    bongo_cat_preferences_scrollbar_reset(&value->random_dialog_scrollbar);
    value->random_dialog = true;
    value->random_dialog_input_armed = false;
    value->random_dialog_opened_ns = SDL_GetTicksNS();
    value->random_dialog_closing_ns = 0;
    value->render_dirty = true;
}

void bongo_cat_preferences_random_dialog_open(BongoCatPreferences *value,
    BongoCatBehaviorKind kind) {
    if (!value) return;
    bongo_cat_preferences_shortcut_cancel(value);
    value->random_dialog_kind = kind;
    value->random_dialog_scroll = 0;
    bongo_cat_preferences_scrollbar_reset(&value->random_dialog_scrollbar);
    value->random_dialog = true;
    value->random_dialog_input_armed = false;
    value->random_dialog_opened_ns = SDL_GetTicksNS();
    value->random_dialog_closing_ns = 0;
    value->render_dirty = true;
}

void bongo_cat_preferences_random_dialog_close(BongoCatPreferences *value) {
    if (!value || !value->random_dialog || value->random_dialog_closing_ns) return;
    /* Close with the shared overlay fade: input is dropped immediately, and
       the 180 ms eased scale-and-slide keeps the tall panel responsive
       instead of reading as lag. */
    value->random_dialog_closing_ns = SDL_GetTicksNS();
    value->random_dialog_input_armed = false;
    value->render_dirty = true;
}

static const char *dialog_title(BongoCatPreferences *value) {
    if (value->sequential_model_dialog)
        return tr(value, "pages.preference.cat.labels.sequentialModel",
            "Sequential Models");
    return value->random_dialog_kind == BONGO_CAT_BEHAVIOR_MOTION ?
        tr(value, "pages.preference.cat.labels.randomMotion",
        "Random Motions") :
        tr(value, "pages.preference.cat.labels.randomExpression",
        "Random Expressions");
}

static bool draw_header(BongoCatPreferences *value, struct nk_context *context,
    struct nk_command_buffer *canvas, struct nk_rect panel,
    BongoCatUIPalette p, float opacity, bool enabled) {
    const char *title = dialog_title(value);
    struct nk_rect bounds = nk_rect(panel.x + 20, panel.y + 21, panel.w - 74, 24);
    nk_draw_text(canvas, bounds, title, nk_strlen(title), value->ui.label_font,
        nk_rgba(0, 0, 0, 0), alpha(nk_rgb(247, 125, 170), opacity));
    struct nk_rect close = nk_rect(panel.x + panel.w - 52, panel.y + 17, 32, 32);
    return bongo_cat_ui_close_button(context, canvas, close,
        alpha(p.muted, opacity), alpha(p.accent, opacity), enabled);
}

/* Moves a catalog entry and rewrites the persisted drag order to the full
   catalog sequence, so rescans reproduce it until the user reorders again.
   The sequential model switch walks this same order. */
static void model_order_move(BongoCatApp *app, size_t source, size_t target) {
    BongoCatModelEntry entry = app->models.entries[source];
    if (source < target)
        memmove(&app->models.entries[source],
            &app->models.entries[source + 1],
            (target - source) * sizeof(entry));
    else
        memmove(&app->models.entries[target + 1],
            &app->models.entries[target],
            (source - target) * sizeof(entry));
    app->models.entries[target] = entry;
    const char *ids[BONGO_CAT_MODEL_CAP];
    size_t count = app->models.count;
    if (count > BONGO_CAT_MODEL_CAP) count = BONGO_CAT_MODEL_CAP;
    for (size_t i = 0; i < count; ++i) ids[i] = app->models.entries[i].id;
    bongo_cat_settings_model_order_set(&app->settings, ids, count);
}

static void draw_rows(BongoCatPreferences *value, struct nk_context *context,
    struct nk_command_buffer *canvas, struct nk_rect panel,
    BongoCatUIPalette p, float opacity, bool enabled, size_t count) {
    struct nk_rect viewport = nk_rect(panel.x + 20, panel.y + 72,
        panel.w - 40, panel.h - 92);
    float content_height = count * 56.0f;
    float maximum = NK_MAX(0.0f, content_height - viewport.h);
    BongoCatPreferencesScrollbarResult scroll =
        bongo_cat_preferences_scrollbar_draw(context, canvas, viewport,
            content_height, value->random_dialog_scroll,
            &value->random_dialog_scrollbar, p.accent, opacity, enabled);
    if (scroll.changed) {
        value->random_dialog_scroll = scroll.offset;
        value->render_dirty = true;
    }
    float offset = NK_CLAMP(0.0f, scroll.offset, maximum);
    float row_width = bongo_cat_preferences_scrollbar_content_width(
        viewport, content_height);
    nk_push_scissor(canvas, viewport);
    size_t shown = 0;
    BongoCatApp *app = value->app;
    if (value->sequential_model_dialog) {
        struct nk_input *input = &context->input;
        bool left_down = input->mouse.buttons[NK_BUTTON_LEFT].down;
        struct nk_rect drag_rows[BONGO_CAT_MODEL_CAP];
        size_t drag_indices[BONGO_CAT_MODEL_CAP];
        size_t drag_count = 0;
        for (size_t i = 0; i < app->models.count; ++i) {
            const BongoCatModelEntry *entry = &app->models.entries[i];
            if (bongo_cat_settings_model_hidden(&app->settings, entry->id))
                continue;
            struct nk_rect row = nk_rect(viewport.x,
                viewport.y + shown++ * 56.0f - offset, row_width, 56);
            if (row.y + row.h < viewport.y || row.y > viewport.y + viewport.h)
                continue;
            struct nk_rect toggle = nk_rect(row.x + row.w - 78, row.y + 10, 80, 36);
            struct nk_rect name = nk_rect(row.x + 8, row.y + 9,
                NK_MAX(48.0f, toggle.x - row.x - 16), 38);
            /* The row body (name area) drags to reorder; the toggle
               keeps its click. */
            bool press_here = left_down && enabled &&
                value->model_press_index < 0 &&
                nk_input_is_mouse_hovering_rect(input, name);
            if (enabled && nk_input_is_mouse_hovering_rect(input, name)) {
                bongo_cat_ui_cursor_hover_rect(context, name,
                    BONGO_CAT_UI_CURSOR_POINTER);
                if (press_here) {
                    value->model_press_index = (int)i;
                    value->model_press_point = input->mouse.pos;
                }
            }
            if (left_down && value->model_press_index == (int)i) {
                float dx = input->mouse.pos.x - value->model_press_point.x;
                float dy = input->mouse.pos.y - value->model_press_point.y;
                if (value->model_drag_source < 0 && dx * dx + dy * dy > 64.0f)
                    value->model_drag_source = (int)i;
            }
            bool dragging = value->model_drag_source == (int)i;
            if (dragging) {
                nk_fill_rect(canvas, row, 12, alpha(p.hover_pink, opacity));
                value->render_dirty = true;
            }
            if (drag_count < BONGO_CAT_MODEL_CAP) {
                drag_indices[drag_count] = i;
                drag_rows[drag_count] = row;
                drag_count++;
            }
            const char *label = entry->display_name[0] ?
                entry->display_name : entry->id;
            nk_draw_text(canvas, name, label, nk_strlen(label),
                value->ui.caption_font, nk_rgba(0, 0, 0, 0),
                alpha(p.text, opacity));
            char switch_id[BONGO_CAT_BEHAVIOR_ID_CAP];
            snprintf(switch_id, sizeof(switch_id), "switch:%.*s",
                (int)sizeof(switch_id) - 8, entry->id);
            char id[BONGO_CAT_BEHAVIOR_ID_CAP + 16];
            snprintf(id, sizeof(id), "random-model-%.*s",
                (int)sizeof(id) - 16, entry->id);
            bool state = bongo_cat_settings_random_enabled(&app->settings,
                switch_id);
            if (bongo_cat_pref_control_toggle_rect(context, id, &state,
                    toggle, enabled) &&
                bongo_cat_settings_random_set_enabled(&app->settings,
                    switch_id, state))
                value->render_dirty = true;
        }
        /* Drop the dragged row onto the hovered row's position. */
        if (!left_down) {
            value->model_press_index = -1;
            value->model_drag_source = -1;
        } else {
            int source = value->model_drag_source;
            if (source >= 0) {
                if ((size_t)source >= app->models.count) {
                    value->model_press_index = -1;
                    value->model_drag_source = -1;
                } else {
                    int target = -1;
                    for (size_t r = 0; r < drag_count; ++r) {
                        if ((int)drag_indices[r] == source) continue;
                        if (nk_input_is_mouse_hovering_rect(input,
                                drag_rows[r])) {
                            target = (int)drag_indices[r];
                            break;
                        }
                    }
                    if (target >= 0 && target != source) {
                        model_order_move(app, (size_t)source,
                            (size_t)target);
                        if (value->model_press_index == source)
                            value->model_press_index = target;
                        value->model_drag_source = target;
                        value->render_dirty = true;
                    }
                }
            }
        }
        nk_push_scissor(canvas, nk_window_get_content_region(context));
        return;
    }
    for (size_t i = 0; i < app->behaviors.count; ++i) {
        const BongoCatBehaviorEntry *entry = &app->behaviors.entries[i];
        if (!candidate(app, entry, value->random_dialog_kind)) continue;
        struct nk_rect row = nk_rect(viewport.x,
            viewport.y + shown++ * 56.0f - offset, row_width, 56);
        if (row.y + row.h < viewport.y || row.y > viewport.y + viewport.h)
            continue;
        struct nk_rect toggle = nk_rect(row.x + row.w - 78, row.y + 10, 80, 36);
        struct nk_rect name = nk_rect(row.x + 8, row.y + 9,
            NK_MAX(48.0f, toggle.x - row.x - 16), 38);
        const char *label = bongo_cat_app_behavior_label(app, entry->id);
        nk_draw_text(canvas, name, label && label[0] ? label : entry->label,
            nk_strlen(label && label[0] ? label : entry->label),
            value->ui.caption_font, nk_rgba(0, 0, 0, 0),
            alpha(p.text, opacity));
        bool state = bongo_cat_settings_random_enabled(&app->settings,
            entry->id);
        char id[BONGO_CAT_BEHAVIOR_ID_CAP + 16];
        snprintf(id, sizeof(id), "random-%.*s", (int)sizeof(id) - 8,
            entry->id);
        if (bongo_cat_pref_control_toggle_rect(context, id, &state, toggle,
                enabled) &&
            bongo_cat_settings_random_set_enabled(&app->settings, entry->id,
                state))
            value->render_dirty = true;
    }
    nk_push_scissor(canvas, nk_window_get_content_region(context));
}

void bongo_cat_preferences_random_dialog_draw(
    BongoCatPreferences *value, struct nk_context *context) {
    if (!bongo_cat_preferences_random_dialog_active(value)) return;
    bongo_cat_ui_cursor_reset(context);
    struct nk_rect region = nk_window_get_bounds(context);
    size_t count = value->sequential_model_dialog ?
        model_candidate_count(value->app) :
        candidate_count(value->app, value->random_dialog_kind);
    float width = NK_MIN(540.0f, region.w - 48.0f);
    float height = NK_MIN(92.0f + count * 56.0f, region.h - 48.0f);
    BongoCatOverlayFrame frame = bongo_cat_preferences_overlay_frame(
        region, width, height, value->random_dialog_opened_ns,
        value->random_dialog_closing_ns);
    if (frame.finished) {
        value->random_dialog = false;
        value->sequential_model_dialog = false;
        value->random_dialog_opened_ns = value->random_dialog_closing_ns = 0;
        value->random_dialog_scroll = 0;
        value->model_press_index = -1;
        value->model_drag_source = -1;
        bongo_cat_preferences_scrollbar_reset(&value->random_dialog_scrollbar);
        value->render_dirty = true;
        return;
    }
    BongoCatUIPalette p = bongo_cat_ui_palette(bongo_cat_ui_dark(context));
    bongo_cat_preferences_overlay_draw(context, region, &frame, p);
    bool closing = value->random_dialog_closing_ns != 0;
    bool input_ready = bongo_cat_preferences_overlay_input_ready(context,
        &value->random_dialog_input_armed);
    float opacity = closing ? frame.visibility : 1.0f;
    struct nk_command_buffer *canvas = nk_window_get_canvas(context);
    nk_fill_rect(canvas, frame.panel, 18, alpha(p.surface, opacity));
    nk_stroke_rect(canvas, frame.panel, 18, 1, alpha(p.border, opacity));
    bool close = draw_header(value, context, canvas, frame.panel, p,
        opacity, !closing && input_ready);
    draw_rows(value, context, canvas, frame.panel, p, opacity,
        !closing && input_ready, count);
    bool outside = hit(context, region, !closing && input_ready) &&
        !nk_input_is_mouse_hovering_rect(&context->input, frame.panel);
    if (close || outside) bongo_cat_preferences_random_dialog_close(value);
    if (frame.visibility < 1.0f || closing) value->render_dirty = true;
}

void bongo_cat_preferences_sequential_model_pref_row(
    BongoCatPreferences *value, struct nk_context *context) {
    BongoCatApp *app = value->app;
    bongo_cat_pref_row_icon(context, BONGO_CAT_PREF_ICON_MULTIPLE_MODELS);
    if (bongo_cat_pref_toggle_float_config(context, "random-model", tr(value,
        "pages.preference.cat.labels.sequentialModel", "Sequential Models"),
        tr(value, "pages.preference.cat.labels.secondsUnit", "s"),
        &app->settings.window.sequential_model, 1.0f,
        &app->settings.window.sequential_model_interval_seconds, 3600.0f, 1.0f,
        BONGO_CAT_DEFAULT_SEQUENTIAL_MODEL_SECONDS))
        bongo_cat_preferences_sequential_model_dialog_open(value);
}
