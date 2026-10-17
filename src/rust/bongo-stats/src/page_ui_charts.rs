use super::{act, clicks, tr, State};
use crate::{metrics::Snapshot, usage};
use bongo_plugin::page::{action, color, read, text, Page, Rect};

fn duration(ms: u64) -> String {
    let seconds = ms / 1000;
    format!(
        "{}:{:02}:{:02}",
        seconds / 3600,
        seconds / 60 % 60,
        seconds % 60
    )
}
fn bars(
    p: &Page<'_>,
    values: &[f64],
    labels: &[String],
    tooltips: &[String],
    height: f32,
    interval: usize,
) {
    p.row(1, height);
    let Some((r, _)) = p.region() else {
        return;
    };
    if values.is_empty() {
        return;
    }
    p.fill(r, 12.0, color::SURFACE);
    p.stroke(r, 12.0, 1.0, color::BORDER_SUBTLE);
    let maximum = values.iter().copied().fold(1.0f64, f64::max);
    let width = (r.w - 16.0) / values.len() as f32;
    let bottom = r.y + r.h - 28.0;
    for (i, value) in values.iter().enumerate() {
        let h = (*value / maximum) as f32 * (r.h - 44.0);
        let bar = Rect::new(
            r.x + 8.0 + i as f32 * width,
            bottom - h,
            (width - 3.0).max(1.0),
            h,
        );
        if h > 0.0 {
            p.fill(
                bar,
                2.0,
                if i + 1 == values.len() {
                    color::ACCENT_HOVER
                } else {
                    color::ACCENT
                },
            );
        }
        if i % interval == 0 {
            p.paint_text(
                Rect::new(bar.x, bottom + 5.0, width * interval as f32, 20.0),
                &labels[i],
                0,
                color::MUTED,
                0,
            );
        }
        if p.input(Rect::new(bar.x, r.y, width, r.h), 0) {
            p.tooltip(&tooltips[i]);
        }
    }
}
pub(super) fn activity(p: &Page<'_>, data: &Snapshot, hourly: bool) {
    p.section(&if hourly {
        tr(p, "hourly", "Today by hour")
    } else {
        tr(p, "daily", "Last 30 days")
    });
    let metrics: Vec<_> = if hourly {
        data.hours.iter().collect()
    } else {
        data.days.iter().map(|d| &d.metrics).collect()
    };
    let values: Vec<_> = metrics
        .iter()
        .map(|m| m.keys as f64 + clicks(m) as f64)
        .collect();
    let labels: Vec<_> = (0..metrics.len())
        .map(|i| {
            if hourly {
                format!("{i:02}")
            } else {
                format!(
                    "{:02}/{:02}",
                    data.days[i].date / 100 % 100,
                    data.days[i].date % 100
                )
            }
        })
        .collect();
    let tooltips: Vec<_> = metrics
        .iter()
        .enumerate()
        .map(|(i, m)| {
            format!(
                "{}: {} {} / {} {}",
                labels[i],
                tr(p, "keys", "Key presses"),
                m.keys,
                tr(p, "clicks", "Mouse clicks"),
                clicks(m)
            )
        })
        .collect();
    bars(
        p,
        &values,
        &labels,
        &tooltips,
        160.0,
        if hourly { 6 } else { 7 },
    );
    p.wrapped(
        &tr(p, "activity", "Bar height: key presses + mouse clicks"),
        42.0,
    );
}
pub(super) fn usage(state: &mut State, p: &Page<'_>) {
    let Some(data) = (unsafe { p.read::<usage::Snapshot>(read::USAGE, state.period) }) else {
        return;
    };
    p.section(&tr(p, "applications", "Application usage"));
    let mut enabled = data.enabled;
    if p.toggle(
        "application-statistics-enabled",
        &tr(p, "trackApplications", "Track foreground application time"),
        &mut enabled,
    ) {
        act(p, action::USAGE_ENABLE, enabled);
    }
    p.wrapped(&tr(p, "usageHint", "Samples each second; pauses after 5 idle minutes, on lock or sleep. App names and durations stay local."), 54.0);
    if data.enabled != 0 && data.available == 0 {
        p.wrapped(&tr(p, "usageUnavailable", "Foreground app tracking is unavailable in this desktop session. Wayland is not supported; X11 requires xcb and xcb-screensaver."), 54.0);
    }
    p.status_row(
        "application-duration",
        &tr(p, "usageTotal", "Total active time (h:mm:ss)"),
        &duration(data.total_ms),
    );
    let count = (data.count as usize).min(data.apps.len());
    if count == 0 {
        p.wrapped(
            &tr(
                p,
                "usageEmpty",
                "No application usage recorded for this period yet.",
            ),
            38.0,
        );
    }
    let names: Vec<_> = data.apps[..count].iter().map(|a| text(&a.name)).collect();
    p.glyphs(&names, &mut state.glyph_hash);
    for (i, entry) in data.apps[..count].iter().enumerate() {
        p.status_row(
            &format!("application-{i}"),
            names[i],
            &duration(entry.milliseconds),
        );
        p.row(1, 8.0);
        p.progress(if data.total_ms == 0 {
            0.0
        } else {
            (entry.milliseconds as f64 / data.total_ms as f64) as f32
        });
    }
    p.label(&tr(
        p,
        "usageHourly",
        "Today's application time by hour (minutes)",
    ));
    let values: Vec<_> = data.hours.iter().map(|&ms| ms as f64 / 60000.0).collect();
    let labels: Vec<_> = (0..24).map(|i| format!("{i:02}:00")).collect();
    let tooltips: Vec<_> = data
        .hours
        .iter()
        .enumerate()
        .map(|(i, &ms)| format!("{i:02}:00  {}", duration(ms)))
        .collect();
    bars(p, &values, &labels, &tooltips, 140.0, 6);
    p.row(1, 36.0);
    if p.button(
        "usage-export",
        &tr(p, "usageExport", "Export application CSV"),
        0,
    ) && act(p, action::EXPORT_USAGE, 0)
    {
        p.notice(
            &tr(p, "exportQueued", "Export queued in the background"),
            false,
        );
    }
}
