#include "preferences_state.h"
#include "ui_software.h"
#include "bongo_cat/path.h"
#include "bongo_cat/image.h"
#include <string.h>
void bongo_cat_preferences_items_preview_clear(BongoCatPreferences *value, bool graphics) {
    if (graphics && value->item_preview_texture)
        bongo_cat_ui_texture_delete(&value->ui, 1, &value->item_preview_texture);
    value->item_preview_texture = 0; value->item_preview_asset[0] = 0;
    value->item_preview_width = value->item_preview_height = 0;
}
void bongo_cat_preferences_items_preview(BongoCatPreferences *value, struct nk_context *context, const char *asset) {
    if (strcmp(value->item_preview_asset, asset)) {
        bongo_cat_preferences_items_preview_clear(value, true);
        SDL_strlcpy(value->item_preview_asset, asset, sizeof(value->item_preview_asset));
        char directory[BONGO_CAT_PATH_CAP], path[BONGO_CAT_PATH_CAP];
        uint64_t size = 0; int width = 0, height = 0; BongoCatError error = {0};
        if (bongo_cat_path_join(directory, sizeof(directory), value->app->data_root, "items") &&
            bongo_cat_path_join(path, sizeof(path), directory, asset) &&
            bongo_cat_path_file_size(path, &size) && size <= 64u * 1024u * 1024u &&
            bongo_cat_image_info(path, &width, &height) && width > 0 && height > 0 &&
            (uint64_t)width * height <= 16777216)
            value->item_preview_texture = bongo_cat_ui_texture_thumbnail(&value->ui, path, 192, 192,
                &value->item_preview_width, &value->item_preview_height, &error);
    }
    if (!value->item_preview_texture) return;
    nk_layout_row_dynamic(context, 96, 1);
    struct nk_rect bounds;
    if (nk_widget(&bounds, context) == NK_WIDGET_INVALID) return;
    float scale = NK_MIN(96.0f / value->item_preview_width, 96.0f / value->item_preview_height);
    float width = value->item_preview_width * scale, height = value->item_preview_height * scale;
    struct nk_image image = nk_image_id((int)value->item_preview_texture);
    nk_draw_image(nk_window_get_canvas(context), nk_rect(bounds.x + (bounds.w - width) / 2,
        bounds.y + (bounds.h - height) / 2, width, height), &image, nk_rgba(255, 255, 255, 255));
}
