//! This plugin owns its management card and exports the common card ABI.
use bongo_plugin::card::Definition;
bongo_plugin::export_card!(bongo_cat_store_ui_query_v1, Definition {
    id: "store-plugin", title_key: "pages.preference.store.title", title: "Store",
    description_key: "pages.preference.store.pluginHint",
    description: "Rust plugin for the community model catalog. The Store page appears while the plugin is enabled.",
    busy: None, lock_active: false, toggle_active: false,
});

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn query_requires_the_card_abi() {
        assert!(bongo_cat_store_ui_query_v1(0).is_null());
        let api = unsafe { &*bongo_cat_store_ui_query_v1(1) };
        assert_eq!(
            api.struct_size as usize,
            std::mem::size_of::<bongo_plugin::ui::UiContribution>()
        );
    }
}
