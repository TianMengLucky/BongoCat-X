//! This plugin owns its management card and exports the common card ABI.
use bongo_plugin::card::Definition;
bongo_plugin::export_card!(bongo_cat_sounds_ui_query_v1, Definition {
    id: "sounds-plugin", title_key: "pages.preference.sounds.title", title: "Sound effects",
    description_key: "pages.preference.sounds.pluginHint",
    description: "Assign a sound to each keyboard key. A dedicated worker handles import, decoding, playback and saving. Disabling preserves bindings and audio files.",
    busy: Some(("pages.preference.sounds.busy", "Worker is processing audio")), lock_active: false, toggle_active: false,
});

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn query_requires_the_card_abi() {
        assert!(bongo_cat_sounds_ui_query_v1(0).is_null());
        let api = unsafe { &*bongo_cat_sounds_ui_query_v1(1) };
        assert_eq!(
            api.struct_size as usize,
            std::mem::size_of::<bongo_plugin::ui::UiContribution>()
        );
    }
}
