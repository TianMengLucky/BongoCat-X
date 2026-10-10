//! Mver hand-layout files (`hand-layout.json`).
//!
//! Multi-key display joins extracted characters inside one shared bubble
//! and draws the first held key's hand. It uses three source regions: the
//! hand, the bubble frame and its character area, plus separator spacing. These are
//! authored per model in `resources/<group>/hand-layout.json`. The file is
//! optional: the C side pre-fills its defaults and fields absent from the
//! document keep those values, mirroring the configuration-JSON semantics.

use crate::config::{integer, object, write_message, Failure, LAYOUT, OK};
use crate::unique::UniqueValue;
use serde_json::Value;
use std::os::raw::{c_char, c_int};

const SIZE_LIMIT: usize = 256 * 1024;
const COORDINATE_LIMIT: i64 = 32768;
const RECT_LIMIT: i64 = 8192;
const GAP_LIMIT: i64 = 4096;
const MAX_KEYS_LIMIT: i64 = 8;
const DESCRIPTION: &str = "Layout";

/// A source-sprite region in image pixels, top-left origin.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct BongoSafeHandRect {
    pub x: c_int,
    pub y: c_int,
    pub width: c_int,
    pub height: c_int,
}

/// `hand-layout.json` fields via the C ABI. The caller pre-fills defaults.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct BongoSafeHandLayout {
    pub hand: BongoSafeHandRect,
    pub bubble: BongoSafeHandRect,
    pub characters: BongoSafeHandRect,
    pub gap: c_int,
    pub max_keys: c_int,
}

fn rect_ok(rect: &BongoSafeHandRect) -> bool {
    let x = rect.x as i64;
    let y = rect.y as i64;
    let width = rect.width as i64;
    let height = rect.height as i64;
    x >= 0
        && x <= COORDINATE_LIMIT
        && y >= 0
        && y <= COORDINATE_LIMIT
        && width >= 1
        && width <= RECT_LIMIT
        && height >= 1
        && height <= RECT_LIMIT
}

fn read_rect(
    root: &serde_json::Map<String, Value>,
    key: &str,
    target: &mut BongoSafeHandRect,
) -> Result<(), Failure> {
    let Some(fields) = object(root, DESCRIPTION, key)? else {
        return Ok(());
    };
    integer(fields, DESCRIPTION, "x", &mut target.x)?;
    integer(fields, DESCRIPTION, "y", &mut target.y)?;
    integer(fields, DESCRIPTION, "width", &mut target.width)?;
    integer(fields, DESCRIPTION, "height", &mut target.height)?;
    Ok(())
}

fn parse_document(
    bytes: &[u8],
    layout: &mut BongoSafeHandLayout,
) -> Result<(), Failure> {
    if bytes.len() > SIZE_LIMIT {
        return Err(Failure::format(
            "hand-layout.json exceeds the size limit".to_string(),
        ));
    }
    let UniqueValue(parsed) = serde_json::from_slice::<UniqueValue>(bytes)
        .map_err(|_| Failure::format("Invalid or duplicate-key hand-layout JSON".to_string()))?;
    let root = parsed
        .as_object()
        .ok_or_else(|| Failure::format("hand-layout.json must be an object".to_string()))?;
    if let Some(version) = root.get("schemaVersion") {
        let version = version
            .as_i64()
            .or_else(|| version.as_u64().map(|value| value as i64));
        if version != Some(1) {
            return Err(Failure::format(
                "hand-layout.json schemaVersion must be 1".to_string(),
            ));
        }
    }
    read_rect(root, "hand", &mut layout.hand)?;
    read_rect(root, "bubble", &mut layout.bubble)?;
    // Older files only describe the frame. Derive its interior until the
    // author supplies an explicit character crop.
    if root.get("bubble").is_some_and(Value::is_object)
        && !root.get("characters").is_some_and(Value::is_object) {
        let bubble = layout.bubble;
        layout.characters = BongoSafeHandRect {
            x: bubble.x.saturating_add(bubble.width.saturating_mul(24) / 204),
            y: bubble.y.saturating_add(bubble.height.saturating_mul(38) / 174),
            width: (bubble.width.saturating_mul(168) / 204).max(1),
            height: (bubble.height.saturating_mul(101) / 174).max(1),
        };
    }
    read_rect(root, "characters", &mut layout.characters)?;
    integer(root, DESCRIPTION, "gap", &mut layout.gap)?;
    integer(root, DESCRIPTION, "maxKeys", &mut layout.max_keys)?;
    if !rect_ok(&layout.hand) || !rect_ok(&layout.bubble) ||
        !rect_ok(&layout.characters) {
        return Err(Failure::format(
            "hand-layout.json rectangles are out of range".to_string(),
        ));
    }
    let bubble = &layout.bubble;
    let characters = &layout.characters;
    if characters.x < bubble.x || characters.y < bubble.y ||
        characters.x as i64 + characters.width as i64 >
            bubble.x as i64 + bubble.width as i64 ||
        characters.y as i64 + characters.height as i64 >
            bubble.y as i64 + bubble.height as i64 {
        return Err(Failure::format(
            "hand-layout.json characters must fit inside the bubble".to_string(),
        ));
    }
    if layout.gap < 0 || layout.gap as i64 > GAP_LIMIT {
        return Err(Failure::format(
            "hand-layout.json field 'gap' is out of range".to_string(),
        ));
    }
    if layout.max_keys < 1 || layout.max_keys as i64 > MAX_KEYS_LIMIT {
        return Err(Failure::format(
            "hand-layout.json field 'maxKeys' is out of range".to_string(),
        ));
    }
    Ok(())
}

/// # Safety
/// `bytes` must be readable for `size` bytes; `layout` must be writable for
/// `layout_size` bytes and pre-filled with the caller's defaults; `message`
/// must be writable for `message_capacity` bytes. Returns a BongoCatResult
/// code (0 = OK) and writes a diagnostic into `message` on failure.
#[no_mangle]
pub unsafe extern "C" fn bongo_safe_hand_layout_parse(
    bytes: *const u8,
    size: usize,
    layout: *mut BongoSafeHandLayout,
    layout_size: usize,
    message: *mut c_char,
    message_capacity: usize,
) -> c_int {
    crate::guard(LAYOUT, || {
        if bytes.is_null() && size > 0 || layout.is_null() || message.is_null() {
            return LAYOUT;
        }
        if layout_size != std::mem::size_of::<BongoSafeHandLayout>() {
            return LAYOUT;
        }
        let data = if size == 0 {
            &[][..]
        } else {
            std::slice::from_raw_parts(bytes, size)
        };
        let mut parsed = *layout;
        match parse_document(data, &mut parsed) {
            Ok(()) => {
                *layout = parsed;
                OK
            },
            Err(failure) => write_message(message, message_capacity, &failure),
        }
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    fn defaults() -> BongoSafeHandLayout {
        BongoSafeHandLayout {
            hand: BongoSafeHandRect { x: 450, y: 629, width: 317, height: 257 },
            bubble: BongoSafeHandRect { x: 47, y: 123, width: 204, height: 174 },
            characters: BongoSafeHandRect { x: 71, y: 161, width: 168, height: 101 },
            gap: 12,
            max_keys: 5,
        }
    }

    fn parse(json: &str) -> Result<BongoSafeHandLayout, c_int> {
        let mut layout = defaults();
        let mut message = [0i8; 256];
        let code = unsafe {
            bongo_safe_hand_layout_parse(
                json.as_ptr(),
                json.len(),
                &mut layout,
                std::mem::size_of::<BongoSafeHandLayout>(),
                message.as_mut_ptr(),
                message.len(),
            )
        };
        if code == OK {
            Ok(layout)
        } else {
            assert!(message[0] != 0, "failures must explain themselves");
            Err(code)
        }
    }

    #[test]
    fn parses_authored_fields_and_keeps_absent_defaults() {
        let layout = parse(
            r#"{"schemaVersion":1,"hand":{"x":10,"y":20,"width":100,"height":80},
               "bubble":{"x":1,"y":2,"width":30,"height":40},"gap":6,"maxKeys":3}"#,
        )
        .unwrap();
        assert_eq!((layout.hand.x, layout.hand.y, layout.hand.width, layout.hand.height), (10, 20, 100, 80));
        assert_eq!((layout.bubble.x, layout.bubble.y, layout.bubble.width, layout.bubble.height), (1, 2, 30, 40));
        assert_eq!(layout.gap, 6);
        assert_eq!(layout.max_keys, 3);
        // A partial file only overrides what it names.
        let partial = parse(r#"{"bubble":{"width":64}}"#).unwrap();
        assert_eq!(partial.bubble.width, 64);
        assert_eq!(partial.bubble.x, 47, "absent fields keep the defaults");
        assert_eq!(partial.hand.width, 317);
        assert_eq!(partial.gap, 12);
        let empty = parse("{}").unwrap();
        assert_eq!(empty.hand.width, 317);
        assert_eq!(empty.max_keys, 5);
    }

    #[test]
    fn rejects_wrong_types_versions_and_ranges() {
        for json in [
            "not json",
            "[]",
            r#"{"schemaVersion":2}"#,
            r#"{"schemaVersion":"1"}"#,
            r#"{"hand":"nope"}"#,
            r#"{"hand":{"width":1.5}}"#,
            r#"{"hand":{"width":0}}"#,
            r#"{"hand":{"x":-1}}"#,
            r#"{"bubble":{"y":-1}}"#,
            r#"{"gap":1,"gap":2}"#,
            r#"{"bubble":{"width":30,"width":40}}"#,
            r#"{"bubble":{"height":0}}"#,
            r#"{"gap":-1}"#,
            r#"{"gap":99999}"#,
            r#"{"maxKeys":0}"#,
            r#"{"maxKeys":9}"#,
        ] {
            assert!(parse(json).is_err(), "{json} must be rejected");
        }
        let oversized = format!("{{\"hand\":{{\"x\":{}}}}}", "1".repeat(SIZE_LIMIT));
        assert!(parse(&oversized).is_err());
    }

    #[test]
    fn failure_keeps_the_callers_layout_unchanged() {
        let mut layout = defaults();
        let mut message = [0i8; 256];
        let json = br#"{"hand":{"x":20},"maxKeys":0}"#;
        let code = unsafe {
            bongo_safe_hand_layout_parse(
                json.as_ptr(), json.len(), &mut layout,
                std::mem::size_of::<BongoSafeHandLayout>(),
                message.as_mut_ptr(), message.len(),
            )
        };
        assert_ne!(code, OK);
        assert_eq!(layout.hand.x, 450);
        assert_eq!(layout.max_keys, 5);
        assert_ne!(message[0], 0);
    }

    #[test]
    fn boundary_arguments_fail_cleanly() {
        let mut layout = defaults();
        let mut message = [0i8; 64];
        unsafe {
            assert_eq!(
                bongo_safe_hand_layout_parse(
                    std::ptr::null::<u8>(),
                    4,
                    &mut layout,
                    std::mem::size_of::<BongoSafeHandLayout>(),
                    message.as_mut_ptr(),
                    message.len(),
                ),
                LAYOUT
            );
            assert_eq!(
                bongo_safe_hand_layout_parse(
                    b"{}".as_ptr(),
                    2,
                    &mut layout,
                    std::mem::size_of::<BongoSafeHandLayout>() + 1,
                    message.as_mut_ptr(),
                    message.len(),
                ),
                LAYOUT
            );
            assert_eq!(
                bongo_safe_hand_layout_parse(
                    b"{}".as_ptr(),
                    2,
                    std::ptr::null_mut(),
                    std::mem::size_of::<BongoSafeHandLayout>(),
                    message.as_mut_ptr(),
                    message.len(),
                ),
                LAYOUT
            );
        }
    }
}