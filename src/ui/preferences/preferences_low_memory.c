#include "preferences_state.h"
#include "preferences_widgets.h"
#include "ui_icons.h"
#include "runtime.h"
#include "bongo_cat/i18n.h"

void bongo_cat_preferences_low_memory_row(BongoCatApp *app,
    BongoCatApplicationPreferences *options, struct nk_context *context) {
    bongo_cat_pref_row_icon(context, BONGO_CAT_PREF_ICON_RENDER_QUALITY);
    if (bongo_cat_pref_toggle(context, "low-memory-mode",
            bongo_cat_i18n_get(app->i18n,
                "pages.preference.general.labels.lowMemoryMode", "Low Memory Mode"),
            bongo_cat_i18n_get(app->i18n,
                "pages.preference.general.hints.lowMemoryMode",
                "Limit texture quality to 10%, texture edges to 512 pixels and Live2D masks to 256 pixels. Suspend large rendering optimizations. Reloads the renderer; disabling restores saved quality. Memory depends on the model and driver."),
            &options->low_memory_mode)) app->render_backend_swap_pending = true;
}
