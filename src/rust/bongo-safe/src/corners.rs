include!(concat!(env!("OUT_DIR"), "/corners.rs"));
/// Immutable, aligned shader words embedded at build time. Caller never frees them.
/// # Safety
/// `bytes` must point to writable usize storage.
#[no_mangle]
pub unsafe extern "C" fn bongo_safe_corner_shader(fragment: bool, bytes: *mut usize) -> *const u32 {
    let words = if fragment { FRAGMENT } else { VERTEX };
    if bytes.is_null() {
        return std::ptr::null();
    }
    *bytes = std::mem::size_of_val(words);
    words.as_ptr()
}
