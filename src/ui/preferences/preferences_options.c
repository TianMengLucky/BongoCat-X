#include "preferences_state.h"
#include "preferences_options.h"
#include "preferences_theme.h"
#include "preferences_notice.h"
#include "bongo_cat/safe_ffi.h"
#include "ui_paint.h"

void bongo_cat_preferences_options_style(BongoCatPreferences *value, struct nk_context *context) {
    BongoCatApp *app = value->app;
    const char *labels[] = {
        bongo_cat_i18n_get(app->i18n, "pages.preference.optionsLayout.expanded", "Expanded options"),
        bongo_cat_i18n_get(app->i18n, "pages.preference.optionsLayout.folded", "Collapsible groups")};
    nk_layout_row_dynamic(context, 38, 1);
    int selected = bongo_cat_pref_segments(context, "options-layout", labels, 2,
        value->collapsible_options ? 1 : 0);
    if ((selected != 0) != value->collapsible_options) {
        if (bongo_safe_preferences_collapsible_set(app->settings.extensions_json,
            sizeof(app->settings.extensions_json), selected != 0)) {
            value->collapsible_options = selected != 0;
            value->scroll_current[0] = 0; value->scroll_target[0] = 0;
            nk_group_set_scroll(context, "page-settings", 0, 0);
            value->render_dirty = true;
        } else bongo_cat_preferences_notice_show(app, bongo_cat_i18n_get(app->i18n,
            "pages.preference.optionsLayout.saveFailed", "Unable to save the options layout"), true);
    }
    nk_layout_row_dynamic(context, 44, 1);
    nk_label_wrap(context, bongo_cat_i18n_get(app->i18n, "pages.preference.optionsLayout.hint",
        "Choose a layout. In collapsible mode, click a group title to show or hide its options."));
}

bool bongo_cat_preferences_options_begin(BongoCatPreferences *value,
    struct nk_context *context, enum BongoCatOptionsSection section,
    const char *title, BongoCatPrefIcon icon) {
    if (!value->collapsible_options) {
        bongo_cat_pref_section_icon(context, title, icon);
        return true;
    }
    struct nk_style_tab saved = context->style.tab;
    BongoCatUIPalette palette = bongo_cat_ui_palette(bongo_cat_ui_dark(context));
    context->style.tab.background = nk_style_item_color(palette.surface);
    context->style.tab.border_color = palette.border_subtle;
    context->style.tab.text = palette.text;
    context->style.tab.padding = nk_vec2(12, 12);
    context->style.tab.rounding = 10;
    context->style.tab.border = 1;
    enum nk_collapse_states before = value->options_sections[section];
    nk_style_push_font(context, bongo_cat_ui_label_font(context));
    bool open = nk_tree_state_push(context, NK_TREE_TAB, title, &value->options_sections[section]) != 0;
    nk_style_pop_font(context);
    context->style.tab = saved;
    if (before != value->options_sections[section]) value->render_dirty = true;
    if (open) bongo_cat_pref_section_rows_begin(context);
    return open;
}

void bongo_cat_preferences_options_end(BongoCatPreferences *value, struct nk_context *context) {
    if (value->collapsible_options) nk_tree_state_pop(context);
}
