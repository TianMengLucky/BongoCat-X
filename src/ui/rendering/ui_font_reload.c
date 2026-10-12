#include "ui_software.h"
#include "ui_font_atlas.h"

#include <SDL3/SDL_opengl.h>
#include <string.h>

bool bongo_cat_ui_font_atlas_reload(BongoCatUIBackend *ui,
    const char *body_path, const char *body_fallback_path,
    const char *body_korean_fallback_path, const char *heading_path,
    const char *heading_fallback_path,
    const char *heading_korean_fallback_path,
    const nk_rune *glyph_ranges, const nk_rune *heading_ranges, float raster_scale) {
    if (!ui) return false;
    if (ui->frame_building) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
            "Refusing to replace a font atlas while a UI frame is being built");
        return false;
    }
    BongoCatUIBackend next;
    memset(&next, 0, sizeof(next));
    next.software_renderer = ui->software_renderer;
    if (!nk_init_default(&next.context, NULL)) return false;
    bool created = bongo_cat_ui_font_atlas_create(&next, body_path,
        body_fallback_path, body_korean_fallback_path, heading_path,
        heading_fallback_path, heading_korean_fallback_path, glyph_ranges, heading_ranges,
        raster_scale);
    nk_free(&next.context);
    if (!created) {
        if (next.font_texture) bongo_cat_ui_texture_delete(&next, 1, &next.font_texture);
        if (next.atlas.permanent.alloc)
            bongo_cat_ui_font_atlas_destroy(&next);
        return false;
    }
    if (ui->font_texture) bongo_cat_ui_texture_delete(ui, 1, &ui->font_texture);
    bongo_cat_ui_font_atlas_destroy(ui);
    ui->atlas = next.atlas;
    ui->null_texture = next.null_texture;
    ui->font_texture = next.font_texture;
    ui->font_atlas_width = next.font_atlas_width;
    ui->font_atlas_height = next.font_atlas_height;
    memcpy(ui->font_handles, next.font_handles, sizeof(ui->font_handles));
    /* next lives on the stack: rebase every handle after moving its atlas. */
    ui->caption_font = &ui->font_handles[0];
    ui->body_font = &ui->font_handles[1];
    ui->label_font = &ui->font_handles[2];
    ui->heading_font = &ui->font_handles[3];
    ui->hero_font = &ui->font_handles[4];
    ui->latin_glyph_ranges = next.latin_glyph_ranges;
    ui->cjk_glyph_ranges = next.cjk_glyph_ranges;
    ui->korean_glyph_ranges = next.korean_glyph_ranges;
    ui->heading_primary_ranges = next.heading_primary_ranges;
    ui->heading_cjk_ranges = next.heading_cjk_ranges;
    ui->heading_korean_ranges = next.heading_korean_ranges;
    ui->custom_font_loaded = next.custom_font_loaded;
    ui->font_probe_loaded = next.font_probe_loaded;
    ui->font_path_found = next.font_path_found;
    ui->font_file_loaded = next.font_file_loaded;
    ui->raster_scale = raster_scale;
    nk_style_set_font(&ui->context, ui->body_font);
    return true;
}
