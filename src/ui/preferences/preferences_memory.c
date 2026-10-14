#include "preferences_state.h"
#include "preferences_widgets.h"
#include "ui_icons.h"
#include "runtime.h"
#include "bongo_cat/i18n.h"
#include "bongo_cat/safe_ffi.h"
#include "preferences_notice.h"
#include "bongo_cat/model_plugin.h"

void bongo_cat_preferences_memory_row(BongoCatApp *app, struct nk_context *context) {
    bool system = bongo_safe_render_system_memory_get(app->settings.extensions_json,
        sizeof(app->settings.extensions_json));
    const char *items[] = {
        bongo_cat_i18n_get(app->i18n, "pages.preference.general.options.memoryGpu", "Video memory first"),
        bongo_cat_i18n_get(app->i18n, "pages.preference.general.options.memorySystem", "System memory first")};
    bongo_cat_pref_row_icon(context, BONGO_CAT_PREF_ICON_RENDER_QUALITY);
    int next = bongo_cat_pref_combo(context, "render-memory-preference",
        bongo_cat_i18n_get(app->i18n, "pages.preference.general.labels.memoryPreference", "Render memory preference"),
        bongo_cat_i18n_get(app->i18n, "pages.preference.general.hints.memoryPreference",
            "Live2D Vulkan only; reloads rendering. Video memory first prefers VRAM and minimizes CPU copies, upload staging and pipeline caches. System memory first prefers RAM. Required CPU access and unsupported allocations fall back automatically; integrated GPUs share memory."),
        items, 2, system ? 1 : 0);
    if ((next == 1) != system) {
        if (bongo_safe_render_system_memory_set(app->settings.extensions_json,
                sizeof(app->settings.extensions_json), next == 1)) app->render_backend_swap_pending = true;
        else bongo_cat_preferences_notice_show(app, bongo_cat_i18n_get(app->i18n,
            "pages.preference.general.hints.memoryPreferenceSaveFailed",
            "Unable to save the memory preference"), true);
    }
    if (system && !(bongo_cat_model_runtime_optimizations(app->model_runtime) & BONGO_CAT_OPTIMIZE_SYSTEM_MEMORY))
        bongo_cat_pref_status(context, "render-memory-status",
            bongo_cat_i18n_get(app->i18n, "pages.preference.general.labels.memoryPreference", "Render memory preference"),
            bongo_cat_i18n_get(app->i18n,
            "pages.preference.general.hints.memoryPreferenceUnavailable",
            "The current renderer does not support system memory preference."));
}
