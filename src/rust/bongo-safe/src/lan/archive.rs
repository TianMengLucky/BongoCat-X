//! Session-only resource archives. Never call model import/catalog installation.
use sha2::{Digest, Sha256};
use std::sync::atomic::{AtomicBool, Ordering};
use std::{
    fs::{self, File},
    io::{self, Read, Write},
    path::{Path, PathBuf},
};
use tempfile::{NamedTempFile, TempDir};
use zip::{write::SimpleFileOptions, CompressionMethod, ZipArchive, ZipWriter};
pub const MAX_BYTES: u64 = 256 * 1024 * 1024;
pub const MAX_ZIP: u64 = 128 * 1024 * 1024;
const MAX_FILES: usize = 4096;
pub fn invalid() -> io::Error {
    io::Error::new(io::ErrorKind::InvalidData, "invalid LAN model")
}
pub fn resource(name: &str) -> bool {
    !name.is_empty()
        && name.len() < 1024
        && !name.contains(['\\', ':', '\0', '<', '>', '"', '|', '?', '*'])
        && name.split('/').all(|p| {
            !p.is_empty()
                && p != "."
                && p != ".."
                && !p.ends_with(['.', ' '])
                && !p.chars().any(|c| c.is_control())
                && !matches!(
                    p.split('.')
                        .next()
                        .unwrap_or("")
                        .to_ascii_uppercase()
                        .as_str(),
                    "CON"
                        | "PRN"
                        | "AUX"
                        | "NUL"
                        | "COM1"
                        | "COM2"
                        | "COM3"
                        | "COM4"
                        | "COM5"
                        | "COM6"
                        | "COM7"
                        | "COM8"
                        | "COM9"
                        | "LPT1"
                        | "LPT2"
                        | "LPT3"
                        | "LPT4"
                        | "LPT5"
                        | "LPT6"
                        | "LPT7"
                        | "LPT8"
                        | "LPT9"
                )
        })
        && matches!(
            Path::new(name)
                .extension()
                .and_then(|s| s.to_str())
                .unwrap_or("")
                .to_ascii_lowercase()
                .as_str(),
            "json"
                | "moc3"
                | "png"
                | "jpg"
                | "jpeg"
                | "webp"
                | "inp"
                | "inx"
                | "bin"
                | "wav"
                | "mp3"
                | "ogg"
                | "flac"
                | "txt"
        )
}
fn files(
    root: &Path,
    dir: &Path,
    out: &mut Vec<(String, PathBuf)>,
    depth: u32,
    cancel: &AtomicBool,
) -> io::Result<()> {
    if depth > 24 {
        return Err(invalid());
    }
    for entry in fs::read_dir(dir)? {
        if cancel.load(Ordering::Relaxed) {
            return Err(io::ErrorKind::ConnectionAborted.into());
        }
        let entry = entry?;
        let kind = entry.file_type()?;
        if kind.is_symlink() {
            return Err(invalid());
        }
        if kind.is_dir() {
            files(root, &entry.path(), out, depth + 1, cancel)?;
        } else if kind.is_file() {
            let path = entry.path();
            let name = path
                .strip_prefix(root)
                .map_err(|_| invalid())?
                .to_str()
                .ok_or_else(invalid)?
                .replace('\\', "/");
            if resource(&name) {
                out.push((name, path));
            }
            if out.len() > MAX_FILES {
                return Err(invalid());
            }
        }
    }
    Ok(())
}
pub fn pack(
    root: &Path,
    adapter: &Path,
    cache: &Path,
    cancel: &AtomicBool,
) -> io::Result<(NamedTempFile, String)> {
    let mut entries = Vec::new();
    files(root, root, &mut entries, 0, cancel)?;
    for (name, _) in &mut entries {
        *name = format!("model/{name}");
    }
    let mut assets = Vec::new();
    if !adapter.as_os_str().is_empty() {
        files(adapter, adapter, &mut assets, 0, cancel)?;
    }
    for (name, path) in assets {
        entries.push((format!("adapter/{name}"), path));
    }
    if entries.len() > MAX_FILES {
        return Err(invalid());
    }
    entries.sort_by(|a, b| a.0.cmp(&b.0));
    let mut temp = NamedTempFile::new_in(cache)?;
    let mut zip = ZipWriter::new(temp.as_file_mut());
    let options = SimpleFileOptions::default().compression_method(CompressionMethod::Deflated);
    let mut total = 0;
    for (name, path) in entries {
        let mut file = File::open(path)?;
        let size = file.metadata()?.len();
        total += size;
        if total > MAX_BYTES {
            return Err(invalid());
        }
        zip.start_file(name, options)?;
        let copied = io::copy(&mut CancelRead(&mut file, cancel).take(size + 1), &mut zip)?;
        if copied != size {
            return Err(invalid());
        }
    }
    zip.finish()?;
    if temp.as_file().metadata()?.len() > MAX_ZIP {
        return Err(invalid());
    }
    let digest = digest(temp.path())?;
    Ok((temp, digest))
}
pub fn digest(path: &Path) -> io::Result<String> {
    let mut hash = Sha256::new();
    io::copy(&mut File::open(path)?, &mut HashWriter(&mut hash))?;
    Ok(format!("{:x}", hash.finalize()))
}
struct HashWriter<'a>(&'a mut Sha256);
impl Write for HashWriter<'_> {
    fn write(&mut self, data: &[u8]) -> io::Result<usize> {
        self.0.update(data);
        Ok(data.len())
    }
    fn flush(&mut self) -> io::Result<()> {
        Ok(())
    }
}
// Check every JSON string before handing resource references to native renderers.
fn json_safe(v: &serde_json::Value) -> bool {
    match v {
        serde_json::Value::String(s) => {
            !s.contains(['\\', ':', '\0'])
                && !s.starts_with('/')
                && !s.split('/').any(|p| p == "..")
        }
        serde_json::Value::Array(a) => a.iter().all(json_safe),
        serde_json::Value::Object(o) => o.values().all(json_safe),
        _ => true,
    }
}
pub fn unpack(
    file: &Path,
    cache: &Path,
    setting: &str,
    cancel: &AtomicBool,
) -> io::Result<TempDir> {
    if !resource(setting) {
        return Err(invalid());
    }
    let mut zip = ZipArchive::new(File::open(file)?)?;
    if zip.len() > MAX_FILES {
        return Err(invalid());
    }
    let dir = tempfile::Builder::new().prefix("peer-").tempdir_in(cache)?;
    let mut total = 0u64;
    let mut names = std::collections::HashSet::new();
    for i in 0..zip.len() {
        let mut entry = zip.by_index(i)?;
        let name = entry.name().to_owned();
        if !resource(&name)
            || !names.insert(name.to_lowercase())
            || entry.unix_mode().is_some_and(|m| m & 0o170000 == 0o120000)
        {
            return Err(invalid());
        }
        total = total.checked_add(entry.size()).ok_or_else(invalid)?;
        if total > MAX_BYTES {
            return Err(invalid());
        }
        let path = dir.path().join(&name);
        fs::create_dir_all(path.parent().ok_or_else(invalid)?)?;
        let mut out = fs::OpenOptions::new()
            .write(true)
            .create_new(true)
            .open(&path)?;
        let size = entry.size();
        if io::copy(&mut CancelRead(&mut entry, cancel).take(size + 1), &mut out)? != size {
            return Err(invalid());
        }
        if name.to_ascii_lowercase().ends_with(".json") {
            if size > 8 * 1024 * 1024 {
                return Err(invalid());
            }
            let bytes = fs::read(&path)?;
            let json: serde_json::Value = match serde_json::from_slice(&bytes) {
                Ok(json) => json,
                Err(_) => json5::from_str(std::str::from_utf8(&bytes).map_err(|_| invalid())?)
                    .map_err(|_| invalid())?,
            };
            if !json_safe(&json) {
                return Err(invalid());
            }
        }
    }
    if !dir.path().join(setting).is_file() {
        return Err(invalid());
    }
    Ok(dir)
}
#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn hostile_names() {
        for s in [
            "../cat.png",
            "/cat.png",
            "C:cat.png",
            "a\\b.png",
            "CON.png",
            "x.dll",
            "a/../b.png",
        ] {
            assert!(!resource(s), "{s}");
        }
        assert!(resource("resources/cat.model3.json"));
        assert!(!json_safe(
            &serde_json::json!({"FileReferences":{"Moc":"../secret.moc3"}})
        ));
    }

    #[test]
    fn traversal_archive_is_not_extracted() {
        let source = tempfile::tempdir().unwrap();
        let mut file = NamedTempFile::new_in(source.path()).unwrap();
        let mut zip = ZipWriter::new(file.as_file_mut());
        zip.start_file("../escape.json", SimpleFileOptions::default())
            .unwrap();
        zip.write_all(b"{}").unwrap();
        zip.finish().unwrap();
        assert!(unpack(
            file.path(),
            source.path(),
            "model/cat.json",
            &AtomicBool::new(false)
        )
        .is_err());
        assert_eq!(fs::read_dir(source.path()).unwrap().count(), 1);
    }
    #[test]
    fn cancelled_package_leaves_no_cache() {
        let source = tempfile::tempdir().unwrap();
        fs::write(source.path().join("cat.json"), b"{}").unwrap();
        assert!(pack(
            source.path(),
            source.path(),
            source.path(),
            &AtomicBool::new(true)
        )
        .is_err());
        assert_eq!(fs::read_dir(source.path()).unwrap().count(), 1);
    }
    #[test]
    fn round_trip() {
        let source = tempfile::tempdir().unwrap();
        fs::write(source.path().join("cat.model3.json"), b"{}").unwrap();
        let (zip, hash) = pack(
            source.path(),
            source.path(),
            source.path(),
            &AtomicBool::new(false),
        )
        .unwrap();
        assert_eq!(hash.len(), 64);
        let copy = unpack(
            zip.path(),
            source.path(),
            "model/cat.model3.json",
            &AtomicBool::new(false),
        )
        .unwrap();
        assert!(copy.path().join("model/cat.model3.json").is_file());
        assert!(unpack(
            zip.path(),
            source.path(),
            "missing.json",
            &AtomicBool::new(false)
        )
        .is_err());
    }
}

struct CancelRead<'a, R>(&'a mut R, &'a AtomicBool);
impl<R: Read> Read for CancelRead<'_, R> {
    fn read(&mut self, buffer: &mut [u8]) -> io::Result<usize> {
        if self.1.load(Ordering::Relaxed) {
            return Err(io::ErrorKind::ConnectionAborted.into());
        }
        self.0.read(buffer)
    }
}
