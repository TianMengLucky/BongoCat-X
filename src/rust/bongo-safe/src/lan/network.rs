//! A room host relays encrypted model/pose streams; clients need one outbound socket.
use super::{
    archive,
    assets::{self, Asset, Manifest},
    wire::{self, Channel},
    Model, Peer, Pose, Shared, MAX_MEMBERS,
};
use std::{
    io,
    net::{IpAddr, SocketAddr, TcpListener, TcpStream},
    path::PathBuf,
    sync::{
        atomic::{AtomicBool, Ordering},
        Arc, Mutex,
    },
    time::Duration,
};
pub struct Options {
    pub host: bool,
    pub address: SocketAddr,
    pub code: String,
    pub models: Vec<Model>,
    pub selected: usize,
    pub cache: PathBuf,
}
pub(super) fn stopped(c: &AtomicBool) -> io::Result<()> {
    if c.load(Ordering::Relaxed) {
        Err(io::ErrorKind::Interrupted.into())
    } else {
        Ok(())
    }
}
fn allowed(ip: IpAddr) -> bool {
    match ip {
        IpAddr::V4(v) => v.is_private() || v.is_link_local() || v.is_loopback(),
        IpAddr::V6(v) => {
            !v.is_unspecified()
                && !v.is_multicast()
                && v.to_ipv4_mapped().map_or(true, |v| allowed(IpAddr::V4(v)))
        }
    }
}
fn channel(stream: TcpStream, host: bool, code: &str, c: &AtomicBool) -> io::Result<Channel> {
    stream.set_nodelay(true)?;
    stream.set_read_timeout(Some(Duration::from_millis(100)))?;
    stream.set_write_timeout(Some(Duration::from_millis(100)))?;
    let mut s = Channel::new(stream, host);
    s.authenticate(code, c)?;
    Ok(s)
}
fn json(s: &mut Channel, v: &serde_json::Value, c: &AtomicBool) -> io::Result<()> {
    wire::send(s, v.to_string().as_bytes(), c)
}
fn read_json(s: &mut Channel, c: &AtomicBool) -> io::Result<serde_json::Value> {
    Ok(serde_json::from_slice(&wire::receive(
        s,
        wire::MAX_PLAINTEXT,
        c,
    )?)?)
}
fn peer_json(p: &Peer) -> serde_json::Value {
    serde_json::json!({"id":p.id,"model":p.asset.manifest().json(),"pose":p.pose.encode()})
}
fn serve(
    stream: TcpStream,
    id: u32,
    o: &Options,
    local: &Arc<Asset>,
    shared: &Mutex<Shared>,
    c: &AtomicBool,
) -> io::Result<()> {
    let mut s = channel(stream, true, &o.code, c)?;
    let offer = read_json(&mut s, c)?;
    if offer["version"] != 3 {
        return Err(archive::invalid());
    }
    let manifest = Manifest::parse(&offer["model"])?;
    let installed = assets::installed(o, &manifest, local, c)?;
    wire::send(&mut s, &[u8::from(installed.is_none())], c)?;
    let asset = match installed {
        Some(a) => a,
        None => Asset::receive(&mut s, &manifest, &o.cache, c)?,
    };
    shared
        .lock()
        .map_err(|_| archive::invalid())?
        .upsert(Peer::new(id, asset, Pose::default()));
    loop {
        stopped(c)?;
        let pose = Pose::decode(&wire::receive(&mut s, 80, c)?).ok_or_else(archive::invalid)?;
        let roster = {
            let mut state = shared.lock().map_err(|_| archive::invalid())?;
            if let Some(p) = state.peers.iter_mut().find(|p| p.id == id) {
                p.pose = pose;
            }
            let mut roster = vec![Peer::new(1, local.clone(), state.local)];
            roster.extend(state.peers.iter().filter(|p| p.id != id).cloned());
            roster
        };
        json(
            &mut s,
            &serde_json::json!({"version":3,"members":roster.iter().map(peer_json).collect::<Vec<_>>()}),
            c,
        )?;
        let wanted: Vec<u32> = serde_json::from_value(read_json(&mut s, c)?)?;
        if wanted.len() > MAX_MEMBERS
            || wanted
                .iter()
                .collect::<std::collections::HashSet<_>>()
                .len()
                != wanted.len()
        {
            return Err(archive::invalid());
        }
        for id in wanted {
            roster
                .iter()
                .find(|p| p.id == id)
                .ok_or_else(archive::invalid)?
                .asset
                .send(&mut s, c)?;
        }
    }
}
fn host(
    o: Arc<Options>,
    local: Arc<Asset>,
    shared: Arc<Mutex<Shared>>,
    cancel: Arc<AtomicBool>,
) -> io::Result<()> {
    let listener = TcpListener::bind(o.address)?;
    listener.set_nonblocking(true)?;
    let mut workers: Vec<std::thread::JoinHandle<()>> = Vec::new();
    let mut next = 2u32;
    while !cancel.load(Ordering::Relaxed) {
        let mut i = 0;
        while i < workers.len() {
            if workers[i].is_finished() {
                let _ = workers.swap_remove(i).join();
            } else {
                i += 1;
            }
        }
        match listener.accept() {
            Ok((stream, ip))
                if allowed(ip.ip()) && workers.len() < MAX_MEMBERS - 1 && next < u32::MAX =>
            {
                let id = next;
                next = next.saturating_add(1);
                let (o, local, state, c) =
                    (o.clone(), local.clone(), shared.clone(), cancel.clone());
                let w = std::thread::Builder::new()
                    .name(format!("lan-member-{id}"))
                    .spawn(move || {
                        let _ = serve(stream, id, &o, &local, &state, &c);
                        if let Ok(mut s) = state.lock() {
                            s.remove(id);
                        }
                    });
                match w {
                    Ok(w) => workers.push(w),
                    Err(e) => {
                        cancel.store(true, Ordering::Relaxed);
                        for w in workers {
                            let _ = w.join();
                        }
                        return Err(e);
                    }
                }
            }
            Ok(_) => {}
            Err(e) if e.kind() == io::ErrorKind::WouldBlock => {
                std::thread::sleep(Duration::from_millis(25))
            }
            Err(e) => {
                cancel.store(true, Ordering::Relaxed);
                for w in workers {
                    let _ = w.join();
                }
                return Err(e);
            }
        }
    }
    for w in workers {
        let _ = w.join();
    }
    Ok(())
}
fn client(
    o: &Options,
    local: Arc<Asset>,
    shared: &Mutex<Shared>,
    c: &AtomicBool,
) -> io::Result<()> {
    if !allowed(o.address.ip()) {
        return Err(archive::invalid());
    }
    let mut s = channel(
        TcpStream::connect_timeout(&o.address, Duration::from_secs(2))?,
        false,
        &o.code,
        c,
    )?;
    json(
        &mut s,
        &serde_json::json!({"version":3,"model":local.manifest().json()}),
        c,
    )?;
    match wire::receive(&mut s, 1, c)?.as_slice() {
        [0] => {}
        [1] => local.send(&mut s, c)?,
        _ => return Err(archive::invalid()),
    }
    loop {
        stopped(c)?;
        let pose = shared.lock().map_err(|_| archive::invalid())?.local;
        wire::send(&mut s, &pose.encode(), c)?;
        let message = read_json(&mut s, c)?;
        if message["version"] != 3 {
            return Err(archive::invalid());
        }
        let members = message["members"]
            .as_array()
            .filter(|a| !a.is_empty() && a.len() < MAX_MEMBERS)
            .ok_or_else(archive::invalid)?;
        let existing = shared.lock().map_err(|_| archive::invalid())?.peers.clone();
        let mut next = Vec::new();
        let mut needed = Vec::new();
        let mut ids = std::collections::HashSet::new();
        for entry in members {
            let id = entry["id"]
                .as_u64()
                .filter(|id| *id > 0 && *id <= u32::MAX as u64)
                .ok_or_else(archive::invalid)? as u32;
            if !ids.insert(id) {
                return Err(archive::invalid());
            }
            let manifest = Manifest::parse(&entry["model"])?;
            let bytes: Vec<u8> = serde_json::from_value(entry["pose"].clone())?;
            let pose = Pose::decode(&bytes).ok_or_else(archive::invalid)?;
            let found = existing
                .iter()
                .find(|p| p.id == id && p.wire_hash == manifest.hash)
                .map(|p| p.asset.clone());
            let found = match found {
                Some(a) => Some(a),
                None => assets::installed(o, &manifest, &local, c)?,
            };
            if found.is_none() {
                needed.push(id);
            }
            next.push((id, manifest, pose, found));
        }
        json(&mut s, &serde_json::json!(needed), c)?;
        let mut peers = Vec::new();
        for (id, manifest, pose, asset) in next {
            let asset = match asset {
                Some(a) => a,
                None => Asset::receive(&mut s, &manifest, &o.cache, c)?,
            };
            let mut peer = Peer::new(id, asset, pose);
            peer.wire_hash = manifest.hash;
            peers.push(peer);
        }
        shared
            .lock()
            .map_err(|_| archive::invalid())?
            .replace(peers);
        std::thread::sleep(Duration::from_millis(33));
    }
}
pub fn run(o: Options, shared: &Arc<Mutex<Shared>>, cancel: &Arc<AtomicBool>) -> io::Result<()> {
    if o.selected >= o.models.len() || !(8..=64).contains(&o.code.len()) {
        return Err(archive::invalid());
    }
    let local = Asset::pack(&o.models[o.selected], &o.cache, cancel)?;
    let result = if o.host {
        host(Arc::new(o), local, shared.clone(), cancel.clone())
    } else {
        client(&o, local, shared, cancel)
    };
    if let Ok(mut s) = shared.lock() {
        s.disconnect();
    }
    result
}
#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn addresses() {
        assert!(allowed("fd12::1".parse().unwrap()));
        assert!(!allowed("ff02::1".parse().unwrap()));
        assert!(!allowed("8.8.8.8".parse().unwrap()));
    }
}

#[cfg(test)]
#[path = "room_tests.rs"]
mod room_tests;
