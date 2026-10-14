#include "preferences_state.h"
#include "preferences_live2d_setup.h"
#include "preferences_overlay.h"
#include "about/preferences_about_internal.h"
#include "ui_paint.h"
#include <stdio.h>
static const char *label(BongoCatPreferences *value, const char *key, const char *fallback) {
    char full[128]; snprintf(full, sizeof(full), "pages.preference.live2dSetup.%s", key);
    return bongo_cat_i18n_get(value->app->i18n, full, fallback);
}
static void text(struct nk_command_buffer *canvas, struct nk_rect bounds, const char *label,
    const struct nk_user_font *font, struct nk_color color) {
    nk_draw_text(canvas, bounds, label, nk_strlen(label), font, nk_rgba(0, 0, 0, 0), color);
}
static bool button(BongoCatPreferences *value, struct nk_context *context, struct nk_rect bounds,
    const char *label, bool primary, bool enabled, BongoCatUIPalette p) {
    struct nk_command_buffer *canvas = nk_window_get_canvas(context);
    bool hover = enabled && nk_input_is_mouse_hovering_rect(&context->input, bounds);
    nk_fill_rect(canvas, bounds, 10, !enabled ? p.field : primary ?
        (hover ? p.accent_hover : p.accent) : hover ? p.selection : p.field);
    nk_stroke_rect(canvas, bounds, 10, 1, p.border_subtle);
    const struct nk_user_font *font = value->ui.caption_font;
    float width = font->width(font->userdata, font->height, label, nk_strlen(label));
    text(canvas, nk_rect(bounds.x + NK_MAX(8.0f, (bounds.w - width) * .5f),
        bounds.y + (bounds.h - font->height) * .5f, bounds.w - 16, font->height), label, font,
        !enabled ? p.muted : primary ? nk_rgb(255, 255, 255) : p.text);
    if (hover) bongo_cat_ui_cursor_hover_rect(context, bounds, BONGO_CAT_UI_CURSOR_POINTER);
    return hover && nk_input_is_mouse_click_in_rect(&context->input, NK_BUTTON_LEFT, bounds);
}
void bongo_cat_preferences_live2d_setup_draw(BongoCatPreferences *value, struct nk_context *context) {
    if (!bongo_cat_preferences_live2d_setup_active(value)) return;
    BongoCatLive2DSetup *job = value->live2d_setup;
    int phase = atomic_load_explicit(&job->phase, memory_order_acquire);
    struct nk_rect region = nk_window_get_bounds(context);
    BongoCatOverlayFrame frame = bongo_cat_preferences_overlay_frame(region,
        NK_MIN(740.0f, region.w - 48), NK_MIN(580.0f, region.h - 48), job->opened_ns, 0);
    BongoCatUIPalette p = bongo_cat_ui_palette(bongo_cat_ui_dark(context));
    bongo_cat_preferences_overlay_draw(context, region, &frame, p);
    struct nk_command_buffer *canvas = nk_window_get_canvas(context);
    struct nk_rect panel = frame.panel;
    nk_fill_rect(canvas, panel, 18, p.surface);
    nk_stroke_rect(canvas, panel, 18, 1, p.border);
    text(canvas, nk_rect(panel.x + 24, panel.y + 20, panel.w - 48, 28),
        label(value, "title", "Live2D SDK license agreement"), value->ui.label_font, p.text);
#ifdef _WIN32
    const char *description = label(value, "description", "Cubism Core is missing. Read both official agreements before downloading. Cancel keeps the plugin enabled without rendering.");
#else
    const char *description = label(value, "unsupported", "Automatic Core installation is available on Windows. Import a compatible Core manually on this platform.");
#endif
    float description_height = NK_MIN(panel.h * .3f,
        bongo_cat_about_text_height(description, value->ui.caption_font, panel.w - 48));
    (void)bongo_cat_about_paragraph(context, nk_rect(panel.x + 24, panel.y + 58, panel.w - 48, description_height),
        description, value->ui.caption_font, p.muted, false, 20);
    float box_y = 72.0f + description_height;
    struct nk_rect box = nk_rect(panel.x + 24, panel.y + box_y, panel.w - 48, NK_MAX(64.0f, panel.h - box_y - 68));
    nk_fill_rect(canvas, box, 10, p.field);
    bool input_ready = bongo_cat_preferences_overlay_input_ready(context, &job->input_armed);
    if (phase == BONGO_CAT_SDK_LICENSE_LOADING) {
        text(canvas, nk_rect(box.x + 16, box.y + 16, box.w - 32, 24),
            label(value, "loading", "Loading official license agreements..."), value->ui.body_font, p.muted);
    } else if (phase == BONGO_CAT_SDK_DOWNLOADING) {
        char number[128];
        snprintf(number, sizeof(number), "%s %.1f MiB", label(value, "downloading", "Downloading SDK..."),
            (double)atomic_load(&job->received) / 1048576.0);
        text(canvas, nk_rect(box.x + 16, box.y + 20, box.w - 32, 28), number, value->ui.body_font, p.accent);
        uint64_t total = atomic_load(&job->total), received = atomic_load(&job->received);
        if (total) {
            struct nk_rect bar = nk_rect(box.x + 16, box.y + 64, box.w - 32, 6);
            nk_fill_rect(canvas, bar, 3, p.border_subtle);
            bar.w *= NK_CLAMP(0.0f, (float)((double)received / (double)total), 1.0f);
            nk_fill_rect(canvas, bar, 3, p.accent);
        }
    } else if (phase == BONGO_CAT_SDK_FAILED) {
        text(canvas, nk_rect(box.x + 16, box.y + 16, box.w - 32, 24),
            label(value, "failed", "Setup failed; the plugin remains enabled without rendering."), value->ui.caption_font, p.danger);
        (void)bongo_cat_about_paragraph(context, nk_rect(box.x + 16, box.y + 50, box.w - 32, box.h - 66),
            job->message, value->ui.caption_font, p.muted, false, 20);
    } else if (job->licenses_ready) {
        struct nk_rect viewport = nk_rect(box.x + 12, box.y + 12, box.w - 24, box.h - 24);
        float width = viewport.w - 16;
        if (job->text_width != width || job->text_height <= 0) {
            job->text_height = bongo_cat_about_text_height(job->terms, value->ui.caption_font, width);
            job->text_width = width;
        }
        BongoCatPreferencesScrollbarResult scroll = bongo_cat_preferences_scrollbar_draw(context,
            canvas, viewport, job->text_height, job->scroll, &job->scrollbar, p.accent, 1, input_ready);
        job->scroll = scroll.offset;
        if (scroll.changed) value->render_dirty = true;
        struct nk_rect clip = canvas->clip;
        nk_push_scissor(canvas, nk_rect(viewport.x, viewport.y, width, viewport.h));
        (void)bongo_cat_about_paragraph(context, nk_rect(viewport.x, viewport.y - job->scroll,
            width, viewport.h + job->scroll), job->terms, value->ui.caption_font, p.text, false, 20);
        nk_push_scissor(canvas, clip);
    }
    float gap = 12, width = (panel.w - 48 - gap) * .5f;
    struct nk_rect cancel = nk_rect(panel.x + 24, panel.y + panel.h - 52, width, 36);
    struct nk_rect accept = nk_rect(cancel.x + width + gap, cancel.y, width, 36);
    if (button(value, context, cancel, label(value, "cancel", "Cancel and continue"), false, input_ready, p))
        bongo_cat_preferences_live2d_setup_cancel(value);
    bool ready = input_ready && job->licenses_ready &&
        (phase == BONGO_CAT_SDK_LICENSE_READY || phase == BONGO_CAT_SDK_FAILED);
#ifndef _WIN32
    ready = false;
#endif
    if (button(value, context, accept, label(value, "accept", "Agree and download"), true, ready, p))
        bongo_cat_preferences_live2d_setup_accept(value);
    if (frame.visibility < 1.0f || phase == BONGO_CAT_SDK_LICENSE_LOADING || phase == BONGO_CAT_SDK_DOWNLOADING)
        value->render_dirty = true;
}
