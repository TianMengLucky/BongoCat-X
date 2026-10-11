use crate::{guarded, usage, worker, Command, Instance};
use std::ffi::{c_char, c_void};
use std::sync::atomic::Ordering;

unsafe extern "C" fn configure(
    instance: *mut c_void,
    context: *mut c_void,
    metrics: Option<worker::MetricsPoll>,
    application: Option<worker::ApplicationPoll>,
) -> u32 {
    guarded(0, || {
        if instance.is_null() {
            return 0;
        }
        let (Some(metrics), Some(application)) = (metrics, application) else {
            return 0;
        };
        (&*instance.cast::<Instance>())
            .commands
            .try_send(Command::Configure(worker::Source {
                context,
                metrics,
                application,
            }))
            .is_ok() as u32
    })
}
unsafe extern "C" fn snapshot(
    instance: *mut c_void,
    period: u32,
    output: *mut usage::Snapshot,
) -> u32 {
    guarded(0, || {
        if instance.is_null()
            || period > 3
            || output.is_null()
            || (*output).struct_size as usize != std::mem::size_of::<usage::Snapshot>()
        {
            return 0;
        }
        let instance = &*instance.cast::<Instance>();
        let Ok(cache) = instance.cache.try_lock() else {
            return 0;
        };
        *output = cache.usage[period as usize].clone();
        (*output).export_status = instance.export_status.load(Ordering::Acquire);
        1
    })
}
unsafe extern "C" fn set_enabled(instance: *mut c_void, enabled: u32) -> u32 {
    guarded(0, || {
        if instance.is_null() {
            return 0;
        }
        (&*instance.cast::<Instance>())
            .commands
            .try_send(Command::Enable(enabled != 0))
            .is_ok() as u32
    })
}
unsafe extern "C" fn export_csv(instance: *mut c_void, path: *const c_char) -> u32 {
    guarded(0, || crate::queue_export(instance, path, true))
}
unsafe extern "C" fn export_status(instance: *mut c_void) -> u32 {
    guarded(3, || {
        if instance.is_null() {
            return 3;
        }
        (&*instance.cast::<Instance>())
            .export_status
            .load(Ordering::Acquire)
    })
}
#[repr(C)]
pub struct Plugin {
    abi_version: u32,
    struct_size: u32,
    configure: unsafe extern "C" fn(
        *mut c_void,
        *mut c_void,
        Option<worker::MetricsPoll>,
        Option<worker::ApplicationPoll>,
    ) -> u32,
    snapshot: unsafe extern "C" fn(*mut c_void, u32, *mut usage::Snapshot) -> u32,
    set_enabled: unsafe extern "C" fn(*mut c_void, u32) -> u32,
    export_csv: unsafe extern "C" fn(*mut c_void, *const c_char) -> u32,
    export_status: unsafe extern "C" fn(*mut c_void) -> u32,
}
static PLUGIN: Plugin = Plugin {
    abi_version: 1,
    struct_size: std::mem::size_of::<Plugin>() as u32,
    configure,
    snapshot,
    set_enabled,
    export_csv,
    export_status,
};
#[no_mangle]
pub extern "C" fn bongo_cat_statistics_usage_query(abi: u32) -> *const Plugin {
    if abi == 1 {
        &PLUGIN
    } else {
        std::ptr::null()
    }
}
