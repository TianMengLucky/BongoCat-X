//! Display units never alter the plugin's raw counters, database, or CSV exports.
use crate::config_ui::{read, set_value};
use serde_json::{json, Value};
use std::ffi::c_char;

const UNIT_CAP: usize = 65;
const UNIT_CHARACTERS: usize = 16;
const SCALE_MIN: f64 = 1e-9;
const SCALE_MAX: f64 = 1e9;

#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct StatisticsUnits {
    count: [u8; UNIT_CAP],
    movement: [u8; UNIT_CAP],
    scroll: [u8; UNIT_CAP],
    movement_scale: f64,
    scroll_scale: f64,
}

impl Default for StatisticsUnits {
    fn default() -> Self {
        Self {
            count: [0; UNIT_CAP],
            movement: [0; UNIT_CAP],
            scroll: [0; UNIT_CAP],
            movement_scale: 1.0,
            scroll_scale: 1.0,
        }
    }
}

fn valid_scale(scale: f64) -> bool {
    scale.is_finite() && (SCALE_MIN..=SCALE_MAX).contains(&scale)
}

fn valid_label(label: &str) -> bool {
    label.len() < UNIT_CAP
        && label.chars().count() <= UNIT_CHARACTERS
        && !label.chars().any(char::is_control)
}

fn label_bytes(value: &Value, key: &str) -> [u8; UNIT_CAP] {
    let mut result = [0; UNIT_CAP];
    if let Some(text) = value
        .get(key)
        .and_then(Value::as_str)
        .filter(|s| valid_label(s))
    {
        result[..text.len()].copy_from_slice(text.as_bytes());
    }
    result
}

fn label_text(bytes: &[u8; UNIT_CAP]) -> Option<&str> {
    let end = bytes.iter().position(|&byte| byte == 0)?;
    let text = std::str::from_utf8(&bytes[..end]).ok()?;
    valid_label(text).then_some(text.trim())
}

/// # Safety
/// `extensions` addresses `capacity` readable bytes; `out` is writable and aligned.
#[no_mangle]
pub unsafe extern "C" fn bongo_safe_statistics_units_get(
    extensions: *const c_char,
    capacity: usize,
    out: *mut StatisticsUnits,
) -> bool {
    crate::guard(false, || {
        let Some(out) = out.as_mut() else {
            return false;
        };
        *out = StatisticsUnits::default();
        let Some(root) = read(extensions, capacity) else {
            return false;
        };
        if let Some(value) = root
            .get("preferencesUi")
            .and_then(|v| v.get("statisticsUnits"))
        {
            out.count = label_bytes(value, "count");
            out.movement = label_bytes(value, "movement");
            out.scroll = label_bytes(value, "scroll");
            out.movement_scale = value
                .get("movementScale")
                .and_then(Value::as_f64)
                .filter(|&s| valid_scale(s))
                .unwrap_or(1.0);
            out.scroll_scale = value
                .get("scrollScale")
                .and_then(Value::as_f64)
                .filter(|&s| valid_scale(s))
                .unwrap_or(1.0);
        }
        true
    })
}

/// # Safety
/// `extensions` addresses `capacity` writable bytes; `units` is readable and aligned.
#[no_mangle]
pub unsafe extern "C" fn bongo_safe_statistics_units_set(
    extensions: *mut c_char,
    capacity: usize,
    units: *const StatisticsUnits,
) -> bool {
    crate::guard(false, || {
        let Some(units) = units.as_ref() else {
            return false;
        };
        let (Some(count), Some(movement), Some(scroll)) = (
            label_text(&units.count),
            label_text(&units.movement),
            label_text(&units.scroll),
        ) else {
            return false;
        };
        if !valid_scale(units.movement_scale) || !valid_scale(units.scroll_scale) {
            return false;
        }
        set_value(
            extensions,
            capacity,
            "statisticsUnits",
            json!({
                "count": count, "movement": movement, "scroll": scroll,
                "movementScale": units.movement_scale, "scrollScale": units.scroll_scale,
            }),
        )
    })
}

#[cfg(test)]
mod tests {
    use super::*;
    const CAP: usize = 4096;

    #[test]
    fn labels_accept_sixteen_four_byte_characters() {
        assert!(valid_label(&"𠀀".repeat(UNIT_CHARACTERS)));
        assert!(!valid_label(&"𠀀".repeat(UNIT_CHARACTERS + 1)));
    }

    #[test]
    fn custom_units_round_trip_without_touching_other_settings() {
        let mut buffer = [0u8; CAP];
        let initial = br#"{"other":{"enabled":true},"preferencesUi":{"textColor":1193046}}"#;
        buffer[..initial.len()].copy_from_slice(initial);
        let mut units = StatisticsUnits::default();
        units.count[..3].copy_from_slice("次".as_bytes());
        units.movement[..2].copy_from_slice(b"km");
        units.scroll[..3].copy_from_slice("圈".as_bytes());
        units.movement_scale = 0.000001;
        units.scroll_scale = 0.125;
        unsafe {
            let ptr = buffer.as_mut_ptr().cast();
            assert!(bongo_safe_statistics_units_set(ptr, CAP, &units));
            let mut restored = StatisticsUnits::default();
            assert!(bongo_safe_statistics_units_get(ptr, CAP, &mut restored));
            assert_eq!(restored, units);
            let root = read(ptr, CAP).unwrap();
            assert_eq!(root["other"]["enabled"], true);
            assert_eq!(root["preferencesUi"]["textColor"], 1193046);
            assert!(bongo_safe_statistics_units_set(
                ptr,
                CAP,
                &StatisticsUnits::default()
            ));
            assert!(bongo_safe_statistics_units_get(ptr, CAP, &mut restored));
            assert_eq!(restored, StatisticsUnits::default());
        }
    }

    #[test]
    fn invalid_values_and_buffers_do_not_modify_storage() {
        let mut buffer = [0u8; CAP];
        let mut units = StatisticsUnits::default();
        unsafe {
            let ptr = buffer.as_mut_ptr().cast();
            assert!(bongo_safe_statistics_units_set(ptr, CAP, &units));
            let before = buffer;
            for scale in [
                0.0,
                -1.0,
                f64::NAN,
                f64::INFINITY,
                SCALE_MIN / 2.0,
                SCALE_MAX * 2.0,
            ] {
                units.movement_scale = scale;
                assert!(!bongo_safe_statistics_units_set(ptr, CAP, &units));
                assert_eq!(buffer, before);
            }
            units = StatisticsUnits::default();
            for label in [b"bad\n".as_slice(), &[0xff], b"12345678901234567"] {
                units.count.fill(0);
                units.count[..label.len()].copy_from_slice(label);
                assert!(!bongo_safe_statistics_units_set(ptr, CAP, &units));
                assert_eq!(buffer, before);
            }
            units.count.fill(b'x');
            assert!(!bongo_safe_statistics_units_set(ptr, CAP, &units));
            assert_eq!(buffer, before);
            units = StatisticsUnits::default();
            assert!(!bongo_safe_statistics_units_set(ptr, 8, &units));
            assert!(!bongo_safe_statistics_units_set(ptr, CAP, std::ptr::null()));
            assert!(!bongo_safe_statistics_units_set(
                std::ptr::null_mut(),
                CAP,
                &units
            ));
            assert_eq!(buffer, before);
        }
    }

    #[test]
    fn malformed_or_absent_settings_fall_back_per_field() {
        let mut buffer = [0u8; CAP];
        let initial = br#"{"preferencesUi":{"statisticsUnits":{"count":"ok","movement":"bad\n","scroll":42,"movementScale":-1,"scrollScale":0}}}"#;
        buffer[..initial.len()].copy_from_slice(initial);
        let mut units = StatisticsUnits::default();
        unsafe {
            assert!(bongo_safe_statistics_units_get(
                buffer.as_ptr().cast(),
                CAP,
                &mut units
            ));
            assert_eq!(&units.count[..3], b"ok\0");
            assert_eq!(units.movement, [0; UNIT_CAP]);
            assert_eq!(units.scroll, [0; UNIT_CAP]);
            assert_eq!(units.movement_scale, 1.0);
            assert_eq!(units.scroll_scale, 1.0);
            assert!(!bongo_safe_statistics_units_get(
                std::ptr::null(),
                CAP,
                &mut units
            ));
            assert_eq!(units, StatisticsUnits::default());
            assert!(!bongo_safe_statistics_units_get(
                buffer.as_ptr().cast(),
                CAP,
                std::ptr::null_mut()
            ));
            buffer.fill(0);
            assert!(bongo_safe_statistics_units_get(
                buffer.as_ptr().cast(),
                CAP,
                &mut units
            ));
            assert_eq!(units, StatisticsUnits::default());
        }
    }
}
