//! A shared, bounded ZIP reader for imported models, renderer plugins and SDKs.
use crate::http::{self, Failure};
use std::{
    collections::HashSet,
    ffi::c_char,
    fs::{self, File},
    io::{Read, Write},
    path::{Path, PathBuf},
};
use zip::{read::HasZipMetadata, ZipArchive};
#[path = "archive_sdk.rs"]
mod sdk;
type Outcome<T> = Result<T, Failure>;
const MAX_ENTRIES: usize = 20_000;
const MAX_BYTES: u64 = 2 * 1024 * 1024 * 1024;
fn format_error() -> Failure {
    Failure(3, "Invalid, encrypted, unsafe or oversized ZIP".into())
}
pub(super) fn io_error(e: std::io::Error) -> Failure {
    Failure(2, format!("Archive I/O error: {e}"))
}
pub(super) fn relative(name: &str) -> Outcome<PathBuf> {
    let name = name.replace('\\', "/");
    if name.starts_with('/') || name.is_empty() || name.len() >= 1024 || name.contains('\0') {
        return Err(format_error());
    }
    let mut result = PathBuf::new();
    for (depth, part) in name.trim_end_matches('/').split('/').enumerate() {
        let base = part.split('.').next().unwrap_or("").to_ascii_uppercase();
        if depth >= 24
            || part.is_empty()
            || part.ends_with(['.', ' '])
            || part.bytes().any(|b| b < 32 || b":*?\"<>|".contains(&b))
            || matches!(base.as_str(), "CON" | "PRN" | "AUX" | "NUL")
            || (base.len() == 4
                && (base.starts_with("COM") || base.starts_with("LPT"))
                && base.as_bytes()[3].is_ascii_digit())
        {
            return Err(format_error());
        }
        result.push(part);
    }
    Ok(result)
}
#[cfg(windows)]
pub(crate) fn legacy_name(raw: &[u8]) -> Outcome<String> {
    #[link(name = "kernel32")]
    extern "system" {
        fn MultiByteToWideChar(
            page: u32,
            flags: u32,
            input: *const u8,
            length: i32,
            output: *mut u16,
            capacity: i32,
        ) -> i32;
    }
    let mut wide = [0u16; 1024];
    let count = unsafe {
        MultiByteToWideChar(
            936,
            8,
            raw.as_ptr(),
            raw.len() as i32,
            wide.as_mut_ptr(),
            wide.len() as i32,
        )
    };
    if count <= 0 {
        return Err(format_error());
    }
    String::from_utf16(&wide[..count as usize]).map_err(|_| format_error())
}
#[cfg(not(windows))]
pub(crate) fn legacy_name(_: &[u8]) -> Outcome<String> {
    Err(format_error())
}
pub(super) struct Entry {
    index: usize,
    name: PathBuf,
    directory: bool,
    size: u64,
}
pub(super) fn plan(zip: &mut ZipArchive<File>) -> Outcome<Vec<Entry>> {
    if zip.is_empty() || zip.len() > MAX_ENTRIES {
        return Err(format_error());
    }
    let mut result = Vec::with_capacity(zip.len());
    let mut seen = HashSet::new();
    let mut total = 0u64;
    for index in 0..zip.len() {
        let entry = zip.by_index(index).map_err(|_| format_error())?;
        let kind = entry.unix_mode().unwrap_or(0) & 0o170000;
        if entry.encrypted()
            || !matches!(kind, 0 | 0o100000 | 0o040000)
            || entry.size() > MAX_BYTES - total
        {
            return Err(format_error());
        }
        let raw = entry.name_raw();
        if raw.len() >= 1024 {
            return Err(format_error());
        }
        let name = match std::str::from_utf8(raw) {
            Ok(name) => name.to_owned(),
            Err(_) if !entry.get_metadata().is_utf8 => legacy_name(raw)?,
            Err(_) => return Err(format_error()),
        };
        let name = relative(&name)?;
        let key = name.to_string_lossy().to_lowercase();
        if !seen.insert(key) {
            return Err(format_error());
        }
        total += entry.size();
        result.push(Entry {
            index,
            name,
            directory: entry.is_dir(),
            size: entry.size(),
        });
    }
    Ok(result)
}
pub(super) fn unpack(
    zip: &mut ZipArchive<File>,
    entries: &[Entry],
    root: &Path,
    strip: Option<&Path>,
) -> Outcome<()> {
    for planned in entries {
        let name = if let Some(prefix) = strip {
            let name = planned
                .name
                .strip_prefix(prefix)
                .map_err(|_| format_error())?;
            if name.as_os_str().is_empty() {
                continue;
            }
            name
        } else {
            &planned.name
        };
        let target = root.join(name);
        if planned.directory {
            fs::create_dir_all(target).map_err(io_error)?;
            continue;
        }
        if let Some(parent) = target.parent() {
            fs::create_dir_all(parent).map_err(io_error)?;
        }
        let mut output = File::create_new(target).map_err(io_error)?;
        let mut entry = zip.by_index(planned.index).map_err(|_| format_error())?;
        let mut copied = 0u64;
        let mut buffer = [0; 64 * 1024];
        loop {
            let count = entry.read(&mut buffer).map_err(|_| format_error())?;
            if count == 0 {
                break;
            }
            copied += count as u64;
            if copied > planned.size {
                return Err(format_error());
            }
            output.write_all(&buffer[..count]).map_err(io_error)?;
        }
        if copied != planned.size {
            return Err(format_error());
        }
    }
    Ok(())
}
pub(super) unsafe fn output(path: &Path, pointer: *mut c_char, capacity: usize) -> Outcome<()> {
    let name = path.to_str().ok_or_else(format_error)?;
    if pointer.is_null() || name.len() >= capacity {
        return Err(Failure(2, "Archive path is too long".into()));
    }
    std::ptr::copy_nonoverlapping(name.as_ptr(), pointer.cast(), name.len());
    *pointer.add(name.len()) = 0;
    Ok(())
}
#[no_mangle]
pub unsafe extern "C" fn bongo_safe_archive_extract(
    source: *const c_char,
    base: *const c_char,
    directory: *mut c_char,
    temporary: *mut c_char,
    capacity: usize,
    error: *mut c_char,
    error_capacity: usize,
) -> i32 {
    if directory.is_null() || temporary.is_null() || capacity == 0 {
        return 1;
    }
    *directory = 0;
    *temporary = 0;
    crate::guard(4, || {
        let result = (|| {
            let source = Path::new(http::text(source)?);
            let base = Path::new(http::text(base)?);
            let mut zip = ZipArchive::new(File::open(source).map_err(io_error)?)
                .map_err(|_| format_error())?;
            let entries = plan(&mut zip)?;
            fs::create_dir_all(base).map_err(io_error)?;
            let stage = tempfile::Builder::new()
                .prefix("model-")
                .tempdir_in(base)
                .map_err(io_error)?;
            let stem = source
                .file_stem()
                .and_then(|s| s.to_str())
                .unwrap_or("model");
            let name = relative(stem)
                .ok()
                .filter(|p| p.components().count() == 1)
                .unwrap_or_else(|| "model".into());
            let root = stage.path().join(name);
            fs::create_dir(&root).map_err(io_error)?;
            unpack(&mut zip, &entries, &root, None)?;
            output(&root, directory, capacity)?;
            output(stage.path(), temporary, capacity)?;
            let _ = stage.keep();
            Ok::<_, Failure>(())
        })();
        match result {
            Ok(()) => {
                http::message(error, error_capacity, "");
                0
            }
            Err(Failure(code, message)) => {
                *directory = 0;
                *temporary = 0;
                http::message(error, error_capacity, &message);
                code
            }
        }
    })
}
#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn rejects_cross_platform_path_aliases_and_traversal() {
        for path in [
            "../x", "a/../x", "/x", "\\x", "C:x", "NUL.txt", "a./x", "a//x", "a/\0x",
        ] {
            assert!(relative(path).is_err(), "{path:?}");
        }
        assert_eq!(
            relative("model\\textures/a.png").unwrap(),
            Path::new("model/textures/a.png")
        );
    }
}
