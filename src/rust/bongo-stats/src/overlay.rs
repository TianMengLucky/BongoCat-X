use bongo_plugin::overlay::{Api, Canvas, Visitor};
use bongo_plugin::page::{color, text, Rect};
use std::ffi::c_void;
pub struct State {
    page: u32,
    pressed: Option<u32>,
    metrics: crate::metrics::Metrics,
    apps: crate::usage::Snapshot,
}
impl Default for State {
    fn default() -> Self {
        Self {
            page: 0,
            pressed: None,
            metrics: Default::default(),
            apps: Default::default(),
        }
    }
}
fn tr(c: &Canvas, key: &str, fallback: &str) -> String {
    c.tr(&format!("pages.preference.statistics.{key}"), fallback)
}
fn inputs(state: &State, c: &Canvas) {
    let m = &state.metrics;
    let names = [
        tr(c, "keys", "Key presses"),
        tr(c, "clicks", "Mouse clicks"),
        tr(c, "overlayMovement", "Pointer travel"),
        tr(c, "overlayScroll", "Scroll"),
    ];
    let numbers = [
        m.keys.to_string(),
        m.buttons
            .iter()
            .fold(0u64, |a, &b| a.saturating_add(b))
            .to_string(),
        format!("{:.0}", m.movement),
        format!("{:.1}", m.scroll),
    ];
    for (i, (name, number)) in names.iter().zip(numbers.iter()).enumerate() {
        let r = Rect::new(
            14.0 + (i % 2) as f32 * 180.0,
            44.0 + (i / 2) as f32 * 84.0,
            172.0,
            76.0,
        );
        c.fill(r, 12.0, color::FIELD);
        c.stroke(r, 12.0, 1.0, color::BORDER_SUBTLE);
        c.text(
            Rect::new(r.x + 12.0, r.y + 8.0, r.w - 24.0, 20.0),
            name,
            1,
            color::MUTED,
            0,
        );
        let font = if c.measure(number, 3) > r.w - 24.0 {
            0
        } else {
            3
        };
        c.text(
            Rect::new(r.x + 12.0, r.y + 30.0, r.w - 24.0, 34.0),
            number,
            font,
            color::ACCENT,
            0,
        );
    }
}
fn applications(state: &State, c: &Canvas) {
    let snapshot = &state.apps;
    let count = (snapshot.count as usize).min(3);
    if count == 0 {
        let label = if snapshot.enabled != 0 {
            tr(
                c,
                "usageEmpty",
                "No application usage recorded for this period yet.",
            )
        } else {
            tr(c, "trackApplications", "Track foreground application time")
        };
        c.text(
            Rect::new(20.0, 65.0, 340.0, 112.0),
            &label,
            0,
            color::MUTED,
            0,
        );
        return;
    }
    let maximum = snapshot.apps[0].milliseconds.max(1);
    for (i, entry) in snapshot.apps[..count].iter().enumerate() {
        let r = Rect::new(14.0, 44.0 + i as f32 * 56.0, 352.0, 50.0);
        c.fill(r, 10.0, color::FIELD);
        let icon = Rect::new(r.x + 8.0, r.y + 7.0, 36.0, 36.0);
        if !c
            .resource("application-icons", text(&entry.id), ".bmp")
            .is_some_and(|p| c.image(i as u32, &p, icon))
        {
            c.fill(icon, 9.0, color::SELECTION);
            c.stroke(
                Rect::new(icon.x + 9.0, icon.y + 10.0, 18.0, 16.0),
                3.0,
                2.0,
                color::ACCENT,
            );
        }
        let seconds = entry.milliseconds / 1000;
        let number = format!(
            "{}:{:02}:{:02}",
            seconds / 3600,
            seconds / 60 % 60,
            seconds % 60
        );
        let width = c.measure(&number, 1);
        c.text(
            Rect::new(r.x + 54.0, r.y + 5.0, (278.0 - width - 10.0).max(1.0), 22.0),
            text(&entry.name),
            0,
            color::TEXT,
            0,
        );
        c.text(
            Rect::new(r.x + 54.0, r.y + 5.0, 278.0, 22.0),
            &number,
            1,
            color::MUTED,
            2,
        );
        let mut bar = Rect::new(r.x + 54.0, r.y + 33.0, 278.0, 6.0);
        c.fill(bar, 3.0, color::BORDER_SUBTLE);
        bar.w *= (entry.milliseconds as f64 / maximum as f64) as f32;
        c.fill(bar, 3.0, color::ACCENT);
    }
}
fn footer(state: &State, c: &Canvas) {
    c.line(
        Rect::new(14.0, 218.0, 352.0, 0.0),
        1.0,
        color::BORDER_SUBTLE,
    );
    let names = [
        tr(c, "input", "Keyboard and mouse"),
        tr(c, "applications", "Application usage"),
    ];
    for (i, name) in names.iter().enumerate() {
        let mut r = Rect::new(14.0 + i as f32 * 180.0, 226.0, 172.0, 28.0);
        let selected = state.page == i as u32;
        if selected {
            c.fill(r, 8.0, color::SELECTION);
        }
        let width = c.measure(name, 1);
        r.x += ((r.w - width) / 2.0).max(0.0);
        r.w = r.w.min(width + 1.0);
        c.text(
            r,
            name,
            1,
            if selected {
                color::ACCENT
            } else {
                color::MUTED
            },
            0,
        );
    }
}
unsafe extern "C" fn draw(instance: *mut c_void, canvas: *const Canvas) -> u32 {
    crate::guarded(0, || {
        let (Some(instance), Some(c)) = (
            instance.cast::<crate::Instance>().as_ref(),
            Canvas::get(canvas),
        ) else {
            return 0;
        };
        let Ok(mut state) = instance.overlay.try_lock() else {
            return 0;
        };
        if let Ok(cache) = instance.cache.try_lock() {
            state.metrics = cache.metrics.today;
            state.apps = cache.usage[0].clone();
        }
        // The window shell clears transparently; the plugin chooses its panel.
        let background = (c.color(color::SURFACE) & 0x00ffffff) | (238 << 24);
        (c.rect)(
            c.context,
            Rect::new(0.0, 0.0, 380.0, 264.0),
            18.0,
            0.0,
            background,
        );
        c.stroke(
            Rect::new(0.5, 0.5, 379.0, 263.0),
            18.0,
            1.0,
            color::BORDER_SUBTLE,
        );
        let title = if state.page != 0 {
            tr(c, "applications", "Application usage")
        } else {
            tr(c, "input", "Keyboard and mouse")
        };
        c.text(
            Rect::new(16.0, 10.0, 280.0, 26.0),
            &title,
            2,
            color::TEXT,
            0,
        );
        c.text(
            Rect::new(294.0, 10.0, 70.0, 26.0),
            &tr(c, "today", "Today"),
            1,
            color::MUTED,
            2,
        );
        if state.page != 0 {
            applications(&state, c);
        } else {
            inputs(&state, c);
        }
        footer(&state, c);
        1
    })
}
fn target(x: f32, y: f32) -> Option<u32> {
    ((14.0..366.0).contains(&x) && (226.0..254.0).contains(&y)).then_some(u32::from(x >= 194.0))
}
unsafe extern "C" fn pointer(instance: *mut c_void, x: f32, y: f32, down: u32) {
    crate::guarded((), || {
        let Some(instance) = instance.cast::<crate::Instance>().as_ref() else {
            return;
        };
        let Ok(mut state) = instance.overlay.try_lock() else {
            return;
        };
        let page = target(x, y);
        if down != 0 {
            state.pressed = page;
        } else {
            if page.is_some() && page == state.pressed {
                state.page = page.unwrap_or(0);
            }
            state.pressed = None;
        }
    });
}
unsafe extern "C" fn visit_text(
    instance: *mut c_void,
    visitor: Option<Visitor>,
    context: *mut c_void,
) -> u32 {
    crate::guarded(0, || {
        let (Some(instance), Some(visitor)) =
            (instance.cast::<crate::Instance>().as_ref(), visitor)
        else {
            return 0;
        };
        let Ok(mut state) = instance.overlay.try_lock() else {
            return 0;
        };
        if let Ok(cache) = instance.cache.try_lock() {
            state.apps = cache.usage[0].clone();
        }
        for entry in state.apps.apps.iter().take(state.apps.count as usize) {
            bongo_plugin::overlay::visit(visitor, context, text(&entry.name));
        }
        state.page
    })
}
static API: Api = Api {
    abi_version: 1,
    struct_size: std::mem::size_of::<Api>() as u32,
    width: 380,
    height: 264,
    title_key: c"pages.preference.statistics.title".as_ptr(),
    title: c"Statistics".as_ptr(),
    draw,
    pointer,
    visit_text,
};
#[no_mangle]
pub extern "C" fn bongo_cat_statistics_overlay_query_v1(abi: u32) -> *const Api {
    if abi == 1 {
        &API
    } else {
        std::ptr::null()
    }
}
#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn footer_targets_match_bounds() {
        assert_eq!(target(20.0, 230.0), Some(0));
        assert_eq!(target(200.0, 230.0), Some(1));
        assert_eq!(target(200.0, 100.0), None);
        assert_eq!(target(366.0, 230.0), None);
    }
}
