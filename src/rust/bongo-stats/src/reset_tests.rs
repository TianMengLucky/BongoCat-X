use crate::{
    metrics::{Metrics, Store},
    storage,
    usage::App,
};
use chrono::{Local, Timelike};
use std::sync::{Arc, Mutex};
fn directory() -> std::path::PathBuf {
    std::env::temp_dir().join(format!(
        "bongo-reset-{}-{}",
        std::process::id(),
        std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)
            .unwrap()
            .as_nanos()
    ))
}
fn populated(directory: &std::path::Path) -> (rusqlite::Connection, Arc<Mutex<Store>>) {
    let (mut connection, mut store) = storage::open(directory).unwrap();
    let now = Local::now();
    store.ingest(
        now.date_naive(),
        now.hour(),
        Metrics {
            keys: 7,
            buttons: [2, 1, 0, 0, 0],
            movement: 42.0,
            scroll: 3.0,
        },
    );
    let mut key_counts = [0; 256];
    key_counts[65] = 7;
    store.keyboard.ingest(now.date_naive(), key_counts);
    store.words.ingest(crate::words::Batch {
        date: now.date_naive(),
        at: std::time::Instant::now(),
        counts: [('中', 3)].into(),
    });
    store.usage.apps.insert(
        "editor".into(),
        App {
            name: "Editor".into(),
            total_ms: 0,
        },
    );
    store.usage.add(
        crate::metrics::date_id(now.date_naive()),
        now.hour(),
        "editor",
        2500,
    );
    store.words.timer.set_enabled(true, std::time::Instant::now());
    store.usage.set_enabled(false);
    let shared = Arc::new(Mutex::new(store));
    storage::save(&mut connection, &shared).unwrap();
    (connection, shared)
}
#[test]
fn reset_clears_all_history_and_preserves_settings_after_reopen() {
    let directory = directory();
    let (mut connection, shared) = populated(&directory);
    storage::reset(&mut connection, &shared).unwrap();
    let today = Local::now().date_naive();
    {
        let store = shared.lock().unwrap();
        let snapshot = store.snapshot(today, false);
        assert_eq!(snapshot.today, Metrics::default());
        assert_eq!(snapshot.week, Metrics::default());
        assert_eq!(snapshot.month, Metrics::default());
        assert_eq!(snapshot.lifetime, Metrics::default());
        assert!(snapshot.hours.iter().all(|m| *m == Metrics::default()));
        assert!(snapshot
            .days
            .iter()
            .all(|d| d.metrics == Metrics::default()));
        assert_eq!(store.keyboard.snapshot(today, 3), [0; 256]);
        assert_eq!(store.words.snapshot(today, 3, 1).total, 0);
        assert_eq!(store.words.timer.snapshot().enabled, 1);
        assert_eq!(store.words.timer.snapshot().typing_ms, 0);
        assert_eq!(store.words.timer.snapshot().idle_ms, 0);
        assert!(!store.usage.enabled);
        assert_eq!(store.usage.snapshot(today, 3).total_ms, 0);
        assert_eq!(store.usage.snapshot(today, 3).count, 0);
    }
    // A later save must never reinsert the cleared dirty rows.
    storage::save(&mut connection, &shared).unwrap();
    drop(connection);
    let (connection, store) = storage::open(&directory).unwrap();
    assert_eq!(store.lifetime, Metrics::default());
    assert!(store.hours.is_empty());
    assert_eq!(store.keyboard.snapshot(today, 3), [0; 256]);
    assert_eq!(store.words.snapshot(today, 3, 1).total, 0);
    assert!(store.usage.apps.is_empty());
    assert!(store.usage.hours.is_empty());
    assert!(!store.usage.enabled);
    drop(connection);
    std::fs::remove_dir_all(directory).unwrap();
}
#[test]
fn failed_reset_rolls_back_database_and_keeps_memory() {
    let directory = directory();
    let (mut connection, shared) = populated(&directory);
    connection
        .execute_batch(
            "CREATE TRIGGER fail_reset BEFORE DELETE ON applications
        BEGIN SELECT RAISE(ABORT, 'test reset failure'); END;",
        )
        .unwrap();
    assert!(storage::reset(&mut connection, &shared).is_err());
    assert_eq!(shared.lock().unwrap().lifetime.keys, 7);
    assert_eq!(
        shared
            .lock()
            .unwrap()
            .usage
            .snapshot(Local::now().date_naive(), 3)
            .total_ms,
        2500
    );
    drop(connection);
    let (connection, store) = storage::open(&directory).unwrap();
    assert_eq!(store.lifetime.keys, 7);
    assert_eq!(store.words.snapshot(Local::now().date_naive(), 3, 1).total, 3);
    assert_eq!(
        store.usage.snapshot(Local::now().date_naive(), 3).total_ms,
        2500
    );
    drop(connection);
    std::fs::remove_dir_all(directory).unwrap();
}
#[test]
fn worker_reset_orders_batches_and_publishes_completion() {
    let directory = directory();
    let name = std::ffi::CString::new(directory.to_str().unwrap()).unwrap();
    unsafe {
        let instance = crate::create(name.as_ptr());
        assert!(!instance.is_null());
        let metrics = Metrics {
            keys: 7,
            ..Metrics::default()
        };
        crate::ingest(instance, &metrics);
        assert_eq!(crate::flush(instance), 1);
        assert_eq!(crate::reset_ffi::reset(instance), 1);
        crate::ingest(
            instance,
            &Metrics {
                keys: 3,
                ..Metrics::default()
            },
        );
        assert_eq!(crate::flush(instance), 1);
        assert_eq!(crate::reset_ffi::status(instance), 2);
        let mut snapshot = Store::default().snapshot(Local::now().date_naive(), false);
        assert_eq!(crate::snapshot(instance, &mut snapshot), 1);
        assert_eq!(snapshot.lifetime.keys, 3);
        assert_eq!(crate::destroy(instance), 1);
    }
    let (connection, store) = storage::open(&directory).unwrap();
    assert_eq!(store.lifetime.keys, 3);
    drop(connection);
    std::fs::remove_dir_all(directory).unwrap();
}
#[test]
fn reset_extension_rejects_unknown_versions_and_null_instances() {
    assert!(crate::reset_ffi::bongo_cat_statistics_reset_query(0).is_null());
    assert!(crate::reset_ffi::bongo_cat_statistics_reset_query(2).is_null());
    assert!(!crate::reset_ffi::bongo_cat_statistics_reset_query(1).is_null());
    assert_eq!(unsafe { crate::reset_ffi::reset(std::ptr::null_mut()) }, 0);
    assert_eq!(unsafe { crate::reset_ffi::status(std::ptr::null_mut()) }, 3);
}
