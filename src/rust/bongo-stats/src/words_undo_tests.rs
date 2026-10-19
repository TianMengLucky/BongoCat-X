use crate::{words::Batch, words_undo::History};
use chrono::{Days, NaiveDate};
use std::time::{Duration, Instant};

fn batch(text: &str, at: Instant) -> Batch {
    Batch {
        date: NaiveDate::from_ymd_opt(2026, 10, 19).unwrap(),
        at,
        counts: crate::words_capture::histogram(&text.encode_utf16().collect::<Vec<_>>()),
    }
}
fn input(h: &mut History, key: usize, at: Instant) {
    let mut keys = [0; 256];
    keys[key] = 1;
    h.input(&keys, false, at);
}
fn observe(h: &mut History, text: &str, at: Instant) {
    h.observe(vec![1], &text.chars().collect::<Vec<_>>(), at);
}
fn commit(h: &mut History, before: &str, after: &str, text: &str, at: Instant) {
    observe(h, before, at);
    input(h, 65, at);
    observe(h, after, at);
    h.committed(&[1], &batch(text, at));
}

#[test]
fn backspace_subtracts_only_observed_credits_once_including_supplementary_han() {
    let at = Instant::now();
    let mut h = History::default();
    commit(&mut h, "中", "中中文𠮷，", "中文𠮷", at);
    input(&mut h, 8, at);
    observe(&mut h, "中中文𠮷", at);
    assert!(h.removed.is_empty()); // punctuation was never counted
    observe(&mut h, "中中文", at);
    assert_eq!(h.removed.pop_front().unwrap().counts, [('𠮷', 1)].into());
    observe(&mut h, "中中文", at);
    assert!(h.removed.is_empty());
    observe(&mut h, "", at);
    assert_eq!(
        h.removed.pop_front().unwrap().counts,
        [('中', 1), ('文', 1)].into()
    );
    assert!(h.removed.is_empty()); // the original historical 中 is excluded
}

#[test]
fn pinyin_backspace_and_untriggered_changes_do_not_subtract() {
    let at = Instant::now();
    let mut h = History::default();
    commit(&mut h, "", "中文", "中文", at);
    input(&mut h, 65, at);
    observe(&mut h, "中文zhong", at);
    input(&mut h, 8, at);
    observe(&mut h, "中文zhon", at);
    assert!(h.removed.is_empty());
    observe(&mut h, "", at + Duration::from_secs(1));
    assert!(h.removed.is_empty()); // clearing/sending is not Backspace
}

#[test]
fn focus_mouse_shortcuts_and_navigation_discard_deletion_provenance() {
    let at = Instant::now();
    for key in [37, 46, 162] {
        let mut h = History::default();
        commit(&mut h, "", "中", "中", at);
        input(&mut h, key, at);
        observe(&mut h, "中", at);
        input(&mut h, 8, at);
        observe(&mut h, "", at);
        assert!(h.removed.is_empty());
    }
    let mut h = History::default();
    commit(&mut h, "", "中", "中", at);
    h.input(&[0; 256], true, at);
    observe(&mut h, "中", at);
    input(&mut h, 8, at);
    observe(&mut h, "", at);
    assert!(h.removed.is_empty());
    commit(&mut h, "", "中", "中", at);
    h.observe(vec![2], &['中'], at);
    input(&mut h, 8, at);
    h.observe(vec![2], &[], at);
    assert!(h.removed.is_empty());
}

#[test]
fn delayed_events_bind_only_new_text_and_preserve_original_commit_date() {
    let at = Instant::now();
    let mut h = History::default();
    observe(&mut h, "中", at);
    let mut b = batch("中", at);
    b.date = b.date - Days::new(1);
    h.committed(&[1], &b); // callback precedes the provider's updated text
    input(&mut h, 65, at);
    observe(&mut h, "中中", at);
    input(&mut h, 8, at);
    observe(&mut h, "中", at);
    let removed = h.removed.pop_front().unwrap();
    assert_eq!(removed.date, b.date);
    assert_eq!(removed.at, b.at);
    assert_eq!(removed.counts, b.counts);
    observe(&mut h, "", at);
    assert!(h.removed.is_empty());
}

#[test]
fn sliding_context_never_subtracts_a_clipped_left_edge() {
    let at = Instant::now();
    let mut h = History::default();
    let prefix: String = (0..256)
        .map(|n| char::from_u32(0x4e00 + n).unwrap())
        .collect();
    commit(&mut h, "", &prefix, &prefix, at);
    input(&mut h, 65, at);
    let after: String = prefix.chars().skip(1).chain(['𠮷']).collect();
    observe(&mut h, &after, at);
    h.committed(&[1], &batch("𠮷", at));
    assert!(h.removed.is_empty());
    input(&mut h, 8, at);
    observe(&mut h, &prefix, at);
    assert_eq!(h.removed.pop_front().unwrap().counts, [('𠮷', 1)].into());
    assert!(h.removed.is_empty());
}

#[test]
fn a_delayed_commit_from_another_control_cannot_credit_the_current_input() {
    let at = Instant::now();
    let mut h = History::default();
    observe(&mut h, "", at);
    input(&mut h, 65, at);
    observe(&mut h, "中", at);
    h.committed(&[2], &batch("中", at));
    h.committed(&[], &batch("中", at));
    input(&mut h, 8, at);
    observe(&mut h, "", at);
    assert!(h.removed.is_empty());
}
