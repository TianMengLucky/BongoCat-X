use super::*;

// Cache writes are opportunistic; serialize tests that require persistence.
static CACHE_WRITES: std::sync::Mutex<()> = std::sync::Mutex::new(());

fn fixture() -> (tempfile::TempDir, PathBuf) {
    let directory = tempfile::tempdir().unwrap();
    let path = directory.path().join("atlas.png");
    let image = image::RgbaImage::from_fn(4, 4, |x, y| {
        if (x + y) % 2 == 0 {
            image::Rgba([255, 0, 0, 0])
        } else {
            image::Rgba([0, 0, 255, 255])
        }
    });
    image.save(&path).unwrap();
    (directory, path)
}

#[test]
fn streaming_resize_preserves_source_coverage_and_avoids_transparent_color_bleed() {
    let (_directory, path) = fixture();
    let texture = png_stream::decode(&path, 2, 2, false, &|| false).unwrap();
    assert_eq!((texture.width, texture.height), (2, 2));
    assert_eq!((texture.source_width, texture.source_height), (4, 4));
    for pixel in texture.pixels.chunks_exact(4) {
        assert_eq!(pixel, [0, 0, 128, 128]);
    }
    for y in 0..4 {
        for x in 0..4 {
            assert_eq!(
                texture.alpha[y * 4 + x],
                if (x + y) % 2 == 0 { 0 } else { 255 }
            );
        }
    }
}

#[test]
fn native_cache_rejects_corruption_and_misnamed_entries() {
    let _writer = CACHE_WRITES.lock().unwrap();
    let (directory, path) = fixture();
    let root = directory.path().join("cache");
    let texture = decode(&path, 3, 3, false, &|| false).unwrap();
    let key = cache::key(&[1; 32], 3, 3, false);
    cache::store(&root, &key, &texture, &|| false);
    let loaded = cache::load(&root, &key, 3, 3, false, &|| false).unwrap();
    assert_eq!(loaded.pixels, texture.pixels);
    assert_eq!(loaded.alpha, texture.alpha);
    assert!(cache::load(&root, &key, 2, 2, false, &|| false).is_none());
    assert!(cache::load(&root, &key, 3, 3, true, &|| false).is_none());
    let other_key = cache::key(&[2; 32], 3, 3, false);
    std::fs::copy(root.join(&key), root.join(&other_key)).unwrap();
    assert!(cache::load(&root, &other_key, 3, 3, false, &|| false).is_none());
    let mut bytes = std::fs::read(root.join(&key)).unwrap();
    *bytes.last_mut().unwrap() ^= 1;
    std::fs::write(root.join(&key), bytes).unwrap();
    assert!(cache::load(&root, &key, 3, 3, false, &|| false).is_none());
}

#[test]
fn tiny_atlas_cache_keeps_rgba_when_a_bc7_block_would_be_larger() {
    let _writer = CACHE_WRITES.lock().unwrap();
    let directory = tempfile::tempdir().unwrap();
    let texture = Texture {
        pixels: vec![0, 0, 0, 0],
        width: 1,
        height: 1,
        source_width: 1,
        source_height: 1,
        format: 0,
        alpha: [0; ALPHA_BYTES],
    };
    let key = cache::key(&[3; 32], 1, 1, true);
    cache::store(directory.path(), &key, &texture, &|| false);
    let loaded = cache::load(directory.path(), &key, 1, 1, true, &|| false).unwrap();
    assert_eq!((loaded.format, loaded.pixels.len()), (0, 4));
}

#[test]
fn ffi_cancellation_and_invalid_bounds_return_empty_storage() {
    let (_directory, path) = fixture();
    let path = std::ffi::CString::new(path.to_str().unwrap()).unwrap();
    let mut output = std::mem::MaybeUninit::<NativeTexture>::uninit();
    unsafe extern "C" fn cancelled(_: *mut c_void) -> bool {
        true
    }
    assert!(!unsafe {
        bongo_safe_native_texture_load(
            path.as_ptr(),
            2,
            2,
            false,
            output.as_mut_ptr(),
            Some(cancelled),
            std::ptr::null_mut(),
        )
    });
    let output = unsafe { output.assume_init() };
    assert!(output.pixels.is_null());
    assert_eq!(output.size, 0);
    assert!(png_stream::decode(Path::new(path.to_str().unwrap()), 2, 2, false, &|| true).is_none());
    let mut output = std::mem::MaybeUninit::<NativeTexture>::uninit();
    assert!(!unsafe {
        bongo_safe_native_texture_load(
            path.as_ptr(),
            0,
            2,
            false,
            output.as_mut_ptr(),
            None,
            std::ptr::null_mut(),
        )
    });
    assert!(unsafe { output.assume_init() }.pixels.is_null());
}

#[test]
fn ffi_transfers_exact_native_payload_with_matching_release_size() {
    let (_directory, path) = fixture();
    let path = std::ffi::CString::new(path.to_str().unwrap()).unwrap();
    let mut output = std::mem::MaybeUninit::<NativeTexture>::uninit();
    assert!(unsafe {
        bongo_safe_native_texture_load(
            path.as_ptr(),
            2,
            2,
            false,
            output.as_mut_ptr(),
            None,
            std::ptr::null_mut(),
        )
    });
    let output = unsafe { output.assume_init() };
    assert_eq!(
        (output.width, output.height, output.format, output.size),
        (2, 2, 0, 16)
    );
    let pixels = unsafe { std::slice::from_raw_parts(output.pixels, output.size) };
    assert_eq!(pixels, [0, 0, 128, 128].repeat(4));
    unsafe { crate::image::bongo_safe_free_pixels(output.pixels, output.size) };
}

#[cfg(all(
    target_arch = "x86_64",
    any(target_os = "windows", target_os = "linux")
))]
#[test]
fn bc7_handles_partial_edge_blocks_and_small_images() {
    for (w, h) in [(1, 1), (5, 3), (7, 9)] {
        let mut texture = Texture {
            pixels: vec![128; w * h * 4],
            width: w as u32,
            height: h as u32,
            source_width: w as u32,
            source_height: h as u32,
            format: 0,
            alpha: [128; ALPHA_BYTES],
        };
        compress(&mut texture, &|| false).unwrap();
        assert_eq!(texture.format, 1);
        assert_eq!(texture.pixels.len(), w.div_ceil(4) * h.div_ceil(4) * 16);
        assert_eq!(texture.alpha, [128; ALPHA_BYTES]);
    }
}

#[test]
fn streaming_bc7_preserves_rgba_results_alpha_and_tiny_image_fallback() {
    let directory = tempfile::tempdir().unwrap();
    let path = directory.path().join("edge-atlas.png");
    for (w, h) in [(1, 1), (1, 7), (5, 3), (7, 9), (17, 11)] {
        image::RgbaImage::from_fn(w, h, |x, y| {
            image::Rgba([(x * 19) as u8, (y * 23) as u8, 255, ((x + y) * 31) as u8])
        })
        .save(&path)
        .unwrap();
        for (max_w, max_h) in [(w, h), (5, 4)] {
            #[allow(unused_mut)]
            let mut expected = png_stream::decode(&path, max_w, max_h, false, &|| false).unwrap();
            #[cfg(all(
                target_arch = "x86_64",
                any(target_os = "windows", target_os = "linux")
            ))]
            if expected.width.div_ceil(4) as usize * expected.height.div_ceil(4) as usize * 16
                < expected.pixels.len()
            {
                compress(&mut expected, &|| false).unwrap();
            }
            let actual = png_stream::decode(&path, max_w, max_h, true, &|| false).unwrap();
            assert_eq!(
                actual.format, expected.format,
                "{w}x{h} bound={max_w}x{max_h}"
            );
            assert_eq!(
                actual.pixels, expected.pixels,
                "{w}x{h} bound={max_w}x{max_h}"
            );
            assert_eq!(actual.alpha, expected.alpha);
        }
    }
}

#[test]
fn streaming_bc7_stops_mid_decode_and_rejects_truncated_pixels() {
    let (_directory, path) = fixture();
    let calls = std::cell::Cell::new(0);
    assert!(png_stream::decode(&path, 4, 4, true, &|| {
        calls.set(calls.get() + 1);
        calls.get() >= 3
    })
    .is_none());
    let mut bytes = std::fs::read(&path).unwrap();
    bytes.truncate(bytes.len() / 2);
    std::fs::write(&path, bytes).unwrap();
    assert!(png_stream::decode(&path, 4, 4, true, &|| false).is_none());
}
