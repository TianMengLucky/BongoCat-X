//! FFI bindings for the generic plugin UI ABI
//! (`include/bongo_cat/plugin_ui.h`).
//!
//! No Nuklear, SDL or graphics handle crosses this boundary: the host lends
//! an opaque [`Ui`] context plus this versioned widget table, and the plugin
//! builds its own page from the primitives. A second service table lets the
//! plugin read its own state and drive enable/remove without touching host
//! configuration directly.

use std::ffi::{c_char, c_void};
use std::mem::size_of;

/// ABI revision both sides must agree on.
pub const BONGO_CAT_PLUGIN_UI_ABI: u32 = 1;

/// Opaque host drawing context; only the host dereferences it.
#[repr(C)]
pub struct Ui {
    _private: [u8; 0],
}

/// Versioned widget service table the host lends to a plugin.
#[repr(C)]
pub struct UiHost {
    pub abi_version: u32,
    pub struct_size: u32,
    pub text: unsafe extern "C" fn(*mut Ui, *const c_char, *const c_char) -> *const c_char,
    pub intro: unsafe extern "C" fn(*mut Ui, *const c_char),
    pub card_begin: unsafe extern "C" fn(*mut Ui, *const c_char) -> u32,
    pub card_end: unsafe extern "C" fn(*mut Ui),
    pub label: unsafe extern "C" fn(*mut Ui, *const c_char),
    pub label_wrapped: unsafe extern "C" fn(*mut Ui, *const c_char, f32),
    pub spacer: unsafe extern "C" fn(*mut Ui, f32),
    pub row: unsafe extern "C" fn(*mut Ui, u32, f32),
    pub button: unsafe extern "C" fn(*mut Ui, *const c_char, *const c_char) -> u32,
    pub toggle: unsafe extern "C" fn(*mut Ui, *const c_char, *const c_char, *mut u32) -> u32,
    pub toggle_available:
        unsafe extern "C" fn(*mut Ui, *const c_char, *const c_char, *mut u32, u32) -> u32,
    pub slider: unsafe extern "C" fn(
        *mut Ui,
        *const c_char,
        *const c_char,
        *const c_char,
        *mut f32,
        f32,
        f32,
        f32,
        f32,
    ) -> u32,
}

/// What the host knows about one plugin.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct CardState {
    pub struct_size: u32,
    pub installed: u32,
    pub enabled: u32,
    pub active: u32,
    pub managed: u32,
    pub busy: u32,
}

impl CardState {
    /// A zeroed state carrying the expected size, ready for `card_state`.
    pub const fn empty() -> Self {
        Self {
            struct_size: size_of::<Self>() as u32,
            installed: 0,
            enabled: 0,
            active: 0,
            managed: 0,
            busy: 0,
        }
    }

    /// True when the host filled the record in.
    pub fn valid(&self) -> bool {
        self.struct_size == size_of::<Self>() as u32
    }
}

/// Host operations bound to a single plugin. `context` is host-owned; forward
/// it back to the callbacks and never retain the table.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct UiServices {
    pub abi_version: u32,
    pub struct_size: u32,
    pub context: *mut c_void,
    pub card_state: unsafe extern "C" fn(*mut c_void, *mut CardState),
    /// Recorded by the host and applied after `draw` returns, because
    /// disabling or removing a plugin unloads the library running this code.
    pub set_enabled: unsafe extern "C" fn(*mut c_void, u32),
    pub remove: unsafe extern "C" fn(*mut c_void),
}

/// Input event routed to the plugin while its page is displayed.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct UiEvent {
    pub struct_size: u32,
    pub kind: u32,
    pub x: f32,
    pub y: f32,
    pub button: u32,
}

/// Optional UI contribution the plugin exports under its own symbol.
#[repr(C)]
pub struct UiContribution {
    pub abi_version: u32,
    pub struct_size: u32,
    pub draw: unsafe extern "C" fn(*mut c_void, *mut Ui, *const UiHost, *const UiServices) -> u32,
    pub event: Option<unsafe extern "C" fn(*mut c_void, *const UiEvent) -> u32>,
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn event_layout_matches_the_c_abi() {
        assert_eq!(size_of::<UiEvent>(), 20);
    }

    #[test]
    fn card_state_layout_matches_the_c_abi() {
        assert_eq!(size_of::<CardState>(), 24);
        assert!(CardState::empty().valid());
    }

    #[test]
    fn host_layout_matches_the_c_abi() {
        // Two u32 header fields followed by twelve function pointers.
        assert_eq!(size_of::<UiHost>(), 8 + 12 * size_of::<usize>());
    }

    #[test]
    fn services_layout_matches_the_c_abi() {
        // Two u32 header fields, the opaque context and three callbacks.
        assert_eq!(size_of::<UiServices>(), 8 + 4 * size_of::<usize>());
    }

    #[test]
    fn contribution_layout_matches_the_c_abi() {
        // Two u32 header fields, a required draw pointer and a nullable event.
        assert_eq!(size_of::<UiContribution>(), 8 + 2 * size_of::<usize>());
    }
}
