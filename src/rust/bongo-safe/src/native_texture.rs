//! Bounded native texture loading. Cache files and source images stay in Rust.
use crate::{guard, release_exact};
use sha2::{Digest, Sha256};
use std::ffi::{c_char, c_void, CStr};
use std::fs::File;
use std::io::{BufReader, Read};
use std::path::{Path, PathBuf};
use std::sync::{OnceLock, RwLock};

#[cfg(all(
    target_arch = "x86_64",
    any(target_os = "windows", target_os = "linux")
))]
#[path = "native_texture_bc7.rs"]
mod bc7_stream;
#[path = "native_texture_cache.rs"]
mod cache;
#[path = "native_texture_png.rs"]
mod png_stream;
#[cfg(test)]
#[path = "native_texture_tests.rs"]
mod tests;

const MAX_BYTES: usize = 768 * 1024 * 1024;
const MAX_DECODE_BYTES: u64 = 2 * 1024 * 1024 * 1024;
const MAX_DIMENSION: u32 = 1 << 18;
pub(super) const ALPHA_BYTES: usize = 128 * 128;
static CACHE_ROOT: OnceLock<RwLock<Option<PathBuf>>> = OnceLock::new();

#[repr(C)]
pub struct NativeTexture {
    pub pixels: *mut u8,
    pub size: usize,
    pub width: u32,
    pub height: u32,
    pub source_width: u32,
    pub source_height: u32,
    /// 0: premultiplied RGBA8, 1: BC7 UNORM (also premultiplied).
    pub format: u32,
    pub alpha: [u8; ALPHA_BYTES],
}

pub(super) struct Texture {
    pixels: Vec<u8>,
    width: u32,
    height: u32,
    source_width: u32,
    source_height: u32,
    format: u32,
    alpha: [u8; ALPHA_BYTES],
}

pub(super) fn fitted(w: u32, h: u32, max_w: u32, max_h: u32) -> (u32, u32) {
    if w <= max_w && h <= max_h {
        return (w, h);
    }
    if u64::from(max_w) * u64::from(h) <= u64::from(max_h) * u64::from(w) {
        (
            max_w,
            (u64::from(h) * u64::from(max_w) / u64::from(w)).max(1) as u32,
        )
    } else {
        (
            (u64::from(w) * u64::from(max_h) / u64::from(h)).max(1) as u32,
            max_h,
        )
    }
}

pub(super) fn alpha_row(alpha: &mut [u8; ALPHA_BYTES], row: &[u8], w: u32, h: u32, y: u32) {
    let aw = w.min(128) as usize;
    let ay = (u64::from(y) * u64::from(h.min(128)) / u64::from(h)) as usize;
    for (x, pixel) in row.chunks_exact(4).enumerate() {
        let ax = x * aw / w as usize;
        alpha[ay * aw + ax] = alpha[ay * aw + ax].max(pixel[3]);
    }
}

fn decode(
    path: &Path,
    max_w: u32,
    max_h: u32,
    bc7: bool,
    stopped: &dyn Fn() -> bool,
) -> Option<Texture> {
    if let Some(texture) = png_stream::decode(path, max_w, max_h, bc7, stopped) {
        return Some(texture);
    }
    if stopped() {
        return None;
    }
    let (header_width, header_height) = image::ImageReader::open(path)
        .ok()?
        .with_guessed_format()
        .ok()?
        .into_dimensions()
        .ok()?;
    if header_width == 0
        || header_height == 0
        || u64::from(header_width) * u64::from(header_height) * 4 > MAX_DECODE_BYTES
    {
        return None;
    }
    let mut reader = image::ImageReader::open(path)
        .ok()?
        .with_guessed_format()
        .ok()?;
    let mut limits = image::Limits::default();
    limits.max_image_width = Some(MAX_DIMENSION);
    limits.max_image_height = Some(MAX_DIMENSION);
    limits.max_alloc = Some(MAX_DECODE_BYTES);
    reader.limits(limits);
    let mut rgba = reader.decode().ok()?.into_rgba8();
    let (sw, sh) = rgba.dimensions();
    if sw == 0 || sh == 0 {
        return None;
    }
    let mut alpha = [0; ALPHA_BYTES];
    for (y, row) in rgba.rows().enumerate() {
        if stopped() {
            return None;
        }
        for (x, pixel) in row.enumerate() {
            let slot = y * sh.min(128) as usize / sh as usize * sw.min(128) as usize
                + x * sw.min(128) as usize / sw as usize;
            alpha[slot] = alpha[slot].max(pixel[3]);
        }
    }
    // Filter premultiplied colors, avoiding color fringes around transparent art.
    for chunk in rgba.chunks_mut(64 * 1024) {
        if stopped() {
            return None;
        }
        for p in chunk.chunks_exact_mut(4) {
            for c in 0..3 {
                p[c] = (u32::from(p[c]) * u32::from(p[3]) / 255) as u8;
            }
        }
    }
    let (w, h) = fitted(sw, sh, max_w, max_h);
    if (w, h) != (sw, sh) {
        rgba = image::imageops::resize(&rgba, w, h, image::imageops::FilterType::Triangle);
    }
    Some(Texture {
        pixels: rgba.into_raw(),
        width: w,
        height: h,
        source_width: sw,
        source_height: sh,
        format: 0,
        alpha,
    })
}

#[cfg(all(
    target_arch = "x86_64",
    any(target_os = "windows", target_os = "linux")
))]
fn compress(texture: &mut Texture, stopped: &dyn Fn() -> bool) -> Option<()> {
    let mut encoder = bc7_stream::Encoder::new(texture.width, texture.height);
    for row in texture.pixels.chunks_exact(texture.width as usize * 4) {
        if stopped() {
            return None;
        }
        encoder.push(row);
    }
    texture.pixels = encoder.finish();
    texture.format = 1;
    Some(())
}

/// # Safety
/// `root` is NULL or a valid NUL-terminated UTF-8 path. Configure before workers.
#[no_mangle]
pub unsafe extern "C" fn bongo_safe_native_texture_cache_root(root: *const c_char) {
    guard((), || {
        let path = if root.is_null() {
            None
        } else {
            CStr::from_ptr(root)
                .to_str()
                .ok()
                .filter(|s| !s.is_empty())
                .map(|s| Path::new(s).join("textures-native-v1"))
        };
        if let Ok(mut value) = CACHE_ROOT.get_or_init(|| RwLock::new(None)).write() {
            *value = path;
        }
    });
}

/// # Safety
/// `path` is a NUL-terminated UTF-8 string; `out` is writable and initially empty.
/// Pixels are released with `bongo_safe_free_pixels(out.pixels, out.size)`.
#[no_mangle]
pub unsafe extern "C" fn bongo_safe_native_texture_load(
    path: *const c_char,
    max_w: u32,
    max_h: u32,
    bc7: bool,
    out: *mut NativeTexture,
    cancelled: Option<unsafe extern "C" fn(*mut c_void) -> bool>,
    userdata: *mut c_void,
) -> bool {
    if out.is_null() {
        return false;
    }
    std::ptr::write_bytes(out, 0, 1);
    if path.is_null() || max_w == 0 || max_h == 0 || max_w > 16384 || max_h > 16384 {
        return false;
    }
    guard(false, || {
        let stopped = || cancelled.is_some_and(|callback| callback(userdata));
        if stopped() {
            return false;
        }
        let Some(path) = CStr::from_ptr(path).to_str().ok().map(Path::new) else {
            return false;
        };
        let Ok(mut source) = File::open(path).map(BufReader::new) else {
            return false;
        };
        let Ok(before) = source.get_ref().metadata() else {
            return false;
        };
        if before.len() > MAX_BYTES as u64 {
            return false;
        }
        let mut hasher = Sha256::new();
        let mut chunk = vec![0; 64 * 1024];
        let mut read_bytes = 0u64;
        loop {
            if stopped() {
                return false;
            }
            let Ok(n) = source.read(&mut chunk) else {
                return false;
            };
            if n == 0 {
                break;
            }
            read_bytes += n as u64;
            if read_bytes > before.len() {
                return false;
            }
            hasher.update(&chunk[..n]);
        }
        let digest: [u8; 32] = hasher.finalize().into();
        let bc7 = bc7
            && cfg!(all(
                target_arch = "x86_64",
                any(target_os = "windows", target_os = "linux")
            ));
        let root = CACHE_ROOT.get().and_then(|root| root.read().ok()?.clone());
        let key = cache::key(&digest, max_w, max_h, bc7);
        let cached = root
            .as_ref()
            .and_then(|root| cache::load(root, &key, max_w, max_h, bc7, &stopped));
        let hit = cached.is_some();
        #[allow(unused_mut)] // Only desktop x64 builds run the BC7 encoder.
        let Some(mut texture) = cached.or_else(|| decode(path, max_w, max_h, bc7, &stopped)) else {
            return false;
        };
        let Ok(after) = std::fs::metadata(path) else {
            return false;
        };
        if before.len() != after.len() || before.modified().ok() != after.modified().ok() {
            return false;
        }
        if !hit {
            #[cfg(all(
                target_arch = "x86_64",
                any(target_os = "windows", target_os = "linux")
            ))]
            if bc7
                && texture.format == 0
                && (texture.width.div_ceil(4) as usize * texture.height.div_ceil(4) as usize * 16)
                    < texture.pixels.len()
                && compress(&mut texture, &stopped).is_none()
            {
                return false;
            }
            if stopped() {
                return false;
            }
            if let Some(root) = root {
                cache::store(&root, &key, &texture, &stopped);
            }
        }
        if stopped() {
            return false;
        }
        (*out).size = texture.pixels.len();
        (*out).width = texture.width;
        (*out).height = texture.height;
        (*out).source_width = texture.source_width;
        (*out).source_height = texture.source_height;
        (*out).format = texture.format;
        (*out).alpha = texture.alpha;
        (*out).pixels = release_exact(texture.pixels);
        true
    })
}
