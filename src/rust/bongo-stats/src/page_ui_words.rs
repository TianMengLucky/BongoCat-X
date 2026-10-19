use super::{metric_cards, tr, State};
use bongo_plugin::page::{action, read, Page};

fn duration(ms: u64) -> String {
    let seconds = ms / 1000;
    format!(
        "{:02}:{:02}:{:02}",
        seconds / 3600,
        seconds / 60 % 60,
        seconds % 60
    )
}

fn timer(p: &Page<'_>) {
    let Some(data) = (unsafe { p.read::<crate::words_timer::Snapshot>(read::TIMER, 0) }) else {
        return;
    };
    let mut enabled = data.enabled;
    if p.toggle(
        "statistics-timer",
        &tr(p, "timer", "Typing timer"),
        &mut enabled,
    ) {
        super::act(p, action::TIMER_ENABLE, enabled);
    }
    metric_cards(
        p,
        "timer-metric",
        &[
            (
                tr(p, "timerTyping", "Typing time"),
                duration(data.typing_ms),
            ),
            (tr(p, "timerIdle", "Idle time"), duration(data.idle_ms)),
        ],
    );
    p.wrapped(&tr(p, "timerHint", "Session timer: enabling starts/resumes; disabling pauses. The five seconds after each captured Han commit count as typing; all other enabled time counts as idle, including unobserved input. Restart clears the timer; Reset statistics clears its times and keeps its switch."), 96.0);
}

pub fn draw(state: &mut State, p: &Page<'_>) {
    if state.word_period != state.period {
        state.word_cache = None;
        state.word_period = state.period;
    }
    if let Some(data) = unsafe { p.read::<crate::words::Snapshot>(read::WORDS, state.period) } {
        state.word_cache = Some(data);
    }
    let Some(data) = state.word_cache else {
        p.wrapped(
            &tr(
                p,
                "wordsUnavailable",
                "Update the Statistics plugin to display character statistics.",
            ),
            56.0,
        );
        return;
    };
    let (key, fallback) = match data.status {
        0 => ("wordsUnsupported", "Character collection requires Windows UI Automation."),
        1 => ("wordsCoverage", "Collecting Han input. Compatibility mode detects candidates confirmed with Space, Enter or number keys. App accessibility support determines coverage."),
        2 => ("wordsFailed", "UI Automation could not start. Character collection is unavailable."),
        _ => ("wordsLost", "Some input events could not be collected. These totals may be incomplete."),
    };
    p.wrapped(&tr(p, key, fallback), 64.0);
    if let Some(mut enabled) = unsafe { p.read::<u32>(read::BACKSPACE, 0) } {
        if p.toggle("statistics-backspace", &tr(p, "subtractBackspace", "Subtract Han characters deleted with Backspace"), &mut enabled) {
            super::act(p, action::BACKSPACE_ENABLE, enabled);
        }
        p.wrapped(&tr(p, "subtractBackspaceHint", "Off by default; saved across restarts. Only subtracts observed, counted characters in the current input. Switching focus, shortcuts or mouse clicks stop tracking. Pinyin edits, pasted and older text are excluded. Totals, frequencies and speeds change; typing time does not."), 112.0);
    }
    timer(p);
    metric_cards(
        p,
        "words-metric",
        &[
            (
                tr(p, "wordsTotal", "Han characters"),
                data.total.to_string(),
            ),
            (
                tr(p, "wordsRecent", "Last 60 seconds (Han/min)"),
                data.recent.to_string(),
            ),
            (
                tr(p, "wordsAverage", "Session average (Han/min)"),
                format!("{:.1}", data.average),
            ),
        ],
    );
    p.wrapped(&tr(p, "wordsSpeedHint", "Speeds use this session, independent of the selected period. Average = Han characters / active minutes: each burst starts with one second; commit gaps over five seconds start a new burst. Punctuation, Latin letters and pasted text are excluded. Han characters from other languages may count."), 112.0);
    p.section(&tr(p, "wordsFrequent", "Frequently typed Han characters"));
    let top = &data.top[..(data.count as usize).min(crate::words::TOP)];
    let labels: Vec<_> = top
        .iter()
        .filter_map(|entry| char::from_u32(entry.codepoint).map(|c| c.to_string()))
        .collect();
    p.glyphs(
        &labels.iter().map(String::as_str).collect::<Vec<_>>(),
        &mut state.word_glyph_hash,
    );
    if top.is_empty() {
        p.wrapped(
            &tr(
                p,
                "wordsEmpty",
                "No Han character commits recorded in this period.",
            ),
            48.0,
        );
    }
    for (index, entry) in top.iter().enumerate() {
        if let Some(c) = char::from_u32(entry.codepoint) {
            p.status_row(
                &format!("han-{index}"),
                &c.to_string(),
                &entry.count.to_string(),
            );
        }
    }
}
