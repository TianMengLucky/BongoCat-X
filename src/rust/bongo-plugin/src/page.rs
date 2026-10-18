//! Optional page UI ABI. Host resources are borrowed only during one draw;
//! each plugin owns its independent UI state, separate from its worker state.
use crate::ui::{Ui, UiHost};
use std::ffi::{c_char, c_void, CStr, CString};
use std::mem::{size_of, MaybeUninit};

pub const ABI: u32 = 1;
#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct Rect {
    pub x: f32,
    pub y: f32,
    pub w: f32,
    pub h: f32,
}
impl Rect {
    pub fn new(x: f32, y: f32, w: f32, h: f32) -> Self {
        Self { x, y, w, h }
    }
    pub fn inset(self, amount: f32) -> Self {
        Self::new(
            self.x + amount,
            self.y + amount,
            (self.w - 2.0 * amount).max(1.0),
            (self.h - 2.0 * amount).max(1.0),
        )
    }
}
pub mod color {
    pub const TEXT: u32 = 0;
    pub const MUTED: u32 = 1;
    pub const SURFACE: u32 = 2;
    pub const FIELD: u32 = 3;
    pub const HOVER: u32 = 4;
    pub const SELECTION: u32 = 5;
    pub const BORDER: u32 = 6;
    pub const BORDER_SUBTLE: u32 = 7;
    pub const ACCENT: u32 = 8;
    pub const ACCENT_HOVER: u32 = 9;
    pub const DANGER: u32 = 10;
    pub const DANGER_BACKGROUND: u32 = 11;
    pub const LINK: u32 = 12;
    pub const LINK_HOVER: u32 = 13;
}
pub mod read {
    pub const STATUS: u32 = 0;
    pub const SNAPSHOT: u32 = 1;
    pub const ENTRY: u32 = 2;
    pub const KEY: u32 = 3;
    pub const USAGE: u32 = 4;
    pub const UNITS: u32 = 5;
    pub const REPOSITORY: u32 = 6;
    pub const INSTALLED: u32 = 7;
    pub const IMAGE: u32 = 8;
    pub const SECTION: u32 = 9;
    pub const HEATMAP: u32 = 10;
}
pub mod action {
    pub const IMPORT: u32 = 0;
    pub const SAVE: u32 = 1;
    pub const OPEN_FOLDER: u32 = 2;
    pub const UPDATE_ITEM: u32 = 3;
    pub const DUPLICATE_ITEM: u32 = 4;
    pub const REMOVE_ITEM: u32 = 5;
    pub const PREVIEW_SOUND: u32 = 6;
    pub const CLEAR_SOUND: u32 = 7;
    pub const RESET_STATS: u32 = 8;
    pub const EXPORT_STATS: u32 = 9;
    pub const OVERLAY: u32 = 10;
    pub const USAGE_ENABLE: u32 = 11;
    pub const EXPORT_USAGE: u32 = 12;
    pub const SET_UNITS: u32 = 13;
    pub const SET_REPOSITORY: u32 = 14;
    pub const REFRESH_STORE: u32 = 15;
    pub const OPEN_URL: u32 = 16;
    pub const DOWNLOAD: u32 = 17;
    pub const UNINSTALL: u32 = 18;
}
#[repr(C)]
#[derive(Default)]
pub struct Status {
    pub busy: u32,
    pub blocked: u32,
    pub status: u32,
    pub count: u32,
    pub operation: u32,
    pub operation_index: u32,
    pub received: u64,
    pub total: u64,
    pub overlay: u32,
    pub reset_available: u32,
    pub reset_busy: u32,
}
#[repr(C)]
pub struct Host {
    pub abi_version: u32,
    pub struct_size: u32,
    pub widgets: *const UiHost,
    pub section: unsafe extern "C" fn(*mut Ui, *const c_char),
    pub status: unsafe extern "C" fn(*mut Ui, *const c_char, *const c_char, *const c_char),
    pub segments:
        unsafe extern "C" fn(*mut Ui, *const c_char, *const *const c_char, u32, u32) -> u32,
    pub edit: unsafe extern "C" fn(*mut Ui, *const c_char, *mut c_char, u32, u32) -> u32,
    pub number: unsafe extern "C" fn(*mut Ui, *const c_char, *const c_char, *mut f64, f64, f64),
    pub disabled: unsafe extern "C" fn(*mut Ui, u32),
    pub region: unsafe extern "C" fn(*mut Ui, *mut Rect) -> u32,
    pub fill_rect: unsafe extern "C" fn(*mut Ui, Rect, f32, u32),
    pub stroke_rect: unsafe extern "C" fn(*mut Ui, Rect, f32, f32, u32),
    pub circle: unsafe extern "C" fn(*mut Ui, Rect, u32),
    pub line: unsafe extern "C" fn(*mut Ui, Rect, f32, u32),
    pub draw_text: unsafe extern "C" fn(*mut Ui, Rect, *const c_char, u32, u32, u32),
    pub measure: unsafe extern "C" fn(*mut Ui, *const c_char, u32) -> f32,
    pub input: unsafe extern "C" fn(*mut Ui, Rect, u32) -> u32,
    pub pointer: unsafe extern "C" fn(*mut Ui, *mut f32, *mut f32),
    pub tooltip: unsafe extern "C" fn(*mut Ui, *const c_char),
    pub color: unsafe extern "C" fn(*mut Ui, u32) -> u32,
    pub width: unsafe extern "C" fn(*mut Ui) -> f32,
    pub spacing: unsafe extern "C" fn(*mut Ui) -> f32,
    pub button: unsafe extern "C" fn(*mut Ui, *const c_char, *const c_char, u32) -> u32,
    pub tree_begin: unsafe extern "C" fn(*mut Ui, *const c_char, *const c_char, *mut u32) -> u32,
    pub tree_end: unsafe extern "C" fn(*mut Ui),
    pub progress: unsafe extern "C" fn(*mut Ui, f32),
    pub image: unsafe extern "C" fn(*mut Ui, *const c_char, Rect) -> u32,
    pub images_begin: unsafe extern "C" fn(*mut Ui),
    pub images_end: unsafe extern "C" fn(*mut Ui),
    pub glyphs: unsafe extern "C" fn(*mut Ui, *const *const c_char, u32, *mut u64),
    pub notice: unsafe extern "C" fn(*mut Ui, *const c_char, u32),
    pub invalidate: unsafe extern "C" fn(*mut Ui),
    pub unfocus: unsafe extern "C" fn(*mut Ui),
}
#[repr(C)]
pub struct Services {
    pub abi_version: u32,
    pub struct_size: u32,
    pub context: *mut c_void,
    pub read: unsafe extern "C" fn(*mut c_void, u32, u32, *mut c_void, u32) -> u32,
    pub action: unsafe extern "C" fn(*mut c_void, u32, u32, *const c_void, u32) -> u32,
}
#[repr(C)]
pub struct Contribution {
    pub abi_version: u32,
    pub struct_size: u32,
    pub create: extern "C" fn() -> *mut c_void,
    pub destroy: unsafe extern "C" fn(*mut c_void),
    pub draw: unsafe extern "C" fn(*mut c_void, *mut Ui, *const Host, *const Services) -> u32,
}

pub(crate) fn c(text: &str) -> CString {
    CString::new(text.split('\0').next().unwrap_or("")).expect("NUL removed")
}
pub fn text(bytes: &[u8]) -> &str {
    let end = bytes.iter().position(|&b| b == 0).unwrap_or(bytes.len());
    std::str::from_utf8(&bytes[..end]).unwrap_or("")
}

pub struct Page<'a> {
    ui: *mut Ui,
    host: &'a Host,
    widgets: &'a UiHost,
    services: &'a Services,
}
impl<'a> Page<'a> {
    /// # Safety
    /// Non-null arguments must point to live host tables/context for this draw.
    pub unsafe fn new(ui: *mut Ui, host: *const Host, services: *const Services) -> Option<Self> {
        #[repr(C)]
        struct Header {
            abi: u32,
            size: u32,
        }
        unsafe fn compatible<T>(p: *const T) -> bool {
            !p.is_null()
                && (*p.cast::<Header>()).abi == ABI
                && (*p.cast::<Header>()).size as usize == size_of::<T>()
        }
        if ui.is_null()
            || !compatible(host)
            || !compatible(services)
            || !compatible((*host).widgets)
        {
            return None;
        }
        Some(Self {
            ui,
            host: &*host,
            widgets: &*(*host).widgets,
            services: &*services,
        })
    }
    pub fn tr(&self, key: &str, fallback: &str) -> String {
        // Copy while both input CStrings still live: the host may return fallback.
        let key = c(key);
        let fallback = c(fallback);
        unsafe {
            let result = (self.widgets.text)(self.ui, key.as_ptr(), fallback.as_ptr());
            if result.is_null() {
                fallback.to_string_lossy().into_owned()
            } else {
                CStr::from_ptr(result).to_string_lossy().into_owned()
            }
        }
    }
    /// # Safety
    /// T must be the operation's C ABI record and allow a zeroed representation.
    pub unsafe fn read<T>(&self, operation: u32, index: u32) -> Option<T> {
        let mut value = MaybeUninit::<T>::zeroed();
        ((self.services.read)(
            self.services.context,
            operation,
            index,
            value.as_mut_ptr().cast(),
            size_of::<T>() as u32,
        ) != 0)
            .then(|| value.assume_init())
    }
    pub fn status(&self) -> Status {
        unsafe { self.read(read::STATUS, 0).unwrap_or_default() }
    }
    pub fn act(&self, operation: u32, index: u32) -> bool {
        unsafe {
            (self.services.action)(self.services.context, operation, index, std::ptr::null(), 0)
                != 0
        }
    }
    /// # Safety
    /// T must be the operation's initialized C ABI record with no borrowed pointers.
    pub unsafe fn send<T>(&self, operation: u32, index: u32, value: &T) -> bool {
        unsafe {
            (self.services.action)(
                self.services.context,
                operation,
                index,
                (value as *const T).cast(),
                size_of::<T>() as u32,
            ) != 0
        }
    }
    pub fn url(&self, url: &str) -> bool {
        let url = c(url);
        unsafe {
            (self.services.action)(
                self.services.context,
                action::OPEN_URL,
                0,
                url.as_ptr().cast(),
                url.as_bytes_with_nul().len() as u32,
            ) != 0
        }
    }
    pub fn intro(&self, text: &str) {
        unsafe { (self.widgets.intro)(self.ui, c(text).as_ptr()) }
    }
    pub fn label(&self, text: &str) {
        unsafe { (self.widgets.label)(self.ui, c(text).as_ptr()) }
    }
    pub fn wrapped(&self, text: &str, height: f32) {
        unsafe { (self.widgets.label_wrapped)(self.ui, c(text).as_ptr(), height) }
    }
    pub fn section(&self, text: &str) {
        unsafe { (self.host.section)(self.ui, c(text).as_ptr()) }
    }
    pub fn row(&self, columns: u32, height: f32) {
        unsafe { (self.widgets.row)(self.ui, columns, height) }
    }
    pub fn button(&self, id: &str, text: &str, style: u32) -> bool {
        unsafe { (self.host.button)(self.ui, c(id).as_ptr(), c(text).as_ptr(), style) != 0 }
    }
    pub fn toggle(&self, id: &str, title: &str, value: &mut u32) -> bool {
        unsafe { (self.widgets.toggle)(self.ui, c(id).as_ptr(), c(title).as_ptr(), value) != 0 }
    }
    pub fn slider(
        &self,
        id: &str,
        title: &str,
        description: &str,
        value: &mut f32,
        range: (f32, f32),
        default: f32,
    ) {
        unsafe {
            (self.widgets.slider)(
                self.ui,
                c(id).as_ptr(),
                c(title).as_ptr(),
                c(description).as_ptr(),
                value,
                range.0,
                range.1,
                1.0,
                default,
            );
        }
    }
    pub fn status_row(&self, id: &str, title: &str, value: &str) {
        unsafe {
            (self.host.status)(
                self.ui,
                c(id).as_ptr(),
                c(title).as_ptr(),
                c(value).as_ptr(),
            );
        }
    }
    pub fn segments(&self, id: &str, labels: &[String], selected: u32) -> u32 {
        let strings: Vec<_> = labels.iter().map(|s| c(s)).collect();
        let pointers: Vec<_> = strings.iter().map(|s| s.as_ptr()).collect();
        unsafe {
            (self.host.segments)(
                self.ui,
                c(id).as_ptr(),
                pointers.as_ptr(),
                pointers.len() as u32,
                selected,
            )
        }
    }
    pub fn edit(&self, id: &str, value: &mut [u8], ascii: bool) {
        unsafe {
            (self.host.edit)(
                self.ui,
                c(id).as_ptr(),
                value.as_mut_ptr().cast(),
                value.len() as u32,
                ascii as u32,
            );
        }
    }
    pub fn number(&self, id: &str, title: &str, value: &mut f64, range: (f64, f64)) {
        unsafe {
            (self.host.number)(
                self.ui,
                c(id).as_ptr(),
                c(title).as_ptr(),
                value,
                range.0,
                range.1,
            );
        }
    }
    pub fn disabled(&self, begin: bool) {
        unsafe { (self.host.disabled)(self.ui, begin as u32) }
    }
    pub fn region(&self) -> Option<(Rect, bool)> {
        let mut r = Rect::default();
        let state = unsafe { (self.host.region)(self.ui, &mut r) };
        (state != 0).then_some((r, state == 2))
    }
    pub fn color(&self, role: u32) -> u32 {
        unsafe { (self.host.color)(self.ui, role) }
    }
    pub fn fill(&self, r: Rect, rounding: f32, role: u32) {
        unsafe { (self.host.fill_rect)(self.ui, r, rounding, self.color(role)) }
    }
    pub fn fill_rgba(&self, r: Rect, rounding: f32, rgba: u32) {
        unsafe { (self.host.fill_rect)(self.ui, r, rounding, rgba) }
    }
    pub fn text_rgba(&self, r: Rect, text: &str, font: u32, rgba: u32, align: u32) {
        unsafe { (self.host.draw_text)(self.ui, r, c(text).as_ptr(), font, rgba, align) }
    }
    pub fn stroke(&self, r: Rect, rounding: f32, width: f32, role: u32) {
        unsafe { (self.host.stroke_rect)(self.ui, r, rounding, width, self.color(role)) }
    }
    pub fn circle(&self, r: Rect, role: u32) {
        unsafe { (self.host.circle)(self.ui, r, self.color(role)) }
    }
    pub fn line(&self, r: Rect, width: f32, role: u32) {
        unsafe { (self.host.line)(self.ui, r, width, self.color(role)) }
    }
    pub fn paint_text(&self, r: Rect, text: &str, font: u32, role: u32, align: u32) {
        unsafe {
            (self.host.draw_text)(self.ui, r, c(text).as_ptr(), font, self.color(role), align)
        }
    }
    pub fn measure(&self, text: &str, font: u32) -> f32 {
        unsafe { (self.host.measure)(self.ui, c(text).as_ptr(), font) }
    }
    pub fn input(&self, r: Rect, kind: u32) -> bool {
        unsafe { (self.host.input)(self.ui, r, kind) != 0 }
    }
    pub fn pointer(&self) -> (f32, f32) {
        let (mut x, mut y) = (0.0, 0.0);
        unsafe { (self.host.pointer)(self.ui, &mut x, &mut y) };
        (x, y)
    }
    pub fn tooltip(&self, text: &str) {
        unsafe { (self.host.tooltip)(self.ui, c(text).as_ptr()) }
    }
    pub fn width(&self) -> f32 {
        unsafe { (self.host.width)(self.ui) }
    }
    pub fn spacing(&self) -> f32 {
        unsafe { (self.host.spacing)(self.ui) }
    }
    pub fn tree(&self, id: &str, title: &str, open: &mut u32) -> bool {
        unsafe { (self.host.tree_begin)(self.ui, c(id).as_ptr(), c(title).as_ptr(), open) != 0 }
    }
    pub fn tree_end(&self) {
        unsafe { (self.host.tree_end)(self.ui) }
    }
    pub fn progress(&self, fraction: f32) {
        unsafe { (self.host.progress)(self.ui, fraction) }
    }
    pub fn image(&self, path: &str, r: Rect) -> bool {
        unsafe { (self.host.image)(self.ui, c(path).as_ptr(), r) != 0 }
    }
    pub fn images_begin(&self) {
        unsafe { (self.host.images_begin)(self.ui) }
    }
    pub fn images_end(&self) {
        unsafe { (self.host.images_end)(self.ui) }
    }
    pub fn glyphs(&self, texts: &[&str], hash: &mut u64) {
        let strings: Vec<_> = texts.iter().map(|s| c(s)).collect();
        let pointers: Vec<_> = strings.iter().map(|s| s.as_ptr()).collect();
        unsafe { (self.host.glyphs)(self.ui, pointers.as_ptr(), pointers.len() as u32, hash) }
    }
    pub fn notice(&self, text: &str, error: bool) {
        unsafe { (self.host.notice)(self.ui, c(text).as_ptr(), error as u32) }
    }
    pub fn invalidate(&self) {
        unsafe { (self.host.invalidate)(self.ui) }
    }
    pub fn unfocus(&self) {
        unsafe { (self.host.unfocus)(self.ui) }
    }
    pub fn card(&self, id: &str) -> bool {
        unsafe { (self.widgets.card_begin)(self.ui, c(id).as_ptr()) != 0 }
    }
    pub fn card_end(&self) {
        unsafe { (self.widgets.card_end)(self.ui) }
    }
}

pub trait PageState: Default {
    fn draw(&mut self, page: &Page<'_>);
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn page_tables_match_the_c_abi() {
        assert_eq!(size_of::<Rect>(), 16);
        assert_eq!(size_of::<Status>(), 56);
        assert_eq!(size_of::<Host>(), 8 + 31 * size_of::<usize>());
        assert_eq!(size_of::<Services>(), 8 + 3 * size_of::<usize>());
        assert_eq!(size_of::<Contribution>(), 8 + 3 * size_of::<usize>());
    }
    #[test]
    fn fixed_text_is_bounded() {
        assert_eq!(text(b"hello\0ignored"), "hello");
        assert_eq!(text(b"hello"), "hello");
        assert_eq!(text(&[255]), "");
    }
}
