use super::{
    archive,
    wire::{self, Channel},
    Model, Options,
};
use std::{
    io::{self, Read, Write},
    path::Path,
    sync::{atomic::AtomicBool, Arc},
};
use tempfile::{NamedTempFile, TempDir};
pub(super) struct Asset {
    pub model: Model,
    pub hash: String,
    packed: NamedTempFile,
    _temporary: Option<TempDir>,
}
#[derive(Clone)]
pub(super) struct Manifest {
    pub hash: String,
    pub identity: String,
    pub setting: String,
}
impl Manifest {
    pub fn json(&self) -> serde_json::Value {
        serde_json::json!({"hash":self.hash,"identity":self.identity,"setting":self.setting})
    }
    pub fn parse(v: &serde_json::Value) -> io::Result<Self> {
        let hash = v["hash"]
            .as_str()
            .filter(|s| s.len() == 64 && s.bytes().all(|c| c.is_ascii_hexdigit()))
            .ok_or_else(archive::invalid)?;
        let identity = v["identity"]
            .as_str()
            .filter(|s| s.is_empty() || s.len() == 64 && s.bytes().all(|c| c.is_ascii_hexdigit()))
            .ok_or_else(archive::invalid)?;
        let setting = v["setting"]
            .as_str()
            .filter(|s| archive::resource(s))
            .ok_or_else(archive::invalid)?;
        Ok(Self {
            hash: hash.into(),
            identity: identity.into(),
            setting: setting.into(),
        })
    }
}
impl Asset {
    pub fn manifest(&self) -> Manifest {
        Manifest {
            hash: self.hash.clone(),
            identity: self.model.digest.clone(),
            setting: self.model.setting.clone(),
        }
    }
    pub fn pack(model: &Model, cache: &Path, cancel: &AtomicBool) -> io::Result<Arc<Self>> {
        let (packed, hash) = archive::pack(
            Path::new(&model.directory),
            Path::new(&model.adapter),
            cache,
            cancel,
        )?;
        Ok(Arc::new(Self {
            model: model.clone(),
            hash,
            packed,
            _temporary: None,
        }))
    }
    pub fn send(&self, s: &mut Channel, c: &AtomicBool) -> io::Result<()> {
        let mut file = std::fs::File::open(self.packed.path())?;
        let mut bytes = vec![0; wire::MAX_PLAINTEXT];
        loop {
            let n = file.read(&mut bytes)?;
            wire::send(s, &bytes[..n], c)?;
            if n == 0 {
                return Ok(());
            }
        }
    }
    pub fn receive(
        s: &mut Channel,
        m: &Manifest,
        cache: &Path,
        c: &AtomicBool,
    ) -> io::Result<Arc<Self>> {
        let mut packed = NamedTempFile::new_in(cache)?;
        let mut total = 0;
        loop {
            let bytes = wire::receive(s, wire::MAX_PLAINTEXT, c)?;
            if bytes.is_empty() {
                break;
            }
            total += bytes.len() as u64;
            if total > archive::MAX_ZIP {
                return Err(archive::invalid());
            }
            packed.write_all(&bytes)?;
        }
        packed.flush()?;
        if archive::digest(packed.path())? != m.hash {
            return Err(archive::invalid());
        }
        let directory = archive::unpack(packed.path(), cache, &format!("model/{}", m.setting), c)?;
        let model = Model {
            directory: directory
                .path()
                .join("model")
                .to_string_lossy()
                .into_owned(),
            adapter: directory
                .path()
                .join("adapter")
                .to_string_lossy()
                .into_owned(),
            setting: m.setting.clone(),
            digest: m.identity.clone(),
        };
        Ok(Arc::new(Self {
            model,
            hash: m.hash.clone(),
            packed,
            _temporary: Some(directory),
        }))
    }
}
pub(super) fn installed(
    o: &Options,
    m: &Manifest,
    local: &Arc<Asset>,
    c: &AtomicBool,
) -> io::Result<Option<Arc<Asset>>> {
    if local.model.setting == m.setting
        && (local.hash == m.hash || !m.identity.is_empty() && local.model.digest == m.identity)
    {
        return Ok(Some(local.clone()));
    }
    for model in &o.models {
        super::network::stopped(c)?;
        if model.setting != m.setting {
            continue;
        }
        if !m.identity.is_empty() && model.digest == m.identity {
            return Asset::pack(model, &o.cache, c).map(Some);
        }
        if model.digest.is_empty() {
            if let Ok(asset) = Asset::pack(model, &o.cache, c) {
                if asset.hash == m.hash {
                    return Ok(Some(asset));
                }
            }
        }
    }
    Ok(None)
}
