//! Optional peer-to-peer LAN add-on. Network state never enters the model catalog.
use bongo_safe::lan::{self as network, Model, Peer, Pose, Shared};
mod settings;
use bongo_plugin::{guard::guard, page, text::buffer};
use std::{
    collections::HashMap,
    ffi::{c_char, c_void, CStr},
    mem::size_of,
    path::PathBuf,
    sync::{
        atomic::{AtomicBool, Ordering},
        Arc, Mutex,
    },
    thread::JoinHandle,
};
bongo_plugin::export_locales!(bongo_cat_lan_locale_query_v1, "pages.preference.lan.", "");
#[repr(C)]
#[derive(Clone, Copy)]
pub struct ModelRecord {
    directory: [u8; 4096],
    adapter: [u8; 4096],
    setting: [u8; 4096],
    digest: [u8; 65],
}
#[repr(C)]
pub struct Snapshot {
    struct_size: u32,
    connected: u32,
    generation: u32,
    status: u32,
    peer_id: u32,
    peer_count: u32,
    model: ModelRecord,
    pose: Pose,
}
struct Instance {
    shared: Arc<Mutex<Shared>>,
    cancel: Arc<AtomicBool>,
    worker: Option<JoinHandle<()>>,
    models: Vec<Model>,
    selected: usize,
    cache: PathBuf,
    address: [u8; 256],
    code: [u8; 128],
    other_address: [u8; 256],
    other_code: [u8; 128],
    details: u32,
    frozen: Vec<Peer>,
    frozen_epoch: u64,
    names: HashMap<u32, [u8; 128]>,
    friend: [u8; 128],
    host: u32,
    addresses: String,
}
impl Instance {
    fn stop(&mut self) {
        self.cancel.store(true, Ordering::Relaxed);
        if let Some(worker) = self.worker.take() {
            let _ = worker.join();
        }
        // Keep TempDir alive until the host has destroyed its model renderer.
        if let Ok(mut state) = self.shared.lock() {
            state.disconnect();
        }
    }
    fn start(&mut self) -> bool {
        if self.worker.is_some() || self.models.is_empty() || !self.frozen.is_empty() {
            return false;
        }
        let Ok(address) = page::text(&self.address).parse() else {
            return false;
        };
        let code = page::text(&self.code).to_owned();
        if code.len() < 8 || code.len() > 64 {
            return false;
        }
        let o = network::Options {
            host: self.host != 0,
            address,
            code,
            models: self.models.clone(),
            selected: self.selected,
            cache: self.cache.clone(),
        };
        self.names.clear();
        self.cancel.store(false, Ordering::Relaxed);
        let cancel = self.cancel.clone();
        let shared = self.shared.clone();
        if let Ok(mut s) = shared.lock() {
            s.status = 1;
        }
        match std::thread::Builder::new()
            .name("lan-peer".into())
            .spawn(move || {
                let result = network::run(o, &shared, &cancel);
                if let Ok(mut s) = shared.lock() {
                    s.disconnect();
                    s.status = if cancel.load(Ordering::Relaxed) {
                        0
                    } else if result.is_err() {
                        3
                    } else {
                        0
                    };
                }
            }) {
            Ok(w) => {
                self.worker = Some(w);
                true
            }
            Err(_) => {
                if let Ok(mut s) = self.shared.lock() {
                    s.status = 3;
                }
                false
            }
        }
    }
}
fn text<const N: usize>(v: &[u8; N]) -> Option<String> {
    std::str::from_utf8(&v[..v.iter().position(|b| *b == 0)?])
        .ok()
        .map(str::to_owned)
}
unsafe extern "C" fn create(cache: *const c_char) -> *mut c_void {
    guard(std::ptr::null_mut(), || {
        if cache.is_null() {
            return std::ptr::null_mut();
        }
        let Ok(cache) = CStr::from_ptr(cache).to_str() else {
            return std::ptr::null_mut();
        };
        let cache = PathBuf::from(cache);
        if std::fs::create_dir_all(&cache).is_err() {
            return std::ptr::null_mut();
        }
        let addresses = settings::local_addresses();
        let address = buffer(addresses.lines().next().unwrap_or("127.0.0.1:39027"));
        Box::into_raw(Box::new(Instance {
            shared: Arc::new(Mutex::new(Shared::default())),
            cancel: Arc::new(AtomicBool::new(false)),
            worker: None,
            models: Vec::new(),
            selected: 0,
            cache,
            address,
            code: [0; 128],
            other_address: [0; 256],
            other_code: [0; 128],
            details: 0,
            frozen: Vec::new(),
            frozen_epoch: 0,
            names: HashMap::new(),
            friend: [0; 128],
            host: 1,
            addresses,
        }))
        .cast()
    })
}
unsafe extern "C" fn destroy(p: *mut c_void) {
    guard((), || {
        if !p.is_null() {
            Box::from_raw(p.cast::<Instance>()).stop();
        }
    })
}
unsafe extern "C" fn disconnect(p: *mut c_void) {
    guard((), || {
        if let Some(v) = p.cast::<Instance>().as_mut() {
            v.stop();
        }
    });
}
unsafe extern "C" fn models(
    p: *mut c_void,
    entries: *const ModelRecord,
    count: u32,
    selected: u32,
) -> u32 {
    guard(0, || {
        let Some(v) = p.cast::<Instance>().as_mut() else {
            return 0;
        };
        if count == 0 && v.worker.is_none() {
            v.models.clear();
            return 1;
        }
        if entries.is_null()
            || count == 0
            || count > 1024
            || selected >= count
            || v.worker.is_some()
        {
            return 0;
        }
        let result: Option<Vec<Model>> = std::slice::from_raw_parts(entries, count as usize)
            .iter()
            .map(|m| {
                Some(Model {
                    directory: text(&m.directory)?,
                    adapter: text(&m.adapter)?,
                    setting: text(&m.setting)?.replace('\\', "/"),
                    digest: text(&m.digest)?,
                })
            })
            .collect();
        let Some(result) = result else {
            return 0;
        };
        v.models = result;
        v.selected = selected as usize;
        1
    })
}
unsafe extern "C" fn tick(p: *mut c_void, local: *const Pose, out: *mut Snapshot) {
    guard((), || {
        let (Some(v), Some(local), Some(out)) =
            (p.cast::<Instance>().as_mut(), local.as_ref(), out.as_mut())
        else {
            return;
        };
        if out.struct_size as usize != size_of::<Snapshot>() {
            return;
        }
        if v.worker.as_ref().is_some_and(|w| w.is_finished()) {
            if let Some(w) = v.worker.take() {
                let _ = w.join();
            }
        }
        if let Ok(mut s) = v.shared.lock() {
            s.local = *local;
            v.frozen = s.peers.clone();
            v.frozen_epoch = s.generation;
            out.connected = u32::from(!v.frozen.is_empty());
            out.peer_count = v.frozen.len() as u32;
            out.generation = s.generation as u32;
            out.status = s.status;
        }
    })
}
unsafe extern "C" fn release(p: *mut c_void) {
    guard((), || {
        if let Some(v) = p.cast::<Instance>().as_mut() {
            let retired = v
                .shared
                .lock()
                .ok()
                .map(|mut s| s.acknowledge(v.frozen_epoch));
            drop(retired);
        }
    })
}
unsafe extern "C" fn peer(p: *mut c_void, index: u32, out: *mut Snapshot) -> u32 {
    guard(0, || {
        let (Some(v), Some(out)) = (p.cast::<Instance>().as_ref(), out.as_mut()) else {
            return 0;
        };
        if out.struct_size as usize != size_of::<Snapshot>() {
            return 0;
        }
        let Some(peer) = v.frozen.get(index as usize) else {
            return 0;
        };
        out.connected = 1;
        out.peer_id = peer.id;
        out.peer_count = v.frozen.len() as u32;
        out.generation = peer.id;
        out.status = 2;
        out.pose = peer.pose;
        let m = &peer.model;
        out.model = ModelRecord {
            directory: buffer(&m.directory),
            adapter: buffer(&m.adapter),
            setting: buffer(&m.setting),
            digest: buffer(&m.digest),
        };
        1
    })
}
#[repr(C)]
pub struct Api {
    abi_version: u32,
    struct_size: u32,
    create: unsafe extern "C" fn(*const c_char) -> *mut c_void,
    destroy: unsafe extern "C" fn(*mut c_void),
    disconnect: unsafe extern "C" fn(*mut c_void),
    models: unsafe extern "C" fn(*mut c_void, *const ModelRecord, u32, u32) -> u32,
    tick: unsafe extern "C" fn(*mut c_void, *const Pose, *mut Snapshot),
    peer: unsafe extern "C" fn(*mut c_void, u32, *mut Snapshot) -> u32,
    release: unsafe extern "C" fn(*mut c_void),
    friend_name: unsafe extern "C" fn(*mut c_void, u32) -> *const c_char,
    settings: unsafe extern "C" fn(
        *mut c_void,
        *mut bongo_plugin::ui::Ui,
        *const page::Host,
        *const page::Services,
    ) -> u32,
}
static API: Api = Api {
    abi_version: 2,
    struct_size: size_of::<Api>() as u32,
    create,
    destroy,
    disconnect,
    models,
    tick,
    peer,
    release,
    friend_name: bongo_cat_lan_friend_v2,
    settings: settings::draw,
};
#[no_mangle]
pub extern "C" fn bongo_cat_lan_query_v2(abi: u32) -> *const Api {
    if abi == 2 {
        &API
    } else {
        std::ptr::null()
    }
}
// Borrowed local alias, consumed immediately on the main thread.
#[no_mangle]
pub unsafe extern "C" fn bongo_cat_lan_friend_v2(p: *mut c_void, id: u32) -> *const c_char {
    guard(std::ptr::null(), || {
        p.cast::<Instance>().as_ref().map_or(std::ptr::null(), |v| {
            v.names.get(&id).unwrap_or(&v.friend).as_ptr().cast()
        })
    })
}
#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn bounded_pose() {
        let p = Pose {
            mask: 3,
            values: [1.; 16],
            ..Pose::default()
        };
        assert_eq!(Pose::decode(&p.encode()).unwrap().mask, 3);
        assert!(Pose::decode(&[0; 67]).is_none());
        let mut bad = p;
        bad.values[0] = f32::NAN;
        assert!(Pose::decode(&bad.encode()).is_none());
    }

    #[test]
    fn abi_layout() {
        assert_eq!(size_of::<ModelRecord>(), 12353);
        assert_eq!(size_of::<Pose>(), 80);
        assert_eq!(size_of::<Snapshot>(), 12460);
    }
}
