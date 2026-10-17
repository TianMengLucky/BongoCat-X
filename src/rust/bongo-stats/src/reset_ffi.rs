use crate::{guarded, Command, Instance};
use std::ffi::c_void;
use std::sync::atomic::Ordering;
pub(super) unsafe extern "C" fn reset(instance: *mut c_void) -> u32 {
    guarded(0, || {
        if instance.is_null() {
            return 0;
        }
        let instance = &*instance.cast::<Instance>();
        let old = instance.reset_status.load(Ordering::Acquire);
        if old == 1
            || instance
                .reset_status
                .compare_exchange(old, 1, Ordering::AcqRel, Ordering::Relaxed)
                .is_err()
        {
            return 0;
        }
        if instance.commands.try_send(Command::Reset).is_err() {
            instance.reset_status.store(3, Ordering::Release);
            return 0;
        }
        if let Ok(mut events) = instance.events.lock() { events.reset = 1; }
        1
    })
}
pub(super) unsafe extern "C" fn status(instance: *mut c_void) -> u32 {
    guarded(3, || {
        if instance.is_null() {
            return 3;
        }
        (&*instance.cast::<Instance>())
            .reset_status
            .load(Ordering::Acquire)
    })
}
#[repr(C)]
pub struct Plugin {
    abi_version: u32,
    struct_size: u32,
    reset: unsafe extern "C" fn(*mut c_void) -> u32,
    status: unsafe extern "C" fn(*mut c_void) -> u32,
}
static PLUGIN: Plugin = Plugin {
    abi_version: 1,
    struct_size: std::mem::size_of::<Plugin>() as u32,
    reset,
    status,
};
#[no_mangle]
pub extern "C" fn bongo_cat_statistics_reset_query(abi: u32) -> *const Plugin {
    if abi == 1 {
        &PLUGIN
    } else {
        std::ptr::null()
    }
}
