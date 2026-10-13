#include "nuklear_config.h"
#include "ui_paint_border.h"

#include <stdio.h>
#include <math.h>
#include <stdlib.h>

static int region_alpha(const unsigned char *pixels, int width,
    int x0, int y0, int size) {
    int count = 0;
    for (int y = y0; y < y0 + size; ++y)
        for (int x = x0; x < x0 + size; ++x)
            if (pixels[((size_t)y * width + x) * 4 + 3]) ++count;
    return count;
}

static int dashed_border_test(void) {
    enum { WIDTH = 100, HEIGHT = 80 };
    unsigned char *pixels = calloc((size_t)WIDTH * HEIGHT, 4);
    if (!pixels) return 1;
    bongo_cat_ui_raster_dashed_rounded(pixels, WIDTH, HEIGHT,
        14, 2, 5, 5, nk_rgb(84, 174, 255));
    enum { ARC_INSET = 2, ARC_REGION = 10 };
    int failed = !region_alpha(pixels, WIDTH, ARC_INSET, ARC_INSET,
            ARC_REGION) ||
        !region_alpha(pixels, WIDTH, WIDTH - ARC_INSET - ARC_REGION,
            ARC_INSET, ARC_REGION) ||
        !region_alpha(pixels, WIDTH, ARC_INSET,
            HEIGHT - ARC_INSET - ARC_REGION, ARC_REGION) ||
        !region_alpha(pixels, WIDTH, WIDTH - ARC_INSET - ARC_REGION,
            HEIGHT - ARC_INSET - ARC_REGION, ARC_REGION) ||
        pixels[((size_t)(HEIGHT / 2) * WIDTH + WIDTH / 2) * 4 + 3];
    free(pixels); return failed;
}

int main(void) {
    if (dashed_border_test()) return 10;
    struct nk_context context;
    struct nk_font_atlas atlas;
    struct nk_draw_null_texture null_texture;
    if (!nk_init_default(&context, NULL)) return 1;
    nk_font_atlas_init_default(&atlas);
    nk_font_atlas_begin(&atlas);
    struct nk_font *font = nk_font_atlas_add_default(&atlas, 16.0f, NULL);
    if (!font) return 2;
    int width, height;
    if (!nk_font_atlas_bake(&atlas, &width, &height, NK_FONT_ATLAS_RGBA32)) return 3;
    nk_font_atlas_end(&atlas, nk_handle_id(1), &null_texture);
    /* Independent sizes can share one atlas and keep identical glyph UVs. */
    struct nk_user_font small = font->handle, large = font->handle;
    small.height = 18.0f; large.height = 36.0f;
    float small_width = small.width(small.userdata, small.height, "Test", 4);
    float large_width = large.width(large.userdata, large.height, "Test", 4);
    if (small_width <= 0 || fabsf(large_width - small_width * 2) > .01f) return 11;
    struct nk_user_font_glyph small_glyph, large_glyph;
    small.query(small.userdata, small.height, &small_glyph, 'A', 0);
    large.query(large.userdata, large.height, &large_glyph, 'A', 0);
    if (fabsf(large_glyph.width - small_glyph.width * 2) > .01f ||
        small_glyph.uv[0].x != large_glyph.uv[0].x ||
        small_glyph.uv[1].y != large_glyph.uv[1].y ||
        font->handle.height != 16.0f) return 12;
    nk_style_set_font(&context, &font->handle);
    if (!nk_begin(&context, "test", nk_rect(0, 0, 320, 240), 0)) return 4;
    nk_layout_row_dynamic(&context, 24, 1);
    nk_label(&context, "Nuklear", NK_TEXT_LEFT);
    nk_end(&context);
    nk_clear(&context);
    nk_free(&context);
    nk_font_atlas_clear(&atlas);
    puts("nuklear smoke passed");
    return 0;
}
