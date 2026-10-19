use super::*;
use std::{fs, thread, time::Instant};

struct Room {
    states: Vec<Arc<Mutex<Shared>>>,
    cancellations: Vec<Arc<AtomicBool>>,
    workers: Vec<thread::JoinHandle<()>>,
    roots: Vec<tempfile::TempDir>,
}
impl Drop for Room {
    fn drop(&mut self) {
        for c in &self.cancellations {
            c.store(true, Ordering::Relaxed);
        }
        for w in self.workers.drain(..) {
            let _ = w.join();
        }
    }
}
impl Room {
    fn start(&mut self, host: bool, address: SocketAddr, number: usize) {
        let root = tempfile::tempdir().unwrap();
        let model = root.path().join("model");
        let cache = root.path().join("cache");
        fs::create_dir_all(&model).unwrap();
        fs::create_dir_all(&cache).unwrap();
        fs::write(model.join("cat.json"), format!("{{\"member\":{number}}}")).unwrap();
        let o = Options {
            host,
            address,
            code: "integration-room-secret".into(),
            selected: 0,
            cache,
            models: vec![Model {
                directory: model.to_string_lossy().into_owned(),
                adapter: String::new(),
                setting: "cat.json".into(),
                digest: String::new(),
            }],
        };
        let state = Arc::new(Mutex::new(Shared::default()));
        state.lock().unwrap().local.values[0] = number as f32;
        let cancel = Arc::new(AtomicBool::new(false));
        let (s, c) = (state.clone(), cancel.clone());
        self.workers.push(thread::spawn(move || {
            let _ = run(o, &s, &c);
        }));
        self.states.push(state);
        self.cancellations.push(cancel);
        self.roots.push(root);
    }
    fn wait(&self, condition: impl Fn() -> bool) {
        let until = Instant::now() + Duration::from_secs(8);
        while !condition() {
            assert!(Instant::now() < until, "room did not converge");
            thread::sleep(Duration::from_millis(20));
        }
    }
}
#[test]
fn three_members_exchange_models_poses_and_leave_independently() {
    let listener = TcpListener::bind("127.0.0.1:0").unwrap();
    let address = listener.local_addr().unwrap();
    drop(listener);
    let mut room = Room {
        states: vec![],
        cancellations: vec![],
        workers: vec![],
        roots: vec![],
    };
    room.start(true, address, 1);
    // A probe waits for listener readiness; the cancelled test joins its worker too.
    room.wait(|| TcpStream::connect_timeout(&address, Duration::from_millis(20)).is_ok());
    room.start(false, address, 2);
    room.start(false, address, 3);
    room.wait(|| {
        room.states
            .iter()
            .all(|s| s.lock().unwrap().peers.len() == 2)
    });
    room.wait(|| {
        room.states.iter().enumerate().all(|(index, s)| {
            let state = s.lock().unwrap();
            let mut values = state
                .peers
                .iter()
                .map(|p| p.pose.values[0] as usize)
                .collect::<Vec<_>>();
            values.sort();
            values == (1..=3).filter(|n| *n != index + 1).collect::<Vec<_>>()
        })
    });
    for (index, state) in room.states.iter().enumerate() {
        let state = state.lock().unwrap();
        for peer in &state.peers {
            let path = PathBuf::from(&peer.model.directory).join("cat.json");
            let json: serde_json::Value = serde_json::from_slice(&fs::read(path).unwrap()).unwrap();
            assert_ne!(json["member"].as_u64().unwrap() as usize, index + 1);
            assert!(PathBuf::from(&peer.model.directory)
                .starts_with(room.roots[index].path().join("cache")));
        }
    }
    let retired_path = {
        let s = room.states[0].lock().unwrap();
        PathBuf::from(
            &s.peers
                .iter()
                .find(|p| p.pose.values[0] == 2.)
                .unwrap()
                .model
                .directory,
        )
    };
    room.cancellations[1].store(true, Ordering::Relaxed);
    room.wait(|| {
        room.states[0].lock().unwrap().peers.len() == 1
            && room.states[2].lock().unwrap().peers.len() == 1
    });
    assert!(
        retired_path.is_dir(),
        "renderer must retain session files until acknowledgement"
    );
    let retired = {
        let mut s = room.states[0].lock().unwrap();
        let epoch = s.generation;
        s.acknowledge(epoch)
    };
    drop(retired);
    // A roster already in flight may still own the departing member for one tick.
    room.wait(|| !retired_path.exists());
    assert_eq!(room.states[2].lock().unwrap().peers[0].id, 1);
}

#[test]
fn wrong_room_code_fails_before_model_exchange() {
    let listener = TcpListener::bind("127.0.0.1:0").unwrap();
    let address = listener.local_addr().unwrap();
    let worker = thread::spawn(move || {
        let (stream, _) = listener.accept().unwrap();
        channel(stream, true, "correct-room-secret", &AtomicBool::new(false)).is_err()
    });
    let stream = TcpStream::connect(address).unwrap();
    assert!(channel(
        stream,
        false,
        "incorrect-room-secret",
        &AtomicBool::new(false)
    )
    .is_err());
    assert!(worker.join().unwrap());
}
