use crate::usage::{App, Usage};
use chrono::{Days, Local};
use rusqlite::{params, Connection, Transaction};
use std::path::Path;

pub fn load(connection: &Connection, usage: &mut Usage) -> rusqlite::Result<()> {
    connection.execute_batch("CREATE TABLE IF NOT EXISTS applications (
        id TEXT PRIMARY KEY, name TEXT NOT NULL, milliseconds INTEGER NOT NULL);
        CREATE TABLE IF NOT EXISTS application_hours (
        day INTEGER NOT NULL, hour INTEGER NOT NULL, app TEXT NOT NULL, milliseconds INTEGER NOT NULL,
        PRIMARY KEY(day,hour,app));
        CREATE TABLE IF NOT EXISTS statistics_options (name TEXT PRIMARY KEY, value INTEGER NOT NULL);")?;
    let mut statement =
        connection.prepare("SELECT id,name,milliseconds FROM applications LIMIT 513")?;
    let rows = statement.query_map([], |row| {
        Ok((
            row.get::<_, String>(0)?,
            row.get::<_, String>(1)?,
            crate::storage::count(row, 2)?,
        ))
    })?;
    for row in rows {
        let (id, name, total_ms) = row?;
        if usage.apps.len() >= 512
            || id.is_empty()
            || id.len() >= 256
            || name.len() >= 256
            || id.contains('\0')
            || name.contains('\0')
        {
            return Err(rusqlite::Error::InvalidQuery);
        }
        usage.apps.insert(id, App { name, total_ms });
    }
    let today = Local::now().date_naive();
    let oldest = crate::metrics::date_id(today.checked_sub_days(Days::new(29)).unwrap_or(today));
    let mut statement = connection.prepare(
        "SELECT day,hour,app,milliseconds FROM application_hours WHERE day>=?1 AND day<=?2 LIMIT 368641",
    )?;
    let rows = statement.query_map([oldest, crate::metrics::date_id(today)], |row| {
        Ok((
            row.get::<_, u32>(0)?,
            row.get::<_, u32>(1)?,
            row.get::<_, String>(2)?,
            crate::storage::count(row, 3)?,
        ))
    })?;
    for row in rows {
        let (day, hour, id, ms) = row?;
        if hour >= 24
            || usage.hours.len() >= 368640
            || !usage.apps.contains_key(&id)
            || chrono::NaiveDate::from_ymd_opt((day / 10000) as i32, day / 100 % 100, day % 100)
                .is_none()
        {
            return Err(rusqlite::Error::InvalidQuery);
        }
        usage.hours.insert((day, hour, id), ms);
    }
    let enabled = connection.query_row(
        "SELECT value FROM statistics_options WHERE name='application_usage'",
        [],
        |row| row.get::<_, u32>(0),
    );
    match enabled {
        Ok(value) => usage.enabled = value != 0,
        Err(rusqlite::Error::QueryReturnedNoRows) => (),
        Err(e) => return Err(e),
    }
    Ok(())
}
pub struct Pending {
    rows: Vec<((u32, u32, String), u64)>,
    apps: Vec<(String, App)>,
    enabled: Option<bool>,
}
pub fn pending(usage: &Usage) -> Pending {
    Pending {
        rows: usage
            .dirty
            .iter()
            .filter_map(|key| usage.hours.get(key).map(|&value| (key.clone(), value)))
            .collect(),
        apps: usage
            .changed
            .iter()
            .filter_map(|id| usage.apps.get(id).map(|app| (id.clone(), app.clone())))
            .collect(),
        enabled: usage.options_dirty.then_some(usage.enabled),
    }
}
pub fn write(transaction: &Transaction<'_>, pending: &Pending) -> rusqlite::Result<()> {
    let mut statement =
        transaction.prepare("INSERT OR REPLACE INTO application_hours VALUES (?1,?2,?3,?4)")?;
    for ((day, hour, id), ms) in &pending.rows {
        statement.execute(params![day, hour, id, crate::storage::integer(*ms)?])?;
    }
    let mut statement =
        transaction.prepare("INSERT OR REPLACE INTO applications VALUES (?1,?2,?3)")?;
    for (id, app) in &pending.apps {
        statement.execute(params![
            id,
            app.name,
            crate::storage::integer(app.total_ms)?
        ])?;
    }
    if let Some(enabled) = pending.enabled {
        transaction.execute(
            "INSERT OR REPLACE INTO statistics_options VALUES ('application_usage',?1)",
            [enabled as u32],
        )?;
    }
    let today = Local::now().date_naive();
    let oldest = crate::metrics::date_id(today.checked_sub_days(Days::new(729)).unwrap_or(today));
    transaction.execute("DELETE FROM application_hours WHERE day<?1", [oldest])?;
    Ok(())
}
pub fn clear(usage: &mut Usage) {
    usage.dirty.clear();
    usage.changed.clear();
    usage.options_dirty = false;
}
fn csv(value: &str) -> String {
    // Also prevent spreadsheet formula evaluation when a process supplies its own name.
    let prefix = if value
        .chars()
        .next()
        .is_some_and(|c| matches!(c, '=' | '+' | '-' | '@' | '\t' | '\r'))
    {
        "'"
    } else {
        ""
    };
    format!("\"{}{}\"", prefix, value.replace('"', "\"\""))
}
pub fn export(connection: &Connection, target: &Path) -> std::io::Result<()> {
    use std::io::Write;
    let database_error = |e| std::io::Error::other(format!("{e}"));
    let mut statement = connection.prepare("SELECT h.day,h.hour,h.app,a.name,h.milliseconds FROM application_hours h JOIN applications a ON a.id=h.app ORDER BY h.day,h.hour,h.app").map_err(database_error)?;
    let rows = statement
        .query_map([], |row| {
            Ok((
                row.get::<_, u32>(0)?,
                row.get::<_, u32>(1)?,
                row.get::<_, String>(2)?,
                row.get::<_, String>(3)?,
                crate::storage::count(row, 4)?,
            ))
        })
        .map_err(database_error)?;
    let mut file = std::io::BufWriter::new(std::fs::File::create(target)?);
    writeln!(
        file,
        "date,hour,application_id,application_name,milliseconds"
    )?;
    for row in rows {
        let (day, hour, id, name, ms) = row.map_err(database_error)?;
        writeln!(file, "{day},{hour},{},{},{ms}", csv(&id), csv(&name))?;
    }
    file.flush()?;
    file.get_ref().sync_all()
}
