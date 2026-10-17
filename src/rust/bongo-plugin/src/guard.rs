//! Panic containment for `extern "C"` entry points.

use std::panic::{catch_unwind, AssertUnwindSafe};

/// Runs `body`, returning `fallback` when it panics.
///
/// Unwinding across the C boundary is undefined behaviour, so every
/// `extern "C"` function a plugin exports must funnel its body through this.
pub fn guard<T>(fallback: T, body: impl FnOnce() -> T) -> T {
    catch_unwind(AssertUnwindSafe(body)).unwrap_or(fallback)
}

#[cfg(test)]
mod tests {
    use super::guard;

    #[test]
    fn panics_are_contained() {
        assert_eq!(guard(7, || panic!("boom")), 7);
    }

    #[test]
    fn normal_results_pass_through() {
        assert_eq!(guard(0, || 42), 42);
    }
}
