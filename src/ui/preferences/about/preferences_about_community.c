#include "preferences_about_internal.h"
#include "preferences_state.h"
#include "preferences_notice.h"
#include "ui_paint.h"
#include <stdio.h>

static const char *tr(BongoCatPreferences *value, const char *key, const char *fallback) {
    return bongo_cat_i18n_get(value->app->i18n, key, fallback);
}
void bongo_cat_about_community(BongoCatPreferences *value, struct nk_context *context) {
    BongoCatUIPalette p = bongo_cat_ui_palette(bongo_cat_ui_dark(context));
    char title[128];
    snprintf(title, sizeof(title), "BongoCat-X · %s · 211957388", tr(value,
        "native.support.qqGroup", "QQ Group"));
    nk_layout_row_dynamic(context, 28, 1);
    nk_label_colored(context, title, NK_TEXT_CENTERED, p.text);
    nk_layout_row_dynamic(context, 40, 1);
    nk_label_wrap(context, tr(value, "native.about.qqHint",
        "Scan with QQ or search for group 211957388 to join."));
    bongo_cat_preferences_qq_load(value);
    struct nk_rect bounds;
    float size = NK_MIN(240.0f, nk_window_get_content_region(context).w);
    nk_layout_row_dynamic(context, size, 1);
    if (nk_widget(&bounds, context) != NK_WIDGET_INVALID && value->qq_texture) {
        /* Display the square code from the original poster, with its quiet
           margin. Keep the supplied JPEG untouched for README readers. */
        int side = (int)(value->qq_width * 0.71f);
        struct nk_image image = nk_subimage_id((int)value->qq_texture,
            (nk_ushort)value->qq_width, (nk_ushort)value->qq_height,
            nk_recti((int)(value->qq_width * 0.145f),
                (int)(value->qq_height * 0.318f), side, side));
        nk_draw_image(nk_window_get_canvas(context),
            nk_rect(bounds.x + (bounds.w - size) * .5f, bounds.y, size, size),
            &image, nk_rgb(255, 255, 255));
    }
    nk_layout_row_dynamic(context, 32, 1);
    if (nk_button_label(context, tr(value, "native.about.qqCopy", "Copy group number"))) {
        bool ok = SDL_SetClipboardText("211957388");
        bongo_cat_preferences_notice_show(value->app, tr(value,
            ok ? "native.about.qqCopied" : "native.about.qqCopyFailed",
            ok ? "Group number copied" : "Unable to copy group number"), !ok);
    }
}
