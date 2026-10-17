#ifndef BONGO_CAT_RUNTIME_PLUGIN_LOCALE_H
#define BONGO_CAT_RUNTIME_PLUGIN_LOCALE_H
#include "bongo_cat/i18n.h"
#include <SDL3/SDL.h>
void bongo_cat_plugin_locale_load(BongoCatI18n *i18n, SDL_SharedObject *library, const char *id);
#endif
