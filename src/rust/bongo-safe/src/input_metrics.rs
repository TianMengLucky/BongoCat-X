use super::{get, Event, Host};
use std::sync::{
    atomic::{AtomicBool, AtomicU64, Ordering},
    Mutex,
};
#[repr(C)]
pub struct Snapshot {
    keys: u64,
    buttons: [u64; 5],
    movement: f64,
    scroll: f64,
}
pub(super) struct Metrics {
    enabled: AtomicBool,
    reset: AtomicBool,
    keys: AtomicU64,
    buttons: [AtomicU64; 5],
    movement: AtomicU64,
    scroll: AtomicU64,
    pointer: Mutex<Option<(f64, f64)>>,
}
impl Metrics {
    pub(super) fn new() -> Self {
        Self {
            enabled: AtomicBool::new(false),
            reset: AtomicBool::new(true),
            keys: AtomicU64::new(0),
            buttons: std::array::from_fn(|_| AtomicU64::new(0)),
            movement: AtomicU64::new(0),
            scroll: AtomicU64::new(0),
            pointer: Mutex::new(None),
        }
    }
    fn collecting(&self) -> bool {
        self.enabled.load(Ordering::Acquire)
    }
    pub(super) fn event(&self, event: &Event) {
        if !self.collecting() {
            return;
        }
        if event.kind == 1 {
            self.keys.fetch_add(1, Ordering::Relaxed);
        } else if event.kind == 3 {
            let names: [&[u8]; 5] = [b"Left", b"Right", b"Middle", b"Back", b"Forward"];
            if let Some(i) = names.iter().position(|name| *name == event.name()) {
                self.buttons[i].fetch_add(1, Ordering::Relaxed);
            }
        }
    }
    fn add(&self, counter: &AtomicU64, value: f64) {
        if self.collecting() && value.is_finite() && value > 0.0 && value < u64::MAX as f64 / 1024.0
        {
            counter.fetch_add((value * 1024.0 + 0.5) as u64, Ordering::Relaxed);
        }
    }
    pub(super) fn pointer(&self, x: f64, y: f64) {
        if !x.is_finite() || !y.is_finite() {
            return;
        }
        let Ok(mut previous) = self.pointer.try_lock() else {
            return;
        };
        if self.reset.swap(false, Ordering::Relaxed) {
            *previous = None;
        }
        if let Some((px, py)) = *previous {
            self.add(&self.movement, (x - px).hypot(y - py));
        }
        *previous = self.collecting().then_some((x, y));
    }
}
#[no_mangle]
pub unsafe extern "C" fn bongo_cat_input_metrics_enabled(host: *const Host) -> bool {
    crate::guard(false, || get(host).is_some_and(|i| i.metrics.collecting()))
}
#[no_mangle]
pub unsafe extern "C" fn bongo_cat_input_metrics_enable(host: *const Host, enabled: bool) {
    crate::guard((), || {
        if let Some(i) = get(host) {
            if enabled {
                i.metrics.reset.store(true, Ordering::Relaxed);
            }
            i.metrics.enabled.store(enabled, Ordering::Release);
        }
    });
}
#[no_mangle]
pub unsafe extern "C" fn bongo_cat_input_metrics_event(host: *const Host, event: *const Event) {
    crate::guard((), || {
        if let (Some(i), Some(e)) = (get(host), event.as_ref()) {
            i.metrics.event(e);
        }
    });
}
#[no_mangle]
pub unsafe extern "C" fn bongo_cat_input_metrics_pointer(host: *const Host, x: f64, y: f64) {
    crate::guard((), || {
        if let Some(i) = get(host) {
            i.metrics.pointer(x, y);
        }
    });
}
#[no_mangle]
pub unsafe extern "C" fn bongo_cat_input_metrics_motion(host: *const Host, x: f64, y: f64) {
    crate::guard((), || {
        if let Some(i) = get(host) {
            i.metrics.add(&i.metrics.movement, x.hypot(y));
        }
    });
}
#[no_mangle]
pub unsafe extern "C" fn bongo_cat_input_metrics_scroll(host: *const Host, x: f64, y: f64) {
    crate::guard((), || {
        if let Some(i) = get(host) {
            i.metrics.add(&i.metrics.scroll, x.abs() + y.abs());
        }
    });
}
#[no_mangle]
pub unsafe extern "C" fn bongo_cat_input_metrics_take(host: *const Host, output: *mut Snapshot) {
    crate::guard((), || {
        if let (Some(i), Some(out)) = (get(host), output.as_mut()) {
            let m = &i.metrics;
            out.keys = m.keys.swap(0, Ordering::Relaxed);
            out.buttons = std::array::from_fn(|n| m.buttons[n].swap(0, Ordering::Relaxed));
            out.movement = m.movement.swap(0, Ordering::Relaxed) as f64 / 1024.0;
            out.scroll = m.scroll.swap(0, Ordering::Relaxed) as f64 / 1024.0;
        }
    });
}
#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn a_busy_pointer_producer_does_not_block_other_metrics() {
        let metrics = Metrics::new();
        metrics.enabled.store(true, Ordering::Release);
        let _held = metrics.pointer.lock().unwrap();
        metrics.pointer(3.0, 4.0);
        metrics.event(&Event {
            kind: 1,
            ..Event::empty()
        });
        assert_eq!(metrics.keys.swap(0, Ordering::Relaxed), 1);
    }
}
