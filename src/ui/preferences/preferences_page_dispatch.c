#include "preferences_page_dispatch.h"
#include "bongo_cat/statistics.h"

void bongo_cat_preferences_draw_page(BongoCatPreferences *value,
    struct nk_context *context) {
    if (value->page == 4 && !bongo_cat_statistics_page(value->app)) value->page = 3;
    switch (value->page) {
    case 0: bongo_cat_preferences_page_settings(value, context); break;
    case 1: bongo_cat_preferences_page_model(value, context); break;
    case 2: bongo_cat_preferences_page_shortcuts(value, context); break;
    case 3: bongo_cat_preferences_page_plugins(value, context); break;
    case 4: bongo_cat_preferences_page_statistics(value, context); break;
    default: bongo_cat_preferences_page_about(value, context); break;
    }
}
