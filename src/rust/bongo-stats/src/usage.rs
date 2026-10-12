use crate::metrics::date_id;
use chrono::{DateTime, Days, Local, NaiveDate, Timelike};
use std::collections::{BTreeMap, BTreeSet};

pub const APP_CAP: usize = 256;
pub const MAX_APPS: usize = 64;
#[repr(C)]
#[derive(Clone, Copy)]
pub struct Sample {
    pub available: u32,
    pub active: u32,
    pub idle_ms: u64,
    pub id: [u8; APP_CAP],
    pub name: [u8; APP_CAP],
}
impl Default for Sample {
    fn default() -> Self {
        Self {
            available: 0,
            active: 0,
            idle_ms: 0,
            id: [0; APP_CAP],
            name: [0; APP_CAP],
        }
    }
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct AppTotal {
    pub id: [u8; APP_CAP],
    pub name: [u8; APP_CAP],
    pub milliseconds: u64,
}
#[repr(C)]
#[derive(Clone)]
pub struct Snapshot {
    pub struct_size: u32,
    pub enabled: u32,
    pub available: u32,
    pub active: u32,
    pub count: u32,
    pub export_status: u32,
    pub total_ms: u64,
    pub hours: [u64; 24],
    pub apps: [AppTotal; MAX_APPS],
}
impl Default for Snapshot {
    fn default() -> Self {
        Self {
            struct_size: std::mem::size_of::<Self>() as u32,
            enabled: 1,
            available: 0,
            active: 0,
            count: 0,
            export_status: 0,
            total_ms: 0,
            hours: [0; 24],
            apps: [AppTotal {
                id: [0; APP_CAP],
                name: [0; APP_CAP],
                milliseconds: 0,
            }; MAX_APPS],
        }
    }
}
pub fn text(bytes: &[u8]) -> Option<&str> {
    let end = bytes.iter().position(|&b| b == 0)?;
    std::str::from_utf8(&bytes[..end])
        .ok()
        .filter(|s| !s.is_empty())
}
pub fn copy_text(bytes: &mut [u8], value: &str) {
    let mut end = value.len().min(bytes.len() - 1);
    while !value.is_char_boundary(end) {
        end -= 1;
    }
    bytes[..end].copy_from_slice(&value.as_bytes()[..end]);
}
#[derive(Clone, Default)]
pub struct App {
    pub name: String,
    pub total_ms: u64,
}
pub struct Usage {
    pub enabled: bool,
    pub available: bool,
    pub active: bool,
    pub apps: BTreeMap<String, App>,
    pub hours: BTreeMap<(u32, u32, String), u64>,
    pub dirty: BTreeSet<(u32, u32, String)>,
    pub changed: BTreeSet<String>,
    pub options_dirty: bool,
    previous: Option<(u64, i64, String)>,
}
impl Default for Usage {
    fn default() -> Self {
        Self {
            enabled: true,
            available: false,
            active: false,
            apps: BTreeMap::new(),
            hours: BTreeMap::new(),
            dirty: BTreeSet::new(),
            changed: BTreeSet::new(),
            options_dirty: false,
            previous: None,
        }
    }
}
impl Usage {
    pub fn set_enabled(&mut self, enabled: bool) {
        self.enabled = enabled;
        self.options_dirty = true;
        self.previous = None;
        self.active = false;
    }
    pub fn sample(&mut self, monotonic_ms: u64, unix_ms: i64, sample: Sample) {
        self.available = sample.available != 0;
        self.active =
            self.enabled && self.available && sample.active != 0 && sample.idle_ms < 300_000;
        let previous = self.previous.take();
        let Some(id) = text(&sample.id).filter(|_| self.active) else {
            self.active = false;
            return;
        };
        // A hard cap bounds memory even if foreground processes change without limit.
        if !self.apps.contains_key(id) && self.apps.len() >= 512 {
            self.active = false;
            return;
        }
        let name = text(&sample.name).unwrap_or(id);
        let entry = self.apps.entry(id.into()).or_default();
        if entry.name != name {
            entry.name = name.into();
            self.changed.insert(id.into());
        }
        if let Some((last_ms, last_unix, last_id)) = previous {
            let elapsed = monotonic_ms.saturating_sub(last_ms);
            // Ignore suspend, stalled probes and clock changes instead of billing the gap.
            if elapsed > 0
                && elapsed <= 5000
                && (unix_ms - last_unix - elapsed as i64).abs() <= 2000
            {
                let mut cursor = last_unix;
                let end = last_unix + elapsed as i64;
                while cursor < end {
                    let next = ((cursor.div_euclid(1000) + 1) * 1000).min(end);
                    if let Some(time) = DateTime::from_timestamp_millis(cursor) {
                        let local = time.with_timezone(&Local);
                        self.add(
                            date_id(local.date_naive()),
                            local.hour(),
                            &last_id,
                            (next - cursor) as u64,
                        );
                    }
                    cursor = next;
                }
            }
        }
        self.previous = Some((monotonic_ms, unix_ms, id.into()));
        let today = Local::now().date_naive();
        let oldest = date_id(today.checked_sub_days(Days::new(29)).unwrap_or(today));
        self.hours.retain(|(day, _, _), _| *day >= oldest);
    }
    pub fn add(&mut self, day: u32, hour: u32, id: &str, milliseconds: u64) {
        if hour >= 24 || milliseconds == 0 {
            return;
        }
        let key = (day, hour, id.to_owned());
        let value = self.hours.entry(key.clone()).or_default();
        *value = value.saturating_add(milliseconds).min(i64::MAX as u64);
        self.dirty.insert(key);
        let app = self.apps.entry(id.into()).or_default();
        app.total_ms = app
            .total_ms
            .saturating_add(milliseconds)
            .min(i64::MAX as u64);
        self.changed.insert(id.into());
    }
    pub fn snapshot(&self, today: NaiveDate, period: u32) -> Snapshot {
        let mut result = Snapshot {
            enabled: self.enabled as u32,
            available: self.available as u32,
            active: self.active as u32,
            ..Snapshot::default()
        };
        let oldest = date_id(
            today
                .checked_sub_days(Days::new(match period {
                    0 => 0,
                    1 => 6,
                    _ => 29,
                }))
                .unwrap_or(today),
        );
        let newest = date_id(today);
        let mut totals: BTreeMap<String, u64> = BTreeMap::new();
        for ((day, hour, id), &ms) in &self.hours {
            if *day == newest {
                result.hours[*hour as usize] = result.hours[*hour as usize].saturating_add(ms);
            }
            if *day >= oldest && *day <= newest && period < 3 {
                let total = totals.entry(id.clone()).or_default();
                *total = total.saturating_add(ms);
            }
        }
        if period == 3 {
            totals = self
                .apps
                .iter()
                .map(|(id, app)| (id.clone(), app.total_ms))
                .collect();
        }
        result.total_ms = totals
            .values()
            .fold(0u64, |sum, &value| sum.saturating_add(value));
        let mut totals: Vec<_> = totals.into_iter().filter(|(_, ms)| *ms > 0).collect();
        totals.sort_by(|a, b| b.1.cmp(&a.1).then(a.0.cmp(&b.0)));
        for (index, (id, ms)) in totals.into_iter().take(MAX_APPS).enumerate() {
            let app = &mut result.apps[index];
            copy_text(&mut app.id, &id);
            copy_text(
                &mut app.name,
                self.apps
                    .get(&id)
                    .map(|a| a.name.as_str())
                    .filter(|s| !s.is_empty())
                    .unwrap_or(&id),
            );
            app.milliseconds = ms;
            result.count += 1;
        }
        result
    }
}
