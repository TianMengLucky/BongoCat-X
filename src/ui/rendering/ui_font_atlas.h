#ifndef BONGO_CAT_UI_FONT_ATLAS_H
#define BONGO_CAT_UI_FONT_ATLAS_H

#include "ui_backend.h"

bool bongo_cat_ui_font_atlas_create(BongoCatUIBackend *ui,
    const char *body_path, const char *body_fallback_path,
    const char *body_korean_fallback_path, const char *heading_path,
    const char *heading_fallback_path,
    const char *heading_korean_fallback_path,
    const nk_rune *glyph_ranges, const nk_rune *heading_ranges, float raster_scale);
bool bongo_cat_ui_font_atlas_reload(BongoCatUIBackend *ui,
    const char *body_path, const char *body_fallback_path,
    const char *body_korean_fallback_path, const char *heading_path,
    const char *heading_fallback_path,
    const char *heading_korean_fallback_path,
    const nk_rune *glyph_ranges, const nk_rune *heading_ranges, float raster_scale);
void bongo_cat_ui_font_atlas_destroy(BongoCatUIBackend *ui);

#endif
