#include "statistics_internal.h"

const BongoCatStatisticsPagePlugin *bongo_cat_statistics_page_api(SDL_SharedObject *library) {
    union { SDL_FunctionPointer function; BongoCatStatisticsPageQuery query; } entry;
    entry.function = SDL_LoadFunction(library, BONGO_CAT_STATISTICS_PAGE_SYMBOL);
    const BongoCatStatisticsPagePlugin *plugin = entry.function ?
        entry.query(BONGO_CAT_STATISTICS_PAGE_ABI) : NULL;
    return plugin && plugin->abi_version == BONGO_CAT_STATISTICS_PAGE_ABI &&
        plugin->struct_size == sizeof(*plugin) && plugin->page ? plugin : NULL;
}

const BongoCatStatisticsPage *bongo_cat_statistics_page(const BongoCatApp *app) {
    return app && app->statistics ? &app->statistics->page : NULL;
}
