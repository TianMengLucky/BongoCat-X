#include "plugin_overlay.h"
#include "bongo_cat/safe_ffi.h"
#include "ui_catime.h"
#include "ui_software.h"
#include "preferences_model_glyphs.h"
#include <string.h>
static struct nk_rect rect(BongoCatPluginUiRect r) {return nk_rect(r.x,r.y,r.w,r.h);}
static struct nk_color rgba(uint32_t c) {return nk_rgba(c&255u,(c>>8)&255u,(c>>16)&255u,c>>24);}
static const struct nk_user_font *font(BongoCatPluginOverlaySurface *s, uint32_t role) {
    return role == BONGO_UI_CAPTION ? s->ui->caption_font :
        role == BONGO_UI_LABEL ? s->ui->label_font :
        role == BONGO_UI_HEADING ? s->ui->heading_font : s->ui->body_font;
}
static const char *text(void *context, const char *key, const char *fallback) {
    return bongo_cat_i18n_get(((BongoCatPluginOverlaySurface *)context)->i18n,key,fallback);
}
static uint32_t color(void *context, uint32_t role) {
    BongoCatUIPalette p = bongo_cat_ui_palette(((BongoCatPluginOverlaySurface *)context)->dark);
    const struct nk_color colors[] = {p.text,p.muted,p.surface_glass,p.field,p.hover,
        p.selection,p.border,p.border_subtle,p.accent,p.accent_hover,p.danger,
        p.danger_background,p.link,p.link_hover};
    struct nk_color c = colors[role < SDL_arraysize(colors) ? role : 0];
    return (uint32_t)c.r | (uint32_t)c.g<<8 | (uint32_t)c.b<<16 | (uint32_t)c.a<<24;
}
static void rectangle(void *context, BongoCatPluginUiRect r, float rounding, float stroke, uint32_t c) {
    struct nk_command_buffer *canvas = nk_window_get_canvas(&((BongoCatPluginOverlaySurface *)context)->ui->context);
    if (stroke > 0) nk_stroke_rect(canvas,rect(r),rounding,stroke,rgba(c));
    else nk_fill_rect(canvas,rect(r),rounding,rgba(c));
}
static void line(void *context, BongoCatPluginUiRect r, float width, uint32_t c) {
    nk_stroke_line(nk_window_get_canvas(&((BongoCatPluginOverlaySurface *)context)->ui->context),
        r.x,r.y,r.x+r.w,r.y+r.h,width,rgba(c));
}
static void draw_text(void *context, BongoCatPluginUiRect r, const char *value,
    uint32_t role, uint32_t c, uint32_t align) {
    if (!value) return;
    BongoCatPluginOverlaySurface *s = context;
    const struct nk_user_font *f = font(s,role);
    float width = f->width(f->userdata,f->height,value,nk_strlen(value));
    if (align == 2) r.x += SDL_max(0,r.w-width);
    else if (align == 1) r.x += SDL_max(0,(r.w-width)/2);
    r.y += (r.h-f->height)/2; r.h = f->height;
    nk_draw_text(nk_window_get_canvas(&s->ui->context),rect(r),value,nk_strlen(value),f,nk_rgba(0,0,0,0),rgba(c));
}
static float measure(void *context, const char *value, uint32_t role) {
    const struct nk_user_font *f = font(context,role);
    return f->width(f->userdata,f->height,value,nk_strlen(value));
}
static uint32_t image(void *context, uint32_t slot, const char *path, BongoCatPluginUiRect r) {
    BongoCatPluginOverlaySurface *s = context;
    if (slot >= BONGO_CAT_PLUGIN_OVERLAY_IMAGES || !path || strlen(path) >= BONGO_CAT_PATH_CAP) return 0;
    if (strcmp(s->paths[slot],path)) {
        bongo_cat_ui_texture_delete(s->ui,1,&s->textures[slot]);
        s->textures[slot] = 0; s->paths[slot][0] = 0;
        BongoCatError error = {0}; int width = 0, height = 0;
        s->textures[slot] = bongo_cat_ui_texture_thumbnail(s->ui,path,64,64,&width,&height,&error);
        if (s->textures[slot]) SDL_strlcpy(s->paths[slot],path,sizeof(s->paths[slot]));
    }
    if (!s->textures[slot]) return 0;
    struct nk_image icon = nk_image_id((int)s->textures[slot]);
    nk_draw_image(nk_window_get_canvas(&s->ui->context),rect(r),&icon,nk_rgb(255,255,255));
    return 1;
}
BongoCatPluginOverlayCanvas bongo_cat_plugin_overlay_canvas(BongoCatPluginOverlaySurface *surface, const char *root) {
    return (BongoCatPluginOverlayCanvas){1,sizeof(BongoCatPluginOverlayCanvas),surface,root,
        text,color,rectangle,line,draw_text,measure,image,bongo_safe_sha256_bytes};
}
void bongo_cat_plugin_overlay_release(BongoCatPluginOverlaySurface *surface) {
    bongo_cat_ui_texture_delete(surface->ui,BONGO_CAT_PLUGIN_OVERLAY_IMAGES,surface->textures);
}
void bongo_cat_plugin_overlay_text(void *context, const char *value) {
    BongoCatPluginOverlayGlyphs *g = context;
    if (!value) return;
    if (g->ranges) bongo_cat_preferences_text_glyphs(g->ranges,g->capacity,value);
    uint64_t hash = UINT64_C(14695981039346656037);
    for (const unsigned char *s = (const unsigned char *)value; *s; ++s)
        hash = (hash ^ *s) * UINT64_C(1099511628211);
    g->hash ^= hash;
}
const BongoCatPluginOverlay *bongo_cat_plugin_overlay_query(SDL_SharedObject *library, const char *symbol) {
    union {SDL_FunctionPointer function; BongoCatPluginOverlayQuery query;} entry;
    entry.function = SDL_LoadFunction(library,symbol);
    const BongoCatPluginOverlay *api = entry.function ? entry.query(1) : NULL;
    return api && api->abi_version == 1 && api->struct_size == sizeof(*api) &&
        api->width > 0 && api->width <= 8192 && api->height > 0 && api->height <= 8192 &&
        api->title_key && api->title && api->draw && api->pointer && api->visit_text ? api : NULL;
}
