use bongo_plugin::io::text;
use bongo_plugin::models::Host;
use bongo_plugin::page::text as fixed_text;
use bongo_plugin::transfer::Receipt;
use std::ffi::{c_char, c_void, CString};
unsafe fn read(root: *const c_char, hash: *const c_char) -> Option<Receipt> {
    let mut receipt = Receipt::default();
    (crate::transfer::receipt(root, hash, &mut receipt) != 0).then_some(receipt)
}
unsafe extern "C" fn installed(root: *const c_char, hash: *const c_char, host: *const Host) -> u32 {
    crate::guard(0, || {
        let (Some(host), Some(receipt)) = (Host::get(host), read(root, hash)) else {
            return 0;
        };
        receipt.ids[..receipt.count]
            .iter()
            .any(|id| (host.query)(host.context, id.as_ptr().cast()) == 1) as u32
    })
}
unsafe extern "C" fn uninstall(root: *const c_char, hash: *const c_char, host: *const Host) -> u32 {
    crate::guard(0, || {
        let (Some(host), Some(receipt)) = (Host::get(host), read(root, hash)) else {
            return 0;
        };
        for id in &receipt.ids[..receipt.count] {
            if (host.query)(host.context, id.as_ptr().cast()) == 1
                && (host.remove)(host.context, id.as_ptr().cast()) == 0
            {
                return 0;
            }
        }
        let (Some(root), Some(hash)) = (text(root), text(hash)) else {
            return 0;
        };
        if let Some(path) = crate::transfer::cache(root, hash, ".installed") {
            let _ = std::fs::remove_file(path);
        }
        1
    })
}
unsafe extern "C" fn sync_names(
    instance: *mut c_void,
    root: *const c_char,
    host: *const Host,
) -> u32 {
    crate::guard(0, || {
        let Some(host) = Host::get(host) else {
            return 0;
        };
        if instance.is_null() || text(root).is_none() {
            return 0;
        }
        // The host only sends this event after the catalog worker is idle.
        let entries = &*instance.cast::<Vec<crate::StoredEntry>>();
        let mut changed = false;
        for entry in entries {
            if entry.model.name.is_empty() || entry.model.name.len() >= 128 {
                continue;
            }
            let (Ok(hash), Ok(name)) = (
                CString::new(entry.model.sha256.as_str()),
                CString::new(entry.model.name.as_str()),
            ) else {
                continue;
            };
            let Some(receipt) = read(root, hash.as_ptr()) else {
                continue;
            };
            for id in &receipt.ids[..receipt.count] {
                if !fixed_text(id).is_empty() && (host.query)(host.context, id.as_ptr().cast()) != 0
                {
                    changed |= (host.rename)(host.context, id.as_ptr().cast(), name.as_ptr()) != 0;
                }
            }
        }
        changed as u32
    })
}
#[repr(C)]
pub struct Api {
    abi_version: u32,
    struct_size: u32,
    installed: unsafe extern "C" fn(*const c_char, *const c_char, *const Host) -> u32,
    uninstall: unsafe extern "C" fn(*const c_char, *const c_char, *const Host) -> u32,
    sync_names: unsafe extern "C" fn(*mut c_void, *const c_char, *const Host) -> u32,
}
static API: Api = Api {
    abi_version: 1,
    struct_size: std::mem::size_of::<Api>() as u32,
    installed,
    uninstall,
    sync_names,
};
#[no_mangle]
pub extern "C" fn bongo_cat_store_models_query_v1(abi: u32) -> *const Api {
    if abi == 1 {
        &API
    } else {
        std::ptr::null()
    }
}
