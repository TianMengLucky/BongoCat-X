//! Memory-safety-critical parsing for BongoCat, exposed over the C ABI.
//!
//! Every untrusted byte stream the application touches (network responses,
//! user-imported model files, their textures, sounds and expression files) is
//! parsed here, in safe Rust, instead of in hand-rolled C. The C side owns
//! the constants and the file access; this crate owns the parsing and every
//! buffer it returns.
//!
//! Buffers returned to C are exactly-sized allocations released through the
//! matching `bongo_safe_free_*` function — never with `free()`. Every entry
//! point catches panics so nothing unwinds across the C boundary.

mod about;
mod audio;
mod config;
mod corners;
mod expression;
mod image;
pub mod inochi;
mod json_dom;
mod mver_json;
mod sha256;
mod unique;

/// Runs `body`, converting a panic into `fallback` instead of unwinding
/// across the C boundary.
pub(crate) fn guard<R>(fallback: R, body: impl FnOnce() -> R) -> R {
    std::panic::catch_unwind(std::panic::AssertUnwindSafe(body)).unwrap_or(fallback)
}

/// Hands an exactly-sized vector to C. The caller releases it through a
/// `bongo_safe_free_*` function with the same element count.
pub(crate) fn release_exact<T>(mut vector: Vec<T>) -> *mut T {
    debug_assert_eq!(vector.len(), vector.capacity());
    let pointer = vector.as_mut_ptr();
    std::mem::forget(vector);
    pointer
}

/// # Safety
/// `pointer` must come from `release_exact` with the same element count and
/// must not have been reclaimed before.
pub(crate) unsafe fn reclaim_exact<T>(pointer: *mut T, count: usize) -> Vec<T> {
    if pointer.is_null() || count == 0 {
        return Vec::new();
    }
    Vec::from_raw_parts(pointer, count, count)
}
