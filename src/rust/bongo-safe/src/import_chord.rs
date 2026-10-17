use serde_json::Value;
fn codes(row: &Value) -> Option<Vec<i64>> {
    row.as_array()?.iter().map(Value::as_i64).collect()
}
fn key(code: i64) -> Option<String> {
    let named = match code {
        1 => "Left",
        2 => "Right",
        4 => "Middle",
        5 => "Back",
        6 => "Forward",
        8 => "Backspace",
        9 => "Tab",
        13 => "Enter",
        16 => "Shift",
        17 => "Control",
        18 => "Alt",
        19 => "Pause",
        20 => "CapsLock",
        27 => "Escape",
        32 => "Space",
        33 => "PageUp",
        34 => "PageDown",
        35 => "End",
        36 => "Home",
        37 => "ArrowLeft",
        38 => "ArrowUp",
        39 => "ArrowRight",
        40 => "ArrowDown",
        44 => "PrintScreen",
        45 => "Insert",
        46 => "Delete",
        91 | 92 => "Meta",
        93 => "Apps",
        106 => "KpMultiply",
        107 => "KpPlus",
        109 => "KpMinus",
        110 => "KpDecimal",
        111 => "KpDivide",
        144 => "NumLock",
        145 => "ScrollLock",
        160 => "ShiftLeft",
        161 => "ShiftRight",
        162 => "ControlLeft",
        163 => "ControlRight",
        164 => "Alt",
        165 => "AltGr",
        186 => "Semicolon",
        187 => "=",
        188 => "Comma",
        189 => "-",
        190 => "Period",
        191 => "Slash",
        192 => "BackQuote",
        219 => "BracketLeft",
        220 => "Backslash",
        221 => "BracketRight",
        222 => "Quote",
        _ => "",
    };
    if !named.is_empty() {
        return Some(named.to_owned());
    }
    Some(match code {
        48..=57 | 65..=90 => char::from(code as u8).to_string(),
        96..=105 => format!("Kp{}", code - 96),
        112..=135 => format!("F{}", code - 111),
        1..=254 => code.to_string(),
        _ => return None,
    })
}
pub(super) fn sound(row: &Value) -> Option<String> {
    let codes = codes(row)?;
    if codes.len() == 1 && matches!(codes[0], 0 | 255) {
        return Some(String::new());
    }
    if codes.is_empty() {
        return None;
    }
    codes
        .into_iter()
        .map(key)
        .collect::<Option<Vec<_>>>()
        .map(|names| names.join("+"))
}
pub(super) fn keyboard(row: &Value) -> Option<String> {
    let codes = codes(row)?;
    let mut modifiers = [false; 4];
    let mut primary = None;
    for code in codes {
        if !(0..=255).contains(&code) {
            return None;
        }
        if code < 8 || code == 255 || (160..=165).contains(&code) {
            return sound(row);
        }
        let modifier = match code {
            17 => Some(0),
            16 => Some(1),
            18 => Some(2),
            91 | 92 => Some(3),
            _ => None,
        };
        if let Some(index) = modifier {
            modifiers[index] = true;
            continue;
        }
        if primary.is_some() {
            return sound(row);
        }
        primary = key(code);
        primary.as_ref()?;
    }
    let Some(primary) = primary else {
        return sound(row);
    };
    let mut keys: Vec<_> = ["Control", "Shift", "Alt", "Meta"]
        .into_iter()
        .zip(modifiers)
        .filter_map(|(name, on)| on.then(|| name.to_owned()))
        .collect();
    keys.push(primary);
    Some(keys.join("+"))
}
pub(super) fn valid(row: &Value, gamepad: bool, audio: bool) -> bool {
    if gamepad
        && row
            .as_array()
            .is_some_and(|a| a.len() == 1 && a[0].as_i64().is_some_and(|n| (0..16).contains(&n)))
    {
        return true;
    }
    (if audio { sound(row) } else { keyboard(row) }).is_some_and(|text| text.len() < 128)
}
#[no_mangle]
pub unsafe extern "C" fn bongo_safe_mver_chord(
    row: *const crate::json_dom::Node,
    audio: bool,
    output: *mut std::ffi::c_char,
    capacity: usize,
) -> bool {
    if output.is_null() || capacity == 0 {
        return false;
    }
    *output = 0;
    crate::guard(false, || {
        let Ok(row) = crate::json_dom::to_value(row, 0) else {
            return false;
        };
        let Some(value) = (if audio { sound(&row) } else { keyboard(&row) }) else {
            return false;
        };
        if value.len() >= capacity {
            return false;
        }
        std::ptr::copy_nonoverlapping(value.as_ptr(), output.cast(), value.len());
        *output.add(value.len()) = 0;
        true
    })
}
#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn preserves_arbitrary_audio_chords_sided_modifiers_and_disabled_rows() {
        assert_eq!(
            sound(&serde_json::json!([17, 65, 66])).as_deref(),
            Some("Control+A+B")
        );
        assert_eq!(
            keyboard(&serde_json::json!([17, 17, 65])).as_deref(),
            Some("Control+A")
        );
        assert_eq!(
            sound(&serde_json::json!([162, 1])).as_deref(),
            Some("ControlLeft+Left")
        );
        assert_eq!(sound(&serde_json::json!([255])).as_deref(), Some(""));
        assert!(sound(&serde_json::json!([255, 65])).is_none());
    }
}
