//! Bounded untrusted LAN protocol and session model archives.
pub mod addresses;
mod archive;
mod assets;
mod network;
mod wire;
pub use network::{run, Options};
#[derive(Clone)]
pub struct Model {
    pub directory: String,
    pub adapter: String,
    pub setting: String,
    pub digest: String,
}
#[repr(C)]
#[derive(Default, Clone, Copy)]
pub struct Pose {
    pub mask: u32,
    pub values: [f32; 16],
    pub pointer_x: f32,
    pub pointer_y: f32,
    pub buttons: u32,
}
impl Pose {
    pub fn encode(&self) -> Vec<u8> {
        let mut out = self.mask.to_be_bytes().to_vec();
        for v in self.values {
            out.extend_from_slice(&v.to_be_bytes());
        }
        out.extend_from_slice(&self.pointer_x.to_be_bytes());
        out.extend_from_slice(&self.pointer_y.to_be_bytes());
        out.extend_from_slice(&self.buttons.to_be_bytes());
        out
    }
    pub fn decode(data: &[u8]) -> Option<Self> {
        if data.len() != 80 {
            return None;
        }
        let mut pose = Self {
            mask: u32::from_be_bytes(data[..4].try_into().ok()?),
            ..Self::default()
        };
        if pose.mask >> 16 != 0 {
            return None;
        }
        for (v, bytes) in pose.values.iter_mut().zip(data[4..68].chunks_exact(4)) {
            *v = f32::from_be_bytes(bytes.try_into().ok()?);
            if !v.is_finite() || v.abs() > 10000.0 {
                return None;
            }
        }
        pose.pointer_x = f32::from_be_bytes(data[68..72].try_into().ok()?);
        pose.pointer_y = f32::from_be_bytes(data[72..76].try_into().ok()?);
        pose.buttons = u32::from_be_bytes(data[76..80].try_into().ok()?);
        if !pose.pointer_x.is_finite()
            || !pose.pointer_y.is_finite()
            || !(0.0..=1.0).contains(&pose.pointer_x)
            || !(0.0..=1.0).contains(&pose.pointer_y)
            || pose.buttons & !7 != 0
        {
            return None;
        }
        Some(pose)
    }
}
pub const MAX_MEMBERS: usize = 8;
#[derive(Clone)]
pub struct Peer {
    pub wire_hash: String,
    pub id: u32,
    pub model: Model,
    pub pose: Pose,
    pub(super) asset: std::sync::Arc<assets::Asset>,
}
impl Peer {
    fn new(id: u32, asset: std::sync::Arc<assets::Asset>, pose: Pose) -> Self {
        Self {
            wire_hash: asset.hash.clone(),
            id,
            model: asset.model.clone(),
            pose,
            asset,
        }
    }
}
#[derive(Default)]
pub struct Shared {
    pub local: Pose,
    pub peers: Vec<Peer>,
    pub status: u32,
    pub generation: u64,
    retired: Vec<(u64, Peer)>,
}
impl Shared {
    pub fn replace(&mut self, peers: Vec<Peer>) {
        let changed = self.peers.len() != peers.len()
            || self
                .peers
                .iter()
                .zip(&peers)
                .any(|(a, b)| a.id != b.id || !std::sync::Arc::ptr_eq(&a.asset, &b.asset));
        if changed {
            self.generation = self.generation.saturating_add(1);
            for old in self.peers.drain(..) {
                if !peers
                    .iter()
                    .any(|p| p.id == old.id && std::sync::Arc::ptr_eq(&p.asset, &old.asset))
                {
                    self.retired.push((self.generation, old));
                }
            }
        }
        self.peers = peers;
        self.status = if self.peers.is_empty() { 1 } else { 2 };
    }
    fn upsert(&mut self, peer: Peer) {
        let mut peers = self.peers.clone();
        peers.retain(|p| p.id != peer.id);
        peers.push(peer);
        self.replace(peers);
    }
    fn remove(&mut self, id: u32) {
        let peers = self.peers.iter().filter(|p| p.id != id).cloned().collect();
        self.replace(peers);
    }
    pub fn disconnect(&mut self) {
        self.replace(Vec::new());
        self.status = 0;
    }
    pub fn acknowledge(&mut self, epoch: u64) -> Vec<Peer> {
        let mut dropped = Vec::new();
        let mut kept = Vec::new();
        for (version, peer) in self.retired.drain(..) {
            if version <= epoch {
                dropped.push(peer);
            } else {
                kept.push((version, peer));
            }
        }
        self.retired = kept;
        dropped
    }
}
