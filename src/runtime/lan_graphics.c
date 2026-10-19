#include "lan_graphics.h"
#include "bongo_cat/lan_plugin.h"
#include "bongo_cat/i18n.h"
#include "ui_font.h"
#include "preferences_model_glyphs.h"
#define LAN_WIDTH 480
#define LAN_NAME_HEIGHT 28
bool bongo_cat_lan_name_init(BongoCatApp *app, BongoCatUIBackend *ui, SDL_Window *window, const char *name) {
    char body[BONGO_CAT_PATH_CAP], fallback[BONGO_CAT_PATH_CAP], korean[BONGO_CAT_PATH_CAP];
    const char *font = bongo_cat_ui_system_font(body, sizeof(body), false);
    const char *cjk = bongo_cat_ui_system_font(fallback, sizeof(fallback), true);
    const char *kr = bongo_cat_ui_system_korean_font(korean, sizeof(korean));
    nk_rune *ranges = SDL_calloc(8192, sizeof(*ranges));
    if (!ranges) return false;
    bongo_cat_i18n_glyph_ranges(app->i18n, ranges, 8192);
    bongo_cat_preferences_text_glyphs(ranges, 8192, name);
    BongoCatError error = {0};
    float layout = 1, raster = 1;
    bongo_cat_ui_query_window_scale(window, &layout, &raster);
    bool ready = bongo_cat_ui_init(ui, window, font, cjk, kr, font, cjk, kr,
        ranges, ranges, 0.8f, raster, &error);
    SDL_free(ranges);
    if (!ready) bongo_cat_ui_destroy(ui);
    return ready;
}
bool bongo_cat_lan_name_draw(BongoCatUIBackend *ui, const char *name) {
    struct nk_context *ctx = &ui->context;
    ctx->style.window.fixed_background = nk_style_item_color(nk_rgba(0, 0, 0, 0));
    ctx->style.window.background = nk_rgba(0, 0, 0, 0);
    ctx->style.window.border = 0;
    bongo_cat_ui_input_begin(ui); bongo_cat_ui_input_end(ui);
    if (nk_begin(ctx, "friend-name", nk_rect(0, 0, LAN_WIDTH, LAN_NAME_HEIGHT), NK_WINDOW_NO_SCROLLBAR)) {
        nk_layout_row_dynamic(ctx, 18, 1);
        nk_label_colored(ctx, name, NK_TEXT_CENTERED, nk_rgba(240, 240, 240, 255));
    }
    nk_end(ctx);
    return bongo_cat_ui_render(ui);
}

void bongo_cat_lan_position(BongoCatApp *app, SDL_Window *window, uint32_t peer_id) {
    SDL_Rect bounds;
    int width=0,height=0;
    SDL_GetWindowSize(window,&width,&height);
    if (!width || !height || !SDL_GetDisplayUsableBounds(SDL_GetDisplayForWindow(app->window),&bounds)) return;
    int columns=SDL_max(1,bounds.w/width),rows=SDL_max(1,bounds.h/height);
    int ordinal=(int)((peer_id-1u)%BONGO_CAT_LAN_PEERS),cell=ordinal%(columns*rows);
    int cascade=(ordinal/(columns*rows))*24;
    int x=bounds.x+(cell%columns)*width+cascade;
    int y=bounds.y+(cell/columns)*height+cascade;
    SDL_SetWindowPosition(window,SDL_clamp(x,bounds.x,bounds.x+SDL_max(0,bounds.w-width)),
        SDL_clamp(y,bounds.y,bounds.y+SDL_max(0,bounds.h-height)));
}
