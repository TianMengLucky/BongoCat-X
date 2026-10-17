use std::ffi::c_char;
#[no_mangle]
pub unsafe extern "C" fn bongo_safe_tauri_key_code(filename: *const c_char) -> i32 {
    crate::guard(-1, || {
        let Ok(filename) = crate::image_rows::text(filename) else {
            return -1;
        };
        let Some((name, ext)) = filename.rsplit_once('.') else {
            return -1;
        };
        if !ext.eq_ignore_ascii_case("png") || filename.len() >= 128 {
            return -1;
        }
        if let Ok(code) = name.trim_start().parse::<i32>() {
            if (0..=255).contains(&code) {
                return code;
            }
        }
        let bytes = name.as_bytes();
        if bytes.len() == 4
            && ((name.get(..3).is_some_and(|p| p.eq_ignore_ascii_case("Key"))
                && bytes[3].is_ascii_uppercase())
                || (name.get(..3).is_some_and(|p| p.eq_ignore_ascii_case("Num"))
                    && bytes[3].is_ascii_digit()))
        {
            return i32::from(bytes[3]);
        }
        if bytes.first().is_some_and(|b| *b == b'F' || *b == b'f') && bytes.len() <= 3 {
            if let Ok(code) = name[1..].parse::<i32>() {
                if (1..=24).contains(&code) {
                    return 111 + code;
                }
            }
        }
        let names: &[(&str, i32)] = &[
            ("Backspace", 8),
            ("Tab", 9),
            ("Return", 13),
            ("Pause", 19),
            ("CapsLock", 20),
            ("Escape", 27),
            ("Space", 32),
            ("PageUp", 33),
            ("PageDown", 34),
            ("End", 35),
            ("Home", 36),
            ("LeftArrow", 37),
            ("UpArrow", 38),
            ("RightArrow", 39),
            ("DownArrow", 40),
            ("PrintScreen", 44),
            ("Insert", 45),
            ("Delete", 46),
            ("Meta", 91),
            ("MetaLeft", 91),
            ("MetaRight", 92),
            ("Apps", 93),
            ("Kp0", 96),
            ("Kp1", 97),
            ("Kp2", 98),
            ("Kp3", 99),
            ("Kp4", 100),
            ("Kp5", 101),
            ("Kp6", 102),
            ("Kp7", 103),
            ("Kp8", 104),
            ("Kp9", 105),
            ("KpMultiply", 106),
            ("KpPlus", 107),
            ("KpMinus", 109),
            ("KpDecimal", 110),
            ("KpDivide", 111),
            ("NumLock", 144),
            ("ScrollLock", 145),
            ("Semicolon", 186),
            ("Equal", 187),
            ("Comma", 188),
            ("Minus", 189),
            ("Period", 190),
            ("Dot", 190),
            ("Slash", 191),
            ("BackQuote", 192),
            ("BracketLeft", 219),
            ("LeftBracket", 219),
            ("Backslash", 220),
            ("BracketRight", 221),
            ("RightBracket", 221),
            ("Quote", 222),
            ("Shift", 16),
            ("ShiftLeft", 16),
            ("ShiftRight", 16),
            ("Control", 17),
            ("ControlLeft", 17),
            ("ControlRight", 17),
            ("Alt", 18),
            ("AltGr", 18),
            ("South", 0),
            ("East", 1),
            ("West", 2),
            ("North", 3),
            ("LeftTrigger", 4),
            ("RightTrigger", 5),
            ("LeftTrigger2", 6),
            ("RightTrigger2", 7),
            ("LeftThumb", 8),
            ("RightThumb", 9),
            ("DPadLeft", 10),
            ("DPadRight", 11),
            ("DPadUp", 12),
            ("DPadDown", 13),
            ("Start", 14),
            ("Select", 15),
        ];
        names
            .iter()
            .find(|(key, _)| name.eq_ignore_ascii_case(key))
            .map_or(-1, |(_, code)| *code)
    })
}
