#include "preferences_notice.h"
#include "preferences_state.h"
#include "preferences_model_glyphs.h"
#include "ui_animation.h"
#include "ui_backend.h"
#include "ui_catime.h"
#include "ui_paint.h"
#include "update_service.h"
#include "bongo_cat/i18n.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

enum { NOTICE_DURATION_MS = 2600, NOTICE_ENTER_MS = 250, NOTICE_EXIT_MS = 160 };

static void update_notice_timer(BongoCatPreferences *value,
    BongoCatPreferenceNotice *notice, uint64_t now) {
    if (!notice->message[0]) return;
    bool hovered = SDL_GetMouseFocus() == value->window &&
        nk_input_is_mouse_hovering_rect(&value->ui.context.input, notice->bounds);
    if (!notice->update_action && (notice->hovered || hovered))
        notice->until_ns += now - notice->timer_updated_ns;
    notice->timer_updated_ns = now;
    notice->hovered = hovered;
}

void bongo_cat_preferences_notice_show_anchored(BongoCatApp *app,
    const char *message, bool error, unsigned duration_ms,
    bool bottom_right) {
    if (!app || !app->preferences || !message || !message[0]) return;
    BongoCatPreferences *value = app->preferences;
    uint64_t now = SDL_GetTicksNS();
    BongoCatPreferenceNotice *target = &value->notices[0];
    for (size_t i = 0; i < sizeof(value->notices) / sizeof(value->notices[0]); ++i) {
        BongoCatPreferenceNotice *notice = &value->notices[i];
        update_notice_timer(value, notice, now);
        if (!notice->message[0] || (notice->closing_ns &&
            now - notice->closing_ns >= NOTICE_EXIT_MS * 1000000ULL)) {
            target = notice; break;
        }
        if (notice->update_action && !notice->closing_ns) continue;
        if (target->update_action || (target->hovered && !notice->hovered) ||
            (notice->hovered == target->hovered &&
                notice->started_ns < target->started_ns)) target = notice;
    }
    memset(target, 0, sizeof(*target));
    SDL_utf8strlcpy(target->message, message, sizeof(target->message));
    if (!bongo_cat_preferences_model_glyphs_ready(value, target->message)) {
        value->font_reload_pending = true;
        value->font_reload_defer_once = false;
    }
    target->error = error;
    target->bottom_right = bottom_right;
    target->started_ns = now;
    target->until_ns = now + (uint64_t)duration_ms * 1000000ULL;
    target->timer_updated_ns = now;
    value->render_dirty = true;
}

void bongo_cat_preferences_notice_show(BongoCatApp *app,
    const char *message, bool error) {
    bongo_cat_preferences_notice_show_anchored(app, message, error,
        NOTICE_DURATION_MS, false);
}

void bongo_cat_preferences_notice_show_update(BongoCatApp *app, const char *message) {
    if (!app || !app->preferences || !message || !message[0]) return;
    BongoCatPreferences *value = app->preferences;
    for (size_t i = 0; i < 4; ++i)
        if (value->notices[i].update_action)
            memset(&value->notices[i], 0, sizeof(value->notices[i]));
    bongo_cat_preferences_notice_show(app, message, false);
    for (size_t i = 0; i < 4; ++i) {
        BongoCatPreferenceNotice *notice = &value->notices[i];
        if (!strcmp(notice->message, message)) {
            notice->top_right = true;
            notice->update_action = true;
            notice->until_ns = UINT64_MAX;
            break;
        }
    }
}

static bool inside(struct nk_rect bounds, float x, float y) {
    return bounds.w > 0 && bounds.h > 0 && x >= bounds.x && y >= bounds.y &&
        x < bounds.x + bounds.w && y < bounds.y + bounds.h;
}

bool bongo_cat_preferences_notice_event(BongoCatPreferences *value,
    const SDL_Event *event) {
    if (!value || !event || (event->type != SDL_EVENT_MOUSE_BUTTON_DOWN &&
            event->type != SDL_EVENT_MOUSE_BUTTON_UP)) return false;
    float scale = value->ui.layout_scale > 0 ? value->ui.layout_scale : 1;
    float x = event->button.x / scale, y = event->button.y / scale;
    bool release = event->type == SDL_EVENT_MOUSE_BUTTON_UP;
    bool captured = value->notice_pointer_captured;
    if (release) value->notice_pointer_captured = false;
    for (size_t i = 4; i > 0; --i) {
        BongoCatPreferenceNotice *notice = &value->notices[i - 1];
        if (!notice->message[0] || !inside(notice->bounds, x, y)) continue;
        if (!release) value->notice_pointer_captured = true;
        if (release && captured && event->button.button == SDL_BUTTON_LEFT &&
                !notice->closing_ns) {
            if (inside(notice->close_bounds, x, y))
                notice->closing_ns = SDL_GetTicksNS();
            else if (notice->update_action && inside(notice->action_bounds, x, y)) {
                if (bongo_cat_update_open(value->app->update))
                    notice->closing_ns = SDL_GetTicksNS();
                else bongo_cat_preferences_notice_show(value->app,
                    bongo_cat_i18n_get(value->app->i18n,
                        "native.support.downloadUpdateFailed", "Unable to download the update"), true);
            }
        }
        value->render_dirty = true;
        return true;
    }
    return captured;
}

void bongo_cat_preferences_notice_clear(BongoCatApp *app) {
    if (!app || !app->preferences) return;
    BongoCatPreferences *value = app->preferences;
    bool cleared = false;
    for (size_t i = 0; i < sizeof(value->notices) / sizeof(value->notices[0]);
        ++i) {
        if (value->notices[i].message[0]) {
            memset(&value->notices[i], 0, sizeof(value->notices[i]));
            cleared = true;
        }
    }
    if (cleared) value->render_dirty = true;
}

static size_t active_notices(BongoCatPreferences *value, uint64_t now,
    BongoCatPreferenceNotice **items) {
    size_t count = 0;
    for (size_t i = 0; i < sizeof(value->notices) / sizeof(value->notices[0]); ++i) {
        BongoCatPreferenceNotice *notice = &value->notices[i];
        update_notice_timer(value, notice, now);
        if (notice->message[0]) {
            if (!notice->closing_ns && notice->until_ns <= now) {
                notice->closing_ns = now;
                value->render_dirty = true;
            } else if (notice->closing_ns &&
                now - notice->closing_ns >= NOTICE_EXIT_MS * 1000000ULL) {
                memset(notice, 0, sizeof(*notice));
                value->render_dirty = true;
            }
        }
        if (notice->message[0]) items[count++] = notice;
    }
    for (size_t i = 1; i < count; ++i) {
        BongoCatPreferenceNotice *item = items[i];
        size_t j = i;
        while (j && items[j - 1]->started_ns > item->started_ns) {
            items[j] = items[j - 1]; j--;
        }
        items[j] = item;
    }
    return count;
}

static int notice_line(const struct nk_user_font *font, const char *text,
    float maximum, float *width) {
    int length = 0, remaining = nk_strlen(text);
    *width = 0;
    while (length < remaining && text[length] != '\n') {
        nk_rune rune;
        int bytes = nk_utf_decode(text + length, &rune, remaining - length);
        if (bytes <= 0) bytes = 1;
        float next_width = font->width(font->userdata, font->height,
            text, length + bytes);
        if (length && next_width > maximum) break;
        length += bytes;
        *width = next_width;
    }
    return length;
}

/* Keep toasts within a fraction of the window so long messages wrap into a
   compact block instead of covering the options underneath. */
static float notice_toast_cap(float width) {
    return NK_MAX(120.0f, NK_MIN(width - 64.0f, width * .58f));
}

static float notice_text_maximum(float width) {
    return NK_MAX(40.0f, notice_toast_cap(width) - 64.0f);
}

static float notice_height(struct nk_context *context,
    BongoCatPreferenceNotice *notice, float width, float *text_width_out,
    size_t *lines_out) {
    const struct nk_user_font *font = bongo_cat_ui_label_font(context);
    float maximum = notice_text_maximum(width);
    float text_width = 0;
    size_t lines = 0;
    for (const char *cursor = notice->message; *cursor; ++lines) {
        float line_width;
        cursor += notice_line(font, cursor, maximum, &line_width);
        text_width = NK_MAX(text_width, line_width);
        if (*cursor == '\n') cursor++;
    }
    float line_height = font->height + 5.0f;
    if (text_width_out) *text_width_out = text_width;
    if (lines_out) *lines_out = lines;
    return NK_MAX(41.0f, lines * line_height + 15.0f) +
        (notice->update_action ? 38.0f : 0.0f);
}

static float draw_notice(BongoCatPreferences *value,
    struct nk_context *context, BongoCatPreferenceNotice *notice,
    float width, float y, uint64_t now) {
    BongoCatUIPalette p = bongo_cat_ui_palette(
        bongo_cat_ui_dark(context));
    const struct nk_user_font *font = bongo_cat_ui_label_font(context);
    float text_width = 0;
    size_t lines = 0;
    float toast_height = notice_height(context, notice, width, &text_width,
        &lines);
    float line_height = font->height + 5.0f;
    float maximum = notice_text_maximum(width);
    const char *action = bongo_cat_i18n_get(value->app->i18n,
        "native.support.updateNow", "Update now");
    float action_width = notice->update_action ? font->width(font->userdata,
        font->height, action, (int)strlen(action)) + 24.0f : 0;
    float toast_width = NK_MIN(NK_MAX(text_width + 64.0f, action_width + 64.0f),
        notice_toast_cap(width));
    float elapsed = (float)(now - notice->started_ns) /
        (NOTICE_ENTER_MS * 1000000.0f);
    float progress = bongo_cat_ui_ease(BONGO_CAT_UI_EASE_SPRING,
        NK_CLAMP(0.0f, elapsed, 1.0f));
    float opacity = NK_CLAMP(0.0f, progress, 1.0f);
    if (notice->closing_ns) {
        /* Expired toasts ease out toward their entry edge instead of
           vanishing, with the fade finished NOTICE_EXIT_MS after expiry. */
        opacity *= 1.0f - bongo_cat_ui_ease(BONGO_CAT_UI_EASE_STANDARD,
            NK_CLAMP(0.0f, (float)(now - notice->closing_ns) /
                (NOTICE_EXIT_MS * 1000000.0f), 1.0f));
    }
    float shown_width = toast_width * (.9f + .1f * progress);
    float slide = 12.0f * (1.0f - progress) + 8.0f * (1.0f - opacity);
    struct nk_rect bounds = nk_rect(
        (notice->bottom_right || notice->top_right) ? (width - shown_width - 20.0f)
                             : (width - shown_width) * .5f,
        notice->bottom_right ? (y + slide) : (y - slide),
        shown_width, toast_height);
    notice->bounds = bounds;
    struct nk_color tone = notice->error ? p.danger : p.pink;
    if (p.effects) bongo_cat_ui_paint_shadow(context, bounds, 20, 0, 8,
        notice->error ? 25.0f : 22.0f, 0, nk_rgba(tone.r, tone.g, tone.b,
        (nk_byte)((notice->error ? 102 : 89) * opacity)));
    struct nk_command_buffer *canvas = nk_window_get_canvas(context);
    nk_fill_rect(canvas, bounds, 20, nk_rgba(tone.r, tone.g, tone.b,
        (nk_byte)(255 * opacity)));
    struct nk_color white = nk_rgba(255, 255, 255, (nk_byte)(255 * opacity));
    notice->close_bounds = nk_rect(bounds.x + bounds.w - 31, bounds.y + 7, 24, 24);
    struct nk_rect close = notice->close_bounds;
    nk_stroke_line(canvas, close.x + 8, close.y + 8, close.x + 16, close.y + 16, 1.5f, white);
    nk_stroke_line(canvas, close.x + 16, close.y + 8, close.x + 8, close.y + 16, 1.5f, white);
    if (nk_input_is_mouse_hovering_rect(&context->input, close))
        bongo_cat_ui_cursor_hover_rect(context, close, BONGO_CAT_UI_CURSOR_POINTER);
    float label_y = bounds.y + 10;
    if (notice->update_action) {
        notice->action_bounds = nk_rect(bounds.x + 16, bounds.y + bounds.h - 36,
            bounds.w - 48, 28);
        bool hover = nk_input_is_mouse_hovering_rect(&context->input, notice->action_bounds);
        nk_fill_rect(canvas, notice->action_bounds, 8,
            nk_rgba(255, 255, 255, (nk_byte)((hover ? 65 : 35) * opacity)));
        nk_draw_text(canvas, nk_rect(notice->action_bounds.x + 8,
            notice->action_bounds.y + 4, notice->action_bounds.w - 16, font->height),
            action, (int)strlen(action), font, nk_rgba(0, 0, 0, 0), white);
        if (hover) bongo_cat_ui_cursor_hover_rect(context, notice->action_bounds,
            BONGO_CAT_UI_CURSOR_POINTER);
    }
    for (const char *cursor = notice->message; *cursor; label_y += line_height) {
        float line_width;
        int length = notice_line(font, cursor, maximum, &line_width);
        struct nk_rect label = nk_rect(bounds.x + 16,
            label_y, NK_MIN(text_width + 1, bounds.w - 48), font->height);
        nk_draw_text(canvas, label, cursor, length, font,
            nk_rgba(0, 0, 0, 0), nk_rgba(255, 255, 255, (nk_byte)(255 * opacity)));
        cursor += length;
        if (*cursor == '\n') cursor++;
    }
    if (!notice->update_action || notice->closing_ns ||
        now - notice->started_ns < NOTICE_ENTER_MS * 1000000ULL)
        value->render_dirty = true;
    return toast_height;
}

void bongo_cat_preferences_notice_draw(BongoCatPreferences *value,
    struct nk_context *context, float width, float height) {
    if (!value) return;
    uint64_t now = SDL_GetTicksNS();
    BongoCatPreferenceNotice *items[4];
    BongoCatPreferenceNotice *bottom[sizeof(items) / sizeof(items[0])];
    size_t count = active_notices(value, now, items);
    size_t bottom_count = 0;
    float y = 20.0f;
    for (size_t i = 0; i < count; ++i) {
        if (items[i]->bottom_right) {
            bottom[bottom_count++] = items[i];
            continue;
        }
        y += draw_notice(value, context, items[i], width, y, now) + 10.0f;
    }
    if (!bottom_count) return;
    float total = 0.0f;
    for (size_t i = 0; i < bottom_count; ++i)
        total += notice_height(context, bottom[i], width, NULL, NULL) + 10.0f;
    y = height - 20.0f - (total - 10.0f);
    for (size_t i = 0; i < bottom_count; ++i)
        y += draw_notice(value, context, bottom[i], width, y, now) + 10.0f;
}
