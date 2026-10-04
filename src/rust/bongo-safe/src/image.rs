//! Image decoding over the C ABI, replacing `stb_image` (and the WebP
//! decoder dependency) for every untrusted byte stream: model textures,
//! contributor avatars and embedded feed images.

use crate::{guard, reclaim_exact, release_exact};
use image::ImageReader;
use std::io::Cursor;
use std::os::raw::c_int;

/// Hard bounds for hostile files. The dimension cap sits far above every
/// legitimate GPU texture limit the app checks after decoding (up to
/// 65536), and the allocation cap bounds memory for absurd dimensions the
/// same way the old C decoder's OOM envelope did.
const MAX_DIMENSION: u32 = 1 << 18;
const MAX_ALLOC_BYTES: u64 = 2 * 1024 * 1024 * 1024;

fn reader(data: &[u8]) -> Option<ImageReader<Cursor<&[u8]>>> {
    let mut reader = ImageReader::new(Cursor::new(data));
    reader = reader.with_guessed_format().ok()?;
    let mut limits = image::Limits::default();
    limits.max_image_width = Some(MAX_DIMENSION);
    limits.max_image_height = Some(MAX_DIMENSION);
    limits.max_alloc = Some(MAX_ALLOC_BYTES);
    reader.limits(limits);
    Some(reader)
}

/// # Safety
/// `data` must be readable for `size` bytes; `width`/`height` writable.
/// Returns an exactly `width * height * 4` byte RGBA buffer released with
/// `bongo_safe_free_pixels`, or NULL.
#[no_mangle]
pub unsafe extern "C" fn bongo_safe_image_decode(
    data: *const u8,
    size: usize,
    width: *mut c_int,
    height: *mut c_int,
) -> *mut u8 {
    if width.is_null() || height.is_null() || (data.is_null() && size > 0) {
        if !width.is_null() {
            *width = 0;
        }
        if !height.is_null() {
            *height = 0;
        }
        return std::ptr::null_mut();
    }
    *width = 0;
    *height = 0;
    let pixels = guard(None, || -> Option<*mut u8> {
        let bytes: &[u8] = if size == 0 {
            &[]
        } else {
            std::slice::from_raw_parts(data, size)
        };
        let decoded = reader(bytes)?.decode().ok()?;
        let rgba = decoded.to_rgba8();
        let (decoded_width, decoded_height) = rgba.dimensions();
        if decoded_width == 0 || decoded_height == 0 {
            return None;
        }
        let total = decoded_width as usize * decoded_height as usize * 4;
        // A fresh exact-size buffer so the free side can reclaim it with the
        // element count alone.
        let mut exact = vec![0u8; total];
        exact.copy_from_slice(rgba.as_raw());
        *width = decoded_width as c_int;
        *height = decoded_height as c_int;
        Some(release_exact(exact))
    })
    .unwrap_or(std::ptr::null_mut());
    if pixels.is_null() {
        *width = 0;
        *height = 0;
    }
    pixels
}

/// Cheap header-only dimensions for the same formats `decode` accepts.
///
/// # Safety
/// `data` must be readable for `size` bytes; `width`/`height` writable.
#[no_mangle]
pub unsafe extern "C" fn bongo_safe_image_info(
    data: *const u8,
    size: usize,
    width: *mut c_int,
    height: *mut c_int,
) -> bool {
    if width.is_null() || height.is_null() || (data.is_null() && size > 0) {
        return false;
    }
    *width = 0;
    *height = 0;
    guard(false, || -> bool {
        let bytes: &[u8] = if size == 0 {
            &[]
        } else {
            std::slice::from_raw_parts(data, size)
        };
        let dimensions = match reader(bytes) {
            Some(reader) => match reader.into_dimensions() {
                Ok(dimensions) => dimensions,
                Err(_) => return false,
            },
            None => return false,
        };
        if dimensions.0 == 0 || dimensions.1 == 0 {
            return false;
        }
        *width = dimensions.0 as c_int;
        *height = dimensions.1 as c_int;
        true
    })
}

/// # Safety
/// `pointer` must come from `bongo_safe_image_decode` with the same pixel
/// count and must not have been freed yet.
#[no_mangle]
pub unsafe extern "C" fn bongo_safe_free_pixels(pointer: *mut u8, count: usize) {
    guard((), || {
        drop(reclaim_exact(pointer, count));
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    /// 1x1 opaque red PNG.
    const RED_PNG: &[u8] = &[
        0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44,
        0x52, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x08, 0x02, 0x00, 0x00, 0x00, 0x90,
        0x77, 0x53, 0xDE, 0x00, 0x00, 0x00, 0x0C, 0x49, 0x44, 0x41, 0x54, 0x08, 0xD7, 0x63, 0xF8,
        0xCF, 0xC0, 0x00, 0x00, 0x03, 0x01, 0x01, 0x00, 0x18, 0xDD, 0x8D, 0xB0, 0x00, 0x00, 0x00,
        0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82,
    ];

    #[test]
    fn decodes_png() {
        let mut width = 0 as c_int;
        let mut height = 0 as c_int;
        let pixels = unsafe {
            bongo_safe_image_decode(RED_PNG.as_ptr(), RED_PNG.len(), &mut width, &mut height)
        };
        assert_eq!(width, 1);
        assert_eq!(height, 1);
        assert!(!pixels.is_null());
        let slice = unsafe { std::slice::from_raw_parts(pixels, 4) };
        assert_eq!(slice[3], 255);
        unsafe { bongo_safe_free_pixels(pixels, 4) };
    }

    #[test]
    fn info_reads_dimensions() {
        let mut width = 0 as c_int;
        let mut height = 0 as c_int;
        assert!(unsafe {
            bongo_safe_image_info(RED_PNG.as_ptr(), RED_PNG.len(), &mut width, &mut height)
        });
        assert_eq!(width, 1);
        assert_eq!(height, 1);
    }

    #[test]
    fn rejects_garbage() {
        let mut width = 0 as c_int;
        let mut height = 0 as c_int;
        let bytes = b"not an image at all";
        let pixels = unsafe {
            bongo_safe_image_decode(bytes.as_ptr(), bytes.len(), &mut width, &mut height)
        };
        assert!(pixels.is_null());
        assert_eq!(width, 0);
    }
}
