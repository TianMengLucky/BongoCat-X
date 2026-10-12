use crate::usage::{copy_text, Sample, Usage};
use chrono::{Local, Timelike};
fn sample(id: &str) -> Sample {
    let mut value = Sample {
        available: 1,
        active: 1,
        ..Sample::default()
    };
    copy_text(&mut value.id, id);
    copy_text(&mut value.name, id);
    value
}
#[test]
fn focus_switch_bills_previous_app_and_idle_breaks_interval() {
    let mut usage = Usage::default();
    let now = Local::now().timestamp_millis();
    usage.sample(1000, now, sample("editor"));
    usage.sample(2000, now + 1000, sample("browser"));
    assert_eq!(usage.apps["editor"].total_ms, 1000);
    let mut idle = sample("browser");
    idle.idle_ms = 300_000;
    usage.sample(3000, now + 2000, idle);
    usage.sample(4000, now + 3000, sample("browser"));
    assert_eq!(usage.apps["browser"].total_ms, 0);
    usage.sample(5000, now + 4000, sample("browser"));
    assert_eq!(usage.apps["browser"].total_ms, 1000);
}
#[test]
fn suspend_clock_change_lock_and_pause_are_not_billed() {
    let mut usage = Usage::default();
    let now = Local::now().timestamp_millis();
    usage.sample(1000, now, sample("editor"));
    usage.sample(100_000, now + 99_000, sample("editor"));
    usage.sample(101_000, now + 199_000, sample("editor"));
    usage.sample(
        102_000,
        now + 200_000,
        Sample {
            available: 1,
            ..Sample::default()
        },
    );
    usage.sample(103_000, now + 201_000, sample("editor"));
    usage.set_enabled(false);
    usage.sample(104_000, now + 202_000, sample("editor"));
    usage.set_enabled(true);
    usage.sample(105_000, now + 203_000, sample("editor"));
    assert_eq!(usage.apps["editor"].total_ms, 0);
    usage.sample(106_000, now + 204_000, sample("editor"));
    assert_eq!(usage.apps["editor"].total_ms, 1000);
}
#[test]
fn hourly_boundary_splits_duration_and_ranks_are_stable() {
    let mut usage = Usage::default();
    let now = Local::now()
        .with_minute(59)
        .unwrap()
        .with_second(59)
        .unwrap()
        .with_nanosecond(500_000_000)
        .unwrap();
    usage.sample(1000, now.timestamp_millis(), sample("editor"));
    usage.sample(2000, now.timestamp_millis() + 1000, sample("editor"));
    assert_eq!(usage.hours.len(), 2);
    assert!(usage.hours.values().all(|&ms| ms == 500));
    let snapshot = usage.snapshot(now.date_naive(), 3);
    assert_eq!(snapshot.total_ms, 1000);
    assert_eq!(snapshot.count, 1);
    assert_eq!(snapshot.apps[0].milliseconds, 1000);
}
#[test]
fn usage_settings_and_totals_survive_database_reopen() {
    let directory = std::env::temp_dir().join(format!(
        "bongo-usage-test-{}-{}",
        std::process::id(),
        std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)
            .unwrap()
            .as_nanos()
    ));
    let (mut connection, mut store) = crate::storage::open(&directory).unwrap();
    let today = Local::now().date_naive();
    store
        .usage
        .sample(0, Local::now().timestamp_millis(), sample("=editor"));
    store
        .usage
        .add(crate::metrics::date_id(today), 0, "=editor", 2500);
    store.usage.set_enabled(false);
    let shared = std::sync::Arc::new(std::sync::Mutex::new(store));
    crate::storage::save(&mut connection, &shared).unwrap();
    let target = directory.join("applications.csv");
    crate::usage_storage::export(&connection, &target).unwrap();
    assert!(std::fs::read_to_string(target)
        .unwrap()
        .contains("\"'=editor\""));
    drop(connection);
    let (connection, store) = crate::storage::open(&directory).unwrap();
    assert!(!store.usage.enabled);
    assert_eq!(store.usage.snapshot(today, 0).total_ms, 2500);
    drop(connection);
    std::fs::remove_dir_all(directory).unwrap();
}
#[test]
fn snapshots_never_wait_for_worker_cache_lock() {
    let instance = crate::Instance {
        cache: std::sync::Arc::new(std::sync::Mutex::new(crate::worker::Cache::default())),
        failed: std::sync::Arc::new(std::sync::atomic::AtomicBool::new(false)),
        export_status: std::sync::Arc::new(std::sync::atomic::AtomicU32::new(0)),
        commands: std::sync::mpsc::sync_channel(1).0,
        worker: None,
    };
    let lock = instance.cache.lock().unwrap();
    let mut output = crate::metrics::Store::default().snapshot(Local::now().date_naive(), false);
    let pointer = (&instance as *const crate::Instance).cast_mut().cast();
    assert_eq!(unsafe { crate::snapshot(pointer, &mut output) }, 0);
    drop(lock);
    assert_eq!(unsafe { crate::snapshot(pointer, &mut output) }, 1);
}

#[test]
fn usage_extension_checks_version_and_c_layout() {
    assert!(crate::usage_ffi::bongo_cat_statistics_usage_query(0).is_null());
    assert!(crate::usage_ffi::bongo_cat_statistics_usage_query(2).is_null());
    assert!(!crate::usage_ffi::bongo_cat_statistics_usage_query(1).is_null());
    #[cfg(target_pointer_width = "64")]
    {
        assert_eq!(std::mem::size_of::<crate::usage::Sample>(), 528);
        assert_eq!(std::mem::size_of::<crate::usage::AppTotal>(), 520);
        assert_eq!(std::mem::size_of::<crate::usage::Snapshot>(), 33504);
        assert_eq!(std::mem::size_of::<crate::metrics::Snapshot>(), 3960);
    }
}
