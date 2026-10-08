//! Comment-preserving Mver JSON5 edits through jsonc-parser's CST.
use jsonc_parser::{
    cst::{CstArray, CstInputValue, CstNode, CstRootNode},
    ParseOptions,
};
use serde_json::Value;
use std::{
    ffi::{c_char, c_void, CStr, CString},
    ptr,
};

fn input(value: Value) -> CstInputValue {
    match value {
        Value::Null => CstInputValue::Null,
        Value::Bool(v) => CstInputValue::Bool(v),
        Value::Number(v) => CstInputValue::Number(v.to_string()),
        Value::String(v) => CstInputValue::String(v),
        Value::Array(v) => CstInputValue::Array(v.into_iter().map(input).collect()),
        Value::Object(v) => {
            CstInputValue::Object(v.into_iter().map(|(k, v)| (k, input(v))).collect())
        }
    }
}
fn parse(text: &str) -> Result<CstRootNode, String> {
    let text = text.trim_start_matches('\u{feff}');
    if text.len() > 16 * 1024 * 1024 {
        return Err("Mver config exceeds 16 MiB".into());
    }
    let _: Value = crate::json_dom::read::compatible_json(text).map_err(|(_, e)| e)?;
    CstRootNode::parse(
        text,
        &ParseOptions {
            allow_missing_commas: false,
            ..Default::default()
        },
    )
    .map_err(|e| e.to_string())
}
fn replace_value(node: CstNode, value: CstInputValue) {
    if let Some(n) = node.as_array() {
        n.replace_with(value);
    } else if let Some(n) = node.as_object() {
        n.replace_with(value);
    } else if let Some(n) = node.as_number_lit() {
        n.replace_with(value);
    } else if let Some(n) = node.as_string_lit() {
        n.replace_with(value);
    } else if let Some(n) = node.as_boolean_lit() {
        n.replace_with(value);
    } else if let Some(n) = node.as_null_keyword() {
        n.replace_with(value);
    } else if let Some(n) = node.as_word_lit() {
        n.replace_with(value);
    }
}
fn replace_array(array: CstArray, values: Vec<Value>) {
    let elements = array.elements();
    for (index, value) in values.iter().cloned().enumerate() {
        if let Some(old) = elements.get(index) {
            replace_value(old.clone(), input(value));
        } else {
            array.append(input(value));
        }
    }
    for old in elements.into_iter().skip(values.len()).rev() {
        old.remove();
    }
}
fn edit(text: &str, mode: &str, field: &str, index: i32, row: &str) -> Result<String, String> {
    if !(-1..5120).contains(&index) {
        return Err("Invalid Mver row index".into());
    }
    let row: Value = json5::from_str(row).map_err(|e| e.to_string())?;
    let values = row
        .as_array()
        .ok_or("Mver shortcut row must be an array")?
        .clone();
    let root = parse(text)?;
    let obj = root.object_value().ok_or("Mver config must be an object")?;
    let section = obj
        .object_value_or_create(mode)
        .ok_or("Mver mode must be an object")?;
    let array = section.array_value_or_set(field);
    if index < 0 {
        replace_array(array, values);
    } else {
        while array.elements().len() < index as usize {
            array.append(input(serde_json::json!([255])));
        }
        if let Some(old) = array.elements().get(index as usize).cloned() {
            if let Some(old_array) = old.as_array() {
                replace_array(old_array, values);
            } else {
                replace_value(old, input(row));
            }
        } else {
            array.append(input(row));
        }
    }
    let mut output = root.to_string();
    if text.starts_with('\u{feff}') {
        output.insert(0, '\u{feff}');
    }
    let _: Value =
        json5::from_str(output.trim_start_matches('\u{feff}')).map_err(|e| e.to_string())?;
    Ok(output)
}
unsafe fn text<'a>(p: *const c_char) -> Option<&'a str> {
    if p.is_null() {
        None
    } else {
        CStr::from_ptr(p).to_str().ok()
    }
}
/// # Safety
/// Buffers must be valid and strings NUL terminated. Free output with bongo_json_free_text.
#[no_mangle]
pub unsafe extern "C" fn bongo_safe_mver_edit(
    data: *const c_char,
    len: usize,
    mode: *const c_char,
    field: *const c_char,
    index: i32,
    row: *const c_char,
) -> *mut c_char {
    crate::guard(ptr::null_mut(), || {
        if data.is_null() || len > 16 * 1024 * 1024 {
            return ptr::null_mut();
        }
        let (Ok(data), Some(mode), Some(field), Some(row)) = (
            std::str::from_utf8(std::slice::from_raw_parts(data.cast(), len)),
            text(mode),
            text(field),
            text(row),
        ) else {
            return ptr::null_mut();
        };
        edit(data, mode, field, index, row)
            .ok()
            .and_then(|v| CString::new(v).ok())
            .map(CString::into_raw)
            .unwrap_or(ptr::null_mut())
    })
}
fn label(node: &CstNode) -> Option<String> {
    if let Some(comment) = node.as_comment() {
        let text = comment.to_string();
        if let Some(text) = text.strip_prefix("//") {
            let text = text.trim();
            if !text.is_empty() {
                return Some(text.to_owned());
            }
        }
    }
    if let CstNode::Container(container) = node {
        for child in container.children() {
            if let Some(label) = label(&child) {
                return Some(label);
            }
        }
    }
    None
}
/// # Safety
/// The callback must remain valid and must copy the borrowed label before returning.
#[no_mangle]
pub unsafe extern "C" fn bongo_safe_mver_labels(
    data: *const c_char,
    len: usize,
    mode: *const c_char,
    callback: Option<
        unsafe extern "C" fn(*mut c_void, *const c_char, usize, *const c_char) -> bool,
    >,
    userdata: *mut c_void,
) -> bool {
    crate::guard(false, || {
        if data.is_null() || len > 16 * 1024 * 1024 {
            return false;
        }
        let (Ok(data), Some(mode), Some(callback)) = (
            std::str::from_utf8(std::slice::from_raw_parts(data.cast(), len)),
            text(mode),
            callback,
        ) else {
            return false;
        };
        let Ok(root) = parse(data) else {
            return false;
        };
        let Some(obj) = root.object_value() else {
            return false;
        };
        if obj.object_value(mode).is_none() {
            return false;
        }
        for field in [
            "l2d_expression",
            "l2d_motion",
            "l2d_motion_lockhand",
            "sounds",
        ] {
            let source = if field == "l2d_motion_lockhand"
                || (mode == "gamepad" && matches!(field, "l2d_expression" | "l2d_motion"))
            {
                "standard"
            } else {
                mode
            };
            if let Some(array) = obj.object_value(source).and_then(|s| s.array_value(field)) {
                for (index, row) in array.elements().into_iter().enumerate().take(5120) {
                    if let Some(label) = label(&row) {
                        let (Ok(field), Ok(label)) = (CString::new(field), CString::new(label))
                        else {
                            continue;
                        };
                        if !callback(userdata, field.as_ptr(), index, label.as_ptr()) {
                            return false;
                        }
                    }
                }
            }
        }
        true
    })
}
#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn preserve_authored_comments_and_unrelated_fields() {
        let source = "{standard:{l2d_motion:[[65, // Author label\n66]]},other:'unchanged'}";
        let result = edit(source, "standard", "l2d_motion", 0, "[67,68]").unwrap();
        assert!(result.contains("// Author label"));
        assert!(result.contains("other:'unchanged'"));
        let json: Value = json5::from_str(&result).unwrap();
        assert_eq!(
            json["standard"]["l2d_motion"][0],
            serde_json::json!([67, 68])
        );
    }
    #[test]
    fn validate_before_edit_and_fill_missing_rows() {
        assert!(edit("{broken", "standard", "l2d_motion", 0, "[66]").is_err());
        let result = edit(
            "{standard:{l2d_motion:null}}",
            "standard",
            "l2d_motion",
            3,
            "[66]",
        )
        .unwrap();
        let json: Value = json5::from_str(&result).unwrap();
        assert_eq!(json["standard"]["l2d_motion"][2], serde_json::json!([255]));
    }
}
