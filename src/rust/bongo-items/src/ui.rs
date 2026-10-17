//! This plugin owns its management card and exports the common card ABI.
use bongo_plugin::card::Definition;
bongo_plugin::export_card!(bongo_cat_items_ui_query_v1, Definition {
    id: "items-plugin", title_key: "pages.preference.items.title", title: "Items",
    description_key: "pages.preference.items.pluginHint",
    description: "Image props with independent placement, layers and saved scenes. Disabling hides the page and props while preserving data.",
    busy: None, lock_active: false, toggle_active: false,
});

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn query_requires_the_card_abi() {
        assert!(bongo_cat_items_ui_query_v1(0).is_null());
        let api = unsafe { &*bongo_cat_items_ui_query_v1(1) };
        assert_eq!(
            api.struct_size as usize,
            std::mem::size_of::<bongo_plugin::ui::UiContribution>()
        );
    }
}
