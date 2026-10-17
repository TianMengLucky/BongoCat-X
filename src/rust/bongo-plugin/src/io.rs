//! Shared plugin-owned persistence and managed assets; native services are borrowed.
pub use serde_json::{json, Value};
use std::ffi::{c_char, c_void, CString};
use std::fs::{self, File};
use std::io::{Read, Write};
use std::mem::size_of;
use std::path::{Path, PathBuf};

pub const ABI: u32 = 1;
pub const DOCUMENT_LIMIT: u64 = 64 * 1024;
pub const ASSET_LIMIT: u64 = 64 * 1024 * 1024;
pub mod operation {
    pub const LOAD: u32 = 0;
    pub const SAVE: u32 = 1;
    pub const IMPORT: u32 = 2;
    pub const CLEAR: u32 = 3;
    pub const PREVIEW: u32 = 4;
    pub const INPUT: u32 = 5;
    pub const RELEASE: u32 = 6;
}
#[repr(C)]
pub struct Host {
    pub abi_version: u32,
    pub struct_size: u32,
    pub context: *mut c_void,
    pub lock: unsafe extern "C" fn(*mut c_void),
    pub unlock: unsafe extern "C" fn(*mut c_void),
    pub sha256: unsafe extern "C" fn(*const c_char, *mut c_char) -> u32,
    pub replace: unsafe extern "C" fn(*const c_char, *const c_char) -> u32,
    pub validate: unsafe extern "C" fn(*const c_char, u32) -> u32,
    pub play: unsafe extern "C" fn(*mut c_void, *const c_char) -> u32,
    pub stop: unsafe extern "C" fn(*mut c_void, *const c_char),
}
#[repr(C)]
pub struct Api {
    pub abi_version: u32,
    pub struct_size: u32,
    pub execute: unsafe extern "C" fn(
        *mut c_void,
        *const c_char,
        *const Host,
        u32,
        u32,
        *const c_char,
        *const c_char,
        u32,
    ) -> u32,
}
pub use crate::text::cstr as text;
pub fn path(value: &Path) -> Option<CString> {
    CString::new(value.to_str()?).ok()
}
impl Host {
    pub unsafe fn get<'a>(host: *const Self) -> Option<&'a Self> {
        let host = host.as_ref()?;
        (host.abi_version == ABI && host.struct_size as usize == size_of::<Self>()).then_some(host)
    }
    /// The caller keeps the instance valid, and all scene access uses this lock.
    pub fn locked<T>(&self, body: impl FnOnce() -> T) -> T {
        struct Unlock<'a>(&'a Host);
        impl Drop for Unlock<'_> {
            fn drop(&mut self) {
                unsafe { (self.0.unlock)(self.0.context) }
            }
        }
        unsafe { (self.lock)(self.context) };
        let _unlock = Unlock(self);
        body()
    }
    pub fn validate(&self, file: &Path, media: u32) -> bool {
        path(file).is_some_and(|file| unsafe { (self.validate)(file.as_ptr(), media) != 0 })
    }
    pub fn play(&self, file: &Path) -> bool {
        if !std::fs::metadata(file).is_ok_and(|m| m.len() > 0 && m.len() <= ASSET_LIMIT) {
            return false;
        }
        path(file).is_some_and(|file| unsafe { (self.play)(self.context, file.as_ptr()) != 0 })
    }
    pub fn stop(&self, file: &Path) {
        if let Some(file) = path(file) {
            unsafe { (self.stop)(self.context, file.as_ptr()) }
        }
    }
    pub fn digest(&self, file: &Path) -> Option<String> {
        let file = path(file)?;
        let mut out = [0 as c_char; 65];
        if unsafe { (self.sha256)(file.as_ptr(), out.as_mut_ptr()) } == 0 {
            return None;
        }
        let bytes: Vec<_> = out[..64].iter().map(|&v| v as u8).collect();
        (out[64] == 0
            && bytes
                .iter()
                .all(|b| b.is_ascii_digit() || (b'a'..=b'f').contains(b)))
        .then(|| String::from_utf8(bytes).ok())
        .flatten()
    }
    pub fn save(&self, file: &Path, value: &Value) -> bool {
        let Some(parent) = file.parent() else {
            return false;
        };
        if fs::create_dir_all(parent).is_err() {
            return false;
        }
        let Ok(bytes) = serde_json::to_vec_pretty(value) else {
            return false;
        };
        let temporary = file.with_extension("json.new");
        let ok = (|| {
            let mut out = File::create(&temporary).ok()?;
            out.write_all(&bytes).ok()?;
            out.sync_all().ok()?;
            drop(out);
            let (from, to) = (path(&temporary)?, path(file)?);
            (unsafe { (self.replace)(from.as_ptr(), to.as_ptr()) } != 0).then_some(())
        })()
        .is_some();
        let _ = fs::remove_file(temporary);
        ok
    }
}
/// A failed load is distinct from a missing document; callers preserve state.
pub fn load(file: &Path) -> Result<Option<Value>, ()> {
    let input = match File::open(file) {
        Ok(input) => input,
        Err(e) if e.kind() == std::io::ErrorKind::NotFound => return Ok(None),
        Err(_) => return Err(()),
    };
    let mut bytes = Vec::new();
    input
        .take(DOCUMENT_LIMIT + 1)
        .read_to_end(&mut bytes)
        .map_err(|_| ())?;
    if bytes.len() as u64 > DOCUMENT_LIMIT {
        return Err(());
    }
    serde_json::from_slice(&bytes).map(Some).map_err(|_| ())
}
/// Removes only assets created by this import when validation or commit fails.
pub struct Asset {
    pub file: PathBuf,
    pub name: String,
    created: bool,
}
impl Asset {
    pub fn import(
        host: &Host,
        directory: &Path,
        source: &Path,
        suffix: &str,
        media: u32,
    ) -> Option<Self> {
        let length = fs::metadata(source).ok()?.len();
        if length == 0 || length > ASSET_LIMIT {
            return None;
        }
        let digest = host.digest(source)?;
        fs::create_dir_all(directory).ok()?;
        let name = format!("{digest}.{suffix}");
        let file = directory.join(&name);
        let mut asset = Self {
            file,
            name,
            created: false,
        };
        // create_new avoids deleting an existing asset after a failed copy.
        match fs::OpenOptions::new()
            .write(true)
            .create_new(true)
            .open(&asset.file)
        {
            Ok(mut output) => {
                asset.created = true;
                let mut input = File::open(source).ok()?.take(ASSET_LIMIT + 1);
                if std::io::copy(&mut input, &mut output).ok()? > ASSET_LIMIT {
                    return None;
                }
                output.sync_all().ok()?;
            }
            Err(e) if e.kind() == std::io::ErrorKind::AlreadyExists => {}
            Err(_) => return None,
        }
        if fs::metadata(&asset.file).ok()?.len() > ASSET_LIMIT
            || host.digest(&asset.file)? != digest
            || !host.validate(&asset.file, media)
        {
            return None;
        }
        Some(asset)
    }
    pub fn commit(mut self) {
        self.created = false;
    }
}
impl Drop for Asset {
    fn drop(&mut self) {
        if self.created {
            let _ = fs::remove_file(&self.file);
        }
    }
}
pub fn fixed<const N: usize>(value: &Value, key: &str) -> Option<[u8; N]> {
    let value = value.get(key)?.as_str()?;
    (value.len() < N && !value.contains('\0')).then(|| crate::text::buffer(value))
}
pub fn filename(source: &Path) -> Option<String> {
    Some(source.file_name()?.to_str()?.to_owned())
}

#[cfg(test)]
mod tests {
    use super::*;
    fn directory() -> PathBuf {
        let path = std::env::temp_dir().join(format!(
            "bongo-plugin-io-{}-{}",
            std::process::id(),
            std::time::SystemTime::now()
                .duration_since(std::time::UNIX_EPOCH)
                .unwrap()
                .as_nanos()
        ));
        fs::create_dir_all(&path).unwrap();
        path
    }
    unsafe extern "C" fn noop(_: *mut c_void) {}
    unsafe extern "C" fn hash(_: *const c_char, out: *mut c_char) -> u32 {
        std::ptr::copy_nonoverlapping(
            b"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\0".as_ptr(),
            out.cast(),
            65,
        );
        1
    }
    unsafe extern "C" fn replace(from: *const c_char, to: *const c_char) -> u32 {
        fs::rename(text(from).unwrap(), text(to).unwrap()).is_ok() as u32
    }
    unsafe extern "C" fn valid(file: *const c_char, _: u32) -> u32 {
        fs::read(text(file).unwrap()).is_ok_and(|b| b == b"valid") as u32
    }
    unsafe extern "C" fn play(_: *mut c_void, _: *const c_char) -> u32 {
        1
    }
    unsafe extern "C" fn stop(_: *mut c_void, _: *const c_char) {}
    fn host() -> Host {
        Host {
            abi_version: ABI,
            struct_size: size_of::<Host>() as u32,
            context: std::ptr::null_mut(),
            lock: noop,
            unlock: noop,
            sha256: hash,
            replace,
            validate: valid,
            play,
            stop,
        }
    }
    #[test]
    fn records_match_c_and_fixed_strings_reject_truncation() {
        assert_eq!(size_of::<Host>(), 8 + 8 * size_of::<usize>());
        assert_eq!(size_of::<Api>(), 8 + size_of::<usize>());
        assert!(fixed::<4>(&json!({"name":"long"}), "name").is_none());
        assert!(fixed::<4>(&json!({"name":"a\u{0}b"}), "name").is_none());
        assert_eq!(
            fixed::<4>(&json!({"name":"好"}), "name"),
            Some([0xe5, 0xa5, 0xbd, 0])
        );
    }
    #[test]
    fn imports_remove_only_their_own_uncommitted_files() {
        let directory = directory();
        let source = directory.join("source");
        let assets = directory.join("assets");
        fs::write(&source, b"valid").unwrap();
        let asset = Asset::import(&host(), &assets, &source, "img", 0).unwrap();
        let target = asset.file.clone();
        assert!(target.is_file());
        drop(asset);
        assert!(!target.exists());
        Asset::import(&host(), &assets, &source, "img", 0)
            .unwrap()
            .commit();
        assert!(target.is_file());
        fs::write(&target, b"invalid").unwrap();
        assert!(Asset::import(&host(), &assets, &source, "img", 0).is_none());
        assert_eq!(fs::read(&target).unwrap(), b"invalid");
        fs::remove_dir_all(directory).unwrap();
    }
    #[test]
    fn documents_preserve_invalid_input_and_bound_reads() {
        let directory = directory();
        let file = directory.join("scene.json");
        assert_eq!(load(&file), Ok(None));
        assert!(host().save(&file, &json!({"version":1})));
        assert_eq!(load(&file), Ok(Some(json!({"version":1}))));
        fs::write(&file, b"invalid").unwrap();
        assert_eq!(load(&file), Err(()));
        assert_eq!(fs::read(&file).unwrap(), b"invalid");
        fs::write(&file, vec![b' '; DOCUMENT_LIMIT as usize + 1]).unwrap();
        assert_eq!(load(&file), Err(()));
        fs::remove_dir_all(directory).unwrap();
    }
}
