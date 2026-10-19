//! Session-only timer. Monotonic intervals are split at the last Han commit + 5s.
use std::{
    ffi::c_void,
    time::{Duration, Instant},
};

const ACTIVE_WINDOW: Duration = Duration::from_secs(5);
#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct Snapshot {
    pub enabled: u32,
    pub reserved: u32,
    pub typing_ms: u64,
    pub idle_ms: u64,
}
#[derive(Default)]
pub struct Timer {
    value: Snapshot,
    last: Option<Instant>,
    active_until: Option<Instant>,
    typing: Duration,
    idle: Duration,
}
impl Timer {
    pub fn tick(&mut self, now: Instant) {
        let Some(last) = self.last else {
            return;
        };
        let now = now.max(last);
        let elapsed = now.duration_since(last);
        let typing = self.active_until.map_or(Duration::ZERO, |until| {
            now.min(until).saturating_duration_since(last)
        });
        self.typing = self.typing.saturating_add(typing);
        self.idle = self.idle.saturating_add(elapsed - typing);
        self.last = Some(now);
    }
    pub fn set_enabled(&mut self, enabled: bool, now: Instant) {
        self.tick(now);
        if self.value.enabled == enabled as u32 {
            return;
        }
        self.value.enabled = enabled as u32;
        self.last = enabled.then_some(now);
        self.active_until = None;
    }
    pub fn commit(&mut self, now: Instant) {
        if self.value.enabled == 0 {
            return;
        }
        // Ignore delayed events from before enabling/resuming the timer.
        if self.last.is_some_and(|last| now < last) {
            return;
        }
        self.tick(now);
        self.active_until = Some(now + ACTIVE_WINDOW);
    }
    pub fn snapshot(&self) -> Snapshot {
        Snapshot {
            typing_ms: self.typing.as_millis().min(u64::MAX as u128) as u64,
            idle_ms: self.idle.as_millis().min(u64::MAX as u128) as u64,
            ..self.value
        }
    }
    pub fn reset(&mut self, now: Instant) {
        let enabled = self.value.enabled != 0;
        *self = Self::default();
        self.set_enabled(enabled, now);
    }
}
unsafe extern "C" fn snapshot(instance: *mut c_void, out: *mut Snapshot) -> u32 {
    crate::guarded(0, || {
        if instance.is_null() || out.is_null() {
            return 0;
        }
        let instance = &*instance.cast::<crate::Instance>();
        let Ok(cache) = instance.cache.try_lock() else {
            return 0;
        };
        out.write(cache.timer);
        1
    })
}
unsafe extern "C" fn enable(instance: *mut c_void, enabled: u32) -> u32 {
    crate::guarded(0, || {
        if instance.is_null() || enabled > 1 {
            return 0;
        }
        let instance = &*instance.cast::<crate::Instance>();
        instance
            .commands
            .try_send(crate::Command::Timer(enabled != 0))
            .is_ok() as u32
    })
}
#[repr(C)]
pub struct Plugin {
    abi_version: u32,
    struct_size: u32,
    snapshot: unsafe extern "C" fn(*mut c_void, *mut Snapshot) -> u32,
    enable: unsafe extern "C" fn(*mut c_void, u32) -> u32,
}
static API: Plugin = Plugin {
    abi_version: 1,
    struct_size: std::mem::size_of::<Plugin>() as u32,
    snapshot,
    enable,
};
#[no_mangle]
pub extern "C" fn bongo_cat_statistics_timer_query(abi: u32) -> *const Plugin {
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
    fn repeated_ticks_keep_submillisecond_time_and_extension_validates_inputs() {
        let start = Instant::now();
        let mut timer = Timer::default();
        timer.set_enabled(true, start);
        for micros in [500, 1000, 1500, 2000] {
            timer.tick(start + Duration::from_micros(micros));
        }
        assert_eq!(timer.snapshot().idle_ms, 2);
        assert!(bongo_cat_statistics_timer_query(0).is_null());
        assert!(!bongo_cat_statistics_timer_query(1).is_null());
        assert_eq!(unsafe { enable(std::ptr::null_mut(), 1) }, 0);
        assert_eq!(
            unsafe { snapshot(std::ptr::null_mut(), std::ptr::null_mut()) },
            0
        );
    }
    #[test]
    fn splits_idle_typing_pause_resume_and_reset() {
        let start = Instant::now();
        let at = |s| start + Duration::from_secs(s);
        let mut timer = Timer::default();
        timer.commit(at(0));
        timer.tick(at(10));
        assert_eq!(timer.snapshot().typing_ms, 0);
        timer.set_enabled(true, at(10));
        timer.commit(at(12));
        timer.tick(at(20));
        assert_eq!(timer.snapshot().typing_ms, 5000);
        assert_eq!(timer.snapshot().idle_ms, 5000);
        timer.set_enabled(false, at(20));
        timer.commit(at(25));
        timer.tick(at(30));
        assert_eq!(timer.snapshot().idle_ms, 5000);
        timer.set_enabled(true, at(30));
        timer.commit(at(25));
        timer.commit(at(32));
        timer.commit(at(34));
        timer.set_enabled(false, at(40));
        assert_eq!(timer.snapshot().typing_ms, 12000);
        assert_eq!(timer.snapshot().idle_ms, 8000);
        timer.set_enabled(true, at(42));
        timer.reset(at(43));
        timer.tick(at(44));
        assert_eq!(timer.snapshot().enabled, 1);
        assert_eq!(timer.snapshot().typing_ms, 0);
        assert_eq!(timer.snapshot().idle_ms, 1000);
        assert_eq!(std::mem::size_of::<Snapshot>(), 24);
    }
}
