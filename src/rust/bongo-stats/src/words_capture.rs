//! Pair event and composition observations without retaining ordered text.
use crate::words::{is_han, Batch};
use std::collections::{BTreeMap, VecDeque};
use std::time::Duration;

pub const MAX_TEXT: usize = 4096;
const MATCH_WINDOW: Duration = Duration::from_secs(1);
const MAX_PENDING: usize = 128;

pub struct Capture {
    pub element: Vec<i32>,
    pub polled: bool,
    pub batch: Batch,
}

pub fn has_preedit(text: &[u16]) -> bool {
    char::decode_utf16(text.iter().copied())
        .filter_map(Result::ok)
        .any(|c| !c.is_whitespace())
}

pub fn histogram(text: &[u16]) -> BTreeMap<char, u64> {
    let mut counts = BTreeMap::new();
    for c in char::decode_utf16(text.iter().copied())
        .filter_map(Result::ok)
        .filter(|c| is_han(*c))
    {
        *counts.entry(c).or_default() += 1;
    }
    counts
}

#[derive(Default)]
pub struct Filter {
    pending: VecDeque<Capture>,
}

impl Filter {
    pub fn accept(&mut self, capture: Capture) -> Option<Batch> {
        if capture.batch.counts.is_empty() {
            return None;
        }
        if capture.element.is_empty() {
            return Some(capture.batch);
        }
        self.pending.retain(|previous| {
            capture
                .batch
                .at
                .max(previous.batch.at)
                .duration_since(capture.batch.at.min(previous.batch.at))
                <= MATCH_WINDOW
        });
        if let Some(index) = self.pending.iter().position(|previous| {
            previous.polled != capture.polled
                && previous.element == capture.element
                && previous.batch.counts == capture.batch.counts
        }) {
            self.pending.remove(index);
            return None;
        }
        if self.pending.len() == MAX_PENDING {
            self.pending.pop_front();
        }
        self.pending.push_back(Capture {
            element: capture.element,
            polled: capture.polled,
            batch: Batch {
                date: capture.batch.date,
                at: capture.batch.at,
                counts: capture.batch.counts.clone(),
            },
        });
        Some(capture.batch)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use chrono::NaiveDate;
    use std::time::Instant;

    fn capture(element: i32, polled: bool, at: Instant) -> Capture {
        Capture {
            element: vec![element],
            polled,
            batch: Batch {
                date: NaiveDate::from_ymd_opt(2026, 10, 19).unwrap(),
                at,
                counts: histogram(&"中文中，A𠮷".encode_utf16().collect::<Vec<_>>()),
            },
        }
    }

    #[test]
    fn pairs_both_sources_in_either_order_but_keeps_repeated_commits() {
        let now = Instant::now();
        for first in [false, true] {
            let mut filter = Filter::default();
            assert!(filter.accept(capture(1, first, now)).is_some());
            assert!(filter.accept(capture(1, first, now)).is_some());
            assert!(filter.accept(capture(1, !first, now)).is_none());
            assert!(filter.accept(capture(1, !first, now)).is_none());
            assert!(filter.accept(capture(1, first, now)).is_some());
        }
    }

    #[test]
    fn unrelated_controls_and_later_commits_are_not_suppressed() {
        let now = Instant::now();
        let mut filter = Filter::default();
        assert!(filter.accept(capture(1, false, now)).is_some());
        assert!(filter.accept(capture(2, true, now)).is_some());
        assert!(filter
            .accept(capture(
                1,
                true,
                now + MATCH_WINDOW + Duration::from_millis(1)
            ))
            .is_some());
    }

    #[test]
    fn counts_han_surrogate_pairs_and_ignores_invalid_utf16() {
        let counts = histogram(&['中' as u16, 0xd842, 0xdfb7, 0xd800, 'A' as u16]);
        assert_eq!(counts, [('中', 1), ('𠮷', 1)].into());
        assert!(histogram(&[]).is_empty());
    }

    #[test]
    fn empty_provider_paragraphs_are_not_active_compositions() {
        assert!(!has_preedit(&[]));
        assert!(!has_preedit(&"\r\n ".encode_utf16().collect::<Vec<_>>()));
        assert!(has_preedit(&"zhongwen".encode_utf16().collect::<Vec<_>>()));
        assert!(has_preedit(&"中文".encode_utf16().collect::<Vec<_>>()));
    }

    #[test]
    fn missing_runtime_ids_do_not_drop_valid_event_counts() {
        let mut filter = Filter::default();
        for polled in [false, true] {
            let mut capture = capture(1, polled, Instant::now());
            capture.element.clear();
            assert!(filter.accept(capture).is_some());
        }
    }
}
