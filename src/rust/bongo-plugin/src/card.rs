//! Shared management widgets; each plugin supplies its own card definition.
use crate::ui::{CardState, Ui, UiContribution, UiHost, UiServices, BONGO_CAT_PLUGIN_UI_ABI};
use std::ffi::{c_void, CStr, CString};
use std::mem::size_of;

pub struct Definition {
    pub id: &'static str,
    pub title_key: &'static str,
    pub title: &'static str,
    pub description_key: &'static str,
    pub description: &'static str,
    pub busy: Option<(&'static str, &'static str)>,
    pub lock_active: bool,
    pub toggle_active: bool,
}
fn c(text: &str) -> CString {
    CString::new(text).expect("static card text")
}

/// # Safety
/// The host must lend valid UI/services tables and drawing context for this call.
pub unsafe fn draw(
    ui: *mut Ui,
    host: *const UiHost,
    services: *const UiServices,
    definition: &Definition,
) -> u32 {
    #[repr(C)]
    struct Header {
        abi: u32,
        size: u32,
    }
    unsafe fn valid<T>(p: *const T) -> bool {
        !p.is_null()
            && (*p.cast::<Header>()).abi == BONGO_CAT_PLUGIN_UI_ABI
            && (*p.cast::<Header>()).size as usize == size_of::<T>()
    }
    if ui.is_null() || !valid(host) || !valid(services) {
        return 0;
    }
    let host = &*host;
    let services = &*services;
    let tr = |key: &str, fallback: &str| {
        let key = c(key);
        let fallback = c(fallback);
        let translated = (host.text)(ui, key.as_ptr(), fallback.as_ptr());
        if translated.is_null() {
            fallback
        } else {
            CStr::from_ptr(translated).to_owned()
        }
    };
    let mut state = CardState::empty();
    (services.card_state)(services.context, &mut state);
    if !state.valid() {
        return 0;
    }
    if (host.card_begin)(ui, c(definition.id).as_ptr()) == 0 {
        return 1;
    }
    (host.label)(ui, tr(definition.title_key, definition.title).as_ptr());
    (host.label_wrapped)(
        ui,
        tr(definition.description_key, definition.description).as_ptr(),
        76.0,
    );
    let (key, fallback) = if state.active != 0 {
        ("active", "In use")
    } else if state.installed == 0 {
        ("missing", "Not installed")
    } else if state.enabled == 0 {
        ("disabled", "Disabled")
    } else {
        ("installed", "Installed")
    };
    (host.label)(
        ui,
        tr(&format!("pages.preference.plugins.{key}"), fallback).as_ptr(),
    );
    let mut enabled = if definition.toggle_active {
        state.active
    } else {
        state.enabled
    };
    let available =
        state.installed != 0 && state.busy == 0 && !(definition.lock_active && state.active != 0);
    if (host.toggle_available)(
        ui,
        c(&format!("enable-{}", definition.id)).as_ptr(),
        tr("pages.preference.plugins.enabled", "Enabled").as_ptr(),
        &mut enabled,
        available as u32,
    ) != 0
    {
        (services.set_enabled)(services.context, enabled);
    }
    if state.busy != 0 {
        if let Some((key, fallback)) = definition.busy {
            (host.label)(ui, tr(key, fallback).as_ptr());
        }
    } else if state.managed != 0 && !(definition.lock_active && state.active != 0) {
        (host.row)(ui, 1, 30.0);
        if (host.button)(
            ui,
            c(&format!("remove-{}", definition.id)).as_ptr(),
            tr("pages.preference.plugins.remove", "Remove plugin").as_ptr(),
        ) != 0
        {
            (services.remove)(services.context);
        }
    }
    (host.card_end)(ui);
    1
}

/// Exports a card with panic containment and the unchanged card ABI.
#[macro_export]
macro_rules! export_card {
    ($symbol:ident, $definition:expr) => {
        unsafe extern "C" fn draw_card(
            _instance: *mut std::ffi::c_void,
            ui: *mut $crate::ui::Ui,
            host: *const $crate::ui::UiHost,
            services: *const $crate::ui::UiServices,
        ) -> u32 {
            $crate::guard::guard(0, || $crate::card::draw(ui, host, services, &$definition))
        }
        static CARD: $crate::ui::UiContribution = $crate::ui::UiContribution {
            abi_version: $crate::ui::BONGO_CAT_PLUGIN_UI_ABI,
            struct_size: std::mem::size_of::<$crate::ui::UiContribution>() as u32,
            draw: draw_card,
            event: None,
        };
        #[no_mangle]
        pub extern "C" fn $symbol(abi: u32) -> *const $crate::ui::UiContribution {
            if abi == $crate::ui::BONGO_CAT_PLUGIN_UI_ABI {
                &CARD
            } else {
                std::ptr::null()
            }
        }
    };
}

// Keep the base contribution layout documented alongside the export macro.
const _: () = assert!(size_of::<UiContribution>() == 8 + 2 * size_of::<*const c_void>());
