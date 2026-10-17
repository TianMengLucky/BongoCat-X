//! Input queues and their invariants are owned by Rust; platform hooks only copy events.
use std::{collections::VecDeque, ffi::c_void, sync::Mutex};
#[path = "input_metrics.rs"]
mod metrics;
use metrics::Metrics;
const QUEUE_CAP: usize = 255;
const RECOVERY_CAP: usize = 64;
#[repr(C)]
#[derive(Clone, Copy)]
pub struct Event {
    kind: i32,
    timestamp: u64,
    name: [u8; 128],
    value: f32,
    x: f64,
    y: f64,
    sequence: u64,
}
impl Event {
    fn empty() -> Self {
        Self {
            kind: 0,
            timestamp: 0,
            name: [0; 128],
            value: 0.0,
            x: 0.0,
            y: 0.0,
            sequence: 0,
        }
    }
    fn name(&self) -> &[u8] {
        &self.name[..self
            .name
            .iter()
            .position(|b| *b == 0)
            .unwrap_or(self.name.len())]
    }
}
struct Queue {
    normal: VecDeque<Event>,
    recovery: VecDeque<Event>,
    next: u64,
    dropped: u64,
    control: u8,
    shift: u8,
    releases: Vec<(u64, [u8; 128])>,
}
impl Queue {
    fn new() -> Self {
        Self {
            normal: VecDeque::with_capacity(QUEUE_CAP),
            recovery: VecDeque::with_capacity(RECOVERY_CAP),
            next: 0,
            dropped: 0,
            control: 0,
            shift: 0,
            releases: Vec::with_capacity(64),
        }
    }
    fn push(&mut self, mut event: Event) -> bool {
        if matches!(event.kind, 1 | 2) {
            let (mask, value) = match event.name() {
                b"ControlLeft" => (1, &mut self.control),
                b"ControlRight" => (2, &mut self.control),
                b"ShiftLeft" => (1, &mut self.shift),
                b"ShiftRight" => (2, &mut self.shift),
                _ => (0, &mut self.control),
            };
            if event.kind == 1 {
                *value |= mask;
            } else {
                *value &= !mask;
            }
        }
        event.sequence = self.next;
        self.next = self.next.wrapping_add(1);
        if self.normal.len() < QUEUE_CAP {
            self.normal.push_back(event);
            return true;
        }
        if matches!(event.kind, 2 | 4) && self.recovery.len() < RECOVERY_CAP {
            self.recovery.push_back(event);
            return true;
        }
        self.dropped = self.dropped.wrapping_add(1);
        false
    }
    fn pop(&mut self) -> Option<Event> {
        if self
            .recovery
            .front()
            .is_some_and(|r| self.normal.front().is_none_or(|n| r.sequence < n.sequence))
        {
            self.recovery.pop_front()
        } else {
            self.normal.pop_front()
        }
    }
}
pub struct Input {
    queue: Mutex<Queue>,
    mouse: Mutex<Option<(f64, f64)>>,
    metrics: Metrics,
}
#[repr(C)]
pub struct Host {
    core: *mut Input,
}
unsafe fn get<'a>(host: *const Host) -> Option<&'a Input> {
    host.as_ref()?.core.as_ref()
}
#[no_mangle]
pub extern "C" fn bongo_safe_input_new(event_size: usize, metrics_size: usize) -> *mut c_void {
    crate::guard(std::ptr::null_mut(), || {
        if event_size != std::mem::size_of::<Event>()
            || metrics_size != std::mem::size_of::<metrics::Snapshot>()
        {
            return std::ptr::null_mut();
        }
        Box::into_raw(Box::new(Input {
            queue: Mutex::new(Queue::new()),
            mouse: Mutex::new(None),
            metrics: Metrics::new(),
        }))
        .cast()
    })
}
#[no_mangle]
pub unsafe extern "C" fn bongo_cat_input_destroy(host: *mut Host) {
    crate::guard((), || {
        if let Some(host) = host.as_mut() {
            if !host.core.is_null() {
                drop(Box::from_raw(host.core));
                host.core = std::ptr::null_mut();
            }
        }
    });
}
#[no_mangle]
pub unsafe extern "C" fn bongo_cat_input_edge(states: *mut bool, code: u32, down: bool) -> bool {
    crate::guard(true, || {
        if states.is_null() || code >= 256 {
            return true;
        }
        let state = &mut *states.add(code as usize);
        let changed = *state != down;
        *state = down;
        changed
    })
}
#[no_mangle]
pub unsafe extern "C" fn bongo_cat_input_push(host: *const Host, event: *const Event) -> bool {
    crate::guard(false, || {
        let (Some(input), Some(event)) = (get(host), event.as_ref()) else {
            return false;
        };
        if !(0..=7).contains(&event.kind) || !event.name.contains(&0) {
            return false;
        }
        input.metrics.event(event);
        input.queue.lock().is_ok_and(|mut q| q.push(*event))
    })
}
#[no_mangle]
pub unsafe extern "C" fn bongo_cat_input_pop(host: *const Host, event: *mut Event) -> bool {
    crate::guard(false, || {
        let (Some(input), Some(event)) = (get(host), event.as_mut()) else {
            return false;
        };
        if let Some(next) = input.queue.lock().ok().and_then(|mut q| q.pop()) {
            *event = next;
            true
        } else {
            false
        }
    })
}
#[no_mangle]
pub unsafe extern "C" fn bongo_cat_input_mouse(host: *const Host, x: f64, y: f64) -> bool {
    crate::guard(false, || {
        let Some(input) = get(host) else {
            return false;
        };
        input.metrics.pointer(x, y);
        input
            .mouse
            .lock()
            .is_ok_and(|mut mouse| mouse.replace((x, y)).is_none())
    })
}
#[no_mangle]
pub unsafe extern "C" fn bongo_cat_input_take_mouse(
    host: *const Host,
    x: *mut f64,
    y: *mut f64,
) -> bool {
    crate::guard(false, || {
        let (Some(input), Some(x), Some(y)) = (get(host), x.as_mut(), y.as_mut()) else {
            return false;
        };
        if let Some((px, py)) = input.mouse.lock().ok().and_then(|mut m| m.take()) {
            *x = px;
            *y = py;
            true
        } else {
            false
        }
    })
}
macro_rules! query {
    ($name:ident, $result:ty, $fallback:expr, $field:ident) => {
        #[no_mangle]
        pub unsafe extern "C" fn $name(host: *const Host) -> $result {
            crate::guard($fallback, || {
                get(host)
                    .and_then(|i| i.queue.lock().ok().map(|q| q.$field))
                    .unwrap_or($fallback)
            })
        }
    };
}
query!(bongo_safe_input_control, u8, 0, control);
query!(bongo_safe_input_shift, u8, 0, shift);
query!(bongo_cat_input_dropped, u64, 0, dropped);
#[no_mangle]
pub unsafe extern "C" fn bongo_cat_input_control_down(host: *const Host) -> bool {
    bongo_safe_input_control(host) != 0
}
#[no_mangle]
pub unsafe extern "C" fn bongo_cat_input_shift_down(host: *const Host) -> bool {
    bongo_safe_input_shift(host) != 0
}
#[no_mangle]
pub unsafe extern "C" fn bongo_cat_input_schedule_release(
    host: *const Host,
    event: *const Event,
    delay: u64,
) {
    crate::guard((), || {
        let (Some(input), Some(event)) = (get(host), event.as_ref()) else {
            return;
        };
        if !matches!(event.kind, 1 | 2) || event.name().is_empty() || !event.name.contains(&0) {
            return;
        }
        let Ok(mut q) = input.queue.lock() else {
            return;
        };
        let index = q.releases.iter().position(|(_, name)| name == &event.name);
        if event.kind == 2 || delay == 0 {
            if let Some(i) = index {
                q.releases.swap_remove(i);
            }
            return;
        }
        let value = (event.timestamp.saturating_add(delay), event.name);
        if let Some(i) = index {
            q.releases[i] = value;
        } else if q.releases.len() < 64 {
            q.releases.push(value);
        }
    });
}
#[no_mangle]
pub unsafe extern "C" fn bongo_cat_input_take_scheduled_release(
    host: *const Host,
    now: u64,
    event: *mut Event,
) -> bool {
    crate::guard(false, || {
        let (Some(input), Some(event)) = (get(host), event.as_mut()) else {
            return false;
        };
        let Ok(mut q) = input.queue.lock() else {
            return false;
        };
        let Some(index) = q.releases.iter().position(|(deadline, _)| *deadline <= now) else {
            return false;
        };
        let (_, name) = q.releases.swap_remove(index);
        *event = Event::empty();
        event.kind = 2;
        event.timestamp = now;
        event.name = name;
        true
    })
}
#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn releases_remain_ordered_when_the_primary_ring_is_full() {
        let mut q = Queue::new();
        for _ in 0..QUEUE_CAP {
            assert!(q.push(Event {
                kind: 1,
                ..Event::empty()
            }));
        }
        assert!(!q.push(Event {
            kind: 1,
            ..Event::empty()
        }));
        assert!(q.push(Event {
            kind: 2,
            ..Event::empty()
        }));
        for i in 0..QUEUE_CAP {
            assert_eq!(q.pop().unwrap().sequence, i as u64);
        }
        let release = q.pop().unwrap();
        assert_eq!(release.kind, 2);
        assert_eq!(release.sequence, 256);
        assert_eq!(q.dropped, 1);
    }
}
