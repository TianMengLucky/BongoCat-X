mod locale;
mod events;
mod overlay;
mod keyboard;
pub mod ui;
mod page_ui;
mod metrics;
mod page_ffi;
mod reset_ffi;
#[cfg(test)]
mod reset_tests;
mod storage;
#[cfg(test)]
mod tests;
mod usage;
mod usage_ffi;
mod usage_storage;
#[cfg(test)]
mod usage_tests;
mod worker;
use metrics::{Metrics, Snapshot};
use std::ffi::{c_char, c_void, CStr};
use std::panic::{catch_unwind, AssertUnwindSafe};
use std::path::{Path, PathBuf};
use std::sync::{
    atomic::{AtomicBool, AtomicU32, Ordering},
    mpsc, Arc, Mutex,
};
use std::thread::JoinHandle;
enum Command {
    Configure(worker::Source),
    Keyboard(keyboard::Source),
    Ingest(Metrics),
    Enable(bool),
    Export(PathBuf, bool),
    Flush(mpsc::Sender<bool>),
    Reset,
    Stop,
}
struct Instance {
    directory: PathBuf,
    events: Mutex<bongo_plugin::events::Completion>,
    overlay: Mutex<overlay::State>,
    cache: Arc<Mutex<worker::Cache>>,
    failed: Arc<AtomicBool>,
    export_status: Arc<AtomicU32>,
    reset_status: Arc<AtomicU32>,
    commands: mpsc::SyncSender<Command>,
    worker: Option<JoinHandle<bool>>,
}
fn guarded<T>(fallback: T, body: impl FnOnce() -> T) -> T {
    catch_unwind(AssertUnwindSafe(body)).unwrap_or(fallback)
}
unsafe fn path<'a>(text: *const c_char) -> Option<&'a Path> {
    if text.is_null() {
        return None;
    }
    CStr::from_ptr(text)
        .to_str()
        .ok()
        .filter(|s| !s.is_empty())
        .map(Path::new)
}
unsafe extern "C" fn create(directory: *const c_char) -> *mut c_void {
    guarded(std::ptr::null_mut(), || {
        let Some(directory) = path(directory) else {
            return std::ptr::null_mut();
        };
        let directory = directory.to_owned();
        let instance_directory = directory.clone();
        let cache = Arc::new(Mutex::new(worker::Cache::default()));
        let failed = Arc::new(AtomicBool::new(false));
        let export_status = Arc::new(AtomicU32::new(0));
        let (commands, receiver) = mpsc::sync_channel(32);
        let shared = cache.clone();
        let error = failed.clone();
        let exported = export_status.clone();
        let reset_status = Arc::new(AtomicU32::new(0));
        let reset = reset_status.clone();
        let Ok(worker) = std::thread::Builder::new()
            .name("bongo-statistics".into())
            .spawn(move || {
                let ok = guarded(false, || {
                    worker::run(directory, receiver, shared, error.clone(), exported, reset)
                });
                if !ok {
                    error.store(true, Ordering::Relaxed);
                }
                ok
            })
        else {
            return std::ptr::null_mut();
        };
        Box::into_raw(Box::new(Instance {
            directory: instance_directory,
            events: Mutex::new(bongo_plugin::events::Completion::default()),
            overlay: Mutex::new(overlay::State::default()),
            cache,
            failed,
            export_status,
            reset_status,
            commands,
            worker: Some(worker),
        }))
        .cast()
    })
}
unsafe extern "C" fn destroy(instance: *mut c_void) -> u32 {
    guarded(0, || {
        if instance.is_null() {
            return 1;
        }
        let mut instance = Box::from_raw(instance.cast::<Instance>());
        let _ = instance.commands.send(Command::Stop);
        instance
            .worker
            .take()
            .is_some_and(|worker| worker.join().unwrap_or(false)) as u32
    })
}
unsafe extern "C" fn ingest(instance: *mut c_void, metrics: *const Metrics) {
    guarded((), || {
        if instance.is_null() || metrics.is_null() || *metrics == Metrics::default() {
            return;
        }
        let instance = &*instance.cast::<Instance>();
        if instance
            .commands
            .try_send(Command::Ingest(*metrics))
            .is_err()
        {
            instance.failed.store(true, Ordering::Relaxed);
        }
    });
}
unsafe extern "C" fn snapshot(instance: *mut c_void, output: *mut Snapshot) -> u32 {
    guarded(0, || {
        if instance.is_null()
            || output.is_null()
            || (*output).struct_size as usize != std::mem::size_of::<Snapshot>()
        {
            return 0;
        }
        let instance = &*instance.cast::<Instance>();
        let Ok(cache) = instance.cache.try_lock() else {
            return 0;
        };
        *output = cache.metrics;
        (*output).storage_error |= instance.failed.load(Ordering::Relaxed) as u32;
        1
    })
}
// Lifecycle-only barrier. Interactive export and settings commands never wait.
unsafe extern "C" fn flush(instance: *mut c_void) -> u32 {
    guarded(0, || {
        if instance.is_null() {
            return 0;
        }
        let instance = &*instance.cast::<Instance>();
        let (sender, receiver) = mpsc::channel();
        if instance.commands.send(Command::Flush(sender)).is_err() {
            return 0;
        }
        receiver.recv().unwrap_or(false) as u32
    })
}
unsafe fn queue_export(instance: *mut c_void, target: *const c_char, applications: bool) -> u32 {
    let Some(target) = path(target) else {
        return 0;
    };
    if instance.is_null() {
        return 0;
    }
    let instance = &*instance.cast::<Instance>();
    let old = instance.export_status.load(Ordering::Acquire);
    if old == 1
        || instance
            .export_status
            .compare_exchange(old, 1, Ordering::AcqRel, Ordering::Relaxed)
            .is_err()
    {
        return 0;
    }
    if instance
        .commands
        .try_send(Command::Export(target.to_owned(), applications))
        .is_err()
    {
        instance.export_status.store(3, Ordering::Release);
        return 0;
    }
    if let Ok(mut events) = instance.events.lock() { events.export = 1; }
    1
}
unsafe extern "C" fn export_csv(instance: *mut c_void, target: *const c_char) -> u32 {
    guarded(0, || queue_export(instance, target, false))
}
#[repr(C)]
pub struct Plugin {
    abi_version: u32,
    struct_size: u32,
    create: unsafe extern "C" fn(*const c_char) -> *mut c_void,
    destroy: unsafe extern "C" fn(*mut c_void) -> u32,
    ingest: unsafe extern "C" fn(*mut c_void, *const Metrics),
    snapshot: unsafe extern "C" fn(*mut c_void, *mut Snapshot) -> u32,
    flush: unsafe extern "C" fn(*mut c_void) -> u32,
    export_csv: unsafe extern "C" fn(*mut c_void, *const c_char) -> u32,
}
static PLUGIN: Plugin = Plugin {
    abi_version: 1,
    struct_size: std::mem::size_of::<Plugin>() as u32,
    create,
    destroy,
    ingest,
    snapshot,
    flush,
    export_csv,
};
#[no_mangle]
pub extern "C" fn bongo_cat_statistics_plugin_query(abi: u32) -> *const Plugin {
    if abi == 1 {
        &PLUGIN
    } else {
        std::ptr::null()
    }
}
