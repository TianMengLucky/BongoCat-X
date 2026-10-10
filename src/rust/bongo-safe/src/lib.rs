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
pub(crate) fn release_exact<T>(vector: Vec<T>) -> *mut T {
    // A growing Vec (notably decoded PCM) may have unused capacity. A boxed
    // slice discards it, and converting back guarantees capacity == length.
    let mut vector = vector.into_boxed_slice().into_vec();
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

#[cfg(test)]
mod allocation_tests {
    use super::{reclaim_exact, release_exact};

    #[test]
    fn transfers_growing_vectors_without_retaining_spare_capacity() {
        let mut values = Vec::<u64>::with_capacity(256);
        values.extend([11, 22, 33]);
        assert!(values.capacity() > values.len());
        let pointer = release_exact(values);
        let restored = unsafe { reclaim_exact(pointer, 3) };
        assert_eq!(restored, [11, 22, 33]);
        assert_eq!(restored.capacity(), restored.len());
    }

    #[test]
    fn empty_vectors_and_null_pointers_can_be_released() {
        let pointer = release_exact(Vec::<u8>::with_capacity(256));
        assert!(unsafe { reclaim_exact(pointer, 0) }.is_empty());
        assert!(unsafe { reclaim_exact::<u8>(std::ptr::null_mut(), 0) }.is_empty());
    }
}
