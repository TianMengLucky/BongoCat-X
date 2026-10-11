//! Optional preferences layout stored in the existing extensions JSON buffer.
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
    crate::guard(false, || {
        read(extensions, capacity)
            .and_then(|root| {
                root.get("preferencesUi")?
                    .get("collapsibleOptions")?
                    .as_bool()
            })
            .unwrap_or(false)
    })
}

#[no_mangle]
pub unsafe extern "C" fn bongo_safe_preferences_collapsible_set(
    extensions: *mut c_char,
    capacity: usize,
    enabled: bool,
) -> bool {
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
        preferences.insert("collapsibleOptions".into(), Value::Bool(enabled));
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
