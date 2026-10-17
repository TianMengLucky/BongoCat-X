use crate::image_rows::text;
use std::{
    ffi::c_char,
    fs,
    path::PathBuf,
    sync::{Mutex, OnceLock, RwLock},
    time::SystemTime,
};
pub(crate) const ENTRY_LIMIT: u64 = 768 * 1024 * 1024;
static ROOT: OnceLock<RwLock<Option<PathBuf>>> = OnceLock::new();
static TRIM: Mutex<()> = Mutex::new(());
pub(crate) fn valid_digest(digest: &str) -> bool {
    digest.len() == 64
        && digest
            .bytes()
            .all(|b| b.is_ascii_digit() || (b'a'..=b'f').contains(&b))
}
pub(crate) fn path(digest: &str, width: i32, height: i32) -> Option<PathBuf> {
    if !valid_digest(digest) || width < 1 || height < 1 {
        return None;
    }
    let root = ROOT.get()?.read().ok()?.clone()?;
    Some(root.join(format!("{digest}-{width}x{height}.rgba")))
}
pub(crate) fn reserve(bytes: u64) -> bool {
    let Some(root) = ROOT.get().and_then(|r| r.read().ok()?.clone()) else {
        return false;
    };
    if bytes > ENTRY_LIMIT {
        return false;
    }
    let Ok(_lock) = TRIM.lock() else {
        return false;
    };
    if fs::create_dir_all(&root).is_err() {
        return false;
    }
    let Ok(entries) = fs::read_dir(&root) else {
        return false;
    };
    let mut files = Vec::new();
    let mut total = 0u64;
    for entry in entries {
        let Ok(entry) = entry else {
            return false;
        };
        let name = entry.file_name();
        let Some(name) = name.to_str() else {
            continue;
        };
        if name.len() < 70
            || !name.get(..64).is_some_and(valid_digest)
            || name.as_bytes()[64] != b'-'
        {
            continue;
        }
        let Ok(meta) = entry.metadata() else {
            continue;
        };
        if !meta.is_file() || entry.file_type().is_ok_and(|t| t.is_symlink()) {
            continue;
        }
        let modified = meta.modified().unwrap_or(SystemTime::UNIX_EPOCH);
        if name.ends_with(".tmp") {
            if modified.elapsed().is_ok_and(|age| age.as_secs() > 86400) {
                let _ = fs::remove_file(entry.path());
            }
        } else if name.ends_with(".rgba") {
            let Some(next) = total.checked_add(meta.len()) else {
                return false;
            };
            total = next;
            files.push((modified, entry.path(), meta.len()));
            if files.len() > 128 {
                return false;
            }
        }
    }
    files.sort_unstable_by_key(|e| e.0);
    let mut count = files.len();
    for (_, path, size) in files {
        if total <= 1024 * 1024 * 1024 - bytes && count < 64 {
            break;
        }
        if fs::remove_file(path).is_ok() {
            total -= size;
            count -= 1;
        }
    }
    total <= 1024 * 1024 * 1024 - bytes && count < 64
}
#[no_mangle]
pub unsafe extern "C" fn bongo_safe_rgba_cache_root(root: *const c_char) {
    crate::guard((), || {
        let root = text(root)
            .ok()
            .filter(|r| !r.is_empty())
            .map(|r| PathBuf::from(r).join("textures-rgba-v1"));
        if let Ok(mut current) = ROOT.get_or_init(|| RwLock::new(None)).write() {
            *current = root;
        }
    });
}
#[no_mangle]
pub unsafe extern "C" fn bongo_safe_rgba_cache_path(
    digest: *const c_char,
    w: i32,
    h: i32,
    output: *mut c_char,
    capacity: usize,
) -> bool {
    crate::guard(false, || {
        if output.is_null() || capacity == 0 {
            return false;
        }
        *output = 0;
        let Some(path) = text(digest).ok().and_then(|d| path(d, w, h)) else {
            return false;
        };
        let Some(path) = path.to_str() else {
            return false;
        };
        if path.len() >= capacity {
            return false;
        }
        std::ptr::copy_nonoverlapping(path.as_ptr(), output.cast(), path.len());
        *output.add(path.len()) = 0;
        true
    })
}
#[no_mangle]
pub extern "C" fn bongo_safe_rgba_cache_reserve(bytes: u64) -> bool {
    crate::guard(false, || reserve(bytes))
}
#[no_mangle]
pub unsafe extern "C" fn bongo_safe_rgba_cache_forget(digest: *const c_char, w: i32, h: i32) {
    crate::guard((), || {
        if let Some(path) = text(digest).ok().and_then(|d| path(d, w, h)) {
            let _ = fs::remove_file(path);
        }
    });
}
