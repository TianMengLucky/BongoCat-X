mod locale;
pub mod ui;
mod page_ui;
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
    paid: u32,
    purchase_url: [u8; 1024],
}
struct StoredEntry {
    model: catalog::Model,
    links: [String; 4],
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
        Box::into_raw(Box::new(Vec::<StoredEntry>::new())).cast()
    })
}
unsafe extern "C" fn destroy(instance: *mut c_void) {
    if !instance.is_null() {
        guard((), || {
            drop(Box::from_raw(instance.cast::<Vec<StoredEntry>>()))
        });
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
            .into_iter()
            .map(|mut model| {
                let links = [
                    catalog::link(&model.repository_path, "blob"),
                    catalog::link(&model.folder_path, "tree"),
                    catalog::raw_link(&model.preview_path),
                    catalog::raw_link(&model.repository_path),
                ];
                model.sha256.make_ascii_lowercase();
                StoredEntry { model, links }
            })
            .collect();
        *instance.cast::<Vec<StoredEntry>>() = entries;
        1
    })
}
unsafe extern "C" fn count(instance: *mut c_void) -> u32 {
    guard(0, || {
        if instance.is_null() {
            0
        } else {
            (&*instance.cast::<Vec<StoredEntry>>()).len() as u32
        }
    })
}
unsafe extern "C" fn entry(instance: *mut c_void, index: u32, out: *mut Entry) -> u32 {
    guard(0, || {
        if instance.is_null() || out.is_null() {
            return 0;
        }
        let Some(value) = (&*instance.cast::<Vec<StoredEntry>>()).get(index as usize) else {
            return 0;
        };
        let model = &value.model;
        out.write(Entry {
            name: buffer(&model.name),
            author: buffer(&model.author),
            author_url: buffer(&model.author_url),
            repository_path: buffer(&model.repository_path),
            folder_path: buffer(&model.folder_path),
            file_url: buffer(&value.links[0]),
            folder_url: buffer(&value.links[1]),
            preview_url: buffer(&value.links[2]),
            download_url: buffer(&value.links[3]),
            sha256: buffer(&model.sha256),
            paid: u32::from(model.paid),
            purchase_url: buffer(&model.purchase_url),
        });
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
    abi_version: 2,
    struct_size: std::mem::size_of::<Api>() as u32,
    create,
    destroy,
    parse,
    count,
    entry,
};
#[no_mangle]
pub extern "C" fn bongo_cat_store_query(abi: u32) -> *const Api {
    if abi == 2 {
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
                "preview_path":"models/cat.png", "sha256":"A".repeat(64)}]))
            .unwrap();
            assert_eq!(parse(instance, bytes.as_ptr(), bytes.len()), 1);
            assert_eq!(count(instance), 1);
            assert_eq!(parse(instance, b"{}".as_ptr(), 2), 0);
            let mut out = std::mem::MaybeUninit::<Entry>::uninit();
            assert_eq!(entry(instance, 0, out.as_mut_ptr()), 1);
            let out = out.assume_init();
            assert_eq!(&out.name[..3], "猫".as_bytes());
            assert_eq!(out.author_url[0], 0);
            assert_eq!(&out.sha256[..64], "a".repeat(64).as_bytes());
            let text = |bytes: &[u8]| {
                String::from_utf8(bytes.iter().copied().take_while(|&v| v != 0).collect()).unwrap()
            };
            assert_eq!(text(&out.file_url), catalog::link("models/cat.zip", "blob"));
            assert_eq!(text(&out.folder_url), catalog::link("models", "tree"));
            assert_eq!(text(&out.preview_url), catalog::raw_link("models/cat.png"));
            assert_eq!(text(&out.download_url), catalog::raw_link("models/cat.zip"));
            assert_eq!(out.paid, 0);
            assert_eq!(out.purchase_url[0], 0);
            let stored = &(&*instance.cast::<Vec<StoredEntry>>())[0];
            let model = &stored.model;
            let heap_bytes = [
                &model.name,
                &model.author,
                &model.author_url,
                &model.repository_path,
                &model.folder_path,
                &model.preview_path,
                &model.sha256,
            ]
            .into_iter()
            .chain(stored.links.iter())
            .map(String::capacity)
            .sum::<usize>();
            assert!(std::mem::size_of::<StoredEntry>() + heap_bytes < std::mem::size_of::<Entry>());
            assert_eq!(count(instance), 1);
            destroy(instance);
        }
    }
}
