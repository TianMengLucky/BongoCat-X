//! Experimental companion. Main-thread state and one cancellable request worker.
mod config;
mod network;
mod overlay;
mod settings;
#[cfg(test)]
mod tests;
use bongo_plugin::{guard::guard, page, text::buffer};
use config::Config;
use std::ffi::{c_char, c_int, c_void};
use std::mem::size_of;
use std::path::PathBuf;
use std::sync::{
    atomic::{AtomicBool, Ordering},
    Arc,
};
use std::thread::JoinHandle;

bongo_plugin::export_locales!(
    bongo_cat_companion_locale_query_v1,
    "pages.preference.companion.",
    ""
);

pub type Cancelled = unsafe extern "C" fn(*mut c_void) -> bool;
#[repr(C)]
#[derive(Clone, Copy)]
pub struct Host {
    abi_version: u32,
    struct_size: u32,
    post: unsafe extern "C" fn(
        *const c_char,
        *const c_char,
        *const c_char,
        Option<Cancelled>,
        *mut c_void,
        *mut *mut u8,
        *mut usize,
    ) -> c_int,
    free_response: unsafe extern "C" fn(*mut u8, usize),
    replace: unsafe extern "C" fn(*const c_char, *const c_char) -> u32,
}
#[repr(C)]
pub struct Snapshot {
    struct_size: u32,
    visible: u32,
    busy: u32,
    sequence: u32,
    status: u32,
    text: [u8; 4096],
}
struct Worker {
    cancel: Arc<AtomicBool>,
    handle: JoinHandle<Result<String, u32>>,
}
struct Instance {
    config: Config,
    draft: Config,
    file: PathBuf,
    host: Host,
    worker: Option<Worker>,
    last_request: Option<u64>,
    last_active: u64,
    initialized: bool,
    idle_sent: bool,
    startup_pending: bool,
    text: String,
    sequence: u32,
    status: u32,
    expires: u64,
    now: u64,
}
impl Instance {
    fn clear(&mut self) {
        self.text.clear();
        self.expires = 0;
        if let Some(w) = &self.worker {
            w.cancel.store(true, Ordering::Release);
        }
    }
    fn poll(&mut self, now: u64) {
        self.now = now;
        if !self.initialized {
            self.last_active = now;
            self.initialized = true;
        }
        if self.worker.as_ref().is_some_and(|w| w.handle.is_finished()) {
            let w = self.worker.take().expect("finished worker");
            let cancelled = w.cancel.load(Ordering::Acquire);
            let result = w.handle.join().unwrap_or(Err(2));
            if !cancelled && self.config.enabled != 0 {
                match result {
                    Ok(text) => {
                        self.text = text;
                        self.status = 0;
                        self.sequence = self.sequence.wrapping_add(1);
                        self.expires = now.saturating_add((self.config.duration * 1000.0) as u64);
                    }
                    Err(status) => {
                        self.status = status;
                    }
                }
            }
        }
        if now >= self.expires {
            self.text.clear();
        }
    }
    fn trigger(&mut self, event: u32, now: u64) {
        self.poll(now);
        if (2..=3).contains(&event) {
            self.last_active = now;
            self.idle_sent = false;
        }
        let index = match event {
            0 if self.startup_pending && self.config.triggers[0] != 0 => 0,
            0 if !self.idle_sent
                && now.saturating_sub(self.last_active) >= (self.config.idle * 1000.0) as u64 =>
            {
                1
            }
            1 => 0,
            2 => 2,
            3 => 3,
            4 => 4,
            5 => 0,
            _ => return,
        };
        if self.config.enabled == 0
            || (event != 5 && self.config.triggers[index] == 0)
            || self.worker.is_some()
            || self.last_request.is_some_and(|last| {
                now.saturating_sub(last) < (self.config.cooldown * 1000.0) as u64
            })
        {
            return;
        }
        self.last_request = Some(now);
        if index == 0 {
            self.startup_pending = false;
        }
        if index == 1 {
            self.idle_sent = true;
        }
        let cancel = Arc::new(AtomicBool::new(false));
        let worker_cancel = cancel.clone();
        let host = self.host;
        let config = self.config.clone();
        match std::thread::Builder::new()
            .name("companion-request".into())
            .spawn(move || network::request(host, config, index, worker_cancel))
        {
            Ok(handle) => self.worker = Some(Worker { cancel, handle }),
            Err(_) => self.status = 2,
        }
    }
    fn save(&mut self) -> bool {
        if !self.draft.valid() {
            return false;
        }
        let Some(parent) = self.file.parent() else {
            return false;
        };
        let temporary = self.file.with_extension("json.new");
        let ok = (|| {
            use std::io::Write;
            std::fs::create_dir_all(parent).ok()?;
            let mut out = std::fs::File::create(&temporary).ok()?;
            out.write_all(self.draft.encode().to_string().as_bytes())
                .ok()?;
            out.sync_all().ok()?;
            drop(out);
            let from = bongo_plugin::io::path(&temporary)?;
            let to = bongo_plugin::io::path(&self.file)?;
            (unsafe { (self.host.replace)(from.as_ptr(), to.as_ptr()) } != 0).then_some(())
        })()
        .is_some();
        let _ = std::fs::remove_file(temporary);
        if ok {
            self.clear();
            let enabling = self.config.enabled == 0 && self.draft.enabled != 0;
            self.config = self.draft.clone();
            self.last_active = self.now;
            self.idle_sent = false;
            if enabling {
                self.startup_pending = true;
                self.last_request = None;
            }
        }
        ok
    }
}
unsafe extern "C" fn create(root: *const c_char, host: *const Host) -> *mut c_void {
    guard(std::ptr::null_mut(), || {
        let (Some(root), Some(host)) = (bongo_plugin::text::cstr(root), host.as_ref()) else {
            return std::ptr::null_mut();
        };
        if host.abi_version != 1 || host.struct_size as usize != size_of::<Host>() {
            return std::ptr::null_mut();
        }
        let file = PathBuf::from(root).join("companion").join("settings.json");
        let Ok(config) = Config::load(&file) else {
            return std::ptr::null_mut();
        };
        Box::into_raw(Box::new(Instance {
            draft: config.clone(),
            config,
            file,
            host: *host,
            worker: None,
            last_request: None,
            last_active: 0,
            initialized: false,
            idle_sent: false,
            startup_pending: true,
            text: String::new(),
            sequence: 0,
            status: 0,
            expires: 0,
            now: 0,
        }))
        .cast()
    })
}
unsafe extern "C" fn destroy(instance: *mut c_void) {
    guard((), || {
        if instance.is_null() {
            return;
        }
        let mut instance = Box::from_raw(instance.cast::<Instance>());
        instance.clear();
        if let Some(w) = instance.worker.take() {
            let _ = w.handle.join();
        }
    })
}
unsafe extern "C" fn event(instance: *mut c_void, event: u32, now: u64) {
    guard((), || {
        if let Some(instance) = instance.cast::<Instance>().as_mut() {
            instance.trigger(event, now);
        }
    })
}
unsafe extern "C" fn snapshot(instance: *mut c_void, out: *mut Snapshot) -> u32 {
    guard(0, || {
        let (Some(instance), Some(out)) = (instance.cast::<Instance>().as_ref(), out.as_mut())
        else {
            return 0;
        };
        if out.struct_size as usize != size_of::<Snapshot>() {
            return 0;
        }
        *out = Snapshot {
            struct_size: size_of::<Snapshot>() as u32,
            visible: u32::from(instance.config.enabled != 0 && !instance.text.is_empty()),
            busy: u32::from(instance.worker.is_some()),
            sequence: instance.sequence,
            status: instance.status,
            text: buffer(&instance.text),
        };
        1
    })
}
#[repr(C)]
pub struct Api {
    abi_version: u32,
    struct_size: u32,
    create: unsafe extern "C" fn(*const c_char, *const Host) -> *mut c_void,
    destroy: unsafe extern "C" fn(*mut c_void),
    event: unsafe extern "C" fn(*mut c_void, u32, u64),
    snapshot: unsafe extern "C" fn(*mut c_void, *mut Snapshot) -> u32,
    settings: unsafe extern "C" fn(
        *mut c_void,
        *mut bongo_plugin::ui::Ui,
        *const page::Host,
        *const page::Services,
    ) -> u32,
}
static API: Api = Api {
    abi_version: 1,
    struct_size: size_of::<Api>() as u32,
    create,
    destroy,
    event,
    snapshot,
    settings: settings::draw,
};
#[no_mangle]
pub extern "C" fn bongo_cat_companion_query_v1(abi: u32) -> *const Api {
    if abi == 1 {
        &API
    } else {
        std::ptr::null()
    }
}
