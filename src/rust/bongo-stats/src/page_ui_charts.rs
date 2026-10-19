use super::{act, tr, State};
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
pub(super) fn activity(p: &Page<'_>, data: &Snapshot, hourly: bool, week: bool) {
    p.section(&if hourly {
        tr(p, "hourly", "Today by hour")
    } else {
        tr(
            p,
            if week { "week" } else { "daily" },
            if week { "7 days" } else { "Last 30 days" },
        )
    });
    let metrics: Vec<_> = if hourly {
        data.hours.iter().collect()
    } else {
        data.days[if week { 23 } else { 0 }..]
            .iter()
            .map(|d| &d.metrics)
            .collect()
    };
    let labels: Vec<_> = (0..metrics.len())
        .map(|i| {
            if hourly {
                format!("{i:02}:00")
            } else {
                let day = data.days[i + if week { 23 } else { 0 }].date;
                format!("{:02}/{:02}", day / 100 % 100, day % 100)
            }
        })
        .collect();
    let series = [
        (
            tr(p, "keys", "Key presses"),
            metrics.iter().map(|m| m.keys as f64).collect::<Vec<_>>(),
            color::ACCENT,
        ),
        (
            tr(p, "left", "Left button"),
            metrics.iter().map(|m| m.buttons[0] as f64).collect(),
            color::LINK,
        ),
        (
            tr(p, "right", "Right button"),
            metrics.iter().map(|m| m.buttons[1] as f64).collect(),
            color::MUTED,
        ),
    ];
    p.row(1, 244.0);
    let Some((r, _)) = p.region() else {
        return;
    };
    p.fill(r, 14.0, color::SURFACE);
    p.stroke(r, 14.0, 1.0, color::BORDER_SUBTLE);
    let plot = Rect::new(r.x + 48.0, r.y + 16.0, (r.w - 64.0).max(1.0), r.h - 50.0);
    let maximum = series
        .iter()
        .flat_map(|(_, v, _)| v)
        .copied()
        .fold(1.0, f64::max);
    for i in 0..=4 {
        let y = plot.y + plot.h * i as f32 / 4.0;
        p.line(Rect::new(plot.x, y, plot.w, 0.0), 1.0, color::BORDER_SUBTLE);
        p.paint_text(
            Rect::new(r.x + 4.0, y - 9.0, 38.0, 18.0),
            &format!("{:.0}", maximum * (4 - i) as f64 / 4.0),
            1,
            color::MUTED,
            2,
        );
    }
    let step = plot.w / (metrics.len().saturating_sub(1)).max(1) as f32;
    for (_, values, role) in &series {
        for i in 1..values.len() {
            let y = plot.y + plot.h * (1.0 - values[i - 1] as f32 / maximum as f32);
            let next_y = plot.y + plot.h * (1.0 - values[i] as f32 / maximum as f32);
            p.line(
                Rect::new(plot.x + (i - 1) as f32 * step, y, step, next_y - y),
                2.0,
                *role,
            );
        }
        for (i, value) in values.iter().enumerate() {
            let x = plot.x + i as f32 * step;
            let y = plot.y + plot.h * (1.0 - *value as f32 / maximum as f32);
            p.circle(Rect::new(x - 2.5, y - 2.5, 5.0, 5.0), *role);
        }
    }
    for (i, label) in labels.iter().enumerate() {
        if i % if hourly {
            6
        } else if week {
            1
        } else {
            7
        } == 0
            || i + 1 == labels.len()
        {
            p.paint_text(
                Rect::new(
                    plot.x + i as f32 * step - 23.0,
                    plot.y + plot.h + 10.0,
                    46.0,
                    20.0,
                ),
                label,
                1,
                color::MUTED,
                1,
            );
        }
        let hit = Rect::new(plot.x + i as f32 * step - step / 2.0, plot.y, step, plot.h);
        if p.input(hit, 0) {
            p.line(
                Rect::new(plot.x + i as f32 * step, plot.y, 0.0, plot.h),
                1.0,
                color::ACCENT,
            );
            let m = metrics[i];
            p.tooltip(&format!(
                "{}\n{}: {}\n{}: {}\n{}: {}\n{}: {:.0}\n{}: {:.1}",
                label,
                tr(p, "keys", "Key presses"),
                m.keys,
                tr(p, "left", "Left button"),
                m.buttons[0],
                tr(p, "right", "Right button"),
                m.buttons[1],
                tr(p, "overlayMovement", "Pointer travel"),
                m.movement,
                tr(p, "overlayScroll", "Scroll"),
                m.scroll
            ));
        }
    }
    let legend_width = series.iter().fold(1.0_f32, |width, (name, _, _)| {
        width.max(p.measure(name, 1) + 13.0)
    });
    let columns = ((p.width() + p.spacing()) / (legend_width + p.spacing()))
        .floor()
        .max(1.0) as u32;
    p.row(columns.min(series.len() as u32), 24.0);
    for (name, _, role) in &series {
        if let Some((r, _)) = p.region() {
            p.circle(Rect::new(r.x, r.y + 8.0, 7.0, 7.0), *role);
            p.paint_text(
                Rect::new(r.x + 13.0, r.y, r.w - 13.0, r.h),
                name,
                1,
                color::MUTED,
                0,
            );
        }
    }
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
    p.images_begin();
    let maximum = data.apps[..count]
        .iter()
        .map(|entry| entry.milliseconds)
        .max()
        .unwrap_or(1)
        .max(1);
    for (i, entry) in data.apps[..count].iter().enumerate() {
        p.row(1, 78.0);
        let Some((r, interactive)) = p.region() else {
            continue;
        };
        let id = text(&entry.id);
        let selected = state.selected_app == id;
        let hovered = p.input(r, 0);
        p.fill(
            r,
            12.0,
            if selected {
                color::SELECTION
            } else if hovered {
                color::HOVER
            } else {
                color::SURFACE
            },
        );
        p.stroke(
            r,
            12.0,
            1.0,
            if selected {
                color::ACCENT
            } else {
                color::BORDER_SUBTLE
            },
        );
        if interactive && p.input(r, 1) {
            state.selected_app = id.to_owned();
            p.invalidate();
        }
        p.input(r, 3);
        let icon = Rect::new(r.x + 14.0, r.y + 17.0, 40.0, 40.0);
        let path = unsafe { p.read::<[u8; 1024]>(read::IMAGE, (state.period << 16) | i as u32) };
        let drawn = path.is_some_and(|path| p.image(text(&path), icon));
        if !drawn {
            p.fill(icon, 10.0, color::SELECTION);
            p.paint_text(
                icon,
                &names[i].chars().next().unwrap_or('?').to_string(),
                2,
                color::ACCENT,
                1,
            );
        }
        let x = r.x + 68.0;
        let number = duration(entry.milliseconds);
        let number_width = p.measure(&number, 1).max(70.0);
        p.paint_text(
            Rect::new(x, r.y + 12.0, (r.w - 96.0 - number_width).max(1.0), 24.0),
            names[i],
            2,
            color::TEXT,
            0,
        );
        p.paint_text(
            Rect::new(
                r.x + r.w - number_width - 14.0,
                r.y + 12.0,
                number_width,
                24.0,
            ),
            &number,
            1,
            color::MUTED,
            2,
        );
        let bar = Rect::new(x, r.y + 46.0, (r.w - 84.0).max(1.0), 8.0);
        p.fill(bar, 4.0, color::FIELD);
        p.fill(
            Rect::new(
                bar.x,
                bar.y,
                bar.w * (entry.milliseconds as f64 / maximum as f64) as f32,
                bar.h,
            ),
            4.0,
            color::ACCENT,
        );
        if hovered {
            p.tooltip(&format!(
                "{}\n{}  ({:.1}%)",
                names[i],
                number,
                100.0 * entry.milliseconds as f64 / data.total_ms.max(1) as f64
            ));
        }
    }
    p.images_end();
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
