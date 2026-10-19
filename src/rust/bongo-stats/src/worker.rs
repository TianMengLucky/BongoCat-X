use crate::{metrics::Metrics, storage, usage, usage_storage, Command};
use chrono::{Local, Timelike};
use std::ffi::c_void;
use std::path::PathBuf;
use std::sync::{
    atomic::{AtomicBool, AtomicU32, Ordering},
    mpsc, Arc, Mutex,
};
use std::time::{Duration, Instant};

pub type MetricsPoll = unsafe extern "C" fn(*mut c_void, *mut Metrics);
pub type ApplicationPoll = unsafe extern "C" fn(*mut c_void, *mut usage::Sample);
pub struct Source {
    pub context: *mut c_void,
    pub metrics: MetricsPoll,
    pub application: ApplicationPoll,
}
// The host promises a worker-safe context that outlives the joined worker.
unsafe impl Send for Source {}
pub struct Cache {
    pub metrics: crate::metrics::Snapshot,
    pub usage: [usage::Snapshot; 4],
    pub keyboard: [crate::keyboard::Counts; 4],
    pub words: [crate::words::Snapshot; 4],
    pub backspace: u32,
    pub timer: crate::words_timer::Snapshot,
}
impl Default for Cache {
    fn default() -> Self {
        Self {
            metrics: crate::metrics::Store::default().snapshot(Local::now().date_naive(), false),
            usage: std::array::from_fn(|_| usage::Snapshot::default()),
            keyboard: [[0; 256]; 4],
            words: [crate::words::Snapshot::default(); 4],
            backspace: 0,
            timer: crate::words_timer::Snapshot::default(),
        }
    }
}
pub fn run(
    directory: PathBuf,
    receiver: mpsc::Receiver<Command>,
    cache: Arc<Mutex<Cache>>,
    failed: Arc<AtomicBool>,
    export_status: Arc<AtomicU32>,
    reset_status: Arc<AtomicU32>,
) -> bool {
    #[cfg(all(windows, not(test)))]
    let mut collector = crate::words_windows::Collector::new().ok();
    #[cfg(all(windows, test))]
    let mut collector: Option<crate::words_windows::Collector> = None;
    let mut opened = None;
    let mut source: Option<Source> = None;
    let mut keyboard: Option<crate::keyboard::Source> = None;
    let mut pending_keys = [0u64; 256];
    let start = Instant::now();
    let mut last_sample = Instant::now() - Duration::from_secs(1);
    let mut last_save = Instant::now();
    let mut last_open = Instant::now() - Duration::from_secs(5);
    let mut pending_metrics = Metrics::default();
    let mut desired_enabled = None;
    let mut desired_backspace = None;
    loop {
        let command =
            receiver.recv_timeout(Duration::from_millis(if cfg!(windows) { 100 } else { 200 }));
        let stop = matches!(
            &command,
            Ok(Command::Stop) | Err(mpsc::RecvTimeoutError::Disconnected)
        );
        #[cfg(windows)]
        if stop {
            if let Some(collector) = &mut collector {
                collector.stop();
            }
        }
        let now = Local::now();
        if let Ok(Command::Enable(enabled)) = &command {
            desired_enabled = Some(*enabled);
        }
        if let Ok(Command::Backspace(enabled)) = &command {
            desired_backspace = Some(*enabled);
        }
        if opened.is_none() && last_open.elapsed() >= Duration::from_secs(5) {
            last_open = Instant::now();
            match storage::open(&directory) {
                Ok((connection, store)) => opened = Some((connection, Arc::new(Mutex::new(store)))),
                Err(_) => failed.store(true, Ordering::Relaxed),
            }
        }
        if let Ok(Command::Configure(value)) = &command {
            source = Some(Source {
                context: value.context,
                metrics: value.metrics,
                application: value.application,
            });
        }
        if let Ok(Command::Keyboard(value)) = &command {
            keyboard = Some(crate::keyboard::Source {
                context: value.context,
                poll: value.poll,
            });
        }
        let mut counts = [0; 256];
        if let Some(source) = &keyboard {
            unsafe {
                (source.poll)(source.context, counts.as_mut_ptr());
            }
            for (value, count) in pending_keys.iter_mut().zip(counts) {
                *value = value.saturating_add(count).min(i64::MAX as u64);
            }
        }
        let mut metrics = Metrics::default();
        if let Some(source) = &source {
            unsafe {
                (source.metrics)(source.context, &mut metrics);
            }
            if metrics.valid() {
                pending_metrics.add(metrics);
            }
        }
        if let Ok(Command::Ingest(metrics)) = &command {
            if metrics.valid() {
                pending_metrics.add(*metrics);
            }
        }
        if let Some((_, shared)) = &opened {
            if let Some(enabled) = desired_backspace.take() {
                let mut store = shared.lock().unwrap_or_else(|e| e.into_inner());
                store.words.backspace = enabled;
                store.words.options_dirty = true;
                store.revision = store.revision.wrapping_add(1);
            }
        }
        #[cfg(windows)]
        if let Some(collector) = &mut collector {
            let enabled = opened.as_ref().is_some_and(|(_, shared)| {
                shared
                    .lock()
                    .unwrap_or_else(|e| e.into_inner())
                    .words
                    .backspace
            });
            collector.set_backspace(enabled);
            collector.poll(&counts, metrics.buttons.iter().any(|&count| count != 0));
        }
        if let Some((connection, shared)) = &mut opened {
            let resetting = matches!(&command, Ok(Command::Reset));
            let sampling = !resetting && last_sample.elapsed() >= Duration::from_secs(1);
            {
                let mut store = shared.lock().unwrap_or_else(|e| e.into_inner());
                #[cfg(windows)]
                if let Some(collector) = &mut collector {
                    while let Some((batch, subtract)) = collector.next_batch() {
                        if subtract {
                            store.words.subtract(batch);
                        } else {
                            store.words.ingest(batch);
                        }
                        store.revision = store.revision.wrapping_add(1);
                    }
                }
                if let Ok(Command::Timer(enabled)) = &command {
                    store.words.timer.set_enabled(*enabled, Instant::now());
                }
                store.words.timer.tick(Instant::now());
                store.words.prune(now.date_naive(), Instant::now());
                store.ingest(now.date_naive(), now.hour(), pending_metrics);
                pending_metrics = Metrics::default();
                store.keyboard.ingest(now.date_naive(), pending_keys);
                pending_keys = [0; 256];
                if let Some(enabled) = desired_enabled.take() {
                    store.usage.set_enabled(enabled);
                    store.revision = store.revision.wrapping_add(1);
                }
                if sampling && !stop {
                    let mut sample = usage::Sample::default();
                    if store.usage.enabled {
                        if let Some(source) = &source {
                            unsafe {
                                (source.application)(source.context, &mut sample);
                            }
                        }
                    }
                    store.usage.sample(
                        start.elapsed().as_millis().min(u64::MAX as u128) as u64,
                        Local::now().timestamp_millis(),
                        sample,
                    );
                    store.revision = store.revision.wrapping_add(1);
                    last_sample = Instant::now();
                }
            }
            let reset_ok = resetting && storage::reset(connection, shared).is_ok();
            if resetting {
                #[cfg(windows)]
                if let Some(collector) = &mut collector {
                    collector.reset_tracking();
                }
                last_sample = Instant::now();
            }
            let should_save = stop
                || last_save.elapsed() >= Duration::from_secs(30)
                || matches!(
                    &command,
                    Ok(Command::Flush(_))
                        | Ok(Command::Export(_, _))
                        | Ok(Command::Enable(_))
                        | Ok(Command::Backspace(_))
                );
            let mut save_ok = true;
            if should_save {
                let ok = storage::save(connection, shared).is_ok();
                save_ok = ok;
                failed.store(!ok, Ordering::Relaxed);
                last_save = Instant::now();
            }
            if let Ok(Command::Export(target, applications)) = &command {
                let ok = if *applications && !failed.load(Ordering::Relaxed) {
                    usage_storage::export(connection, target).is_ok()
                } else if !*applications {
                    storage::export(shared, target).is_ok()
                } else {
                    false
                };
                export_status.store(if ok { 2 } else { 3 }, Ordering::Release);
            }
            if sampling || should_save || resetting || matches!(&command, Ok(Command::Timer(_))) {
                let store = shared.lock().unwrap_or_else(|e| e.into_inner());
                let updated = Cache {
                    metrics: store.snapshot(now.date_naive(), failed.load(Ordering::Relaxed)),
                    backspace: store.words.backspace as u32,
                    timer: store.words.timer.snapshot(),
                    words: std::array::from_fn(|period| {
                        #[cfg(windows)]
                        let status = collector
                            .as_ref()
                            .map(|c| c.status.load(Ordering::Relaxed))
                            .unwrap_or(2);
                        #[cfg(not(windows))]
                        let status = 0;
                        store.words.snapshot(now.date_naive(), period, status)
                    }),
                    keyboard: std::array::from_fn(|period| {
                        store.keyboard.snapshot(now.date_naive(), period)
                    }),
                    usage: std::array::from_fn(|period| {
                        store.usage.snapshot(now.date_naive(), period as u32)
                    }),
                };
                if resetting || matches!(&command, Ok(Command::Flush(_))) {
                    *cache.lock().unwrap_or_else(|e| e.into_inner()) = updated;
                    if resetting {
                        reset_status.store(if reset_ok { 2 } else { 3 }, Ordering::Release);
                    }
                } else if let Ok(mut snapshot) = cache.try_lock() {
                    *snapshot = updated;
                }
            }
            if let Ok(Command::Flush(reply)) = &command {
                let _ = reply.send(save_ok);
            }
        } else {
            if let Ok(mut snapshot) = cache.try_lock() {
                snapshot.metrics.storage_error = 1;
            }
            if let Ok(Command::Flush(reply)) = &command {
                let _ = reply.send(false);
            }
            if matches!(&command, Ok(Command::Reset)) {
                reset_status.store(3, Ordering::Release);
            }
            if matches!(&command, Ok(Command::Export(_, _))) {
                export_status.store(3, Ordering::Release);
            }
        }
        if stop {
            return opened.is_some() && !failed.load(Ordering::Relaxed);
        }
    }
}
