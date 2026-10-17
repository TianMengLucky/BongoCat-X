use crate::Entry;
use bongo_plugin::io::{path, text};
use bongo_plugin::page::text as fixed_text;
use bongo_plugin::transfer::{Host, Receipt};
use std::ffi::{c_char, c_void, CString};
use std::fs::{self, File};
use std::io::{Read, Write};
use std::path::{Path, PathBuf};

const DEFAULT: &str = "https://github.com/TianMengLucky/Awesome-BongoCat/";
const RAW: &str = "https://raw.githubusercontent.com/TianMengLucky/Awesome-BongoCat/";
fn repository(value: &str) -> Option<&str> {
    let value = value.strip_prefix("https://github.com/")?;
    let parts: Vec<_> = value.split('/').collect();
    (parts.len() == 2
        && parts.iter().all(|v| {
            !v.is_empty()
                && *v != "."
                && *v != ".."
                && v.bytes()
                    .all(|b| b.is_ascii_alphanumeric() || b"-_.".contains(&b))
        }))
    .then_some(value)
}
fn rebase(entry: &mut Entry, repo: &str) -> Option<()> {
    let slug = repository(repo)?;
    for (out, prefix, base) in [
        (&mut entry.file_url, DEFAULT, repo.to_owned()),
        (&mut entry.folder_url, DEFAULT, repo.to_owned()),
        (
            &mut entry.preview_url,
            RAW,
            format!("https://raw.githubusercontent.com/{slug}"),
        ),
        (
            &mut entry.download_url,
            RAW,
            format!("https://raw.githubusercontent.com/{slug}"),
        ),
    ] {
        let suffix = fixed_text(out).strip_prefix(prefix)?;
        let url = format!("{base}/{suffix}");
        if url.len() >= out.len() {
            return None;
        }
        *out = bongo_plugin::text::buffer(&url);
    }
    Some(())
}
pub(super) fn cache(root: &str, key: &str, suffix: &str) -> Option<PathBuf> {
    if key.len() != 64
        || !key
            .bytes()
            .all(|b| b.is_ascii_digit() || (b'a'..=b'f').contains(&b))
        || ![
            ".preview",
            ".preview.new",
            ".download",
            ".installed",
            ".installed.new",
        ]
        .contains(&suffix)
    {
        return None;
    }
    Some(Path::new(root).join(format!("{key}{suffix}")))
}
unsafe extern "C" fn cache_path(
    root: *const c_char,
    key: *const c_char,
    suffix: *const c_char,
    out: *mut c_char,
    capacity: usize,
) -> u32 {
    crate::guard(0, || {
        let (Some(root), Some(key), Some(suffix)) = (text(root), text(key), text(suffix)) else {
            return 0;
        };
        let Some(file) = cache(root, key, suffix).and_then(|p| path(&p)) else {
            return 0;
        };
        let bytes = file.as_bytes_with_nul();
        if out.is_null() || bytes.len() > capacity {
            return 0;
        }
        std::ptr::copy_nonoverlapping(bytes.as_ptr(), out.cast(), bytes.len());
        1
    })
}
struct Response<'a> {
    host: &'a Host,
    data: *mut u8,
    length: usize,
}
impl Drop for Response<'_> {
    fn drop(&mut self) {
        unsafe { (self.host.free_response)(self.data, self.length) }
    }
}
fn get<'a>(
    host: &'a Host,
    url: &str,
    accelerate: bool,
    limit: usize,
) -> Result<Response<'a>, bool> {
    let url = CString::new(url).map_err(|_| false)?;
    let mut data = std::ptr::null_mut();
    let mut length = 0;
    let mut message = [0 as c_char; 512];
    let result = unsafe {
        (host.get)(
            url.as_ptr(),
            accelerate,
            limit,
            host.cancelled,
            host.context,
            &mut data,
            &mut length,
            message.as_mut_ptr(),
            message.len(),
        )
    };
    let response = Response { host, data, length };
    if result != 0 || data.is_null() || length > limit {
        return Err(unsafe { text(message.as_ptr()) }.is_some_and(|s| s.contains("404")));
    }
    Ok(response)
}
unsafe extern "C" fn catalog(
    instance: *mut c_void,
    repo: *const c_char,
    accelerate: bool,
    host: *const Host,
) -> u32 {
    crate::guard(4, || {
        let (Some(repo), Some(host)) = (text(repo).and_then(repository), Host::get(host)) else {
            return 4;
        };
        if instance.is_null() {
            return 4;
        }
        let response = match get(
            host,
            &format!("https://raw.githubusercontent.com/{repo}/main/model.json"),
            accelerate,
            2 * 1024 * 1024,
        ) {
            Ok(response) => response,
            Err(true) => return 3,
            Err(false) => return 4,
        };
        if host.locked(|| crate::parse(instance, response.data, response.length)) != 0 {
            2
        } else {
            4
        }
    })
}
unsafe extern "C" fn entry(
    instance: *mut c_void,
    index: u32,
    repo: *const c_char,
    out: *mut Entry,
) -> u32 {
    crate::guard(0, || {
        let Some(repo) = text(repo) else {
            return 0;
        };
        if crate::entry(instance, index, out) == 0 {
            return 0;
        }
        rebase(&mut *out, repo).is_some() as u32
    })
}
pub(super) unsafe extern "C" fn receipt(
    root: *const c_char,
    hash: *const c_char,
    out: *mut Receipt,
) -> u32 {
    crate::guard(0, || {
        let (Some(root), Some(hash)) = (text(root), text(hash)) else {
            return 0;
        };
        if out.is_null() {
            return 0;
        }
        let Some(file) = cache(root, hash, ".installed") else {
            return 0;
        };
        let Ok(input) = File::open(file) else {
            return 0;
        };
        let mut bytes = String::new();
        if input.take(16 * 132 + 1).read_to_string(&mut bytes).is_err() || bytes.len() > 16 * 132 {
            return 0;
        }
        let mut value = Receipt::default();
        for line in bytes.lines() {
            if value.count == value.ids.len()
                || line.is_empty()
                || line.len() >= 128
                || line.contains(['/', '\\', '\0'])
                || line == ".."
            {
                return 0;
            }
            value.ids[value.count] = bongo_plugin::text::buffer(line);
            value.count += 1;
        }
        if value.count == 0 {
            return 0;
        }
        out.write(value);
        1
    })
}
unsafe extern "C" fn save_receipt(
    root: *const c_char,
    hash: *const c_char,
    value: *const Receipt,
    host: *const Host,
) -> u32 {
    crate::guard(0, || {
        let (Some(root), Some(hash), Some(value), Some(host)) =
            (text(root), text(hash), value.as_ref(), Host::get(host))
        else {
            return 0;
        };
        if value.count == 0 || value.count > value.ids.len() {
            return 0;
        }
        let (Some(target), Some(temporary)) = (
            cache(root, hash, ".installed"),
            cache(root, hash, ".installed.new"),
        ) else {
            return 0;
        };
        let ok = (|| {
            fs::create_dir_all(root).ok()?;
            let mut file = File::create(&temporary).ok()?;
            for id in &value.ids[..value.count] {
                let id = fixed_text(id);
                if id.is_empty() || id.contains(['\n', '\r', '/', '\\']) || id == ".." {
                    return None;
                }
                writeln!(file, "{id}").ok()?;
            }
            file.sync_all().ok()?;
            drop(file);
            let (target, temporary) = (path(&target)?, path(&temporary)?);
            ((host.replace)(temporary.as_ptr(), target.as_ptr()) != 0).then_some(())
        })()
        .is_some();
        let _ = fs::remove_file(temporary);
        ok as u32
    })
}
unsafe extern "C" fn asset(
    instance: *mut c_void,
    index: u32,
    repo: *const c_char,
    root: *const c_char,
    accelerate: bool,
    preview: bool,
    host: *const Host,
    out: *mut Receipt,
) -> u32 {
    crate::guard(4, || {
        let (Some(repo), Some(root), Some(host)) = (text(repo), text(root), Host::get(host)) else {
            return 4;
        };
        if instance.is_null() || out.is_null() || (host.cancelled)(host.context) {
            return 4;
        }
        let mut value = std::mem::MaybeUninit::<Entry>::uninit();
        if host.locked(|| {
            entry(
                instance,
                index,
                CString::new(repo).unwrap().as_ptr(),
                value.as_mut_ptr(),
            )
        }) == 0
        {
            return 4;
        }
        let value = value.assume_init();
        if preview {
            let url = fixed_text(&value.preview_url);
            let mut hash = [0 as c_char; 65];
            (host.digest)(url.as_ptr().cast(), url.len(), hash.as_mut_ptr());
            let Some(file) = cache(root, text(hash.as_ptr()).unwrap_or(""), ".preview") else {
                return 4;
            };
            if file.is_file() {
                return 2;
            }
            let Ok(response) = get(host, url, accelerate, 4 * 1024 * 1024) else {
                return 4;
            };
            if (host.cancelled)(host.context) {
                return 4;
            }
            let temporary = file.with_extension("preview.new");
            let ok = (|| {
                fs::create_dir_all(root).ok()?;
                let mut output = File::create(&temporary).ok()?;
                output
                    .write_all(std::slice::from_raw_parts(response.data, response.length))
                    .ok()?;
                output.sync_all().ok()?;
                drop(output);
                let temporary = path(&temporary)?;
                let target = path(&file)?;
                if (host.validate_image)(temporary.as_ptr()) == 0 || (host.cancelled)(host.context)
                {
                    return None;
                }
                ((host.replace)(temporary.as_ptr(), target.as_ptr()) != 0).then_some(())
            })()
            .is_some();
            let _ = fs::remove_file(temporary);
            return if ok { 2 } else { 4 };
        }
        let Some(directory) = cache(root, fixed_text(&value.sha256), ".download") else {
            return 4;
        };
        if fs::create_dir_all(&directory).is_err() {
            return 4;
        }
        let Some(name) = Path::new(fixed_text(&value.repository_path)).file_name() else {
            return 4;
        };
        let file = directory.join(name);
        let Some(target) = path(&file) else {
            return 4;
        };
        let mut message = [0 as c_char; 512];
        let result = (host.download)(
            value.download_url.as_ptr().cast(),
            value.sha256.as_ptr().cast(),
            target.as_ptr(),
            accelerate,
            host.cancelled,
            host.progress,
            host.context,
            message.as_mut_ptr(),
            message.len(),
        );
        let mut receipt_value = Receipt::default();
        let ok = result == 0
            && !(host.cancelled)(host.context)
            && (host.install)(host.context, target.as_ptr(), &mut receipt_value) != 0
            && receipt_value.count > 0;
        let _ = fs::remove_file(file);
        let _ = fs::remove_dir(directory);
        if ok {
            let root = CString::new(root).unwrap();
            let _ = save_receipt(
                root.as_ptr(),
                value.sha256.as_ptr().cast(),
                &receipt_value,
                host,
            );
            out.write(receipt_value);
            3
        } else if text(message.as_ptr()).is_some_and(|s| s.contains("SHA-256")) {
            5
        } else {
            4
        }
    })
}
#[repr(C)]
pub struct Api {
    abi_version: u32,
    struct_size: u32,
    catalog: unsafe extern "C" fn(*mut c_void, *const c_char, bool, *const Host) -> u32,
    entry: unsafe extern "C" fn(*mut c_void, u32, *const c_char, *mut Entry) -> u32,
    asset: unsafe extern "C" fn(
        *mut c_void,
        u32,
        *const c_char,
        *const c_char,
        bool,
        bool,
        *const Host,
        *mut Receipt,
    ) -> u32,
    cache_path: unsafe extern "C" fn(
        *const c_char,
        *const c_char,
        *const c_char,
        *mut c_char,
        usize,
    ) -> u32,
    receipt: unsafe extern "C" fn(*const c_char, *const c_char, *mut Receipt) -> u32,
    save_receipt:
        unsafe extern "C" fn(*const c_char, *const c_char, *const Receipt, *const Host) -> u32,
}
static API: Api = Api {
    abi_version: 1,
    struct_size: std::mem::size_of::<Api>() as u32,
    catalog,
    entry,
    asset,
    cache_path,
    receipt,
    save_receipt,
};
#[no_mangle]
pub extern "C" fn bongo_cat_store_transfer_query_v1(abi: u32) -> *const Api {
    if abi == 1 {
        &API
    } else {
        std::ptr::null()
    }
}
bongo_plugin::export_completion_events!(
    bongo_cat_store_events_query_v1,
    c"pages.preference.store.installed",
    c"Model installed and SHA-256 verified",
    c"pages.preference.store.failed",
    c"Unable to update the store"
);
#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn paths_and_repository_are_confined() {
        assert!(repository("https://github.com/a/b").is_some());
        assert!(repository("https://github.com/a/../b").is_none());
        assert!(cache("cache", "../x", ".preview").is_none());
        assert!(cache("cache", &"a".repeat(64), "/../x").is_none());
        assert!(cache("cache", &"a".repeat(64), ".preview").is_some());
        assert_eq!(
            std::mem::size_of::<Api>(),
            8 + 6 * std::mem::size_of::<usize>()
        );
    }
}
