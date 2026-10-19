use crate::{guarded, words::Snapshot, Instance};
use std::ffi::c_void;

unsafe extern "C" fn snapshot(instance: *mut c_void, period: u32, out: *mut Snapshot) -> u32 {
    guarded(0, || {
        let Some(instance) = instance.cast::<Instance>().as_ref() else {
            return 0;
        };
        if out.is_null() || period >= 4 {
            return 0;
        }
        let Ok(cache) = instance.cache.try_lock() else {
            return 0;
        };
        out.write(cache.words[period as usize]);
        1
    })
}
#[repr(C)]
pub struct Plugin {
    abi_version: u32,
    struct_size: u32,
    snapshot: unsafe extern "C" fn(*mut c_void, u32, *mut Snapshot) -> u32,
}
static API: Plugin = Plugin {
    abi_version: 1,
    struct_size: std::mem::size_of::<Plugin>() as u32,
    snapshot,
};
#[no_mangle]
pub extern "C" fn bongo_cat_statistics_words_query(abi: u32) -> *const Plugin {
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
    fn layout_and_query_match_the_optional_c_abi() {
        assert_eq!(std::mem::size_of::<crate::words::Character>(), 16);
        assert_eq!(std::mem::size_of::<Snapshot>(), 544);
        assert!(bongo_cat_statistics_words_query(0).is_null());
        assert!(bongo_cat_statistics_words_query(2).is_null());
        assert!(!bongo_cat_statistics_words_query(1).is_null());
        assert_eq!(
            unsafe { snapshot(std::ptr::null_mut(), 0, std::ptr::null_mut()) },
            0
        );
    }
}
