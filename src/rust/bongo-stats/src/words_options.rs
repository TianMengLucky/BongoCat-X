use crate::{guarded, Command, Instance};
use std::ffi::c_void;

unsafe extern "C" fn snapshot(instance: *mut c_void, out: *mut u32) -> u32 {
    guarded(0, || {
        let Some(instance) = instance.cast::<Instance>().as_ref() else {
            return 0;
        };
        if out.is_null() {
            return 0;
        }
        let Ok(cache) = instance.cache.try_lock() else {
            return 0;
        };
        out.write(cache.backspace);
        1
    })
}
unsafe extern "C" fn enable(instance: *mut c_void, enabled: u32) -> u32 {
    guarded(0, || {
        let Some(instance) = instance.cast::<Instance>().as_ref() else {
            return 0;
        };
        if enabled > 1 {
            return 0;
        }
        instance
            .commands
            .try_send(Command::Backspace(enabled != 0))
            .is_ok() as u32
    })
}
#[repr(C)]
pub struct Plugin {
    abi_version: u32,
    struct_size: u32,
    snapshot: unsafe extern "C" fn(*mut c_void, *mut u32) -> u32,
    enable: unsafe extern "C" fn(*mut c_void, u32) -> u32,
}
static API: Plugin = Plugin {
    abi_version: 1,
    struct_size: std::mem::size_of::<Plugin>() as u32,
    snapshot,
    enable,
};
#[no_mangle]
pub extern "C" fn bongo_cat_statistics_backspace_query(abi: u32) -> *const Plugin {
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
    fn optional_extension_rejects_invalid_arguments() {
        assert!(bongo_cat_statistics_backspace_query(0).is_null());
        assert!(!bongo_cat_statistics_backspace_query(1).is_null());
        assert!(bongo_cat_statistics_backspace_query(2).is_null());
        assert_eq!(
            std::mem::size_of::<Plugin>(),
            8 + 2 * std::mem::size_of::<usize>()
        );
        assert_eq!(unsafe { enable(std::ptr::null_mut(), 1) }, 0);
        assert_eq!(
            unsafe { snapshot(std::ptr::null_mut(), std::ptr::null_mut()) },
            0
        );
    }

    #[test]
    fn backspace_option_defaults_off_and_survives_restart_and_reset() {
        let directory = std::env::temp_dir().join(format!(
            "bongo-backspace-{}-{}",
            std::process::id(),
            std::time::SystemTime::now()
                .duration_since(std::time::UNIX_EPOCH)
                .unwrap()
                .as_nanos()
        ));
        let path = std::ffi::CString::new(directory.to_str().unwrap()).unwrap();
        unsafe {
            let first = crate::create(path.as_ptr());
            assert!(!first.is_null());
            assert_eq!(crate::flush(first), 1);
            let mut value = 99;
            assert_eq!(snapshot(first, &mut value), 1);
            assert_eq!(value, 0);
            assert_eq!(enable(first, 2), 0);
            assert_eq!(enable(first, 1), 1);
            assert_eq!(crate::flush(first), 1);
            assert_eq!(snapshot(first, &mut value), 1);
            assert_eq!(value, 1);
            assert_eq!(crate::destroy(first), 1);
            let second = crate::create(path.as_ptr());
            assert!(!second.is_null());
            assert_eq!(crate::flush(second), 1);
            assert_eq!(snapshot(second, &mut value), 1);
            assert_eq!(value, 1);
            (&*second.cast::<Instance>())
                .commands
                .send(Command::Reset)
                .unwrap();
            assert_eq!(crate::flush(second), 1);
            assert_eq!(snapshot(second, &mut value), 1);
            assert_eq!(value, 1);
            assert_eq!(enable(second, 0), 1);
            assert_eq!(crate::flush(second), 1);
            assert_eq!(snapshot(second, &mut value), 1);
            assert_eq!(value, 0);
            assert_eq!(crate::destroy(second), 1);
        }
        let (connection, store) = crate::storage::open(&directory).unwrap();
        assert!(!store.words.backspace);
        drop(connection);
        std::fs::remove_dir_all(directory).unwrap();
    }
}
