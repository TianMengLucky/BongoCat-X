//! Daily unordered key histograms. Day zero keeps lifetime totals, never input order.
use crate::{guarded, metrics::date_id, Command, Instance};
use chrono::{Days, NaiveDate};
use rusqlite::{params, Connection};
use std::{
    collections::{BTreeMap, BTreeSet},
    ffi::c_void,
};
pub type Counts = [u64; 256];
pub type Poll = unsafe extern "C" fn(*mut c_void, *mut u64);
pub struct Source {
    pub context: *mut c_void,
    pub poll: Poll,
}
// The host owns this context until the worker is joined.
unsafe impl Send for Source {}
#[derive(Default)]
pub struct Store {
    days: BTreeMap<u32, Counts>,
    pub dirty: BTreeSet<u32>,
}
impl Store {
    pub fn ingest(&mut self, date: NaiveDate, counts: Counts) {
        if counts.iter().all(|&n| n == 0) {
            return;
        }
        let day = date_id(date);
        for id in [0, day] {
            let row = self.days.entry(id).or_insert([0; 256]);
            for (value, count) in row.iter_mut().zip(counts) {
                *value = value.saturating_add(count).min(i64::MAX as u64);
            }
            self.dirty.insert(id);
        }
        let oldest = date_id(date.checked_sub_days(Days::new(729)).unwrap_or(date));
        self.days.retain(|&id, _| id == 0 || id >= oldest);
        self.dirty.retain(|&id| id == 0 || id >= oldest);
    }
    pub fn snapshot(&self, date: NaiveDate, period: usize) -> Counts {
        if period == 3 {
            return self.days.get(&0).copied().unwrap_or([0; 256]);
        }
        let oldest = date_id(
            date.checked_sub_days(Days::new(match period {
                0 => 0,
                1 => 6,
                _ => 29,
            }))
            .unwrap_or(date),
        );
        let mut result = [0u64; 256];
        for (_, counts) in self.days.range(oldest..=date_id(date)) {
            for (value, count) in result.iter_mut().zip(counts) {
                *value = value.saturating_add(*count);
            }
        }
        result
    }
    pub fn pending(&self) -> Vec<(u32, Counts)> {
        self.dirty
            .iter()
            .filter_map(|day| self.days.get(day).map(|counts| (*day, *counts)))
            .collect()
    }
}
pub fn load(connection: &Connection, store: &mut Store, oldest: u32) -> rusqlite::Result<()> {
    connection.execute_batch("CREATE TABLE IF NOT EXISTS keyboard_counts (day INTEGER NOT NULL, key INTEGER NOT NULL, count INTEGER NOT NULL, PRIMARY KEY(day,key));")?;
    let mut query = connection
        .prepare("SELECT day,key,count FROM keyboard_counts WHERE day=0 OR day>=?1 LIMIT 187137")?;
    let mut rows = query.query([oldest])?;
    let mut total = 0;
    while let Some(row) = rows.next()? {
        total += 1;
        let day: u32 = row.get(0)?;
        let key: u32 = row.get(1)?;
        if key >= 256
            || total > 187136
            || (day != 0
                && NaiveDate::from_ymd_opt((day / 10000) as i32, day / 100 % 100, day % 100)
                    .is_none())
        {
            return Err(rusqlite::Error::InvalidQuery);
        }
        store.days.entry(day).or_insert([0; 256])[key as usize] = crate::storage::count(row, 2)?;
    }
    Ok(())
}
pub fn save(connection: &Connection, rows: &[(u32, Counts)], oldest: u32) -> rusqlite::Result<()> {
    let mut statement = connection
        .prepare("INSERT OR REPLACE INTO keyboard_counts(day,key,count) VALUES (?1,?2,?3)")?;
    for (day, counts) in rows {
        for (key, count) in counts.iter().enumerate().filter(|(_, n)| **n > 0) {
            statement.execute(params![day, key as u32, crate::storage::integer(*count)?])?;
        }
    }
    connection.execute(
        "DELETE FROM keyboard_counts WHERE day>0 AND day<?1",
        [oldest],
    )?;
    Ok(())
}
unsafe extern "C" fn configure(
    instance: *mut c_void,
    context: *mut c_void,
    poll: Option<Poll>,
) -> u32 {
    guarded(0, || {
        let (Some(instance), Some(poll)) = (instance.cast::<Instance>().as_ref(), poll) else {
            return 0;
        };
        instance
            .commands
            .try_send(Command::Keyboard(Source { context, poll }))
            .is_ok() as u32
    })
}
unsafe extern "C" fn snapshot(instance: *mut c_void, period: u32, output: *mut u64) -> u32 {
    guarded(0, || {
        let Some(instance) = instance.cast::<Instance>().as_ref() else {
            return 0;
        };
        if period > 3 || output.is_null() {
            return 0;
        }
        let Ok(cache) = instance.cache.try_lock() else {
            return 0;
        };
        std::ptr::copy_nonoverlapping(cache.keyboard[period as usize].as_ptr(), output, 256);
        1
    })
}
#[repr(C)]
pub struct Plugin {
    abi_version: u32,
    struct_size: u32,
    configure: unsafe extern "C" fn(*mut c_void, *mut c_void, Option<Poll>) -> u32,
    snapshot: unsafe extern "C" fn(*mut c_void, u32, *mut u64) -> u32,
}
static PLUGIN: Plugin = Plugin {
    abi_version: 1,
    struct_size: std::mem::size_of::<Plugin>() as u32,
    configure,
    snapshot,
};
#[no_mangle]
pub extern "C" fn bongo_cat_statistics_keyboard_query(abi: u32) -> *const Plugin {
    if abi == 1 {
        &PLUGIN
    } else {
        std::ptr::null()
    }
}
#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn extension_validates_abi_nulls_and_period() {
        assert!(bongo_cat_statistics_keyboard_query(0).is_null());
        let api = unsafe { &*bongo_cat_statistics_keyboard_query(1) };
        assert_eq!(api.struct_size as usize, std::mem::size_of::<Plugin>());
        assert_eq!(
            unsafe { (api.configure)(std::ptr::null_mut(), std::ptr::null_mut(), None) },
            0
        );
        assert_eq!(
            unsafe { (api.snapshot)(std::ptr::null_mut(), 4, std::ptr::null_mut()) },
            0
        );
    }
    #[test]
    fn histogram_periods_persistence_and_reset() {
        let date = NaiveDate::from_ymd_opt(2026, 10, 17).unwrap();
        let mut store = Store::default();
        let mut counts = [0; 256];
        counts[65] = 3;
        counts[162] = 2;
        store.ingest(date - Days::new(8), counts);
        store.ingest(date, counts);
        assert_eq!(store.snapshot(date, 0)[65], 3);
        assert_eq!(store.snapshot(date, 1)[65], 3);
        assert_eq!(store.snapshot(date, 2)[65], 6);
        assert_eq!(store.snapshot(date, 3)[162], 4);
        let db = Connection::open_in_memory().unwrap();
        let mut loaded = Store::default();
        load(&db, &mut loaded, 0).unwrap();
        save(&db, &store.pending(), 0).unwrap();
        load(&db, &mut loaded, 0).unwrap();
        assert_eq!(loaded.snapshot(date, 3), store.snapshot(date, 3));
        db.execute("DELETE FROM keyboard_counts", []).unwrap();
        let mut empty = Store::default();
        load(&db, &mut empty, 0).unwrap();
        assert_eq!(empty.snapshot(date, 3), [0; 256]);
    }
}
