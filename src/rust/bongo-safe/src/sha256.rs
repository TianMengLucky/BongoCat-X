//! SHA-256 over the C ABI, replacing the hand-rolled implementation in
//! `src/core/sha256.c` with the audited `sha2` crate.

use crate::guard;
use sha2::{Digest, Sha256};
use std::os::raw::{c_char, c_void};

/// Streaming state owned by C between `new` and `finish`.
pub struct Sha256State {
    hasher: Sha256,
}

#[no_mangle]
pub extern "C" fn bongo_safe_sha256_new() -> *mut Sha256State {
    guard(std::ptr::null_mut(), || {
        Box::into_raw(Box::new(Sha256State {
            hasher: Sha256::new(),
        }))
    })
}

/// # Safety
/// `state` must be live or NULL; `data` must be readable for `size` bytes.
#[no_mangle]
pub unsafe extern "C" fn bongo_safe_sha256_update(
    state: *mut Sha256State,
    data: *const c_void,
    size: usize,
) {
    if state.is_null() || (data.is_null() && size > 0) {
        return;
    }
    guard((), || {
        let bytes: &[u8] = if size == 0 {
            &[]
        } else {
            std::slice::from_raw_parts(data.cast::<u8>(), size)
        };
        (*state).hasher.update(bytes);
    })
}

/// Consumes the state and writes 64 lowercase hex digits plus a NUL into
/// `output` (65 bytes).
///
/// # Safety
/// `output` must be writable for 65 bytes; `state` must be live or NULL and
/// is released by this call.
#[no_mangle]
pub unsafe extern "C" fn bongo_safe_sha256_finish(state: *mut Sha256State, output: *mut c_char) {
    if output.is_null() {
        return;
    }
    let digest = guard(Vec::new(), || {
        if state.is_null() {
            return Vec::new();
        }
        Box::from_raw(state).hasher.finalize().to_vec()
    });
    let out = std::slice::from_raw_parts_mut(output.cast::<u8>(), 65);
    out.fill(0);
    const HEX: &[u8; 16] = b"0123456789abcdef";
    for (index, byte) in digest.iter().enumerate().take(32) {
        out[index * 2] = HEX[(byte >> 4) as usize];
        out[index * 2 + 1] = HEX[(byte & 15) as usize];
    }
    out[64] = 0;
}

/// # Safety
/// `data` must be readable for `size` bytes and `output` writable for 65.
#[no_mangle]
pub unsafe extern "C" fn bongo_safe_sha256_bytes(
    data: *const c_void,
    size: usize,
    output: *mut c_char,
) {
    let state = bongo_safe_sha256_new();
    if state.is_null() {
        *output = 0;
        return;
    }
    bongo_safe_sha256_update(state, data, size);
    bongo_safe_sha256_finish(state, output);
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::release_exact;

    fn hex(data: &[u8]) -> String {
        let mut output = [0 as c_char; 65];
        unsafe { bongo_safe_sha256_bytes(data.as_ptr().cast(), data.len(), output.as_mut_ptr()) };
        let bytes = unsafe { std::slice::from_raw_parts(output.as_ptr().cast::<u8>(), 65) };
        String::from_utf8(bytes.to_vec()).unwrap()
    }

    #[test]
    fn matches_known_vectors() {
        assert_eq!(
            hex(b""),
            "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855\0"
        );
        assert_eq!(
            hex(b"abc"),
            "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad\0"
        );
        assert_eq!(
            hex(b"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"),
            "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1\0"
        );
    }

    #[test]
    fn streams_like_the_one_shot() {
        let mut state = Box::new(Sha256State {
            hasher: Sha256::new(),
        });
        state.hasher.update(b"abcdbcdecdef");
        state
            .hasher
            .update(b"defgefghfghighijhijkijkljklmklmnlmnomnopnopq");
        let mut output = [0 as c_char; 65];
        unsafe {
            let raw = Box::into_raw(state);
            bongo_safe_sha256_finish(raw, output.as_mut_ptr())
        };
        let bytes = unsafe { std::slice::from_raw_parts(output.as_ptr().cast::<u8>(), 65) };
        assert_eq!(
            String::from_utf8_lossy(bytes),
            "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1\0"
        );
    }

    #[test]
    fn release_exact_round_trips() {
        let vector = vec![7u8; 16];
        let pointer = release_exact(vector);
        let recovered = unsafe { crate::reclaim_exact(pointer, 16) };
        assert_eq!(recovered, vec![7u8; 16]);
    }
}
