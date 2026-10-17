//! This plugin owns its management card and exports the common card ABI.
use bongo_plugin::card::Definition;
bongo_plugin::export_card!(bongo_cat_statistics_ui_query_v1, Definition {
    id: "statistics-plugin", title_key: "pages.preference.statistics.title", title: "Statistics",
    description_key: "pages.preference.statistics.description",
    description: "Input counts, application time, daily/hourly charts, local storage and CSV export. Disabling keeps history.",
    busy: None, lock_active: false, toggle_active: true,
});

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn query_requires_the_card_abi() {
        assert!(bongo_cat_statistics_ui_query_v1(0).is_null());
        let api = unsafe { &*bongo_cat_statistics_ui_query_v1(1) };
        assert_eq!(
            api.struct_size as usize,
            std::mem::size_of::<bongo_plugin::ui::UiContribution>()
        );
    }
}
