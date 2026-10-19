#include "preferences_state.h"
#include "preferences_overlay.h"
#include "preferences_controls.h"
#include "preferences_widgets.h"
#include "preferences_notice.h"
#include "runtime.h"
#include "ui_animation.h"
#include "ui_backend.h"
#include "ui_paint.h"
#include "bongo_cat/app.h"
#include "bongo_cat/hand_layout.h"
#include "bongo_cat/overlay.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

/* Crop regions of the active side's hand-layout.json. The panel lays out
   twelve pixel fields; edits are written back after the drag settles so a
   held stepper does not rewrite the file on every frame. */
#define LAYOUT_DIALOG_WIDTH 540.0f
#define LAYOUT_DIALOG_WRITE_NS 250000000ull
#define LAYOUT_ROW_HEIGHT 44.0f
#define LAYOUT_SECTION_HEIGHT 26.0f
#define LAYOUT_HEADER_HEIGHT 64.0f /* Title and top padding. */
#define LAYOUT_TABS_HEIGHT 48.0f /* Side selector and spacing. */
#define LAYOUT_FOOTER_HEIGHT 76.0f /* Reset button and vertical padding. */
#define LAYOUT_FIELDS_HEIGHT (3 * (LAYOUT_SECTION_HEIGHT + 2 * LAYOUT_ROW_HEIGHT) + \
    LAYOUT_ROW_HEIGHT) /* Three rectangles followed by gap and key count. */

typedef struct LayoutField {
    const char *label;
    const char *id;
    int *pointer;
    int minimum;
    int maximum;
    int step;
    int fallback;
} LayoutField;

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

static void centered(struct nk_command_buffer *canvas, struct nk_rect bounds,
    const char *text, const struct nk_user_font *font, struct nk_color color) {
    float width = font->width(font->userdata, font->height, text, nk_strlen(text));
    nk_draw_text(canvas, nk_rect(bounds.x + (bounds.w - width) * .5f,
        bounds.y + (bounds.h - font->height) * .5f, width + 1, font->height),
        text, nk_strlen(text), font, nk_rgba(0, 0, 0, 0), color);
}

static const char *overlay_directory(const BongoCatPreferences *value) {
    if (!value->app || !value->app->overlay) return "";
    BongoCatOverlayInputDiagnostics diagnostics =
        bongo_cat_overlay_input_diagnostics(value->app->overlay);
    return diagnostics.directory ? diagnostics.directory : "";
}

/* The left key folder is always editable; the right one only once it carries
   its own file, since editing a mirrored side would flip its origin. */
static bool side_editable(const BongoCatPreferences *value, bool right) {
    return value->app && value->app->overlay &&
        bongo_cat_overlay_multi_key_layout_editable(value->app->overlay, right);
}

static void reset_state(BongoCatPreferences *value) {
    value->layout_dialog = false;
    value->layout_dialog_input_armed = false;
    value->layout_dialog_opened_ns = 0;
    value->layout_dialog_closing_ns = 0;
    value->layout_dialog_write_ns = 0;
    value->layout_dialog_scroll = 0;
    bongo_cat_preferences_scrollbar_reset(&value->layout_dialog_scrollbar);
    value->layout_dialog_directory[0] = '\0';
}

static void load_draft(BongoCatPreferences *value) {
    if (!bongo_cat_overlay_multi_key_layout_get(value->app->overlay,
            value->layout_dialog_right, &value->layout_dialog_draft))
        bongo_cat_hand_layout_defaults(&value->layout_dialog_draft);
    value->layout_dialog_write_ns = 0;
}

static void save_now(BongoCatPreferences *value) {
    BongoCatApp *app = value->app;
    if (!app || !app->overlay) return;
    if (bongo_cat_overlay_multi_key_layout_save(app->overlay,
            value->layout_dialog_right, &value->layout_dialog_draft)) {
        bongo_cat_app_reapply_input(app);
        app->dirty = true;
    } else {
        bongo_cat_preferences_notice_show(app, tr(value,
            "pages.preference.cat.hints.layoutWriteFailed",
            "Cannot save the layout. Keep the characters inside the bubble and "
            "make sure the model folder is writable."), true);
    }
}

/* Flush a pending edit immediately (closing or switching sides) so the
   debounce cannot swallow the last change. */
static void flush(BongoCatPreferences *value) {
    if (!value->layout_dialog_write_ns) return;
    value->layout_dialog_write_ns = 0;
    save_now(value);
}

static void submit(BongoCatPreferences *value) {
    if (!value->layout_dialog_write_ns) return;
    if (SDL_GetTicksNS() < value->layout_dialog_write_ns + LAYOUT_DIALOG_WRITE_NS) {
        value->render_dirty = true;
        return;
    }
    flush(value);
}

bool bongo_cat_preferences_layout_dialog_active(
    const BongoCatPreferences *value) {
    return value && value->layout_dialog;
}

void bongo_cat_preferences_layout_dialog_open(BongoCatPreferences *value) {
    if (!value || !value->app) return;
    bool left = side_editable(value, false);
    bool right = side_editable(value, true);
    if (!left && !right) return;
    bongo_cat_preferences_shortcut_cancel(value);
    value->layout_dialog_right = !left;
    value->layout_dialog = true;
    value->layout_dialog_input_armed = false;
    value->layout_dialog_opened_ns = SDL_GetTicksNS();
    value->layout_dialog_closing_ns = 0;
    value->layout_dialog_scroll = 0;
    bongo_cat_preferences_scrollbar_reset(&value->layout_dialog_scrollbar);
    snprintf(value->layout_dialog_directory,
        sizeof(value->layout_dialog_directory), "%s", overlay_directory(value));
    load_draft(value);
    value->render_dirty = true;
}

void bongo_cat_preferences_layout_dialog_close(BongoCatPreferences *value) {
    if (!value || !value->layout_dialog || value->layout_dialog_closing_ns) return;
    flush(value);
    /* Fade out with the shared overlay so the tall panel does not disappear
       abruptly. Input is dropped right away. */
    value->layout_dialog_closing_ns = SDL_GetTicksNS();
    value->layout_dialog_input_armed = false;
    value->render_dirty = true;
}

static bool draw_header(BongoCatPreferences *value, struct nk_context *context,
    struct nk_command_buffer *canvas, struct nk_rect panel,
    BongoCatUIPalette p, float opacity, bool enabled) {
    const char *title = tr(value, "pages.preference.cat.labels.multiKeyLayout",
        "Multi-Key Layout");
    struct nk_rect bounds = nk_rect(panel.x + 20, panel.y + 21, panel.w - 74, 24);
    nk_draw_text(canvas, bounds, title, nk_strlen(title), value->ui.label_font,
        nk_rgba(0, 0, 0, 0), alpha(nk_rgb(247, 125, 170), opacity));
    struct nk_rect close = nk_rect(panel.x + panel.w - 52, panel.y + 17, 32, 32);
    return bongo_cat_ui_close_button(context, canvas, close,
        alpha(p.muted, opacity), alpha(p.accent, opacity), enabled);
}

static void draw_tabs(BongoCatPreferences *value, struct nk_context *context,
    struct nk_command_buffer *canvas, struct nk_rect panel,
    BongoCatUIPalette p, float opacity, bool enabled) {
    if (!side_editable(value, false) || !side_editable(value, true)) return;
    struct nk_rect wrapper = nk_rect(panel.x + 20, panel.y + 64, panel.w - 40, 40);
    nk_fill_rect(canvas, wrapper, 10, alpha(p.field, opacity));
    const char *labels[2] = {
        tr(value, "pages.preference.cat.labels.layoutLeft", "Left"),
        tr(value, "pages.preference.cat.labels.layoutRight", "Right")};
    float width = (wrapper.w - 6) / 2;
    for (int i = 0; i < 2; ++i) {
        struct nk_rect button = nk_rect(wrapper.x + 3 + width * i,
            wrapper.y + 3, width, 34);
        bool current = value->layout_dialog_right == (i == 1);
        bool hover = enabled && nk_input_is_mouse_hovering_rect(
            &context->input, button);
        char id[32];
        snprintf(id, sizeof(id), "layout-segment-%d", i);
        float active = bongo_cat_ui_animate_eased(context, id,
            current ? 1.0f : 0.0f, 200, BONGO_CAT_UI_EASE_STANDARD);
        struct nk_color background = bongo_cat_ui_color_mix(
            hover ? p.hover : p.field, p.accent, active);
        nk_fill_rect(canvas, button, 8, alpha(background, opacity));
        centered(canvas, button, labels[i], value->ui.caption_font,
            alpha(bongo_cat_ui_color_mix(p.muted, p.on_accent, active), opacity));
        if (hover) bongo_cat_ui_cursor_hover_rect(context, button,
            BONGO_CAT_UI_CURSOR_POINTER);
        if (hit(context, button, enabled) && !current) {
            flush(value);
            bongo_cat_pref_controls_reset(context);
            value->layout_dialog_right = (i == 1);
            load_draft(value);
            value->render_dirty = true;
        }
    }
}

static void draw_field(struct nk_context *context,
    struct nk_command_buffer *canvas, BongoCatPreferences *value,
    BongoCatUIPalette p, float opacity, struct nk_rect cell,
    const LayoutField *field) {
    if (!field) return;
    nk_draw_text(canvas, nk_rect(cell.x, cell.y, cell.w * .34f, cell.h),
        field->label, nk_strlen(field->label), value->ui.caption_font,
        nk_rgba(0, 0, 0, 0), alpha(p.text, opacity));
    struct nk_rect bounds = nk_rect(cell.x + cell.w * .36f, cell.y + 2.0f,
        cell.w * .64f, cell.h - 4.0f);
    bongo_cat_pref_control_int_rect(context, field->id, field->minimum,
        field->pointer, field->maximum, field->step, field->fallback, bounds);
}

static void draw_pair(struct nk_context *context,
    struct nk_command_buffer *canvas, BongoCatPreferences *value,
    BongoCatUIPalette p, float opacity, float x, float y, float w,
    const LayoutField *first, const LayoutField *second) {
    float gap = 16.0f;
    float half = (w - gap) * .5f;
    draw_field(context, canvas, value, p, opacity,
        nk_rect(x, y, half, LAYOUT_ROW_HEIGHT), first);
    draw_field(context, canvas, value, p, opacity,
        nk_rect(x + half + gap, y, half, LAYOUT_ROW_HEIGHT), second);
}

static void draw_section(struct nk_context *context, struct nk_command_buffer *canvas,
    BongoCatPreferences *value, BongoCatUIPalette p, float opacity, float x,
    float y, float w, const char *title, const char *prefix,
    BongoSafeHandRect *rect, const BongoSafeHandRect *fallback,
    const char *x_label, const char *y_label, const char *w_label,
    const char *h_label) {
    nk_draw_text(canvas, nk_rect(x, y, w, 20), title, nk_strlen(title),
        value->ui.label_font, nk_rgba(0, 0, 0, 0),
        alpha(nk_rgb(247, 125, 170), opacity));
    char x_id[40], y_id[40], w_id[40], h_id[40];
    snprintf(x_id, sizeof(x_id), "layout-%.*s-x", 16, prefix);
    snprintf(y_id, sizeof(y_id), "layout-%.*s-y", 16, prefix);
    snprintf(w_id, sizeof(w_id), "layout-%.*s-w", 16, prefix);
    snprintf(h_id, sizeof(h_id), "layout-%.*s-h", 16, prefix);
    LayoutField fx = {x_label, x_id, &rect->x, 0, 32768, 1, fallback->x};
    LayoutField fy = {y_label, y_id, &rect->y, 0, 32768, 1, fallback->y};
    LayoutField fw = {w_label, w_id, &rect->width, 1, 8192, 1, fallback->width};
    LayoutField fh = {h_label, h_id, &rect->height, 1, 8192, 1,
        fallback->height};
    float row = y + LAYOUT_SECTION_HEIGHT;
    draw_pair(context, canvas, value, p, opacity, x, row, w, &fx, &fy);
    draw_pair(context, canvas, value, p, opacity, x, row + LAYOUT_ROW_HEIGHT, w,
        &fw, &fh);
}

static void draw_fields(BongoCatPreferences *value, struct nk_context *context,
    struct nk_command_buffer *canvas, struct nk_rect panel,
    BongoCatUIPalette p, float opacity, bool tabs, bool enabled) {
    float top = LAYOUT_HEADER_HEIGHT + (tabs ? LAYOUT_TABS_HEIGHT : 0.0f);
    struct nk_rect viewport = nk_rect(panel.x + 20.0f, panel.y + top,
        panel.w - 40.0f, panel.h - top - LAYOUT_FOOTER_HEIGHT);
    BongoCatPreferencesScrollbarResult scroll =
        bongo_cat_preferences_scrollbar_draw(context, canvas, viewport,
            LAYOUT_FIELDS_HEIGHT, value->layout_dialog_scroll,
            &value->layout_dialog_scrollbar, p.accent, opacity, enabled);
    value->layout_dialog_scroll = scroll.offset;
    if (scroll.changed) {
        bongo_cat_pref_controls_reset(context);
        value->render_dirty = true;
    }
    /* Scrolling the fields must not also change the hovered number. */
    if (enabled && scroll.scrollable &&
        nk_input_is_mouse_hovering_rect(&context->input, viewport))
        context->input.mouse.scroll_delta.y = 0;
    float x = viewport.x, w = bongo_cat_preferences_scrollbar_content_width(
        viewport, LAYOUT_FIELDS_HEIGHT);
    float y = viewport.y - scroll.offset;
    struct nk_rect clip = canvas->clip;
    nk_push_scissor(canvas, nk_rect(viewport.x, viewport.y, w, viewport.h));
    bool was_rom = (context->current->layout->flags & NK_WINDOW_ROM) != 0;
    bool was_disabled = context->current->widgets_disabled != 0;
    if (!enabled) {
        if (!was_disabled) nk_widget_disable_begin(context);
        context->current->layout->flags |= NK_WINDOW_ROM;
    }
    BongoSafeHandLayout fallback;
    bongo_cat_hand_layout_defaults(&fallback);
    BongoSafeHandLayout *draft = &value->layout_dialog_draft;
    const char *x_label = tr(value, "pages.preference.cat.labels.layoutX", "X");
    const char *y_label = tr(value, "pages.preference.cat.labels.layoutY", "Y");
    const char *w_label = tr(value, "pages.preference.cat.labels.layoutWidth",
        "Width");
    const char *h_label = tr(value, "pages.preference.cat.labels.layoutHeight",
        "Height");
    draw_section(context, canvas, value, p, opacity, x, y, w, tr(value,
        "pages.preference.cat.labels.layoutHand", "Hand"), "hand", &draft->hand,
        &fallback.hand, x_label, y_label, w_label, h_label);
    y += LAYOUT_SECTION_HEIGHT + LAYOUT_ROW_HEIGHT * 2;
    draw_section(context, canvas, value, p, opacity, x, y, w, tr(value,
        "pages.preference.cat.labels.layoutBubble", "Bubble"), "bubble",
        &draft->bubble, &fallback.bubble, x_label, y_label, w_label, h_label);
    y += LAYOUT_SECTION_HEIGHT + LAYOUT_ROW_HEIGHT * 2;
    draw_section(context, canvas, value, p, opacity, x, y, w, tr(value,
        "pages.preference.cat.labels.layoutCharacters", "Characters"),
        "characters", &draft->characters, &fallback.characters, x_label,
        y_label, w_label, h_label);
    y += LAYOUT_SECTION_HEIGHT + LAYOUT_ROW_HEIGHT * 2;
    LayoutField gap = {tr(value, "pages.preference.cat.labels.layoutGap", "Gap"),
        "layout-gap", &draft->gap, 0, 4096, 1, fallback.gap};
    LayoutField max_keys = {tr(value,
        "pages.preference.cat.labels.layoutMaxKeys", "Max Keys"),
        "layout-max-keys", &draft->max_keys, 1, 8, 1, fallback.max_keys};
    draw_pair(context, canvas, value, p, opacity, x, y, w, &gap, &max_keys);
    if (!enabled) {
        if (!was_disabled) nk_widget_disable_end(context);
        if (!was_rom) context->current->layout->flags &= ~(nk_flags)NK_WINDOW_ROM;
    }
    nk_push_scissor(canvas, clip);
}

static bool draw_reset(BongoCatPreferences *value, struct nk_context *context,
    struct nk_command_buffer *canvas, struct nk_rect panel,
    BongoCatUIPalette p, float opacity, bool enabled) {
    struct nk_rect button = nk_rect(panel.x + 20, panel.y + panel.h - 60,
        panel.w - 40, 40);
    bool hover = enabled && nk_input_is_mouse_hovering_rect(
        &context->input, button);
    nk_fill_rect(canvas, button, 12, alpha(hover ? p.hover : p.field, opacity));
    centered(canvas, button, tr(value,
        "pages.preference.cat.labels.layoutReset", "Restore Defaults"),
        value->ui.caption_font, alpha(p.text, opacity));
    if (hover) bongo_cat_ui_cursor_hover_rect(context, button,
        BONGO_CAT_UI_CURSOR_POINTER);
    return hit(context, button, enabled);
}

void bongo_cat_preferences_layout_dialog_draw(BongoCatPreferences *value,
    struct nk_context *context) {
    if (!value || !value->layout_dialog) return;
    /* The pet may switch models while the panel is open; its crops then
       describe another folder, so close instead of editing the wrong side. */
    if (strcmp(value->layout_dialog_directory, overlay_directory(value)) != 0) {
        reset_state(value);
        value->render_dirty = true;
        return;
    }
    bongo_cat_ui_cursor_reset(context);
    struct nk_rect region = nk_window_get_bounds(context);
    bool tabs = side_editable(value, false) && side_editable(value, true);
    float width = NK_MIN(LAYOUT_DIALOG_WIDTH, region.w - 48.0f);
    float height = NK_MIN(LAYOUT_HEADER_HEIGHT + (tabs ? LAYOUT_TABS_HEIGHT : 0.0f) +
        LAYOUT_FIELDS_HEIGHT + LAYOUT_FOOTER_HEIGHT, region.h - 48.0f);
    BongoCatOverlayFrame frame = bongo_cat_preferences_overlay_frame(region,
        width, height, value->layout_dialog_opened_ns,
        value->layout_dialog_closing_ns);
    if (frame.finished) {
        reset_state(value);
        value->render_dirty = true;
        return;
    }
    BongoCatUIPalette p = bongo_cat_ui_palette(bongo_cat_ui_dark(context));
    bongo_cat_preferences_overlay_draw(context, region, &frame, p);
    bool closing = value->layout_dialog_closing_ns != 0;
    bool input_ready = bongo_cat_preferences_overlay_input_ready(context,
        &value->layout_dialog_input_armed);
    bool enabled = !closing && input_ready;
    float opacity = closing ? frame.visibility : 1.0f;
    struct nk_command_buffer *canvas = nk_window_get_canvas(context);
    nk_fill_rect(canvas, frame.panel, 18, alpha(p.surface, opacity));
    nk_stroke_rect(canvas, frame.panel, 18, 1, alpha(p.border, opacity));
    bool close = draw_header(value, context, canvas, frame.panel, p, opacity,
        enabled);
    BongoSafeHandLayout before = value->layout_dialog_draft;
    nk_push_scissor(canvas, frame.panel);
    draw_tabs(value, context, canvas, frame.panel, p, opacity, enabled);
    draw_fields(value, context, canvas, frame.panel, p, opacity, tabs, enabled);
    bool reset = draw_reset(value, context, canvas, frame.panel, p, opacity,
        enabled);
    nk_push_scissor(canvas, nk_window_get_content_region(context));
    if (reset) {
        bongo_cat_hand_layout_defaults(&value->layout_dialog_draft);
        bongo_cat_pref_controls_reset(context);
        value->layout_dialog_write_ns = SDL_GetTicksNS();
        value->render_dirty = true;
    } else if (memcmp(&before, &value->layout_dialog_draft, sizeof(before)) != 0) {
        value->layout_dialog_write_ns = SDL_GetTicksNS();
        value->render_dirty = true;
    }
    submit(value);
    bool outside = hit(context, region, enabled) &&
        !nk_input_is_mouse_hovering_rect(&context->input, frame.panel);
    if (close || outside) bongo_cat_preferences_layout_dialog_close(value);
    if (frame.visibility < 1.0f || closing) value->render_dirty = true;
}
