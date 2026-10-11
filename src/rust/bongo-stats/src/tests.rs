use crate::metrics::{date_id, Metrics, Store};
use chrono::{Days, NaiveDate};

fn day() -> NaiveDate {
    NaiveDate::from_ymd_opt(2026, 10, 11).unwrap()
}
#[test]
fn midnight_rolls_over_without_losing_lifetime() {
    let mut store = Store::default();
    let yesterday = day().checked_sub_days(Days::new(1)).unwrap();
    store.ingest(
        yesterday,
        23,
        Metrics {
            keys: 7,
            buttons: [1, 2, 0, 0, 0],
            ..Metrics::default()
        },
    );
    let snapshot = store.snapshot(day(), false);
    assert_eq!(snapshot.today, Metrics::default());
    assert_eq!(snapshot.week.keys, 7);
    assert_eq!(snapshot.lifetime.keys, 7);
    store.ingest(
        day(),
        0,
        Metrics {
            keys: 3,
            ..Metrics::default()
        },
    );
    let snapshot = store.snapshot(day(), false);
    assert_eq!(snapshot.today.keys, 3);
    assert_eq!(snapshot.hours[0].keys, 3);
    assert_eq!(snapshot.lifetime.keys, 10);
}
#[test]
fn ranges_are_calendar_days_and_ignore_future_rows() {
    let mut store = Store::default();
    for offset in [0, 6, 7, 29, 30] {
        store.ingest(
            day().checked_sub_days(Days::new(offset)).unwrap(),
            12,
            Metrics {
                keys: 1,
                ..Metrics::default()
            },
        );
    }
    store.ingest(
        day().checked_add_days(Days::new(1)).unwrap(),
        12,
        Metrics {
            keys: 5,
            ..Metrics::default()
        },
    );
    let snapshot = store.snapshot(day(), false);
    assert_eq!(snapshot.today.keys, 1);
    assert_eq!(snapshot.week.keys, 2);
    assert_eq!(snapshot.month.keys, 4);
    assert_eq!(snapshot.lifetime.keys, 10);
    assert_eq!(snapshot.days[29].date, date_id(day()));
}
#[test]
fn invalid_batches_do_not_poison_the_database() {
    let mut store = Store::default();
    for movement in [f64::NAN, f64::INFINITY, -1.0] {
        store.ingest(
            day(),
            0,
            Metrics {
                keys: 1,
                movement,
                ..Metrics::default()
            },
        );
    }
    store.ingest(
        day(),
        24,
        Metrics {
            keys: 1,
            ..Metrics::default()
        },
    );
    assert!(store.hours.is_empty());
    assert_eq!(store.lifetime, Metrics::default());
}
#[test]
fn durable_reopen_and_csv_export_preserve_aggregates() {
    let directory = std::env::temp_dir().join(format!(
        "bongo-statistics-test-{}-{}",
        std::process::id(),
        std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)
            .unwrap()
            .as_nanos()
    ));
    std::fs::create_dir_all(&directory).unwrap();
    let (mut connection, mut store) = crate::storage::open(&directory).unwrap();
    let today = chrono::Local::now().date_naive();
    store.ingest(
        today,
        2,
        Metrics {
            keys: 11,
            buttons: [4, 2, 1, 0, 1],
            movement: 12.5,
            scroll: 2.75,
        },
    );
    let shared = std::sync::Arc::new(std::sync::Mutex::new(store));
    connection.execute_batch("PRAGMA query_only=ON").unwrap();
    assert!(crate::storage::save(&mut connection, &shared).is_err());
    assert!(!shared.lock().unwrap().dirty.is_empty());
    connection.execute_batch("PRAGMA query_only=OFF").unwrap();
    crate::storage::save(&mut connection, &shared).unwrap();
    assert!(shared.lock().unwrap().dirty.is_empty());
    drop(connection);
    let (connection, reopened) = crate::storage::open(&directory).unwrap();
    assert_eq!(reopened.snapshot(today, false).today.keys, 11);
    assert_eq!(reopened.lifetime.movement, 12.5);
    let csv = directory.join("export.csv");
    crate::storage::export(&shared, &csv).unwrap();
    let csv = std::fs::read_to_string(csv).unwrap();
    assert!(csv.contains("movement_input_units,scroll_input_units"));
    assert!(csv.contains(",2,11,4,2,1,0,1,12.5000,2.7500"));
    assert!(!csv.contains("coordinates"));
    drop(connection);
    std::fs::remove_dir_all(directory).unwrap();
}
#[test]
fn old_history_is_bounded_but_lifetime_survives() {
    let mut store = Store::default();
    store.ingest(
        day().checked_sub_days(Days::new(730)).unwrap(),
        0,
        Metrics {
            keys: 9,
            ..Metrics::default()
        },
    );
    store.ingest(
        day(),
        0,
        Metrics {
            keys: 1,
            ..Metrics::default()
        },
    );
    assert_eq!(store.hours.len(), 1);
    assert_eq!(store.snapshot(day(), false).lifetime.keys, 10);
}
#[test]
fn query_rejects_incompatible_hosts() {
    assert!(crate::bongo_cat_statistics_plugin_query(0).is_null());
    assert!(crate::bongo_cat_statistics_plugin_query(2).is_null());
    assert!(!crate::bongo_cat_statistics_plugin_query(1).is_null());
}

#[test]
fn sqlite_counters_validate_signed_integer_limits() {
    let connection = rusqlite::Connection::open_in_memory().unwrap();
    for value in [0, 1, i64::MAX as u64] {
        let stored = crate::storage::integer(value).unwrap();
        let restored = connection
            .query_row("SELECT ?1", [stored], |row| crate::storage::count(row, 0))
            .unwrap();
        assert_eq!(restored, value);
    }
    assert!(crate::storage::integer(u64::MAX).is_err());
    assert!(connection
        .query_row("SELECT -1", [], |row| crate::storage::count(row, 0))
        .is_err());
}
