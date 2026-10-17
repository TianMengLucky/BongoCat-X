//! Borrowed transfer services and the existing model-import receipt ABI.
use std::ffi::{c_char, c_void};
pub type Cancel = unsafe extern "C" fn(*mut c_void) -> bool;
pub type Progress = unsafe extern "C" fn(*mut c_void, u64, u64);
#[repr(C)]
#[derive(Clone)]
pub struct Receipt {
    pub ids: [[u8; 128]; 16],
    pub installed: [bool; 16],
    pub count: usize,
    pub installed_count: usize,
}
impl Default for Receipt {
    fn default() -> Self {
        Self {
            ids: [[0; 128]; 16],
            installed: [false; 16],
            count: 0,
            installed_count: 0,
        }
    }
}
#[repr(C)]
pub struct Host {
    pub abi_version: u32,
    pub struct_size: u32,
    pub context: *mut c_void,
    pub cancelled: Cancel,
    pub progress: Progress,
    pub lock: unsafe extern "C" fn(*mut c_void),
    pub unlock: unsafe extern "C" fn(*mut c_void),
    pub get: unsafe extern "C" fn(
        *const c_char,
        bool,
        usize,
        Cancel,
        *mut c_void,
        *mut *mut u8,
        *mut usize,
        *mut c_char,
        usize,
    ) -> i32,
    pub free_response: unsafe extern "C" fn(*mut u8, usize),
    pub download: unsafe extern "C" fn(
        *const c_char,
        *const c_char,
        *const c_char,
        bool,
        Cancel,
        Progress,
        *mut c_void,
        *mut c_char,
        usize,
    ) -> i32,
    pub install: unsafe extern "C" fn(*mut c_void, *const c_char, *mut Receipt) -> u32,
    pub digest: unsafe extern "C" fn(*const c_void, usize, *mut c_char),
    pub replace: unsafe extern "C" fn(*const c_char, *const c_char) -> u32,
    pub validate_image: unsafe extern "C" fn(*const c_char) -> u32,
}
impl Host {
    pub unsafe fn get<'a>(host: *const Self) -> Option<&'a Self> {
        let host = host.as_ref()?;
        (host.abi_version == 1 && host.struct_size as usize == std::mem::size_of::<Self>())
            .then_some(host)
    }
    pub fn locked<T>(&self, body: impl FnOnce() -> T) -> T {
        struct Unlock<'a>(&'a Host);
        impl Drop for Unlock<'_> {
            fn drop(&mut self) {
                unsafe { (self.0.unlock)(self.0.context) }
            }
        }
        unsafe { (self.lock)(self.context) };
        let _unlock = Unlock(self);
        body()
    }
}
#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn layouts_match_transfer_and_import_headers() {
        let pointer = std::mem::size_of::<usize>();
        assert_eq!(std::mem::size_of::<Host>(), 8 + 12 * pointer);
        assert_eq!(std::mem::size_of::<Receipt>(), 16 * 128 + 16 + 2 * pointer);
    }
}
