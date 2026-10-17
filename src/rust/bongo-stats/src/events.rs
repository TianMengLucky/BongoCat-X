use bongo_plugin::events::{Api, Notification};
use std::ffi::c_void;
use std::sync::atomic::Ordering;
unsafe extern "C" fn event(
    instance: *mut c_void,
    event: u32,
    result: u32,
    out: *mut Notification,
) -> u32 {
    crate::guarded(0, || {
        if out.is_null() {
            return 0;
        }
        let Some(instance) = instance.cast::<crate::Instance>().as_ref() else {
            return 0;
        };
        if event >= 2 {
            let pointer = (instance as *const crate::Instance).cast_mut().cast();
            let ok = match event {
                2 if result <= 1 => instance
                    .directory
                    .parent()
                    .map(|root| {
                        root.join(if result == 0 {
                            "statistics-export.csv"
                        } else {
                            "application-usage.csv"
                        })
                    })
                    .and_then(|path| bongo_plugin::io::path(&path))
                    .is_some_and(|path| {
                        crate::queue_export(pointer, path.as_ptr(), result != 0) != 0
                    }),
                3 => crate::reset_ffi::reset(pointer) != 0,
                4 => instance
                    .commands
                    .try_send(crate::Command::Enable(result != 0))
                    .is_ok(),
                _ => false,
            };
            if ok {
                out.write(Notification {
                    flags: 1,
                    error: 0,
                    key: std::ptr::null(),
                    fallback: std::ptr::null(),
                });
            }
            return ok as u32;
        }
        if event != 0 {
            return 0;
        }
        let Ok(mut previous) = instance.events.try_lock() else {
            return 0;
        };
        let exported = instance.export_status.load(Ordering::Acquire);
        if exported != previous.export && exported >= 2 {
            previous.export = exported;
            out.write(if exported == 2 {
                Notification::new(
                    c"pages.preference.statistics.exported",
                    c"CSV saved in the data folder",
                    false,
                )
            } else {
                Notification::new(
                    c"pages.preference.statistics.failed",
                    c"Statistics export failed",
                    true,
                )
            });
            return 1;
        }
        previous.export = exported;
        let reset = instance.reset_status.load(Ordering::Acquire);
        if previous.reset == 1 && reset >= 2 {
            previous.reset = reset;
            let mut notification = if reset == 2 {
                Notification::new(
                    c"pages.preference.statistics.resetDone",
                    c"Statistics reset",
                    false,
                )
            } else {
                Notification::new(
                    c"pages.preference.statistics.failed",
                    c"Unable to reset statistics",
                    true,
                )
            };
            if reset == 2 {
                notification.flags |= 2;
            }
            out.write(notification);
            return 1;
        }
        previous.reset = reset;
        0
    })
}
static API: Api = Api {
    abi_version: 1,
    struct_size: std::mem::size_of::<Api>() as u32,
    event,
};
#[no_mangle]
pub extern "C" fn bongo_cat_statistics_events_query_v1(abi: u32) -> *const Api {
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
    fn completion_is_delivered_once_and_poll_never_waits() {
        let instance = crate::Instance {
            directory: Default::default(),
            events: std::sync::Mutex::new(bongo_plugin::events::Completion {
                export: 1,
                reset: 1,
            }),
            overlay: std::sync::Mutex::new(crate::overlay::State::default()),
            cache: std::sync::Arc::new(std::sync::Mutex::new(crate::worker::Cache::default())),
            failed: std::sync::Arc::new(std::sync::atomic::AtomicBool::new(false)),
            export_status: std::sync::Arc::new(std::sync::atomic::AtomicU32::new(2)),
            reset_status: std::sync::Arc::new(std::sync::atomic::AtomicU32::new(2)),
            commands: std::sync::mpsc::sync_channel(1).0,
            worker: None,
        };
        let pointer = (&instance as *const crate::Instance).cast_mut().cast();
        let mut out = Notification::new(c"", c"", false);
        unsafe {
            let lock = instance.events.lock().unwrap();
            assert_eq!(event(pointer, 0, 0, &mut out), 0);
            drop(lock);
            assert_eq!(event(pointer, 0, 0, &mut out), 1);
            assert_eq!(
                std::ffi::CStr::from_ptr(out.key),
                c"pages.preference.statistics.exported"
            );
            assert_eq!(event(pointer, 0, 0, &mut out), 1);
            assert_eq!(out.flags, 3);
            assert_eq!(event(pointer, 0, 0, &mut out), 0);
        }
    }
}
