use super::*;
use std::ffi::c_void;

#[repr(C)]
pub struct ParseError {
    code: i32,
    pub pos: usize,
    message: [c_char; 256],
}
fn parse(bytes: &[u8], flags: u32) -> Result<Value, (usize, String)> {
    if bytes.len() > 16 * 1024 * 1024 {
        return Err((0, "JSON exceeds 16 MiB".into()));
    }
    let text = if flags & 2 != 0 {
        String::from_utf8_lossy(bytes)
    } else {
        std::borrow::Cow::Borrowed(
            std::str::from_utf8(bytes).map_err(|e| (e.valid_up_to(), e.to_string()))?,
        )
    };
    let text = if flags & 16 != 0 || flags & 1 != 0 {
        text.trim_start_matches('\u{feff}')
    } else {
        &text
    };
    match serde_json::from_str(text) {
        Ok(value) => Ok(value),
        Err(error) => {
            let offset = text
                .split_inclusive('\n')
                .take(error.line().saturating_sub(1))
                .map(str::len)
                .sum::<usize>()
                + error.column().saturating_sub(1);
            if flags & (1 | 4 | 8) != 0 {
                compatible_json(text)
            } else {
                Err((offset, error.to_string()))
            }
        }
    }
}
// json5/Pest has no nesting limit. This guard only bounds nesting; the
// existing libraries perform all syntax parsing and value conversion.
pub(crate) fn compatible_json(text: &str) -> Result<Value, (usize, String)> {
    let mut iter = text.bytes().enumerate().peekable();
    let mut depth = 0usize;
    while let Some((pos, byte)) = iter.next() {
        match byte {
            b'"' | b'\'' => {
                while let Some((_, next)) = iter.next() {
                    if next == b'\\' {
                        iter.next();
                    } else if next == byte {
                        break;
                    }
                }
            }
            b'/' if iter.peek().is_some_and(|(_, b)| *b == b'/') => {
                for (_, b) in iter.by_ref() {
                    if b == b'\n' {
                        break;
                    }
                }
            }
            b'/' if iter.peek().is_some_and(|(_, b)| *b == b'*') => {
                iter.next();
                while let Some((_, b)) = iter.next() {
                    if b == b'*' && iter.peek().is_some_and(|(_, b)| *b == b'/') {
                        iter.next();
                        break;
                    }
                }
            }
            b'{' | b'[' => {
                depth += 1;
                if depth > 128 {
                    return Err((pos, "JSON nesting exceeds 128".into()));
                }
            }
            b'}' | b']' => depth = depth.saturating_sub(1),
            _ => {}
        }
    }
    let options = jsonc_parser::ParseOptions {
        allow_missing_commas: false,
        ..Default::default()
    };
    match jsonc_parser::parse_to_serde_value(text, &options) {
        Ok(value) => Ok(value),
        Err(error) => json5::from_str(text).map_err(|e| (error.range().start, e.to_string())),
    }
}

unsafe fn failure(error: *mut ParseError, pos: usize, message: &str) {
    if let Some(error) = error.as_mut() {
        error.code = 1;
        error.pos = pos;
        error.message.fill(0);
        for (to, from) in error.message.iter_mut().take(255).zip(message.bytes()) {
            *to = from as c_char;
        }
    }
}
/// # Safety
/// All input buffers must be valid for their lengths; allocator must be null.
#[no_mangle]
pub unsafe extern "C" fn bongo_json_read_opts(
    data: *const c_char,
    len: usize,
    flags: u32,
    _: *const c_void,
    error: *mut ParseError,
) -> *mut Doc {
    crate::guard(ptr::null_mut(), || {
        if let Some(error) = error.as_mut() {
            error.code = 0;
            error.pos = 0;
            error.message.fill(0);
        }
        if data.is_null() || len == 0 || len > 16 * 1024 * 1024 {
            failure(error, 0, "Invalid JSON buffer size");
            return ptr::null_mut();
        }
        let bytes = std::slice::from_raw_parts(data.cast(), len);
        match parse(bytes, flags) {
            Ok(value) => {
                let mut doc = Box::new(Doc::new());
                doc.root = doc.from_value(value);
                Box::into_raw(doc)
            }
            Err((pos, message)) => {
                failure(error, pos, &message);
                ptr::null_mut()
            }
        }
    })
}
#[no_mangle]
pub unsafe extern "C" fn bongo_json_read(data: *const c_char, len: usize, flags: u32) -> *mut Doc {
    bongo_json_read_opts(data, len, flags, ptr::null(), ptr::null_mut())
}
#[no_mangle]
pub unsafe extern "C" fn bongo_json_doc_free(doc: *mut Doc) {
    crate::guard((), || {
        if !doc.is_null() {
            drop(Box::from_raw(doc));
        }
    });
}
#[no_mangle]
pub unsafe extern "C" fn bongo_json_doc_get_root(doc: *const Doc) -> *mut Node {
    doc.as_ref().map(|v| v.root).unwrap_or(ptr::null_mut())
}
#[no_mangle]
pub unsafe extern "C" fn bongo_safe_model_json_parse(
    data: *const c_char,
    len: usize,
    normalized: *mut bool,
) -> *mut Doc {
    crate::guard(ptr::null_mut(), || {
        if let Some(normalized) = normalized.as_mut() {
            *normalized = false;
        }
        let strict = bongo_json_read(data, len, 0);
        if !strict.is_null() || data.is_null() || len > 4 * 1024 * 1024 {
            return strict;
        }
        let mut error = ParseError {
            code: 0,
            pos: 0,
            message: [0; 256],
        };
        let mut result = bongo_json_read_opts(data, len, 4 | 8 | 16, ptr::null(), &mut error);
        if result.is_null() && error.pos < len {
            let original = std::slice::from_raw_parts(data.cast::<u8>(), len);
            let text = std::str::from_utf8(original).unwrap_or("");
            let p = error.pos + text.len() - text.trim_start_matches('\u{feff}').len();
            if p >= len {
                return ptr::null_mut();
            }
            let next = |start| {
                original
                    .iter()
                    .enumerate()
                    .skip(start)
                    .find(|(_, v)| !v.is_ascii_whitespace())
                    .map(|(i, _)| i)
            };
            if original[p] == b']' {
                if let Some(second) = next(p + 1) {
                    if let Some(third) = next(second + 1) {
                        if original[second] == b'}' && original[third] == b'}' {
                            let mut repaired = original.to_vec();
                            repaired[p] = b' ';
                            repaired[second] = b' ';
                            result = bongo_json_read(repaired.as_ptr().cast(), len, 4 | 8 | 16);
                            if !result.is_null() {
                                let value = to_value((*result).root, 0).ok();
                                if !value
                                    .as_ref()
                                    .is_some_and(|v| v["FileReferences"].is_object())
                                {
                                    bongo_json_doc_free(result);
                                    result = ptr::null_mut();
                                }
                            }
                        }
                    }
                }
            }
        }
        if let Some(normalized) = normalized.as_mut() {
            *normalized = !result.is_null();
        }
        result
    })
}
