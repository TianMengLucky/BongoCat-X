use std::ffi::c_void;
#[path = "../../bongo-safe/src/store_catalog.rs"]
mod catalog;
#[repr(C)]
#[derive(Clone, Copy)]
pub struct Entry {
    name: [u8; 256],
    author: [u8; 256],
    author_url: [u8; 1024],
    repository_path: [u8; 1024],
    folder_path: [u8; 1024],
    file_url: [u8; 2048],
    folder_url: [u8; 2048],
    preview_url: [u8; 2048],
    download_url: [u8; 2048],
    sha256: [u8; 65],
}
fn buffer<const N: usize>(value: &str) -> [u8; N] {
    let mut out = [0; N];
    out[..value.len()].copy_from_slice(value.as_bytes());
    out
}
fn guard<T>(fallback: T, body: impl FnOnce() -> T) -> T {
    std::panic::catch_unwind(std::panic::AssertUnwindSafe(body)).unwrap_or(fallback)
}
extern "C" fn create() -> *mut c_void {
    guard(std::ptr::null_mut(), || {
        Box::into_raw(Box::new(Vec::<Entry>::new())).cast()
    })
}
unsafe extern "C" fn destroy(instance: *mut c_void) {
    if !instance.is_null() {
        guard((), || drop(Box::from_raw(instance.cast::<Vec<Entry>>())));
    }
}
unsafe extern "C" fn parse(instance: *mut c_void, bytes: *const u8, length: usize) -> u32 {
    guard(0, || {
        if instance.is_null() || bytes.is_null() || length > 2 * 1024 * 1024 {
            return 0;
        }
        let Ok(models) = catalog::parse(std::slice::from_raw_parts(bytes, length)) else {
            return 0;
        };
        let entries = models
            .iter()
            .map(|m| Entry {
                name: buffer(&m.name),
                author: buffer(&m.author),
                author_url: buffer(&m.author_url),
                repository_path: buffer(&m.repository_path),
                folder_path: buffer(&m.folder_path),
                file_url: buffer(&catalog::link(&m.repository_path, "blob")),
                folder_url: buffer(&catalog::link(&m.folder_path, "tree")),
                preview_url: buffer(&catalog::raw_link(&m.preview_path)),
                download_url: buffer(&catalog::raw_link(&m.repository_path)),
                sha256: buffer(&m.sha256.to_ascii_lowercase()),
            })
            .collect();
        *instance.cast::<Vec<Entry>>() = entries;
        1
    })
}
unsafe extern "C" fn count(instance: *mut c_void) -> u32 {
    guard(0, || {
        if instance.is_null() {
            0
        } else {
            (&*instance.cast::<Vec<Entry>>()).len() as u32
        }
    })
}
unsafe extern "C" fn entry(instance: *mut c_void, index: u32, out: *mut Entry) -> u32 {
    guard(0, || {
        if instance.is_null() || out.is_null() {
            return 0;
        }
        let Some(value) = (&*instance.cast::<Vec<Entry>>()).get(index as usize) else {
            return 0;
        };
        out.write(*value);
        1
    })
}
#[repr(C)]
pub struct Api {
    abi_version: u32,
    struct_size: u32,
    create: extern "C" fn() -> *mut c_void,
    destroy: unsafe extern "C" fn(*mut c_void),
    parse: unsafe extern "C" fn(*mut c_void, *const u8, usize) -> u32,
    count: unsafe extern "C" fn(*mut c_void) -> u32,
    entry: unsafe extern "C" fn(*mut c_void, u32, *mut Entry) -> u32,
}
static API: Api = Api {
    abi_version: 1,
    struct_size: std::mem::size_of::<Api>() as u32,
    create,
    destroy,
    parse,
    count,
    entry,
};
#[no_mangle]
pub extern "C" fn bongo_cat_store_query(abi: u32) -> *const Api {
    if abi == 1 {
        &API
    } else {
        std::ptr::null()
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn failed_refresh_preserves_the_previous_catalog() {
        unsafe {
            let instance = create();
            assert!(!instance.is_null());
            let bytes = serde_json::to_vec(&serde_json::json!([{"name":"猫", "author":"作者",
                "repository_path":"models/cat.zip", "folder_path":"models",
                "preview_path":"models/cat.png", "sha256":"a".repeat(64)}]))
            .unwrap();
            assert_eq!(parse(instance, bytes.as_ptr(), bytes.len()), 1);
            assert_eq!(count(instance), 1);
            assert_eq!(parse(instance, b"{}".as_ptr(), 2), 0);
            let mut out = std::mem::MaybeUninit::<Entry>::uninit();
            assert_eq!(entry(instance, 0, out.as_mut_ptr()), 1);
            let out = out.assume_init();
            assert_eq!(&out.name[..3], "猫".as_bytes());
            assert_eq!(out.author_url[0], 0);
            assert_eq!(count(instance), 1);
            destroy(instance);
        }
    }
}
