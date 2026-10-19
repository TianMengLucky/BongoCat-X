//! Bounded provenance for committed characters in the current editable control.
use crate::words::{is_han, Batch};
use crate::words_text::CONTEXT;
use chrono::NaiveDate;
use std::collections::{BTreeMap, VecDeque};
use std::time::{Duration, Instant};

#[derive(Clone, Copy)]
struct Entry {
    c: char,
    eligible: bool,
    credit: Option<(NaiveDate, Instant)>,
}
#[derive(Default)]
pub struct History {
    // shortcut: only the current bounded input is reversible; extend tracking if older edits are needed.
    element: Vec<i32>,
    text: Vec<Entry>,
    typing: Option<Instant>,
    backspace: Option<Instant>,
    pending: VecDeque<Batch>,
    pub removed: VecDeque<Batch>,
}
impl History {
    pub fn input(&mut self, keys: &[u64; 256], blocked: bool, at: Instant) {
        if blocked
            || [
                9, 27, 33, 34, 35, 36, 37, 38, 39, 40, 45, 46, 91, 92, 162, 163, 164, 165,
            ]
            .iter()
            .any(|&k| keys[k] != 0)
        {
            *self = Self::default();
            return;
        }
        if keys[8] != 0 {
            self.backspace = Some(at);
        }
        if (48..=90)
            .chain(96..=111)
            .chain(186..=222)
            .any(|k| keys[k] != 0)
        {
            self.typing = Some(at);
            self.backspace = None;
        }
    }

    pub fn observe(&mut self, element: Vec<i32>, after: &[char], at: Instant) {
        let entry = |&c| Entry {
            c,
            eligible: false,
            credit: None,
        };
        if self.element != element {
            let typing = self.element.is_empty().then_some(self.typing).flatten();
            *self = Self::default();
            self.typing = typing;
            self.element = element;
            self.text = after.iter().map(entry).collect();
            return;
        }
        let eligible = self
            .typing
            .is_some_and(|t| at.saturating_duration_since(t) < Duration::from_secs(5));
        let deleting = self
            .backspace
            .is_some_and(|t| at.saturating_duration_since(t) < Duration::from_millis(500));
        let before: Vec<_> = self.text.iter().map(|e| e.c).collect();
        let mut prefix = before.iter().zip(after).take_while(|(a, b)| a == b).count();
        let mut suffix = before[prefix..]
            .iter()
            .rev()
            .zip(after[prefix..].iter().rev())
            .take_while(|(a, b)| a == b)
            .count();
        let mut clipped = false;
        // A full caret window can slide in either direction; clipping is not deletion.
        let mut old_start = 0;
        let mut new_start = 0;
        if before != after && before.len() >= CONTEXT as usize && after.len() >= CONTEXT as usize {
            let cap = before.len().min(after.len());
            if let Some(n) = (1..cap)
                .rev()
                .find(|&n| before[before.len() - n..] == after[..n])
            {
                old_start = before.len() - n;
                prefix = n;
                suffix = 0;
                clipped = true;
            } else if let Some(n) = (1..cap)
                .rev()
                .find(|&n| before[..n] == after[after.len() - n..])
            {
                new_start = after.len() - n;
                prefix = n;
                suffix = 0;
            }
        }
        let mut changes = BTreeMap::<(NaiveDate, Instant), BTreeMap<char, u64>>::new();
        if deleting && !clipped {
            for e in &self.text[old_start + prefix..before.len() - suffix] {
                if let Some(stamp) = e.credit {
                    *changes.entry(stamp).or_default().entry(e.c).or_default() += 1;
                }
            }
        }
        let mut updated: Vec<_> = after[..new_start].iter().map(entry).collect();
        updated.extend_from_slice(&self.text[old_start..old_start + prefix]);
        updated.extend(
            after[new_start + prefix..after.len() - suffix]
                .iter()
                .map(|&c| Entry {
                    c,
                    eligible: eligible && is_han(c),
                    credit: None,
                }),
        );
        updated.extend_from_slice(&self.text[before.len() - suffix..]);
        self.text = updated;
        for ((date, at), counts) in changes {
            self.removed.push_back(Batch { date, at, counts });
        }
        let mut pending = std::mem::take(&mut self.pending);
        while let Some(batch) = pending.pop_front() {
            if at.saturating_duration_since(batch.at) < Duration::from_secs(1)
                && !self.attach(&batch)
            {
                self.pending.push_back(batch);
            }
        }
    }

    fn attach(&mut self, batch: &Batch) -> bool {
        let mut left = batch.counts.clone();
        let mut indices = Vec::new();
        for (i, e) in self.text.iter().enumerate().rev() {
            if !e.eligible || e.credit.is_some() {
                continue;
            }
            if let Some(n) = left.get_mut(&e.c).filter(|n| **n != 0) {
                *n -= 1;
                indices.push(i);
            }
        }
        if left.values().any(|&n| n != 0) {
            return false;
        }
        for i in indices {
            self.text[i].credit = Some((batch.date, batch.at));
            self.text[i].eligible = false;
        }
        true
    }

    pub fn committed(&mut self, element: &[i32], batch: &Batch) {
        if element.is_empty() || element != self.element {
            return;
        }
        if !self.attach(batch) {
            if self.pending.len() == 32 {
                self.pending.pop_front();
            }
            self.pending.push_back(Batch {
                date: batch.date,
                at: batch.at,
                counts: batch.counts.clone(),
            });
        }
    }
}
