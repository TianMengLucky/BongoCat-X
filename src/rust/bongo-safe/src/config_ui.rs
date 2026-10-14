//! Preferences appearance and layout stored in the existing extensions JSON buffer.
use crate::unique::UniqueValue;
use serde_json::{Map, Value};
use std::ffi::c_char;

const CAP: usize = 4096;

unsafe fn read(extensions: *const c_char, capacity: usize) -> Option<Map<String, Value>> {
    if extensions.is_null() || capacity == 0 || capacity > CAP {
        return None;
    }
    let bytes = std::slice::from_raw_parts(extensions.cast::<u8>(), capacity);
    let length = bytes.iter().position(|&byte| byte == 0)?;
    if length == 0 {
        return Some(Map::new());
    }
    let UniqueValue(value) = serde_json::from_slice(&bytes[..length]).ok()?;
    value.as_object().cloned()
}

#[no_mangle]
pub unsafe extern "C" fn bongo_safe_preferences_collapsible_get(
    extensions: *const c_char,
    capacity: usize,
) -> bool {
    get_bool(extensions, capacity, "collapsibleOptions", false)
}

// Stored in extensions to preserve the public settings structure layout.
#[no_mangle]
pub unsafe extern "C" fn bongo_safe_gl_true_transparency_get(
    extensions: *const c_char, capacity: usize,
) -> bool {
    get_bool(extensions, capacity, "glTrueTransparency", false)
}

#[no_mangle]
pub unsafe extern "C" fn bongo_safe_gl_true_transparency_set(
    extensions: *mut c_char, capacity: usize, enabled: bool,
) -> bool {
    set_bool(extensions, capacity, "glTrueTransparency", enabled)
}

// Stored in extensions to preserve the public settings structure layout.
#[no_mangle]
pub unsafe extern "C" fn bongo_safe_render_system_memory_get(
    extensions: *const c_char, capacity: usize,
) -> bool {
    get_bool(extensions, capacity, "renderSystemMemory", false)
}

#[no_mangle]
pub unsafe extern "C" fn bongo_safe_render_system_memory_set(
    extensions: *mut c_char, capacity: usize, enabled: bool,
) -> bool {
    set_bool(extensions, capacity, "renderSystemMemory", enabled)
}

// Keep the existing public settings ABI; absent settings retain automatic checks.
#[no_mangle]
pub unsafe extern "C" fn bongo_safe_auto_update_get(
    extensions: *const c_char, capacity: usize,
) -> bool {
    get_bool(extensions, capacity, "autoCheckUpdates", true)
}

#[no_mangle]
pub unsafe extern "C" fn bongo_safe_auto_update_set(
    extensions: *mut c_char, capacity: usize, enabled: bool,
) -> bool {
    set_bool(extensions, capacity, "autoCheckUpdates", enabled)
}

unsafe fn get_bool(extensions: *const c_char, capacity: usize, key: &str, default: bool) -> bool {
    crate::guard(default, || {
        read(extensions, capacity)
            .and_then(|root| {
                root.get("preferencesUi")?
                    .get(key)?
                    .as_bool()
            })
            .unwrap_or(default)
    })
}

#[no_mangle]
pub unsafe extern "C" fn bongo_safe_preferences_collapsible_set(
    extensions: *mut c_char,
    capacity: usize,
    enabled: bool,
) -> bool {
    set_bool(extensions, capacity, "collapsibleOptions", enabled)
}

#[no_mangle]
pub unsafe extern "C" fn bongo_safe_preferences_rounded_get(
    extensions: *const c_char,
    capacity: usize,
) -> bool {
    get_bool(extensions, capacity, "roundedCorners", true)
}

#[no_mangle]
pub unsafe extern "C" fn bongo_safe_preferences_rounded_set(
    extensions: *mut c_char,
    capacity: usize,
    enabled: bool,
) -> bool {
    set_bool(extensions, capacity, "roundedCorners", enabled)
}

#[no_mangle]
pub unsafe extern "C" fn bongo_safe_statistics_overlay_get(
    extensions: *const c_char, capacity: usize,
) -> bool {
    get_bool(extensions, capacity, "statisticsOverlay", false)
}

#[no_mangle]
pub unsafe extern "C" fn bongo_safe_statistics_overlay_set(
    extensions: *mut c_char, capacity: usize, enabled: bool,
) -> bool {
    set_bool(extensions, capacity, "statisticsOverlay", enabled)
}

unsafe fn set_bool(extensions: *mut c_char, capacity: usize, key: &str, enabled: bool) -> bool {
    crate::guard(false, || {
        let Some(mut root) = read(extensions, capacity) else {
            return false;
        };
        let preferences = root
            .entry("preferencesUi")
            .or_insert_with(|| Value::Object(Map::new()));
        let Some(preferences) = preferences.as_object_mut() else {
            return false;
        };
        preferences.insert(key.into(), Value::Bool(enabled));
        let Ok(json) = serde_json::to_vec(&root) else {
            return false;
        };
        if json.len() >= capacity {
            return false;
        }
        let output = std::slice::from_raw_parts_mut(extensions.cast::<u8>(), capacity);
        output.fill(0);
        output[..json.len()].copy_from_slice(&json);
        true
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn automatic_updates_default_enabled_and_preserve_other_settings() {
        let mut buffer = [0u8; CAP];
        let initial = br#"{"other":{"value":42}}"#;
        buffer[..initial.len()].copy_from_slice(initial);
        unsafe {
            assert!(bongo_safe_auto_update_get(buffer.as_ptr().cast(), CAP));
            assert!(bongo_safe_auto_update_set(buffer.as_mut_ptr().cast(), CAP, false));
            assert!(!bongo_safe_auto_update_get(buffer.as_ptr().cast(), CAP));
            assert_eq!(read(buffer.as_ptr().cast(), CAP).unwrap()["other"]["value"], 42);
            assert!(bongo_safe_auto_update_set(buffer.as_mut_ptr().cast(), CAP, true));
            assert!(bongo_safe_auto_update_get(buffer.as_ptr().cast(), CAP));
        }
    }

    #[test]
    fn memory_preference_preserves_extensions_and_handles_invalid_buffers() {
        let mut buffer = [0u8; CAP];
        unsafe {
            assert!(!bongo_safe_render_system_memory_get(buffer.as_ptr().cast(), CAP));
            assert!(bongo_safe_preferences_rounded_set(buffer.as_mut_ptr().cast(), CAP, false));
            for enabled in [true, false, true] {
                assert!(bongo_safe_render_system_memory_set(buffer.as_mut_ptr().cast(), CAP, enabled));
                assert_eq!(bongo_safe_render_system_memory_get(buffer.as_ptr().cast(), CAP), enabled);
                assert!(!bongo_safe_preferences_rounded_get(buffer.as_ptr().cast(), CAP));
            }
            let before = buffer;
            assert!(!bongo_safe_render_system_memory_set(buffer.as_mut_ptr().cast(), 16, false));
            assert_eq!(buffer, before);
            assert!(!bongo_safe_render_system_memory_get(std::ptr::null(), CAP));
            assert!(!bongo_safe_render_system_memory_set(std::ptr::null_mut(), CAP, true));
            for initial in [br#"{"preferencesUi":{"renderSystemMemory":"true"}}"#.as_slice(),
                br#"{"preferencesUi":{"renderSystemMemory":true,"renderSystemMemory":false}}"#, b"{bad}"] {
                buffer.fill(0);
                buffer[..initial.len()].copy_from_slice(initial);
                assert!(!bongo_safe_render_system_memory_get(buffer.as_ptr().cast(), CAP));
            }
        }
    }

    #[test]
    fn statistics_overlay_defaults_and_preserves_other_settings() {
        let mut buffer = [0u8; CAP];
        let initial = br#"{"other":{"enabled":true},"preferencesUi":{"roundedCorners":false}}"#;
        buffer[..initial.len()].copy_from_slice(initial);
        unsafe {
            assert!(!bongo_safe_statistics_overlay_get(buffer.as_ptr().cast(), CAP));
            for enabled in [true, false, true] {
                assert!(bongo_safe_statistics_overlay_set(buffer.as_mut_ptr().cast(), CAP, enabled));
                assert_eq!(bongo_safe_statistics_overlay_get(buffer.as_ptr().cast(), CAP), enabled);
                assert!(!bongo_safe_preferences_rounded_get(buffer.as_ptr().cast(), CAP));
                assert_eq!(read(buffer.as_ptr().cast(), CAP).unwrap()["other"]["enabled"], true);
            }
            let before = buffer;
            assert!(!bongo_safe_statistics_overlay_set(buffer.as_mut_ptr().cast(), 8, false));
            assert_eq!(buffer, before);
            assert!(!bongo_safe_statistics_overlay_get(std::ptr::null(), CAP));
        }
    }

    #[test]
    fn corners_default_and_preserve_layout() {
        let mut buffer = [0u8; CAP];
        unsafe {
            assert!(bongo_safe_preferences_rounded_get(buffer.as_ptr().cast(), CAP));
            assert!(bongo_safe_preferences_collapsible_set(buffer.as_mut_ptr().cast(), CAP, true));
            for rounded in [false, true, false] {
                assert!(bongo_safe_preferences_rounded_set(buffer.as_mut_ptr().cast(), CAP, rounded));
                assert_eq!(bongo_safe_preferences_rounded_get(buffer.as_ptr().cast(), CAP), rounded);
                assert!(bongo_safe_preferences_collapsible_get(buffer.as_ptr().cast(), CAP));
            }
            let before = buffer;
            assert!(!bongo_safe_preferences_rounded_set(buffer.as_mut_ptr().cast(), 16, true));
            assert_eq!(buffer, before);
        }
    }

    #[test]
    fn layout_round_trips_without_erasing_other_extensions() {
        let mut buffer = [0u8; CAP];
        let initial = br#"{"other":{"enabled":true},"preferencesUi":{"extra":7}}"#;
        buffer[..initial.len()].copy_from_slice(initial);
        for enabled in [true, false, true] {
            unsafe {
                assert!(bongo_safe_preferences_collapsible_set(
                    buffer.as_mut_ptr().cast(),
                    CAP,
                    enabled
                ));
                assert_eq!(
                    bongo_safe_preferences_collapsible_get(buffer.as_ptr().cast(), CAP),
                    enabled
                );
                let root = read(buffer.as_ptr().cast(), CAP).unwrap();
                assert_eq!(root["other"]["enabled"], true);
                assert_eq!(root["preferencesUi"]["extra"], 7);
            }
        }
    }

    #[test]
    fn invalid_or_full_buffers_remain_unchanged_and_default_to_expanded() {
        for initial in [
            br#"{"preferencesUi":false}"#.as_slice(),
            b"{bad}",
            br#"{"x":1,"x":2}"#,
            b"{}",
        ] {
            let mut buffer = [0u8; 32];
            buffer[..initial.len()].copy_from_slice(initial);
            let before = buffer;
            unsafe {
                assert!(!bongo_safe_preferences_collapsible_get(
                    buffer.as_ptr().cast(),
                    buffer.len()
                ));
                assert!(!bongo_safe_preferences_collapsible_set(
                    buffer.as_mut_ptr().cast(),
                    buffer.len(),
                    true
                ));
            }
            assert_eq!(buffer, before);
        }
        let mut unterminated = [b'x'; 32];
        let before = unterminated;
        assert!(!unsafe {
            bongo_safe_preferences_collapsible_set(unterminated.as_mut_ptr().cast(), 32, true)
        });
        assert_eq!(unterminated, before);
        assert!(!unsafe { bongo_safe_preferences_collapsible_get(std::ptr::null(), CAP) });
    }
}
