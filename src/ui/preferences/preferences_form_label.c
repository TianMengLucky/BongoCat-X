#include "preferences_widgets.h"
#include "preferences_widgets_internal.h"
#include "ui_backend.h"
#include "ui_catime.h"
#include "ui_tooltip.h"

static struct nk_context *icon_context;
static BongoCatPrefIcon pending_icon;
static bool icon_pending;

void bongo_cat_pref_row_icon(struct nk_context *context,
    BongoCatPrefIcon icon) {
    icon_context = context;
    pending_icon = icon;
    icon_pending = true;
}

void bongo_cat_pref_row_icon_clear(struct nk_context *context) {
    if (icon_context != context) return;
    icon_context = NULL;
    icon_pending = false;
}

void bongo_cat_pref_form_title_sized(struct nk_context *context,
    const char *title, float control_width) {
    float available = nk_window_get_content_region(context).w;
    float right = NK_MIN(control_width, available * .5f);
    float left = NK_MAX(1.0f, available - right - context->style.window.spacing.x);
    nk_layout_row_begin(context, NK_STATIC, 36, 2);
    nk_layout_row_push(context, left);
    bongo_cat_pref_form_label(context, title);
    nk_layout_row_push(context, right);
}

/* Measure and draw with the same glyph advances so translated titles wrap consistently. */
int bongo_cat_pref_title_text(const struct nk_context *context, const char *title,
    struct nk_rect bounds, bool draw) {
    const struct nk_user_font *font = bongo_cat_ui_label_font(context);
    struct nk_color color = bongo_cat_ui_palette(bongo_cat_ui_dark(context)).text;
    float x = 0.0f;
    int line = 0, remaining = nk_strlen(title);
    while (remaining > 0) {
        nk_rune rune = 0;
        int length = nk_utf_decode(title, &rune, remaining);
        if (length <= 0) length = 1;
        float width = font->width(font->userdata, font->height, title, length);
        if (rune == '\n' || (x > 0 && x + width > bounds.w)) {
            x = 0.0f;
            ++line;
        }
        if (rune != '\n') {
            if (draw && (line + 1) * font->height <= bounds.h)
                nk_draw_text(nk_window_get_canvas((struct nk_context *)context),
                    nk_rect(bounds.x + x, bounds.y + line * font->height,
                        NK_MIN(width + 1.0f, bounds.w - x), font->height),
                    title, length, font, nk_rgba(0, 0, 0, 0), color);
            x += width;
        }
        title += length;
        remaining -= length;
    }
    return line + 1;
}

void bongo_cat_pref_form_label(struct nk_context *context, const char *title) {
    struct nk_rect bounds;
    enum nk_widget_layout_states state = nk_widget(&bounds, context);
    bool icon = icon_pending && icon_context == context;
    BongoCatPrefIcon row_icon = pending_icon;
    bongo_cat_pref_row_icon_clear(context);
    if (state == NK_WIDGET_INVALID) return;
    if (icon) {
        BongoCatUIPalette p = bongo_cat_ui_palette(bongo_cat_ui_dark(context));
        bongo_cat_pref_icon_draw(nk_window_get_canvas(context),
            nk_rect(bounds.x + 5, bounds.y + (bounds.h - 18) * .5f, 18, 18),
            row_icon, p.accent);
    }
    float inset = icon ? 33.0f : 5.0f;
    bounds.x += inset;
    bounds.w = NK_MAX(1.0f, bounds.w - inset);
    const struct nk_user_font *font = bongo_cat_ui_label_font(context);
    int lines = bongo_cat_pref_title_text(context, title, bounds, false);
    bounds.y += NK_MAX(0.0f, (bounds.h - lines * font->height) * .5f);
    bongo_cat_pref_title_text(context, title, bounds, true);
    if (lines * font->height > bounds.h)
        bongo_cat_ui_text_tooltip(context, title, bounds);
}
