//! Unordered Han character counts; committed strings never reach storage.
use chrono::{Days, NaiveDate};
use rusqlite::{params, Connection};
use std::collections::{BTreeMap, BTreeSet, VecDeque};
use std::time::Instant;

pub const TOP: usize = 32;
const IDLE_MS: u64 = 5_000;
#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct Character {
    pub codepoint: u32,
    pub reserved: u32,
    pub count: u64,
}
#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct Snapshot {
    pub status: u32, // 0 unsupported, 1 subscribed (coverage varies), 2 failed, 3 dropped events.
    pub count: u32,
    pub total: u64,
    pub recent: u64,
    pub average: f64,
    pub top: [Character; TOP],
}
pub struct Batch {
    pub date: NaiveDate,
    pub at: Instant,
    pub counts: BTreeMap<char, u64>,
}
pub fn is_han(c: char) -> bool {
    matches!(c as u32, 0x3400..=0x4dbf | 0x4e00..=0x9fff |
        0xf900..=0xfaff | 0x20000..=0x2a6df | 0x2a700..=0x2b73f |
        0x2b740..=0x2b81f | 0x2b820..=0x2ceaf | 0x2ceb0..=0x2ebef |
        0x2ebf0..=0x2ee5f | 0x2f800..=0x2fa1f | 0x30000..=0x3134f |
        0x31350..=0x323af | 0x323b0..=0x3347f)
}
#[derive(Default)]
pub struct Store {
    rows: BTreeMap<(u32, char), u64>,
    pub dirty: BTreeSet<(u32, char)>,
    recent: VecDeque<(Instant, u64)>,
    last: Option<Instant>,
    session_count: u64,
    active_ms: u64,
    pruned_day: u32,
    pub backspace: bool,
    pub options_dirty: bool,
    pub timer: crate::words_timer::Timer,
}
impl Store {
    pub fn ingest(&mut self, batch: Batch) {
        let total = batch
            .counts
            .iter()
            .filter(|(c, _)| is_han(**c))
            .fold(0u64, |sum, (_, n)| sum.saturating_add(*n));
        if total == 0 {
            return;
        }
        self.timer.commit(batch.at);
        let day = crate::metrics::date_id(batch.date);
        for (c, count) in batch.counts {
            if !is_han(c) {
                continue;
            }
            for id in [(0, c), (day, c)] {
                let value = self.rows.entry(id).or_default();
                *value = value.saturating_add(count).min(i64::MAX as u64);
                self.dirty.insert(id);
            }
        }
        // Each burst starts with one second; intervals over five seconds start a new burst.
        let at = self.last.map_or(batch.at, |last| batch.at.max(last));
        let elapsed = self
            .last
            .map(|last| at.saturating_duration_since(last).as_millis());
        self.active_ms = self.active_ms.saturating_add(match elapsed {
            Some(ms) if ms <= IDLE_MS as u128 => ms as u64,
            _ => 1_000,
        });
        self.last = Some(at);
        self.session_count = self.session_count.saturating_add(total);
        self.recent.push_back((at, total));
        self.prune(batch.date, batch.at);
    }
    pub fn subtract(&mut self, batch: Batch) {
        let day = crate::metrics::date_id(batch.date);
        let mut total = 0u64;
        for (c, n) in batch.counts {
            if !is_han(c) { continue; }
            let removed = self.rows.get(&(0, c)).copied().unwrap_or(0).min(n);
            total = total.saturating_add(removed);
            for id in [(0, c), (day, c)] {
                if let Some(count) = self.rows.get_mut(&id) {
                    *count = count.saturating_sub(removed);
                    self.dirty.insert(id);
                }
            }
        }
        self.session_count = self.session_count.saturating_sub(total);
        if let Some((_, count)) = self.recent.iter_mut().find(|(at, _)| *at == batch.at) {
            *count = count.saturating_sub(total);
        }
    }
    pub fn prune(&mut self, date: NaiveDate, at: Instant) {
        let oldest = crate::metrics::date_id(date - Days::new(29));
        if self.pruned_day != oldest {
            self.rows.retain(|&(day, _), _| day == 0 || day >= oldest);
            self.dirty.retain(|&(day, _)| day == 0 || day >= oldest);
            self.pruned_day = oldest;
        }
        while self
            .recent
            .front()
            .is_some_and(|(time, _)| at.saturating_duration_since(*time).as_secs() >= 60)
        {
            self.recent.pop_front();
        }
    }
    pub fn snapshot(&self, date: NaiveDate, period: usize, status: u32) -> Snapshot {
        let today = crate::metrics::date_id(date);
        let oldest = crate::metrics::date_id(
            date - Days::new(match period {
                0 => 0,
                1 => 6,
                _ => 29,
            }),
        );
        let mut counts = BTreeMap::<char, u64>::new();
        let (first, last) = if period == 3 { (0, 0) } else { (oldest, today) };
        for (&(day, c), &count) in self.rows.range((first, '\0')..=(last, char::MAX)) {
            if count != 0 && ((period == 3 && day == 0) || (period < 3 && day >= oldest && day <= today)) {
                let value = counts.entry(c).or_default();
                *value = value.saturating_add(count);
            }
        }
        let mut result = Snapshot {
            status,
            total: counts.values().fold(0u64, |sum, n| sum.saturating_add(*n)),
            recent: self
                .recent
                .iter()
                .fold(0u64, |sum, (_, n)| sum.saturating_add(*n)),
            average: self.session_count as f64 * 60_000.0 / self.active_ms.max(1) as f64,
            ..Snapshot::default()
        };
        let mut ranked: Vec<_> = counts.into_iter().collect();
        ranked.sort_unstable_by(|a, b| b.1.cmp(&a.1).then(a.0.cmp(&b.0)));
        result.count = ranked.len().min(TOP) as u32;
        for (out, (c, count)) in result.top.iter_mut().zip(ranked) {
            *out = Character {
                codepoint: c as u32,
                reserved: 0,
                count,
            };
        }
        result
    }
    pub fn load(&mut self, db: &Connection, oldest: u32, latest: u32) -> rusqlite::Result<()> {
        db.execute_batch("CREATE TABLE IF NOT EXISTS han_counts(day INTEGER NOT NULL, codepoint INTEGER NOT NULL, count INTEGER NOT NULL CHECK(count>=0), PRIMARY KEY(day,codepoint));")?;
        db.execute_batch("CREATE TABLE IF NOT EXISTS statistics_options (name TEXT PRIMARY KEY, value INTEGER NOT NULL);")?;
        match db.query_row("SELECT value FROM statistics_options WHERE name='subtract_backspace'", [], |row| row.get::<_, u32>(0)) {
            Ok(value @ 0..=1) => self.backspace = value != 0,
            Ok(_) => return Err(rusqlite::Error::InvalidQuery),
            Err(rusqlite::Error::QueryReturnedNoRows) => (),
            Err(e) => return Err(e),
        }
        let mut query = db.prepare(
            "SELECT day,codepoint,count FROM han_counts WHERE day=0 OR (day>=?1 AND day<=?2)",
        )?;
        let mut rows = query.query([oldest, latest])?;
        while let Some(row) = rows.next()? {
            let day: u32 = row.get(0)?;
            let cp: u32 = row.get(1)?;
            let c = char::from_u32(cp)
                .filter(|c| is_han(*c))
                .ok_or(rusqlite::Error::InvalidQuery)?;
            if day != 0
                && NaiveDate::from_ymd_opt((day / 10000) as i32, day / 100 % 100, day % 100)
                    .is_none()
            {
                return Err(rusqlite::Error::InvalidQuery);
            }
            self.rows.insert((day, c), crate::storage::count(row, 2)?);
        }
        Ok(())
    }
    pub fn pending(&self) -> Vec<(u32, char, u64)> {
        self.dirty
            .iter()
            .filter_map(|&(day, c)| self.rows.get(&(day, c)).map(|&n| (day, c, n)))
            .collect()
    }
}
pub fn save(db: &Connection, rows: &[(u32, char, u64)], oldest: u32) -> rusqlite::Result<()> {
    let mut insert =
        db.prepare("INSERT OR REPLACE INTO han_counts(day,codepoint,count) VALUES(?1,?2,?3)")?;
    for &(day, c, n) in rows {
        insert.execute(params![day, c as u32, crate::storage::integer(n)?])?;
    }
    db.execute("DELETE FROM han_counts WHERE day>0 AND day<?1", [oldest])?;
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn idle_gaps_empty_commits_retention_and_saturation() {
        let date = NaiveDate::from_ymd_opt(2026, 10, 18).unwrap();
        let start = Instant::now();
        let mut store = Store::default();
        for seconds in [0, 10] {
            store.ingest(Batch {
                date,
                at: start + std::time::Duration::from_secs(seconds),
                counts: [('中', 3), ('A', 100)].into(),
            });
        }
        assert_eq!(store.snapshot(date, 0, 1).total, 6);
        assert_eq!(store.snapshot(date, 0, 1).average, 180.0);
        store.ingest(Batch {
            date,
            at: start,
            counts: [('A', 1)].into(),
        });
        assert_eq!(store.snapshot(date, 0, 1).average, 180.0);
        store.prune(
            date + Days::new(30),
            start + std::time::Duration::from_secs(80),
        );
        assert_eq!(store.snapshot(date + Days::new(30), 2, 1).total, 0);
        assert_eq!(store.snapshot(date + Days::new(30), 3, 1).total, 6);
        store.ingest(Batch {
            date,
            at: start,
            counts: [('中', u64::MAX), ('文', u64::MAX)].into(),
        });
        assert_eq!(store.snapshot(date, 3, 1).top[0].count, i64::MAX as u64);
    }
    #[test]
    fn counts_periods_speed_and_roundtrip() {
        let date = NaiveDate::from_ymd_opt(2026, 10, 18).unwrap();
        let start = Instant::now();
        let mut store = Store::default();
        let batch = |date, at| Batch {
            date,
            at,
            counts: [('中', 2), ('文', 1)].into(),
        };
        store.ingest(batch(date - Days::new(8), start));
        store.ingest(batch(date, start + std::time::Duration::from_secs(2)));
        assert_eq!(store.snapshot(date, 0, 1).total, 3);
        assert_eq!(store.snapshot(date, 1, 1).total, 3);
        assert_eq!(store.snapshot(date, 2, 1).total, 6);
        assert_eq!(store.snapshot(date, 3, 1).top[0].codepoint, '中' as u32);
        assert_eq!(store.snapshot(date, 3, 1).average, 120.0);
        store.prune(date, start + std::time::Duration::from_secs(63));
        assert_eq!(store.snapshot(date, 0, 1).recent, 0);
        let db = Connection::open_in_memory().unwrap();
        let mut loaded = Store::default();
        loaded.load(&db, 0, crate::metrics::date_id(date)).unwrap();
        save(&db, &store.pending(), 0).unwrap();
        loaded.load(&db, 0, crate::metrics::date_id(date)).unwrap();
        assert_eq!(loaded.snapshot(date, 3, 1).total, 6);
        assert_eq!(loaded.snapshot(date, 3, 1).average, 0.0);
        assert!(is_han('𠮷'));
        assert!(!is_han('あ'));
        assert!(!is_han('，'));
        assert!(!is_han('A'));
    }
}
