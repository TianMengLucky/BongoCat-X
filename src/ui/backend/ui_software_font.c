/* Keep the full-quality font atlas in A8. Reuse one small native glyph texture. */
#include "ui_software_font.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
struct BongoCatUIFontMask {
    unsigned char *pixels;
    int width, height, texture_width, texture_height;
    SDL_PixelFormat format;
    unsigned char *row;
    size_t row_capacity;
};
BongoCatUIFontMask *bongo_cat_ui_font_mask_create(const unsigned char *pixels, int w, int h) {
    if (!pixels || w <= 0 || h <= 0 || (size_t)h > SIZE_MAX/(size_t)w) return NULL;
    BongoCatUIFontMask *mask = calloc(1,sizeof(*mask));
    if (!mask) return NULL;
    mask->pixels = malloc((size_t)w*h);
    if (!mask->pixels) { free(mask); return NULL; }
    memcpy(mask->pixels,pixels,(size_t)w*h); mask->width=w; mask->height=h;
    return mask;
}
void bongo_cat_ui_font_mask_free(BongoCatUIFontMask *mask) {
    if (mask) { free(mask->row); free(mask->pixels); free(mask); }
}
bool bongo_cat_ui_font_mask_draw(SDL_Renderer *renderer, BongoCatUIFontMask *mask,
    SDL_Texture **texture, SDL_FRect source, SDL_FRect target, SDL_FColor color,
    float sx, float sy) {
    if (!mask || sx <= 0 || sy <= 0 || !isfinite(source.x) || !isfinite(source.y) ||
        !isfinite(source.w) || !isfinite(source.h) || source.x < 0 || source.y < 0 ||
        source.x > 1 || source.y > 1 || source.w < 0 || source.h < 0 ||
        source.w > 1 || source.h > 1) return false;
    /* UVs encode integer texels, but non-power-of-two normalization introduces
       float roundoff. SDL truncates source rectangles: recover exact texels. */
    int x=(int)lroundf(source.x*mask->width), y=(int)lroundf(source.y*mask->height);
    int w=(int)lroundf(source.w*mask->width), h=(int)lroundf(source.h*mask->height);
    if (x < 0 || y < 0 || w <= 0 || h <= 0 || x > mask->width-w || y > mask->height-h)
        return false;
    // A guard on every side preserves linear sampling at glyph edges.
    int left=SDL_max(0,x-1), top=SDL_max(0,y-1);
    int right=SDL_min(mask->width,x+w+1), bottom=SDL_min(mask->height,y+h+1);
    int gw=right-left, gh=bottom-top;
    if (!mask->format && *texture) mask->format=(SDL_PixelFormat)SDL_GetNumberProperty(
        SDL_GetTextureProperties(*texture),SDL_PROP_TEXTURE_FORMAT_NUMBER,SDL_PIXELFORMAT_RGBA32);
    if (!*texture || mask->texture_width < gw || mask->texture_height < gh) {
        if (!SDL_FlushRenderer(renderer)) return false;
        SDL_DestroyTexture(*texture); *texture=NULL;
        mask->texture_width=SDL_max(64,gw); mask->texture_height=SDL_max(64,gh);
        *texture=SDL_CreateTexture(renderer,mask->format,SDL_TEXTUREACCESS_STREAMING,
            mask->texture_width,mask->texture_height);
        if (!*texture || !SDL_SetTextureBlendMode(*texture,SDL_BLENDMODE_BLEND) ||
                !SDL_SetTextureScaleMode(*texture,SDL_SCALEMODE_LINEAR)) return false;
    }
    void *target_pixels=NULL; int pitch=0;
    if (gw > INT_MAX/4 || !SDL_LockTexture(*texture,NULL,&target_pixels,&pitch)) return false;
    if (mask->row_capacity < (size_t)gw*4) {
        unsigned char *row=realloc(mask->row,(size_t)gw*4);
        if (row) { mask->row=row; mask->row_capacity=(size_t)gw*4; }
    }
    unsigned char *row=mask->row;
    bool ok=mask->row_capacity >= (size_t)gw*4;
    for (int i=0; ok && i<gh; ++i) {
        for (int j=0; j<gw; ++j) {
            row[j*4]=row[j*4+1]=row[j*4+2]=255;
            row[j*4+3]=mask->pixels[(size_t)(top+i)*mask->width+left+j];
        }
        ok=SDL_ConvertPixels(gw,1,SDL_PIXELFORMAT_RGBA32,row,gw*4,mask->format,
            (unsigned char *)target_pixels+(size_t)i*pitch,pitch);
    }
    SDL_UnlockTexture(*texture);
    if (!ok) return false;
    source=(SDL_FRect){(float)(x-left),(float)(y-top),(float)w,(float)h};
    // Render in physical pixels. SDL software copy truncates logical rectangles
    // before applying render scale, which otherwise shifts individual glyphs.
    float tx=roundf(target.x*sx), ty=roundf(target.y*sy);
    target=(SDL_FRect){tx,ty,roundf((target.x+target.w)*sx)-tx,
        roundf((target.y+target.h)*sy)-ty};
    SDL_Rect clip; SDL_GetRenderClipRect(renderer,&clip);
    bool clipped=SDL_RenderClipEnabled(renderer);
    SDL_Rect physical={(int)floorf(clip.x*sx),(int)floorf(clip.y*sy),
        (int)ceilf((clip.x+clip.w)*sx)-(int)floorf(clip.x*sx),
        (int)ceilf((clip.y+clip.h)*sy)-(int)floorf(clip.y*sy)};
    ok=SDL_SetRenderScale(renderer,1,1) && SDL_SetRenderClipRect(renderer,clipped ? &physical : NULL) &&
        SDL_SetTextureColorModFloat(*texture,color.r,color.g,color.b) &&
        SDL_SetTextureAlphaModFloat(*texture,color.a) &&
        SDL_RenderTexture(renderer,*texture,&source,&target) && SDL_FlushRenderer(renderer);
    // Flush before replacing the small scratch contents for another glyph.
    bool restored=SDL_SetRenderScale(renderer,sx,sy) && SDL_SetRenderClipRect(renderer,clipped ? &clip : NULL);
    return ok && restored;
}
