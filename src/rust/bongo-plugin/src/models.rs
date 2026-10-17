//! Main-thread model services; plugins never dereference host catalog records.
use std::ffi::{c_char, c_void};
#[repr(C)]
pub struct Host {
    pub abi_version: u32,
    pub struct_size: u32,
    pub context: *mut c_void,
    pub query: unsafe extern "C" fn(*mut c_void, *const c_char) -> u32,
    pub remove: unsafe extern "C" fn(*mut c_void, *const c_char) -> u32,
    pub rename: unsafe extern "C" fn(*mut c_void, *const c_char, *const c_char) -> u32,
}
impl Host {
    pub unsafe fn get<'a>(host: *const Self) -> Option<&'a Self> {
        let host = host.as_ref()?;
        (host.abi_version == 1 && host.struct_size as usize == std::mem::size_of::<Self>())
            .then_some(host)
    }
}
