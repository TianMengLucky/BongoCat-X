//! Live2D expression (`.exp3.json`) parsing for the Live2D bridge.
//!
//! The Cubism SDK bundles a hand-rolled JSON parser whose number reader
//! (`CubismJson::ParseNumeric`) accepts only `-`, digits and `.` — one number
//! in scientific notation, which Cubism Editor itself exports (for example
//! `"Value": -2.980232238769531e-7`), fails the whole file and the expression
//! silently never applies (upstream issue #68). The bridge therefore parses
//! expression files here and builds the SDK motion from the parsed record
//! instead of handing raw expression bytes to the SDK parser.
//!
//! Semantics mirror `CubismExpressionMotion::Parse` wherever it is
//! well-defined: missing `FadeInTime`/`FadeOutTime` default to one second,
//! unknown blend modes fall back to `Add`, non-numeric `Value`s to `0`, and a
//! missing or non-array `Parameters` key yields an (empty) expression rather
//! than an error.

use crate::{guard, reclaim_exact, release_exact};
use serde_json::Value as Json;
use std::ffi::CString;
use std::os::raw::{c_char, c_int};

/// Blend mode of an expression parameter (`bongo_safe_expression_parameter`).
pub const BLEND_ADD: u8 = 0;
pub const BLEND_MULTIPLY: u8 = 1;
pub const BLEND_OVERWRITE: u8 = 2;

const SIZE_LIMIT: usize = 4 * 1024 * 1024;
const PARAMETER_LIMIT: usize = 4096;
const ID_LIMIT: usize = 1024;
const DEFAULT_FADE_SECONDS: f32 = 1.0;

#[repr(C)]
pub struct BongoSafeExpressionParameter {
    pub id: *mut c_char,
    pub value: f32,
    pub blend: u8,
}

#[repr(C)]
pub struct BongoSafeExpression {
    pub fade_in_seconds: f32,
    pub fade_out_seconds: f32,
    pub parameters: *mut BongoSafeExpressionParameter,
    pub parameter_count: c_int,
}

struct ParameterDraft {
    id: CString,
    value: f32,
    blend: u8,
}

struct ExpressionDraft {
    fade_in: f32,
    fade_out: f32,
    parameters: Vec<ParameterDraft>,
}

fn parse_document(bytes: &[u8]) -> Option<ExpressionDraft> {
    if bytes.len() > SIZE_LIMIT {
        return None;
    }
    let document = serde_json::from_slice::<Json>(bytes).ok()?;
    let root = document.as_object()?;
    let mut draft = ExpressionDraft {
        fade_in: fade_seconds(root.get("FadeInTime")),
        fade_out: fade_seconds(root.get("FadeOutTime")),
        parameters: Vec::new(),
    };
    if let Some(parameters) = root.get("Parameters").and_then(Json::as_array) {
        // A file naming more parameters than any model could address is
        // rejected outright rather than silently truncated.
        if parameters.len() > PARAMETER_LIMIT {
            return None;
        }
        for parameter in parameters {
            if let Some(item) = parse_parameter(parameter) {
                draft.parameters.push(item);
            }
        }
    }
    Some(draft)
}

/// Entries without a usable string `Id` are dropped: no model parameter can
/// match them anyway. The SDK parser would instead intern a garbage id.
fn parse_parameter(parameter: &Json) -> Option<ParameterDraft> {
    let object = parameter.as_object()?;
    let id = object.get("Id")?.as_str()?;
    if id.is_empty() || id.len() > ID_LIMIT {
        return None;
    }
    Some(ParameterDraft {
        id: CString::new(id).ok()?,
        value: object.get("Value").and_then(Json::as_f64).unwrap_or(0.0) as f32,
        blend: blend_mode(object.get("Blend")),
    })
}

fn fade_seconds(value: Option<&Json>) -> f32 {
    value
        .and_then(Json::as_f64)
        .map_or(DEFAULT_FADE_SECONDS, |seconds| seconds as f32)
}

/// Missing, null, `"Add"` or any unknown value all mean Additive, like the
/// SDK parser does.
fn blend_mode(value: Option<&Json>) -> u8 {
    match value.and_then(Json::as_str) {
        Some("Multiply") => BLEND_MULTIPLY,
        Some("Overwrite") => BLEND_OVERWRITE,
        _ => BLEND_ADD,
    }
}

/// Moves `draft` across the boundary. Every buffer the record points at is an
/// exact allocation released through `bongo_safe_free_expression`.
fn publish(draft: ExpressionDraft) -> *mut BongoSafeExpression {
    let count = draft.parameters.len();
    let mut parameters: Vec<BongoSafeExpressionParameter> = Vec::with_capacity(count);
    for item in draft.parameters {
        parameters.push(BongoSafeExpressionParameter {
            id: item.id.into_raw(),
            value: item.value,
            blend: item.blend,
        });
    }
    let shell = vec![BongoSafeExpression {
        fade_in_seconds: draft.fade_in,
        fade_out_seconds: draft.fade_out,
        parameters: release_exact(parameters),
        parameter_count: count as c_int,
    }];
    release_exact(shell)
}

/// # Safety
/// `data` must be readable for `size` bytes and `out_expression` writable.
/// On success `*out_expression` holds a record released by
/// `bongo_safe_free_expression`; on failure the call returns false and
/// leaves `*out_expression` untouched.
#[no_mangle]
pub unsafe extern "C" fn bongo_safe_expression_parse(
    data: *const u8,
    size: usize,
    out_expression: *mut *mut BongoSafeExpression,
) -> bool {
    guard(false, || {
        if data.is_null() || out_expression.is_null() {
            return false;
        }
        let bytes = std::slice::from_raw_parts(data, size);
        match parse_document(bytes) {
            Some(draft) => {
                *out_expression = publish(draft);
                true
            }
            None => false,
        }
    })
}

/// Releases a record returned by `bongo_safe_expression_parse` together with
/// every parameter id. Null and zero-count records are tolerated.
///
/// # Safety
/// `expression` must come from `bongo_safe_expression_parse` and must not
/// have been released before.
#[no_mangle]
pub unsafe extern "C" fn bongo_safe_free_expression(expression: *mut BongoSafeExpression) {
    guard((), || {
        if expression.is_null() {
            return;
        }
        let shell = std::ptr::read(expression);
        let count = shell.parameter_count.max(0) as usize;
        if !shell.parameters.is_null() {
            let parameters = std::slice::from_raw_parts(shell.parameters, count);
            for parameter in parameters {
                if !parameter.id.is_null() {
                    drop(CString::from_raw(parameter.id));
                }
            }
            drop(reclaim_exact(shell.parameters, count));
        }
        drop(reclaim_exact(expression, 1));
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    fn parse(json: &str) -> Option<*mut BongoSafeExpression> {
        unsafe {
            let mut expression: *mut BongoSafeExpression = std::ptr::null_mut();
            if !bongo_safe_expression_parse(json.as_ptr(), json.len(), &mut expression) {
                return None;
            }
            Some(expression)
        }
    }

    /// Reads a released record back: the point is the field values.
    unsafe fn fields(expression: *const BongoSafeExpression) -> BongoSafeExpression {
        std::ptr::read(expression)
    }

    #[test]
    fn parses_scientific_notation_from_editor_exports() {
        // The exact value from the official "50.自定义短发.exp3.json" preset
        // in upstream issue #68.
        let json = concat!(
            "{\n",
            "  \"Type\": \"Live2D Expression\",\n",
            "  \"FadeInTime\": 0.25,\n",
            "  \"FadeOutTime\": 0.5,\n",
            "  \"Parameters\": [\n",
            "    {\"Id\": \"Param707\", \"Value\": -2.980232238769531e-7, \"Blend\": \"Add\"},\n",
            "    {\"Id\": \"ParamAngleX\", \"Value\": 15, \"Blend\": \"Multiply\"},\n",
            "    {\"Id\": \"ParamMouthOpenY\", \"Value\": 0.75, \"Blend\": \"Overwrite\"}\n",
            "  ]\n",
            "}\n"
        );
        let expression = parse(json).expect("scientific notation must parse");
        let fields = unsafe { fields(expression) };
        assert_eq!(fields.parameter_count, 3);
        assert!((fields.fade_in_seconds - 0.25).abs() < f32::EPSILON);
        assert!((fields.fade_out_seconds - 0.5).abs() < f32::EPSILON);
        let parameters = unsafe {
            std::slice::from_raw_parts(fields.parameters, fields.parameter_count as usize)
        };
        assert_eq!(
            unsafe { std::ffi::CStr::from_ptr(parameters[0].id) }
                .to_str()
                .unwrap(),
            "Param707"
        );
        assert!((parameters[0].value - (-2.980_232_2e-7_f32)).abs() < f32::EPSILON);
        assert_eq!(parameters[0].blend, BLEND_ADD);
        assert_eq!(
            unsafe { std::ffi::CStr::from_ptr(parameters[1].id) }
                .to_str()
                .unwrap(),
            "ParamAngleX"
        );
        assert!((parameters[1].value - 15.0).abs() < f32::EPSILON);
        assert_eq!(parameters[1].blend, BLEND_MULTIPLY);
        assert_eq!(parameters[2].blend, BLEND_OVERWRITE);
        assert!((parameters[2].value - 0.75).abs() < f32::EPSILON);
        unsafe { bongo_safe_free_expression(expression) };
    }

    #[test]
    fn mirrors_sdk_defaults_and_fallbacks() {
        // Missing fades default to one second; a missing or non-array
        // Parameters key parses to an empty expression; unknown blends and
        // non-numeric values fall back exactly like the SDK parser.
        for (json, count) in [
            ("{}", 0usize),
            ("{\"Parameters\": \"nope\"}", 0),
            (
                "{\"Parameters\": [{\"Id\": \"P\", \"Blend\": \"Nonsense\"}]}",
                1,
            ),
            ("{\"Parameters\": [{\"Id\": \"P\", \"Value\": \"1.5\"}]}", 1),
        ] {
            let expression = parse(json).unwrap_or_else(|| panic!("{json} must parse"));
            let fields = unsafe { fields(expression) };
            assert!(
                (fields.fade_in_seconds - DEFAULT_FADE_SECONDS).abs() < f32::EPSILON,
                "{json}"
            );
            assert!(
                (fields.fade_out_seconds - DEFAULT_FADE_SECONDS).abs() < f32::EPSILON,
                "{json}"
            );
            assert_eq!(fields.parameter_count, count as c_int, "{json}");
            let parameters: &[BongoSafeExpressionParameter] = if fields.parameters.is_null() {
                &[]
            } else {
                unsafe {
                    std::slice::from_raw_parts(
                        fields.parameters,
                        fields.parameter_count.max(0) as usize,
                    )
                }
            };
            for parameter in &parameters[..count] {
                assert!((parameter.value - 0.0).abs() < f32::EPSILON, "{json}");
                assert_eq!(parameter.blend, BLEND_ADD, "{json}");
            }
            unsafe { bongo_safe_free_expression(expression) };
        }
    }

    #[test]
    fn rejects_invalid_documents_and_pathological_files() {
        for json in [
            "not json",
            "[]",
            "{\"Parameters\": [",
            // More parameters than any model could address: rejected, not
            // silently truncated.
            &format!(
                "{{\"Parameters\": [{}]}}",
                vec!["{\"Id\": \"P\"}"; PARAMETER_LIMIT + 1].join(",")
            ),
        ] {
            assert!(parse(json).is_none(), "{json} must be rejected");
        }
    }

    #[test]
    fn drops_entries_without_a_usable_id() {
        let json = concat!(
            "{\"Parameters\": [",
            "{\"Value\": 1},",                    // no Id
            "{\"Id\": 7, \"Value\": 1},",         // non-string Id
            "{\"Id\": \"\", \"Value\": 1},",      // empty Id
            "{\"Id\": \"Keep\", \"Value\": 2.5}", // survives
            "]}"
        );
        let expression = parse(json).expect("parse");
        let fields = unsafe { fields(expression) };
        assert_eq!(fields.parameter_count, 1);
        let parameters = unsafe { std::slice::from_raw_parts(fields.parameters, 1) };
        assert_eq!(
            unsafe { std::ffi::CStr::from_ptr(parameters[0].id) }
                .to_str()
                .unwrap(),
            "Keep"
        );
        assert!((parameters[0].value - 2.5).abs() < f32::EPSILON);
        unsafe { bongo_safe_free_expression(expression) };
    }

    #[test]
    fn boundary_arguments_fail_cleanly() {
        unsafe {
            let mut expression: *mut BongoSafeExpression = std::ptr::null_mut();
            assert!(!bongo_safe_expression_parse(
                std::ptr::null::<u8>(),
                4,
                &mut expression
            ));
            assert!(!bongo_safe_expression_parse(
                "{}".as_ptr(),
                2,
                std::ptr::null_mut()
            ));
            assert!(!bongo_safe_expression_parse(
                "{}".as_ptr(),
                0,
                &mut expression
            ));
            bongo_safe_free_expression(std::ptr::null_mut());
        }
    }
}
