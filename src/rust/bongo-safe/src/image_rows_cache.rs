//! Validate a disk entry completely before publishing mutable borrowed strips.
use crate::{
    image_cache_storage as storage,
    image_rows::{self, Callbacks, Outcome, FORMAT, IO},
};
use flate2::Crc;
use std::{
    ffi::c_char,
    fs::{self, File},
    io::{Read, Seek, SeekFrom, Write},
    path::Path,
};
const HEADER: usize = 96;
const MAGIC: &[u8; 8] = b"BCRGBA01";
fn crc(bytes: &[u8]) -> u32 {
    let mut crc = Crc::new();
    crc.update(bytes);
    crc.sum()
}
fn put(bytes: &mut [u8], offset: usize, value: u32) {
    bytes[offset..offset + 4].copy_from_slice(&value.to_le_bytes());
}
fn get(bytes: &[u8], offset: usize) -> u32 {
    u32::from_le_bytes(bytes[offset..offset + 4].try_into().unwrap())
}
fn read(file: &mut File, digest: &str, bounds: (u32, u32), cb: Callbacks) -> Outcome {
    let mut header = [0; HEADER];
    file.read_exact(&mut header).map_err(|_| FORMAT)?;
    if &header[..8] != MAGIC
        || &header[32..] != digest.as_bytes()
        || (get(&header, 8), get(&header, 12)) != bounds
    {
        return Err(FORMAT);
    }
    let checksum = get(&header, 28);
    put(&mut header, 28, 0);
    if crc(&header) != checksum {
        return Err(FORMAT);
    }
    let (w, h) = (get(&header, 16), get(&header, 20));
    let bytes = u64::from(w) * u64::from(h) * 4;
    if w == 0
        || h == 0
        || w > bounds.0
        || h > bounds.1
        || w > (i32::MAX / 4) as u32
        || bytes > storage::ENTRY_LIMIT - HEADER as u64
    {
        return Err(FORMAT);
    }
    let stride = w as usize * 4;
    let batch = (1024 * 1024 / stride).max(1).min(h as usize);
    let mut buffer = vec![0; stride * batch];
    let mut remaining = bytes;
    let mut checksum = Crc::new();
    while remaining != 0 {
        cb.check()?;
        let size = remaining.min(buffer.len() as u64) as usize;
        file.read_exact(&mut buffer[..size]).map_err(|_| FORMAT)?;
        checksum.update(&buffer[..size]);
        remaining -= size as u64;
        cb.progress(0.1 * (bytes - remaining) as f32 / bytes as f32);
    }
    let mut extra = [0];
    if checksum.sum() != get(&header, 24) || file.read(&mut extra).map_err(|_| FORMAT)? != 0 {
        return Err(FORMAT);
    }
    if bytes == buffer.len() as u64 {
        cb.emit(&mut buffer, w, h, h, 0)?;
        cb.progress(1.0);
        return Ok(());
    }
    file.seek(SeekFrom::Start(HEADER as u64)).map_err(|_| IO)?;
    let mut y = 0;
    while y < h {
        cb.check()?;
        let rows = (batch as u32).min(h - y);
        let output = &mut buffer[..stride * rows as usize];
        file.read_exact(output).map_err(|_| IO)?;
        cb.emit(output, w, rows, h, y)?;
        y += rows;
        cb.progress(0.1 + 0.9 * y as f32 / h as f32);
    }
    Ok(())
}
fn stamp(path: &Path) -> Option<(u64, std::time::SystemTime)> {
    let meta = fs::metadata(path).ok()?;
    Some((meta.len(), meta.modified().ok()?))
}
fn decode(path: &Path, digest: Option<&str>, bounds: (u32, u32), cb: Callbacks) -> Outcome {
    let target = digest.and_then(|d| storage::path(d, bounds.0 as i32, bounds.1 as i32));
    let before = stamp(path);
    if let (Some(target), Some(digest), Some(_)) = (&target, digest, before) {
        if let Ok(mut file) = File::open(target) {
            match read(&mut file, digest, bounds, cb) {
                Ok(()) => return Ok(()),
                Err(FORMAT) => (),
                Err(e) => return Err(e),
            }
        }
    }
    let mut writer: Option<tempfile::NamedTempFile> = None;
    let mut checksum = Crc::new();
    let mut dimensions = (0, 0);
    let mut next = 0;
    let mut consume = |pixels: &mut [u8], w: u32, rows: u32, h: u32, y: u32| {
        if y == 0 {
            dimensions = (w, h);
            if let Some(parent) = target.as_ref().and_then(|p| p.parent()) {
                if before.is_some()
                    && storage::reserve(u64::from(w) * u64::from(h) * 4 + HEADER as u64)
                {
                    writer = tempfile::NamedTempFile::new_in(parent).ok();
                    if writer
                        .as_mut()
                        .is_some_and(|f| f.write_all(&[0; HEADER]).is_err())
                    {
                        writer = None;
                    }
                }
            }
        }
        if y != next || dimensions != (w, h) || rows == 0 || rows > h - y {
            return Err(FORMAT);
        }
        checksum.update(pixels);
        if writer
            .as_mut()
            .is_some_and(|f| f.write_all(pixels).is_err())
        {
            writer = None;
        }
        next += rows;
        cb.emit(pixels, w, rows, h, y)
    };
    image_rows::decode(path, bounds, cb, &mut consume)?;
    drop(consume);
    cb.check()?;
    if before.is_none() || before != stamp(path) || next != dimensions.1 {
        return Ok(());
    }
    if let (Some(mut writer), Some(target), Some(digest)) = (writer, target, digest) {
        let mut header = [0; HEADER];
        header[..8].copy_from_slice(MAGIC);
        put(&mut header, 8, bounds.0);
        put(&mut header, 12, bounds.1);
        put(&mut header, 16, dimensions.0);
        put(&mut header, 20, dimensions.1);
        put(&mut header, 24, checksum.sum());
        header[32..].copy_from_slice(digest.as_bytes());
        let checksum = crc(&header);
        put(&mut header, 28, checksum);
        if writer.seek(SeekFrom::Start(0)).is_ok()
            && writer.write_all(&header).is_ok()
            && writer.flush().is_ok()
            && writer.persist(target).is_ok()
        {
            storage::reserve(0);
        }
    }
    Ok(())
}
#[no_mangle]
pub unsafe extern "C" fn bongo_safe_cached_png_rows(
    path: *const c_char,
    digest: *const c_char,
    w: i32,
    h: i32,
    cb: Callbacks,
) -> i32 {
    crate::guard(4, || {
        if w < 1 || h < 1 || !cb.valid() {
            return 1;
        }
        image_rows::text(path)
            .and_then(|p| {
                decode(
                    Path::new(p),
                    image_rows::text(digest)
                        .ok()
                        .filter(|d| storage::valid_digest(d)),
                    (w as u32, h as u32),
                    cb,
                )
            })
            .err()
            .unwrap_or(0)
    })
}
#[no_mangle]
pub unsafe extern "C" fn bongo_safe_rgba_cache_read(
    path: *const c_char,
    digest: *const c_char,
    w: i32,
    h: i32,
    cb: Callbacks,
) -> i32 {
    crate::guard(4, || {
        if w < 1 || h < 1 || !cb.valid() {
            return 1;
        }
        let result = (|| {
            let (path, digest) = (image_rows::text(path)?, image_rows::text(digest)?);
            if !storage::valid_digest(digest) {
                return Err(1);
            }
            read(
                &mut File::open(path).map_err(|_| IO)?,
                digest,
                (w as u32, h as u32),
                cb,
            )
        })();
        result.err().unwrap_or(0)
    })
}
