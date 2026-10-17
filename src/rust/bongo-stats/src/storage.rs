use crate::metrics::{date_id, Metrics, Store};
use chrono::{Days, Local};
use rusqlite::{params, Connection};
use std::path::Path;
use std::sync::{Arc, Mutex};
use std::time::Duration;

const COLUMNS: &str = "day,hour,keys,left_clicks,right_clicks,middle_clicks,back_clicks,forward_clicks,movement,scroll";

pub fn count(row: &rusqlite::Row<'_>, column: usize) -> rusqlite::Result<u64> {
    let value = row.get::<_, i64>(column)?;
    u64::try_from(value).map_err(|_| rusqlite::Error::IntegralValueOutOfRange(column, value))
}

pub fn integer(value: u64) -> rusqlite::Result<i64> {
    i64::try_from(value).map_err(|error| rusqlite::Error::ToSqlConversionFailure(Box::new(error)))
}

pub fn open(directory: &Path) -> Result<(Connection, Store), Box<dyn std::error::Error>> {
    std::fs::create_dir_all(directory)?;
    let connection = Connection::open(directory.join("statistics.sqlite3"))?;
    connection.busy_timeout(Duration::from_secs(2))?;
    connection.execute_batch(
        "PRAGMA journal_mode=WAL; PRAGMA synchronous=FULL;
         CREATE TABLE IF NOT EXISTS metrics (
         day INTEGER NOT NULL, hour INTEGER NOT NULL, keys INTEGER NOT NULL,
         left_clicks INTEGER NOT NULL, right_clicks INTEGER NOT NULL,
         middle_clicks INTEGER NOT NULL, back_clicks INTEGER NOT NULL,
         forward_clicks INTEGER NOT NULL, movement REAL NOT NULL, scroll REAL NOT NULL,
         PRIMARY KEY(day,hour));",
    )?;
    let mut store = Store::default();
    let today = Local::now().date_naive();
    let oldest = date_id(today.checked_sub_days(Days::new(729)).unwrap_or(today));
    store.oldest_day = oldest;
    let mut statement = connection.prepare(&format!(
        "SELECT {COLUMNS} FROM metrics WHERE day=0 OR day>=?1 LIMIT 17522"
    ))?;
    let rows = statement.query_map([oldest], |row| {
        Ok((
            row.get::<_, u32>(0)?,
            row.get::<_, u32>(1)?,
            Metrics {
                keys: count(row, 2)?,
                buttons: [
                    count(row, 3)?,
                    count(row, 4)?,
                    count(row, 5)?,
                    count(row, 6)?,
                    count(row, 7)?,
                ],
                movement: row.get(8)?,
                scroll: row.get(9)?,
            },
        ))
    })?;
    for (index, row) in rows.enumerate() {
        let (day, hour, metrics) = row?;
        if index >= 17521 || !metrics.valid() || hour >= 24 {
            return Err("Invalid statistics database".into());
        }
        if day == 0 && hour == 0 {
            store.lifetime = metrics;
        } else {
            chrono::NaiveDate::from_ymd_opt((day / 10000) as i32, day / 100 % 100, day % 100)
                .ok_or("Invalid statistics date")?;
            store.hours.insert((day, hour), metrics);
        }
    }
    drop(statement);
    crate::usage_storage::load(&connection, &mut store.usage)?;
    crate::keyboard::load(&connection, &mut store.keyboard, oldest)?;
    Ok((connection, store))
}

pub fn save(connection: &mut Connection, shared: &Arc<Mutex<Store>>) -> rusqlite::Result<()> {
    let (revision, rows, lifetime, usage, keyboard) = {
        let store = shared.lock().unwrap_or_else(|e| e.into_inner());
        if store.dirty.is_empty()
            && store.usage.dirty.is_empty()
            && store.usage.changed.is_empty()
            && !store.usage.options_dirty
            && store.keyboard.dirty.is_empty()
        {
            return Ok(());
        }
        let rows: Vec<_> = store
            .dirty
            .iter()
            .filter_map(|id| store.hours.get(id).map(|metrics| (*id, *metrics)))
            .collect();
        (
            store.revision,
            rows,
            store.lifetime,
            crate::usage_storage::pending(&store.usage),
            store.keyboard.pending(),
        )
    };
    let transaction = connection.transaction()?;
    {
        let mut statement = transaction.prepare(&format!(
            "INSERT OR REPLACE INTO metrics ({COLUMNS}) VALUES (?1,?2,?3,?4,?5,?6,?7,?8,?9,?10)"
        ))?;
        for ((day, hour), metrics) in rows
            .iter()
            .copied()
            .chain(std::iter::once(((0, 0), lifetime)))
        {
            statement.execute(params![
                day,
                hour,
                integer(metrics.keys)?,
                integer(metrics.buttons[0])?,
                integer(metrics.buttons[1])?,
                integer(metrics.buttons[2])?,
                integer(metrics.buttons[3])?,
                integer(metrics.buttons[4])?,
                metrics.movement,
                metrics.scroll
            ])?;
        }
    }
    let today = Local::now().date_naive();
    let oldest = date_id(today.checked_sub_days(Days::new(729)).unwrap_or(today));
    transaction.execute("DELETE FROM metrics WHERE day>0 AND day<?1", [oldest])?;
    crate::usage_storage::write(&transaction, &usage)?;
    crate::keyboard::save(&transaction, &keyboard, oldest)?;
    transaction.commit()?;
    let mut store = shared.lock().unwrap_or_else(|e| e.into_inner());
    if store.revision == revision {
        store.dirty.clear();
        store.keyboard.dirty.clear();
        crate::usage_storage::clear(&mut store.usage);
    }
    Ok(())
}

// Commit deletion before replacing memory, so a database failure preserves history.
pub fn reset(connection: &mut Connection, shared: &Arc<Mutex<Store>>) -> rusqlite::Result<()> {
    let mut store = shared.lock().unwrap_or_else(|e| e.into_inner());
    let transaction = connection.transaction()?;
    transaction.execute_batch(
        "DELETE FROM metrics; DELETE FROM application_hours; DELETE FROM applications; DELETE FROM keyboard_counts;",
    )?;
    transaction.commit()?;
    let mut empty = Store::default();
    empty.oldest_day = store.oldest_day;
    empty.revision = store.revision.wrapping_add(1);
    empty.usage.enabled = store.usage.enabled;
    *store = empty;
    Ok(())
}

pub fn export(shared: &Arc<Mutex<Store>>, path: &Path) -> std::io::Result<()> {
    use std::io::Write;
    let (hours, lifetime) = {
        let store = shared.lock().unwrap_or_else(|e| e.into_inner());
        (store.hours.clone(), store.lifetime)
    };
    let mut file = std::io::BufWriter::new(std::fs::File::create(path)?);
    writeln!(file, "date,hour,key_presses,left_clicks,right_clicks,middle_clicks,back_clicks,forward_clicks,movement_input_units,scroll_input_units")?;
    for ((day, hour), m) in hours.into_iter().chain(std::iter::once(((0, 0), lifetime))) {
        writeln!(
            file,
            "{day},{hour},{},{},{},{},{},{},{:.4},{:.4}",
            m.keys,
            m.buttons[0],
            m.buttons[1],
            m.buttons[2],
            m.buttons[3],
            m.buttons[4],
            m.movement,
            m.scroll
        )?;
    }
    file.flush()?;
    file.get_ref().sync_all()
}
