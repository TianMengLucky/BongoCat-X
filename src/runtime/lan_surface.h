#ifndef BONGO_CAT_LAN_SURFACE_H
#define BONGO_CAT_LAN_SURFACE_H
#include "bongo_cat/lan.h"
typedef struct BongoCatLanSurface BongoCatLanSurface;
extern const char *const bongo_cat_lan_parameters[BONGO_CAT_LAN_PARAMETERS];
void bongo_cat_lan_surface_destroy(BongoCatLanSurface **);
bool bongo_cat_lan_surface_update(BongoCatApp *, BongoCatLanSurface **,
    const BongoCatLanSnapshot *, const char *);
bool bongo_cat_lan_surface_event(BongoCatLanSurface *, const SDL_Event *);
#endif
