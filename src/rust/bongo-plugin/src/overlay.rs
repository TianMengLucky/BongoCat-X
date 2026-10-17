//! Shared window-independent canvas, reusing the page ABI's geometry and roles.
use crate::page::Rect;
use std::ffi::{c_char, c_void, CStr, CString};
#[repr(C)]
pub struct Canvas {
    pub abi_version: u32,
    pub struct_size: u32,
    pub context: *mut c_void,
    pub resource_root: *const c_char,
    pub text: unsafe extern "C" fn(*mut c_void, *const c_char, *const c_char) -> *const c_char,
    pub color: unsafe extern "C" fn(*mut c_void, u32) -> u32,
    pub rect: unsafe extern "C" fn(*mut c_void, Rect, f32, f32, u32),
    pub line: unsafe extern "C" fn(*mut c_void, Rect, f32, u32),
    pub draw_text: unsafe extern "C" fn(*mut c_void, Rect, *const c_char, u32, u32, u32),
    pub measure: unsafe extern "C" fn(*mut c_void, *const c_char, u32) -> f32,
    pub image: unsafe extern "C" fn(*mut c_void, u32, *const c_char, Rect) -> u32,
    pub digest: unsafe extern "C" fn(*const c_void, usize, *mut c_char),
}
impl Canvas {
    pub unsafe fn get<'a>(value: *const Self) -> Option<&'a Self> {
        let value = value.as_ref()?;
        (value.abi_version == 1 && value.struct_size as usize == std::mem::size_of::<Self>())
            .then_some(value)
    }
    pub fn tr(&self, key: &str, fallback: &str) -> String {
        let key = crate::page::c(key);
        let fallback = crate::page::c(fallback);
        unsafe {
            let result = (self.text)(self.context, key.as_ptr(), fallback.as_ptr());
            if result.is_null() {
                fallback.to_string_lossy().into_owned()
            } else {
                CStr::from_ptr(result).to_string_lossy().into_owned()
            }
        }
    }
    pub fn color(&self, role: u32) -> u32 {
        unsafe { (self.color)(self.context, role) }
    }
    pub fn fill(&self, r: Rect, rounding: f32, role: u32) {
        unsafe { (self.rect)(self.context, r, rounding, 0.0, self.color(role)) }
    }
    pub fn stroke(&self, r: Rect, rounding: f32, width: f32, role: u32) {
        unsafe { (self.rect)(self.context, r, rounding, width, self.color(role)) }
    }
    pub fn line(&self, r: Rect, width: f32, role: u32) {
        unsafe { (self.line)(self.context, r, width, self.color(role)) }
    }
    pub fn text(&self, r: Rect, value: &str, font: u32, role: u32, align: u32) {
        let value = crate::page::c(value);
        unsafe {
            (self.draw_text)(
                self.context,
                r,
                value.as_ptr(),
                font,
                self.color(role),
                align,
            )
        }
    }
    pub fn measure(&self, value: &str, font: u32) -> f32 {
        let value = crate::page::c(value);
        unsafe { (self.measure)(self.context, value.as_ptr(), font) }
    }
    pub fn image(&self, slot: u32, path: &std::path::Path, r: Rect) -> bool {
        crate::io::path(path)
            .is_some_and(|path| unsafe { (self.image)(self.context, slot, path.as_ptr(), r) != 0 })
    }
    pub fn resource(&self, directory: &str, id: &str, suffix: &str) -> Option<std::path::PathBuf> {
        let root = unsafe { crate::text::cstr(self.resource_root) }?;
        let mut digest = [0 as c_char; 65];
        unsafe { (self.digest)(id.as_ptr().cast(), id.len(), digest.as_mut_ptr()) };
        let digest = unsafe { crate::text::cstr(digest.as_ptr()) }?;
        Some(
            std::path::Path::new(root)
                .join(directory)
                .join(format!("{digest}{suffix}")),
        )
    }
}
pub type Visitor = unsafe extern "C" fn(*mut c_void, *const c_char);
#[repr(C)]
pub struct Api {
    pub abi_version: u32,
    pub struct_size: u32,
    pub width: u32,
    pub height: u32,
    pub title_key: *const c_char,
    pub title: *const c_char,
    pub draw: unsafe extern "C" fn(*mut c_void, *const Canvas) -> u32,
    pub pointer: unsafe extern "C" fn(*mut c_void, f32, f32, u32),
    pub visit_text: unsafe extern "C" fn(*mut c_void, Option<Visitor>, *mut c_void) -> u32,
}
// SAFETY: immutable table contains only static strings and function pointers.
unsafe impl Sync for Api {}
pub fn visit(visitor: Visitor, context: *mut c_void, text: &str) {
    if let Ok(text) = CString::new(text) {
        unsafe { visitor(context, text.as_ptr()) }
    }
}
#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn layouts_match_the_overlay_header() {
        let pointer = std::mem::size_of::<usize>();
        assert_eq!(std::mem::size_of::<Canvas>(), 8 + 10 * pointer);
        assert_eq!(std::mem::size_of::<Api>(), 16 + 5 * pointer);
    }
}
