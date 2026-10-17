//! Shared support for Rust BongoCat plugins.
//!
//! Holds the FFI bindings for the generic plugin UI ABI
//! (`include/bongo_cat/plugin_ui.h`) plus the small helpers every plugin
//! needs at the C boundary: panic containment and fixed-size UTF-8 copies.
//! Keeping them in one crate stops each plugin from re-deriving the same
//! layout and guards.

pub mod card;
pub mod events;
pub mod guard;
pub mod io;
pub mod locale;
pub mod models;
pub mod overlay;
pub mod page;
pub mod text;
pub mod transfer;
pub mod ui;

mod page_export;
