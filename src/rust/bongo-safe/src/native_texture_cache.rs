use super::{Texture, ALPHA_BYTES, MAX_BYTES, MAX_DECODE_BYTES, MAX_DIMENSION};
use sha2::{Digest, Sha256};
use std::{
    fs,
    io::{Read, Write},
    path::Path,
    sync::Mutex,
};
const MAGIC: &[u8; 8] = b"BNTEX001";
const HEADER: usize = 8 + 5 * 4 + 32;
const DISK_BUDGET: u64 = 1024 * 1024 * 1024;
static WRITER: Mutex<()> = Mutex::new(());

pub(super) fn key(digest: &[u8; 32], w: u32, h: u32, bc7: bool) -> String {
    let digest: String = digest.iter().map(|byte| format!("{byte:02x}")).collect();
    format!("{digest}-{w}x{h}-{}.ntx", u8::from(bc7))
}

pub(super) fn load(
    root: &Path,
    key: &str,
    max_w: u32,
    max_h: u32,
    bc7: bool,
    stopped: &dyn Fn() -> bool,
) -> Option<Texture> {
    if stopped() {
        return None;
    }
    let mut file = fs::File::open(root.join(key)).ok()?;
    let length = file.metadata().ok()?.len();
    if length > (MAX_BYTES + HEADER + ALPHA_BYTES) as u64 {
        return None;
    }
    let mut header = [0; HEADER];
    file.read_exact(&mut header).ok()?;
    if &header[..8] != MAGIC {
        return None;
    }
    let value = |i| u32::from_le_bytes(header[i..i + 4].try_into().unwrap());
    let (sw, sh, w, h, format) = (value(8), value(12), value(16), value(20), value(24));
    if sw == 0
        || sh == 0
        || sw > MAX_DIMENSION
        || sh > MAX_DIMENSION
        || w == 0
        || h == 0
        || w > max_w
        || h > max_h
        || super::fitted(sw, sh, max_w, max_h) != (w, h)
        || u64::from(sw) * u64::from(sh) * 4 > MAX_DECODE_BYTES
    {
        return None;
    }
    let compressed_size = w.div_ceil(4) as usize * h.div_ceil(4) as usize * 16;
    let rgba_size = w as usize * h as usize * 4;
    let expected_format = u32::from(bc7 && compressed_size < rgba_size);
    if format != expected_format {
        return None;
    }
    let size = if format == 1 {
        compressed_size
    } else {
        rgba_size
    };
    if size > MAX_BYTES || length != (HEADER + ALPHA_BYTES + size) as u64 {
        return None;
    }
    let mut texture = Texture {
        pixels: vec![0; size],
        width: w,
        height: h,
        source_width: sw,
        source_height: sh,
        format,
        alpha: [0; ALPHA_BYTES],
    };
    file.read_exact(&mut texture.alpha).ok()?;
    for chunk in texture.pixels.chunks_mut(64 * 1024) {
        if stopped() {
            return None;
        }
        file.read_exact(chunk).ok()?;
    }
    let mut hash = Sha256::new();
    hash.update(key.as_bytes());
    hash.update(&header[..28]);
    hash.update(texture.alpha);
    hash.update(&texture.pixels);
    if hash.finalize()[..] != header[28..60] {
        return None;
    }
    Some(texture)
}

pub(super) fn store(root: &Path, key: &str, texture: &Texture, stopped: &dyn Fn() -> bool) {
    let _ = try_store(root, key, texture, stopped);
}

fn try_store(root: &Path, key: &str, texture: &Texture, stopped: &dyn Fn() -> bool) -> Option<()> {
    if stopped() || texture.pixels.len() > MAX_BYTES {
        return None;
    }
    let _lock = WRITER.try_lock().ok()?;
    fs::create_dir_all(root).ok()?;
    // Bound both directory scan memory and persistent disk use. Ignore unrelated files.
    let mut entries = Vec::new();
    let mut total = 0u64;
    for entry in fs::read_dir(root).ok()? {
        if stopped() {
            return None;
        }
        let entry = entry.ok()?;
        if entry.path().extension().and_then(|s| s.to_str()) != Some("ntx")
            || !entry.file_type().ok()?.is_file()
        {
            continue;
        }
        if entries.len() >= 128 {
            return None;
        }
        let metadata = entry.metadata().ok()?;
        total = total.checked_add(metadata.len())?;
        entries.push((metadata.modified().ok()?, entry.path(), metadata.len()));
    }
    entries.sort_by_key(|e| e.0);
    let size = (HEADER + ALPHA_BYTES + texture.pixels.len()) as u64;
    let mut count = entries.len();
    for (_, path, bytes) in entries {
        if stopped() {
            return None;
        }
        if total <= DISK_BUDGET.checked_sub(size)? && count < 64 {
            break;
        }
        fs::remove_file(path).ok()?;
        total -= bytes;
        count -= 1;
    }
    let mut header = Vec::from(*MAGIC);
    for value in [
        texture.source_width,
        texture.source_height,
        texture.width,
        texture.height,
        texture.format,
    ] {
        header.extend(value.to_le_bytes());
    }
    let mut hash = Sha256::new();
    hash.update(key.as_bytes());
    hash.update(&header);
    hash.update(texture.alpha);
    hash.update(&texture.pixels);
    header.extend(hash.finalize());
    let mut temporary = tempfile::NamedTempFile::new_in(root).ok()?;
    temporary.write_all(&header).ok()?;
    temporary.write_all(&texture.alpha).ok()?;
    for chunk in texture.pixels.chunks(64 * 1024) {
        if stopped() {
            return None;
        }
        temporary.write_all(chunk).ok()?;
    }
    temporary.flush().ok()?;
    temporary.persist(root.join(key)).ok()?;
    Some(())
}
