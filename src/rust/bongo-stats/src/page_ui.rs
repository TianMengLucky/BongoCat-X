use crate::metrics::Snapshot;
use bongo_plugin::page::{action, color, read, Page, PageState, Rect};

#[path = "page_ui_charts.rs"]
mod charts;
#[path = "page_ui_keyboard.rs"]
mod keyboard;
#[path = "page_ui_units.rs"]
mod units;
#[path = "page_ui_words.rs"]
mod words;
#[derive(Default)]
struct State {
    period: u32,
    history_days: u32,
    selected_app: String,
    reset_confirm: bool,
    units_open: u32,
    units: Option<units::Units>,
    draft: units::Units,
    glyph_hash: u64,
    unit_glyph_hash: u64,
    word_glyph_hash: u64,
    word_cache: Option<crate::words::Snapshot>,
    word_period: u32,
}
fn tr(p: &Page<'_>, key: &str, fallback: &str) -> String {
    p.tr(&format!("pages.preference.statistics.{key}"), fallback)
}
fn act(p: &Page<'_>, op: u32, index: u32) -> bool {
    let ok = p.act(op, index);
    if !ok {
        p.notice(&tr(p, "failed", "Unable to update statistics"), true);
    }
    ok
}
fn grouped(raw: &str, unit: &str) -> String {
    let whole = raw.find('.').unwrap_or(raw.len());
    let mut result = String::with_capacity(raw.len() + unit.len() + whole / 3 + 1);
    for (i, c) in raw.chars().enumerate() {
        if i != 0 && i < whole && (whole - i) % 3 == 0 {
            result.push(',');
        }
        result.push(c);
    }
    result.push(' ');
    result.push_str(unit);
    result
}
fn scaled(amount: f64, scale: f64, precision: usize) -> String {
    let result = format!(
        "{:.*}",
        if scale == 1.0 { precision } else { 2 },
        amount * scale
    );
    if scale == 1.0 || !result.contains('.') {
        result
    } else {
        result
            .trim_end_matches('0')
            .trim_end_matches('.')
            .to_owned()
    }
}
fn metric_cards(p: &Page<'_>, id: &str, cards: &[(String, String)]) {
    let needed = cards.iter().fold(180.0_f32, |width, (title, number)| {
        width.max(p.measure(title, 1).max(p.measure(number, 0)) + 28.0)
    });
    if needed > p.width() {
        // Reuse host wrapping for values or translated titles wider than a whole card.
        for (index, (title, number)) in cards.iter().enumerate() {
            p.status_row(&format!("{id}-{index}"), title, number);
        }
        return;
    }
    let columns = ((p.width() + p.spacing()) / (needed + p.spacing()))
        .floor()
        .max(1.0) as u32;
    p.row(columns.min(cards.len().max(1) as u32), 104.0);
    for (title, number) in cards {
        metric_card(p, title, number);
    }
}
fn metric_card(p: &Page<'_>, title: &str, number: &str) {
    let Some((r, _)) = p.region() else {
        return;
    };
    p.fill(r, 12.0, color::SURFACE);
    p.stroke(r, 12.0, 1.0, color::BORDER_SUBTLE);
    let content = r.inset(14.0);
    p.paint_text(
        Rect::new(content.x, content.y, content.w, 24.0),
        title,
        1,
        color::TEXT,
        0,
    );
    let font = if p.measure(number, 3) <= content.w {
        3
    } else {
        0
    };
    p.paint_text(
        Rect::new(content.x, content.y + 28.0, content.w, 36.0),
        number,
        font,
        color::ACCENT,
        0,
    );
    if p.measure(number, font) > content.w && p.input(r, 0) {
        p.tooltip(number);
    }
}
impl State {
    fn reset(&mut self, p: &Page<'_>) {
        let status = p.status();
        if status.reset_available == 0 {
            self.reset_confirm = false;
            p.wrapped(
                &tr(
                    p,
                    "resetUnavailable",
                    "Update the Statistics plugin to enable reset.",
                ),
                40.0,
            );
            return;
        }
        if status.reset_busy != 0 {
            self.reset_confirm = false;
            p.label(&tr(p, "resetting", "Resetting statistics..."));
            return;
        }
        if self.reset_confirm {
            p.wrapped(&tr(p, "resetConfirm", "Clear all input counts, charts and application time history? This cannot be undone. Settings are kept."), 56.0);
            p.button_row(&[
                tr(p, "resetCancel", "Cancel"),
                tr(p, "resetConfirmButton", "Clear all history"),
            ]);
            if p.button("reset-cancel", &tr(p, "resetCancel", "Cancel"), 4) {
                self.reset_confirm = false;
            }
            if p.button(
                "reset-confirm",
                &tr(p, "resetConfirmButton", "Clear all history"),
                1,
            ) {
                act(p, action::RESET_STATS, 0);
                self.reset_confirm = false;
            }
        } else {
            p.row(1, 36.0);
            if p.button("reset", &tr(p, "reset", "Reset statistics"), 1) {
                self.reset_confirm = true;
                p.invalidate();
            }
        }
    }
}
impl PageState for State {
    fn draw(&mut self, p: &Page<'_>) {
        if self.units.is_none() {
            if let Some(units) = unsafe { p.read::<units::Units>(read::UNITS, 0) } {
                self.draft = units;
                self.draft.fill_defaults(p);
                self.units = Some(units);
            }
        }
        p.intro(&tr(p, "privacy", "Local input totals, app names/durations and Han character frequencies. No sentences, input order, coordinates, window titles, URLs or screenshots are stored."));
        let section = unsafe { p.read::<u32>(read::SECTION, 0) };
        let applications = section == Some(1);
        let mut overlay = p.status().overlay;
        if p.toggle(
            "statistics-overlay",
            &tr(p, "showOverlay", "Show statistics overlay"),
            &mut overlay,
        ) {
            act(p, action::OVERLAY, overlay);
        }
        let Some(data) = (unsafe { p.read::<Snapshot>(read::SNAPSHOT, 0) }) else {
            p.wrapped(&tr(p, "inactive", "Install or enable the Statistics plugin on the Plugins page to start counting."), 64.0);
            return;
        };
        if data.storage_error != 0 {
            p.wrapped(&tr(p, "storageError", "Statistics could not be saved. Check disk space and data directory permissions."), 48.0);
        }
        self.reset(p);
        p.row(1, 40.0);
        self.period = p
            .segments(
                "statistics-period",
                &[
                    tr(p, "today", "Today"),
                    tr(p, "week", "7 days"),
                    tr(p, "month", "30 days"),
                    tr(p, "lifetime", "All time"),
                ],
                self.period,
            )
            .min(3);
        if section == Some(2) {
            words::draw(self, p);
            return;
        }
        if applications {
            charts::usage(self, p);
            return;
        }
        let metrics = [&data.today, &data.week, &data.month, &data.lifetime];
        let m = metrics[self.period as usize];
        let units = self.units.unwrap_or_default();
        let unit = |bytes: &[u8], key: &str, fallback: &str| {
            let label = bongo_plugin::page::text(bytes);
            if label.is_empty() {
                p.tr(key, fallback)
            } else {
                label.to_owned()
            }
        };
        let count_unit = unit(&units.count, "native.statsCountUnit", "counts");
        let movement_unit = unit(&units.movement, "native.statsInputUnit", "input units");
        let scroll_unit = unit(&units.scroll, "native.statsWheelUnit", "wheel units");
        metric_cards(
            p,
            "input-metric",
            &[
                (
                    tr(p, "keys", "Key presses"),
                    grouped(&m.keys.to_string(), &count_unit),
                ),
                (
                    tr(p, "left", "Left button"),
                    grouped(&m.buttons[0].to_string(), &count_unit),
                ),
                (
                    tr(p, "right", "Right button"),
                    grouped(&m.buttons[1].to_string(), &count_unit),
                ),
                (
                    tr(p, "overlayMovement", "Pointer travel"),
                    grouped(&scaled(m.movement, units.movement_scale, 0), &movement_unit),
                ),
                (
                    tr(p, "overlayScroll", "Scroll"),
                    grouped(&scaled(m.scroll, units.scroll_scale, 1), &scroll_unit),
                ),
            ],
        );
        keyboard::draw(self, p);
        p.row(1, 40.0);
        self.history_days = p
            .segments(
                "statistics-trend-period",
                &[tr(p, "week", "7 days"), tr(p, "month", "30 days")],
                self.history_days,
            )
            .min(1);
        charts::activity(p, &data, false, self.history_days == 0);
        charts::activity(p, &data, true, false);
        if section.is_none() {
            charts::usage(self, p);
        }
        for (i, (key, fallback)) in [
            ("middle", "Middle button"),
            ("back", "Back button"),
            ("forward", "Forward button"),
        ]
        .iter()
        .enumerate()
        {
            p.status_row(
                key,
                &tr(p, key, fallback),
                &grouped(&m.buttons[i + 2].to_string(), &count_unit),
            );
        }
        units::draw(self, p);
        p.wrapped(&tr(p, "unitsHint", "Movement uses desktop pixels/points, or raw device units with Linux evdev. Wheel units vary by device; neither value is a physical distance."), 96.0);
        p.button_row(&[
            tr(p, "export", "Export CSV"),
            tr(p, "openFolder", "Open data folder"),
        ]);
        if p.button("export", &tr(p, "export", "Export CSV"), 4) && act(p, action::EXPORT_STATS, 0)
        {
            p.notice(
                &tr(p, "exportQueued", "Export queued in the background"),
                false,
            );
        }
        if p.button("folder", &tr(p, "openFolder", "Open data folder"), 4) {
            act(p, action::OPEN_FOLDER, 0);
        }
    }
}
bongo_plugin::export_page!(bongo_cat_statistics_page_ui_query_v1, State);

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn display_units_keep_grouping_precision_and_raw_values() {
        assert_eq!(grouped("1234567.5", "units"), "1,234,567.5 units");
        assert_eq!(scaled(123.0, 0.01, 0), "1.23");
        assert_eq!(scaled(0.0, 0.01, 0), "0");
        assert_eq!(scaled(613238.47656, 0.001, 0), "613.24");
        assert_eq!(scaled(1200.0, 0.001, 0), "1.2");
        assert_eq!(scaled(12.0, 1.0, 1), "12.0");
    }
}
