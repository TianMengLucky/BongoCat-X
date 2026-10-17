//! Inox2D creates its management card while its renderer is loaded.
use bongo_plugin::card::Definition;
bongo_plugin::export_card!(
    bongo_cat_inox2d_ui_query_v1,
    Definition {
        id: "inox-plugin",
        title_key: "pages.preference.plugins.inoxTitle",
        title: "Inochi2D / Inox2D",
        description_key: "pages.preference.plugins.inoxDescription",
        description: "Inochi2D models in INP / INX containers.",
        busy: None,
        lock_active: true,
        toggle_active: false,
    }
);
