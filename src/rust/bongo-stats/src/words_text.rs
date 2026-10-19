//! Bounded, transient input context for providers without TextEditPattern.
use crate::words::is_han;
use std::collections::BTreeMap;
use std::time::{Duration, Instant};

pub const CONTEXT: i32 = 256;
const INPUT_WINDOW: Duration = Duration::from_secs(5);
const COMMIT_WINDOW: Duration = Duration::from_millis(500);

#[derive(Default)]
pub struct Tracker {
    element: Vec<i32>,
    before: Vec<char>,
    typing: Option<Instant>,
    commit: Option<Instant>,
}

fn inserted<'a>(before: &[char], after: &'a [char]) -> &'a [char] {
    if before == after {
        return &[];
    }
    // The left edge advances when a full caret context receives more text.
    if before.len() >= CONTEXT as usize && after.len() >= CONTEXT as usize {
        if let Some(overlap) = (1..before.len().min(after.len()))
            .rev()
            .find(|&n| before[before.len() - n..] == after[..n])
        {
            return &after[overlap..];
        }
    }
    let prefix = before.iter().zip(after).take_while(|(a, b)| a == b).count();
    let suffix = before[prefix..]
        .iter()
        .rev()
        .zip(after[prefix..].iter().rev())
        .take_while(|(a, b)| a == b)
        .count();
    &after[prefix..after.len() - suffix]
}

impl Tracker {
    pub fn reset(&mut self) {
        *self = Self::default();
    }

    pub fn input(&mut self, keys: &[u64; 256], blocked: bool, at: Instant) {
        // Host key IDs follow the Windows virtual-key layout, including split modifiers.
        let cancel = [
            8, 9, 27, 33, 34, 35, 36, 37, 38, 39, 40, 45, 46, 91, 92, 162, 163, 164, 165,
        ]
        .iter()
        .any(|&key| keys[key] != 0);
        if blocked || cancel {
            self.typing = None;
            self.commit = None;
            return;
        }
        let typed = (48..=90)
            .chain(96..=111)
            .chain(186..=222)
            .any(|key| keys[key] != 0);
        if typed {
            self.typing = Some(at);
        }
        let confirm = [
            13, 32, 166, 186, 187, 188, 189, 190, 191, 192, 219, 220, 221, 222,
        ]
        .into_iter()
        .chain(48..=57)
        .any(|key| keys[key] != 0);
        if confirm
            && self
                .typing
                .is_some_and(|time| at.saturating_duration_since(time) <= INPUT_WINDOW)
        {
            self.commit = Some(at);
        }
    }

    pub fn observe(
        &mut self,
        element: Vec<i32>,
        after: Vec<char>,
        at: Instant,
    ) -> Option<BTreeMap<char, u64>> {
        if self.element != element {
            let typing = if self.element.is_empty() {
                self.typing
            } else {
                None
            };
            self.reset();
            self.typing = typing;
            self.element = element;
            self.before = after;
            return None;
        }
        if !self
            .typing
            .is_some_and(|time| at.saturating_duration_since(time) <= INPUT_WINDOW)
        {
            self.before = after;
            return None;
        }
        let ready = self
            .commit
            .is_some_and(|time| at.saturating_duration_since(time) <= COMMIT_WINDOW);
        if !ready {
            return None;
        }
        let mut counts = BTreeMap::new();
        for &c in inserted(&self.before, &after)
            .iter()
            .filter(|c| is_han(**c))
        {
            *counts.entry(c).or_default() += 1;
        }
        if counts.is_empty() {
            return None;
        }
        self.before = after;
        self.typing = None;
        self.commit = None;
        Some(counts)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn keys(codes: &[usize]) -> [u64; 256] {
        let mut keys = [0; 256];
        for &code in codes {
            keys[code] = 1;
        }
        keys
    }
    fn observe(tracker: &mut Tracker, text: &str, at: Instant) -> Option<BTreeMap<char, u64>> {
        tracker.observe(vec![1], text.chars().collect(), at)
    }

    #[test]
    fn counts_pinyin_replacement_without_text_edit_events() {
        let at = Instant::now();
        let mut tracker = Tracker::default();
        assert!(observe(&mut tracker, "已有", at).is_none());
        tracker.input(&keys(&[90]), false, at);
        assert!(observe(&mut tracker, "已有zhongwen", at).is_none());
        tracker.input(&keys(&[32]), false, at);
        assert_eq!(
            observe(&mut tracker, "已有中文𠮷，", at).unwrap(),
            [('中', 1), ('文', 1), ('𠮷', 1)].into()
        );
        assert!(observe(&mut tracker, "已有中文𠮷，", at).is_none());
    }

    #[test]
    fn first_pinyin_key_is_not_lost_while_establishing_the_focus_baseline() {
        let at = Instant::now();
        let mut tracker = Tracker::default();
        tracker.input(&keys(&[65]), false, at);
        assert!(observe(&mut tracker, "a", at).is_none());
        tracker.input(&keys(&[32]), false, at);
        assert_eq!(observe(&mut tracker, "啊", at).unwrap(), [('啊', 1)].into());
    }

    #[test]
    fn waits_for_confirmation_even_if_preedit_already_contains_han() {
        let at = Instant::now();
        let mut tracker = Tracker::default();
        observe(&mut tracker, "前", at);
        tracker.input(&keys(&[90]), false, at);
        assert!(observe(&mut tracker, "前中", at).is_none());
        tracker.input(&keys(&[32]), false, at);
        assert_eq!(
            observe(&mut tracker, "前中", at).unwrap(),
            [('中', 1)].into()
        );
        tracker.input(&keys(&[90, 32]), false, at);
        assert_eq!(
            observe(&mut tracker, "前中中", at).unwrap(),
            [('中', 1)].into()
        );
    }

    #[test]
    fn skips_focus_history_pastes_navigation_cancellation_and_deletion() {
        let at = Instant::now();
        let mut tracker = Tracker::default();
        observe(&mut tracker, "历史", at);
        for codes in [vec![162, 86], vec![37], vec![27], vec![8]] {
            tracker.input(&keys(&[90]), false, at);
            tracker.input(&keys(&codes), false, at);
            assert!(observe(&mut tracker, "历史中文", at).is_none());
        }
        tracker.input(&keys(&[90, 32]), true, at);
        assert!(observe(&mut tracker, "粘贴内容", at).is_none());
        tracker.input(&keys(&[90, 32]), false, at);
        assert!(tracker
            .observe(vec![2], "其他控件".chars().collect(), at)
            .is_none());
    }

    #[test]
    fn retains_commit_until_provider_finishes_updating_but_expires_it() {
        let at = Instant::now();
        let mut tracker = Tracker::default();
        observe(&mut tracker, "", at);
        tracker.input(&keys(&[90, 32]), false, at);
        assert!(observe(&mut tracker, "zhong", at).is_none());
        assert_eq!(
            observe(&mut tracker, "中", at + Duration::from_millis(100)).unwrap(),
            [('中', 1)].into()
        );
        tracker.input(&keys(&[90, 32]), false, at);
        assert!(observe(&mut tracker, "中文", at + Duration::from_secs(1)).is_none());
    }

    #[test]
    fn handles_full_context_sliding_and_deletion() {
        let before: Vec<_> = (0..CONTEXT)
            .map(|n| char::from_u32(0x4e00 + n as u32).unwrap())
            .collect();
        let mut after = before[1..].to_vec();
        after.push('𠮷');
        assert_eq!(inserted(&before, &after), &['𠮷']);
        assert!(inserted(&before, &before[..before.len() - 1]).is_empty());
    }
}
