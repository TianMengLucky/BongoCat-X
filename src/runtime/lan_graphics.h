#ifndef BONGO_CAT_LAN_GRAPHICS_H
#define BONGO_CAT_LAN_GRAPHICS_H
#include "bongo_cat/app.h"
#include "ui_backend.h"
void bongo_cat_lan_position(BongoCatApp *, SDL_Window *, uint32_t);
bool bongo_cat_lan_name_init(BongoCatApp *, BongoCatUIBackend *, SDL_Window *, const char *);
bool bongo_cat_lan_name_draw(BongoCatUIBackend *, const char *);
#endif
