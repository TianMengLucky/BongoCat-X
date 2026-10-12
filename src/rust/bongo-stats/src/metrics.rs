use chrono::{Datelike, Days, NaiveDate};
use std::collections::{BTreeMap, BTreeSet};

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Metrics {
    pub keys: u64,
    pub buttons: [u64; 5],
    pub movement: f64,
    pub scroll: f64,
}

impl Metrics {
    pub fn valid(&self) -> bool {
        self.movement.is_finite()
            && self.movement >= 0.0
            && self.scroll.is_finite()
            && self.scroll >= 0.0
            && self.keys <= i64::MAX as u64
            && self.buttons.iter().all(|&n| n <= i64::MAX as u64)
    }
    pub fn add(&mut self, other: Self) {
        self.keys = self.keys.saturating_add(other.keys).min(i64::MAX as u64);
        for (value, delta) in self.buttons.iter_mut().zip(other.buttons) {
            *value = value.saturating_add(delta).min(i64::MAX as u64);
        }
        self.movement = (self.movement + other.movement).min(f64::MAX);
        self.scroll = (self.scroll + other.scroll).min(f64::MAX);
    }
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct Day {
    pub date: u32,
    pub metrics: Metrics,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct Snapshot {
    pub struct_size: u32,
    pub storage_error: u32,
    pub today: Metrics,
    pub week: Metrics,
    pub month: Metrics,
    pub lifetime: Metrics,
    pub hours: [Metrics; 24],
    pub days: [Day; 30],
}

pub fn date_id(date: NaiveDate) -> u32 {
    (date.year() as u32) * 10000 + date.month() * 100 + date.day()
}

#[derive(Default)]
pub struct Store {
    pub hours: BTreeMap<(u32, u32), Metrics>,
    pub lifetime: Metrics,
    pub dirty: BTreeSet<(u32, u32)>,
    pub revision: u64,
    pub oldest_day: u32,
    pub usage: crate::usage::Usage,
}

impl Store {
    pub fn ingest(&mut self, date: NaiveDate, hour: u32, metrics: Metrics) {
        if hour >= 24 || !metrics.valid() || metrics == Metrics::default() {
            return;
        }
        let id = (date_id(date), hour);
        if id.0 >= self.oldest_day {
            self.hours.entry(id).or_default().add(metrics);
            self.dirty.insert(id);
        }
        self.lifetime.add(metrics);
        self.dirty.insert((0, 0));
        self.revision = self.revision.wrapping_add(1);
        let oldest = date_id(date.checked_sub_days(Days::new(729)).unwrap_or(date));
        if oldest > self.oldest_day {
            self.hours.retain(|&(day, _), _| day >= oldest);
            self.dirty.retain(|&(day, _)| day == 0 || day >= oldest);
            self.oldest_day = oldest;
        }
    }

    pub fn snapshot(&self, date: NaiveDate, storage_error: bool) -> Snapshot {
        let mut result = Snapshot {
            struct_size: std::mem::size_of::<Snapshot>() as u32,
            storage_error: storage_error as u32,
            today: Metrics::default(),
            week: Metrics::default(),
            month: Metrics::default(),
            lifetime: self.lifetime,
            hours: [Metrics::default(); 24],
            days: [Day::default(); 30],
        };
        for (i, day) in result.days.iter_mut().enumerate() {
            let date = date
                .checked_sub_days(Days::new((29 - i) as u64))
                .unwrap_or(date);
            day.date = date_id(date);
            for hour in 0..24 {
                let metrics = self
                    .hours
                    .get(&(day.date, hour))
                    .copied()
                    .unwrap_or_default();
                day.metrics.add(metrics);
                if i == 29 {
                    result.hours[hour as usize] = metrics;
                }
            }
            result.month.add(day.metrics);
            if i >= 23 {
                result.week.add(day.metrics);
            }
        }
        result.today = result.days[29].metrics;
        result
    }
}
