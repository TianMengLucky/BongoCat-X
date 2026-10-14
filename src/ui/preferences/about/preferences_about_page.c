#include "preferences_about_internal.h"
#include "preferences_state.h"
#include "preferences_widgets.h"

void bongo_cat_preferences_page_about(BongoCatPreferences *value,
    struct nk_context *context) {
    bongo_cat_preferences_about_hero(value, context);
    nk_layout_row_dynamic(context, 420, 1);
    struct nk_style_window saved;
    if (bongo_cat_pref_card_begin(context, "about-community", &saved)) {
        bongo_cat_about_community(value, context);
        bongo_cat_pref_card_end(context, &saved);
    }
    bongo_cat_about_contributors(value, context);
}
