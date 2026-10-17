//! Shared event ABI. Content and interpretation are supplied by each plugin.
use std::ffi::{c_char, c_void};
#[repr(C)]
#[derive(Clone, Copy)]
pub struct Notification {
    pub flags: u32,
    pub error: u32,
    pub key: *const c_char,
    pub fallback: *const c_char,
}
impl Notification {
    pub fn new(
        key: &'static std::ffi::CStr,
        fallback: &'static std::ffi::CStr,
        error: bool,
    ) -> Self {
        Self {
            flags: 1,
            error: error as u32,
            key: key.as_ptr(),
            fallback: fallback.as_ptr(),
        }
    }
}
#[repr(C)]
pub struct Api {
    pub abi_version: u32,
    pub struct_size: u32,
    pub event: unsafe extern "C" fn(*mut c_void, u32, u32, *mut Notification) -> u32,
}
/// Tracks exported/reset completion inside a plugin instance, not in the host.
#[derive(Default)]
pub struct Completion {
    pub export: u32,
    pub reset: u32,
}
#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn layouts_match_the_events_header() {
        let pointer = std::mem::size_of::<usize>();
        assert_eq!(std::mem::size_of::<Notification>(), 8 + 2 * pointer);
        assert_eq!(std::mem::size_of::<Api>(), 8 + pointer);
    }
}

#[macro_export]
macro_rules! export_completion_events {
    ($symbol:ident,$success:expr,$success_text:expr,$failed:expr,$failed_text:expr) => {
        unsafe extern "C" fn completion_event(
            _instance: *mut std::ffi::c_void,
            event: u32,
            result: u32,
            out: *mut $crate::events::Notification,
        ) -> u32 {
            if event != 1 || out.is_null() {
                return 0;
            }
            out.write(if result != 0 {
                $crate::events::Notification::new($success, $success_text, false)
            } else {
                $crate::events::Notification::new($failed, $failed_text, true)
            });
            1
        }
        static EVENTS: $crate::events::Api = $crate::events::Api {
            abi_version: 1,
            struct_size: std::mem::size_of::<$crate::events::Api>() as u32,
            event: completion_event,
        };
        #[no_mangle]
        pub extern "C" fn $symbol(abi: u32) -> *const $crate::events::Api {
            if abi == 1 {
                &EVENTS
            } else {
                std::ptr::null()
            }
        }
    };
}
