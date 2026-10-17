#include "plugin_locale.h"
#include "bongo_cat/plugin_locale.h"
#include "bongo_cat/log.h"
#include <stdio.h>

void bongo_cat_plugin_locale_load(BongoCatI18n *i18n, SDL_SharedObject *library, const char *id) {
    if (!i18n || !library || !id) return;
    char symbol[96];
    snprintf(symbol, sizeof(symbol), "bongo_cat_%s_locale_query_v1", id);
    union { SDL_FunctionPointer function; BongoCatPluginLocaleQuery query; } entry;
    entry.function = SDL_LoadFunction(library, symbol);
    if (!entry.function) { SDL_ClearError(); return; }
    if (!bongo_cat_i18n_register_plugin(i18n, id, entry.query(BONGO_CAT_PLUGIN_LOCALE_ABI)))
        SDL_LogWarn(BONGO_CAT_LOG_LIFECYCLE, "[%s] Cannot load plugin language resources", id);
}
