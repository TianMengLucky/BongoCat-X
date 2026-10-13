#ifndef BONGO_CAT_UI_SOFTWARE_FONT_H
#define BONGO_CAT_UI_SOFTWARE_FONT_H
#include <SDL3/SDL.h>
typedef struct BongoCatUIFontMask BongoCatUIFontMask;
BongoCatUIFontMask *bongo_cat_ui_font_mask_create(const unsigned char *, int, int);
void bongo_cat_ui_font_mask_free(BongoCatUIFontMask *);
bool bongo_cat_ui_font_mask_draw(SDL_Renderer *, BongoCatUIFontMask *, SDL_Texture **,
    SDL_FRect source, SDL_FRect target, SDL_FColor color, float sx, float sy);
#endif
