//! Fixed-size text helpers for ABI structs.

use std::ffi::{c_char, CStr};

/// Copies `value` into a fixed, NUL-terminated buffer, truncating on a
/// character boundary so the result is never split mid-codepoint.
pub fn buffer<const N: usize>(value: &str) -> [u8; N] {
    let mut out = [0u8; N];
    let capacity = N.saturating_sub(1);
    let mut end = value.len().min(capacity);
    while end > 0 && !value.is_char_boundary(end) {
        end -= 1;
    }
    out[..end].copy_from_slice(&value.as_bytes()[..end]);
    out
}

/// Borrows a NUL-terminated UTF-8 C string, returning `None` for a null
/// pointer or invalid UTF-8.
///
/// # Safety
/// `text` must be null or point to a NUL-terminated buffer.
pub unsafe fn cstr<'a>(text: *const c_char) -> Option<&'a str> {
    if text.is_null() {
        return None;
    }
    CStr::from_ptr(text).to_str().ok()
}

#[cfg(test)]
mod tests {
    use super::{buffer, cstr};
    use std::ffi::CString;

    #[test]
    fn buffers_are_nul_terminated() {
        let out: [u8; 4] = buffer("abc");
        assert_eq!(&out, b"abc\0");
    }

    #[test]
    fn truncation_keeps_character_boundaries() {
        let out: [u8; 4] = buffer("abcd");
        assert_eq!(&out, b"abc\0");
        let out: [u8; 3] = buffer("éé");
        assert_eq!(&out, &[0xC3, 0xA9, 0x00]);
    }

    #[test]
    fn cstr_rejects_null_and_borrows_utf8() {
        assert!(unsafe { cstr(std::ptr::null()) }.is_none());
        let value = CString::new("hi").unwrap();
        assert_eq!(unsafe { cstr(value.as_ptr()) }, Some("hi"));
    }
}
