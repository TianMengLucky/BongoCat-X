//! Keyboard sound effects plugin for BongoCat.
//!
//! Mirrors `include/bongo_cat/sounds_plugin.h` byte for byte: the key table is
//! the same 102 canonical input names the host reports, and every entry point
//! is routed through [`bongo_plugin::guard`] so a panic can never unwind across
//! the C boundary.

mod locale;
mod io;
pub mod ui;
mod page_ui;

use bongo_plugin::guard::guard;
use bongo_plugin::text::buffer;
use std::ffi::{c_char, c_void};
use std::mem::size_of;

/// Pads `value` into a fixed, NUL-terminated byte buffer for the ABI structs.
const fn fit<const N: usize>(value: &str) -> [u8; N] {
    let bytes = value.as_bytes();
    let mut out = [0u8; N];
    let mut index = 0;
    while index < bytes.len() && index < N {
        out[index] = bytes[index];
        index += 1;
    }
    out
}

macro_rules! sound_key {
    ($id:literal, $label:literal, $x:expr, $width:expr, $row:expr) => {
        SoundKey {
            id: fit($id),
            label: fit($label),
            x: $x,
            width: $width,
            row: $row,
        }
    };
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct SoundKey {
    pub id: [u8; 32],
    pub label: [u8; 16],
    pub x: f32,
    pub width: f32,
    pub row: u32,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct SoundBinding {
    pub asset: [u8; 80],
    pub name: [u8; 128],
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct SoundScene {
    pub struct_size: u32,
    pub count: u32,
    pub bindings: [SoundBinding; 128],
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct SoundsPage {
    pub struct_size: u32,
    pub id: [u8; 32],
    pub title_key: [u8; 128],
    pub title: [u8; 64],
}

const EMPTY_BINDING: SoundBinding = SoundBinding {
    asset: [0; 80],
    name: [0; 128],
};

static SOUND_KEYS: [SoundKey; 102] = [
    sound_key!("Escape", "Esc", 0.00, 1.00, 0),
    sound_key!("F1", "F1", 1.50, 1.00, 0),
    sound_key!("F2", "F2", 2.55, 1.00, 0),
    sound_key!("F3", "F3", 3.60, 1.00, 0),
    sound_key!("F4", "F4", 4.65, 1.00, 0),
    sound_key!("F5", "F5", 6.10, 1.00, 0),
    sound_key!("F6", "F6", 7.15, 1.00, 0),
    sound_key!("F7", "F7", 8.20, 1.00, 0),
    sound_key!("F8", "F8", 9.25, 1.00, 0),
    sound_key!("F9", "F9", 10.70, 1.00, 0),
    sound_key!("F10", "F10", 11.75, 1.00, 0),
    sound_key!("F11", "F11", 12.80, 1.00, 0),
    sound_key!("F12", "F12", 13.85, 1.00, 0),
    sound_key!("PrintScreen", "PrtSc", 15.50, 1.00, 0),
    sound_key!("ScrollLock", "ScrLk", 16.50, 1.00, 0),
    sound_key!("Pause", "Pause", 17.50, 1.00, 0),
    sound_key!("BackQuote", "`", 0.00, 1.00, 1),
    sound_key!("Num1", "1", 1.00, 1.00, 1),
    sound_key!("Num2", "2", 2.00, 1.00, 1),
    sound_key!("Num3", "3", 3.00, 1.00, 1),
    sound_key!("Num4", "4", 4.00, 1.00, 1),
    sound_key!("Num5", "5", 5.00, 1.00, 1),
    sound_key!("Num6", "6", 6.00, 1.00, 1),
    sound_key!("Num7", "7", 7.00, 1.00, 1),
    sound_key!("Num8", "8", 8.00, 1.00, 1),
    sound_key!("Num9", "9", 9.00, 1.00, 1),
    sound_key!("Num0", "0", 10.00, 1.00, 1),
    sound_key!("Minus", "-", 11.00, 1.00, 1),
    sound_key!("Equal", "=", 12.00, 1.00, 1),
    sound_key!("Backspace", "Bksp", 13.00, 2.00, 1),
    sound_key!("Insert", "Ins", 15.50, 1.00, 1),
    sound_key!("Home", "Home", 16.50, 1.00, 1),
    sound_key!("PageUp", "PgUp", 17.50, 1.00, 1),
    sound_key!("NumLock", "Num", 19.00, 1.00, 1),
    sound_key!("KpDivide", "/", 20.00, 1.00, 1),
    sound_key!("KpMultiply", "*", 21.00, 1.00, 1),
    sound_key!("KpMinus", "-", 22.00, 1.00, 1),
    sound_key!("Tab", "Tab", 0.00, 1.50, 2),
    sound_key!("KeyQ", "Q", 1.50, 1.00, 2),
    sound_key!("KeyW", "W", 2.50, 1.00, 2),
    sound_key!("KeyE", "E", 3.50, 1.00, 2),
    sound_key!("KeyR", "R", 4.50, 1.00, 2),
    sound_key!("KeyT", "T", 5.50, 1.00, 2),
    sound_key!("KeyY", "Y", 6.50, 1.00, 2),
    sound_key!("KeyU", "U", 7.50, 1.00, 2),
    sound_key!("KeyI", "I", 8.50, 1.00, 2),
    sound_key!("KeyO", "O", 9.50, 1.00, 2),
    sound_key!("KeyP", "P", 10.50, 1.00, 2),
    sound_key!("BracketLeft", "[", 11.50, 1.00, 2),
    sound_key!("BracketRight", "]", 12.50, 1.00, 2),
    sound_key!("Backslash", "\\", 13.50, 1.50, 2),
    sound_key!("Delete", "Del", 15.50, 1.00, 2),
    sound_key!("End", "End", 16.50, 1.00, 2),
    sound_key!("PageDown", "PgDn", 17.50, 1.00, 2),
    sound_key!("Kp7", "7", 19.00, 1.00, 2),
    sound_key!("Kp8", "8", 20.00, 1.00, 2),
    sound_key!("Kp9", "9", 21.00, 1.00, 2),
    sound_key!("KpPlus", "+", 22.00, 1.00, 2),
    sound_key!("CapsLock", "Caps", 0.00, 1.75, 3),
    sound_key!("KeyA", "A", 1.75, 1.00, 3),
    sound_key!("KeyS", "S", 2.75, 1.00, 3),
    sound_key!("KeyD", "D", 3.75, 1.00, 3),
    sound_key!("KeyF", "F", 4.75, 1.00, 3),
    sound_key!("KeyG", "G", 5.75, 1.00, 3),
    sound_key!("KeyH", "H", 6.75, 1.00, 3),
    sound_key!("KeyJ", "J", 7.75, 1.00, 3),
    sound_key!("KeyK", "K", 8.75, 1.00, 3),
    sound_key!("KeyL", "L", 9.75, 1.00, 3),
    sound_key!("Semicolon", ";", 10.75, 1.00, 3),
    sound_key!("Quote", "'", 11.75, 1.00, 3),
    sound_key!("Return", "Enter", 12.75, 2.25, 3),
    sound_key!("Kp4", "4", 19.00, 1.00, 3),
    sound_key!("Kp5", "5", 20.00, 1.00, 3),
    sound_key!("Kp6", "6", 21.00, 1.00, 3),
    sound_key!("ShiftLeft", "Shift", 0.00, 2.25, 4),
    sound_key!("KeyZ", "Z", 2.25, 1.00, 4),
    sound_key!("KeyX", "X", 3.25, 1.00, 4),
    sound_key!("KeyC", "C", 4.25, 1.00, 4),
    sound_key!("KeyV", "V", 5.25, 1.00, 4),
    sound_key!("KeyB", "B", 6.25, 1.00, 4),
    sound_key!("KeyN", "N", 7.25, 1.00, 4),
    sound_key!("KeyM", "M", 8.25, 1.00, 4),
    sound_key!("Comma", ",", 9.25, 1.00, 4),
    sound_key!("Period", ".", 10.25, 1.00, 4),
    sound_key!("Slash", "/", 11.25, 1.00, 4),
    sound_key!("ShiftRight", "Shift", 12.25, 2.75, 4),
    sound_key!("UpArrow", "↑", 16.50, 1.00, 4),
    sound_key!("Kp1", "1", 19.00, 1.00, 4),
    sound_key!("Kp2", "2", 20.00, 1.00, 4),
    sound_key!("Kp3", "3", 21.00, 1.00, 4),
    sound_key!("ControlLeft", "Ctrl", 0.00, 1.50, 5),
    sound_key!("Meta", "Win", 1.50, 1.25, 5),
    sound_key!("Alt", "Alt", 2.75, 1.25, 5),
    sound_key!("Space", "Space", 4.00, 6.25, 5),
    sound_key!("AltGr", "AltGr", 10.25, 1.50, 5),
    sound_key!("Apps", "Menu", 13.00, 1.00, 5),
    sound_key!("ControlRight", "Ctrl", 14.00, 1.00, 5),
    sound_key!("LeftArrow", "←", 15.50, 1.00, 5),
    sound_key!("DownArrow", "↓", 16.50, 1.00, 5),
    sound_key!("RightArrow", "→", 17.50, 1.00, 5),
    sound_key!("Kp0", "0", 19.00, 2.00, 5),
    sound_key!("KpDecimal", ".", 21.00, 1.00, 5),
];

fn hex_lower(byte: u8) -> bool {
    byte.is_ascii_digit() || (b'a'..=b'f').contains(&byte)
}

/// Validates a binding exactly as the C implementation did: both buffers must
/// be NUL-terminated, an empty asset requires an empty name, otherwise the
/// asset must be 68 characters ending in `.snd` with 64 lowercase hex digits.
fn valid(binding: &SoundBinding) -> bool {
    if !binding.asset.contains(&0u8) || !binding.name.contains(&0u8) {
        return false;
    }
    if binding.asset[0] == 0 {
        return binding.name[0] == 0;
    }
    let length = binding.asset.iter().position(|&b| b == 0).unwrap_or(0);
    if length != 68 || &binding.asset[64..68] != b".snd" || binding.name[0] == 0 {
        return false;
    }
    binding.asset[..64].iter().all(|&b| hex_lower(b))
}

unsafe fn valid_ptr(binding: *const SoundBinding) -> bool {
    !binding.is_null() && valid(&*binding)
}

/// `strcmp(a, b) == 0` for a NUL-terminated C string and a fixed table id.
unsafe fn name_matches(a: *const c_char, b: &[u8]) -> bool {
    let a = a.cast::<u8>();
    let mut index = 0usize;
    loop {
        let left = *a.add(index);
        let right = *b.get(index).unwrap_or(&0);
        if left != right {
            return false;
        }
        if left == 0 {
            return true;
        }
        index += 1;
    }
}

struct Sounds {
    scene: SoundScene,
    held: [u8; 128],
}

extern "C" fn key_count() -> u32 {
    guard(0, || SOUND_KEYS.len() as u32)
}

extern "C" fn key(index: u32, out: *mut SoundKey) -> u32 {
    guard(0, || {
        if out.is_null() || index >= key_count() {
            return 0;
        }
        unsafe { out.write(SOUND_KEYS[index as usize]) };
        1
    })
}

extern "C" fn create() -> *mut c_void {
    guard(std::ptr::null_mut(), || {
        Box::into_raw(Box::new(Sounds {
            scene: SoundScene {
                struct_size: size_of::<SoundScene>() as u32,
                count: key_count(),
                bindings: [EMPTY_BINDING; 128],
            },
            held: [0; 128],
        }))
        .cast()
    })
}

unsafe extern "C" fn destroy(instance: *mut c_void) {
    if !instance.is_null() {
        guard((), || drop(Box::from_raw(instance.cast::<Sounds>())));
    }
}

unsafe extern "C" fn page(instance: *mut c_void, out: *mut SoundsPage) -> u32 {
    guard(0, || {
        if instance.is_null() || out.is_null() || (*out).struct_size != size_of::<SoundsPage>() as u32
        {
            return 0;
        }
        out.write(SoundsPage {
            struct_size: size_of::<SoundsPage>() as u32,
            id: buffer("sounds"),
            title_key: buffer("pages.preference.sounds.title"),
            title: buffer("Sound effects"),
        });
        1
    })
}

unsafe extern "C" fn snapshot(instance: *mut c_void, out: *mut SoundScene) -> u32 {
    guard(0, || {
        if instance.is_null() || out.is_null() || (*out).struct_size != size_of::<SoundScene>() as u32
        {
            return 0;
        }
        out.write((&*instance.cast::<Sounds>()).scene);
        1
    })
}

unsafe extern "C" fn restore(instance: *mut c_void, scene: *const SoundScene) -> u32 {
    guard(0, || {
        if instance.is_null() || scene.is_null() {
            return 0;
        }
        let scene = &*scene;
        if scene.struct_size != size_of::<SoundScene>() as u32 || scene.count != key_count() {
            return 0;
        }
        for index in 0..scene.count as usize {
            if !valid(&scene.bindings[index]) {
                return 0;
            }
        }
        (&mut *instance.cast::<Sounds>()).scene = *scene;
        1
    })
}

unsafe extern "C" fn bind(instance: *mut c_void, index: u32, binding: *const SoundBinding) -> u32 {
    guard(0, || {
        if instance.is_null() || index >= key_count() || !valid_ptr(binding) {
            return 0;
        }
        (&mut *instance.cast::<Sounds>()).scene.bindings[index as usize] = *binding;
        1
    })
}

unsafe extern "C" fn press(
    instance: *mut c_void,
    name: *const c_char,
    down: u32,
    out: *mut SoundBinding,
) -> u32 {
    guard(0, || {
        if instance.is_null() || name.is_null() || out.is_null() || down > 1 {
            return 0;
        }
        let sounds = &mut *instance.cast::<Sounds>();
        out.write(EMPTY_BINDING);
        for index in 0..key_count() as usize {
            if name_matches(name, &SOUND_KEYS[index].id) {
                let fresh = down != 0 && sounds.held[index] == 0;
                sounds.held[index] = down as u8;
                if fresh && sounds.scene.bindings[index].asset[0] != 0 {
                    out.write(sounds.scene.bindings[index]);
                    return 1;
                }
                return 0;
            }
        }
        0
    })
}

#[repr(C)]
pub struct Api {
    abi_version: u32,
    struct_size: u32,
    create: extern "C" fn() -> *mut c_void,
    destroy: unsafe extern "C" fn(*mut c_void),
    page: unsafe extern "C" fn(*mut c_void, *mut SoundsPage) -> u32,
    key_count: extern "C" fn() -> u32,
    key: extern "C" fn(u32, *mut SoundKey) -> u32,
    snapshot: unsafe extern "C" fn(*mut c_void, *mut SoundScene) -> u32,
    restore: unsafe extern "C" fn(*mut c_void, *const SoundScene) -> u32,
    bind: unsafe extern "C" fn(*mut c_void, u32, *const SoundBinding) -> u32,
    press: unsafe extern "C" fn(*mut c_void, *const c_char, u32, *mut SoundBinding) -> u32,
}

static API: Api = Api {
    abi_version: 1,
    struct_size: size_of::<Api>() as u32,
    create,
    destroy,
    page,
    key_count,
    key,
    snapshot,
    restore,
    bind,
    press,
};

#[no_mangle]
pub extern "C" fn bongo_cat_sounds_query(abi: u32) -> *const Api {
    if abi == 1 {
        &API
    } else {
        std::ptr::null()
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::ffi::CString;

    fn binding() -> SoundBinding {
        let mut value = EMPTY_BINDING;
        value.asset[..64].fill(b'a');
        value.asset[64..68].copy_from_slice(b".snd");
        value.name[..9].copy_from_slice(b"click.wav");
        value
    }

    fn text(bytes: &[u8]) -> &[u8] {
        let end = bytes.iter().position(|&b| b == 0).unwrap_or(bytes.len());
        &bytes[..end]
    }

    #[test]
    fn layout_matches_the_c_abi() {
        assert_eq!(size_of::<SoundKey>(), 60);
        assert_eq!(size_of::<SoundBinding>(), 208);
        assert_eq!(size_of::<SoundScene>(), 26632);
        assert_eq!(size_of::<SoundsPage>(), 228);
        assert_eq!(size_of::<Api>(), 8 + 9 * size_of::<usize>());
        assert_eq!(SOUND_KEYS.len(), 102);
    }

    #[test]
    fn query_selects_only_the_current_abi() {
        assert!(bongo_cat_sounds_query(0).is_null());
        let api = unsafe { &*bongo_cat_sounds_query(1) };
        assert_eq!(api.struct_size, size_of::<Api>() as u32);
    }

    #[test]
    fn bindings_drive_fresh_transitions() {
        unsafe {
            let api = &*bongo_cat_sounds_query(1);
            let instance = (api.create)();
            assert!(!instance.is_null());
            let mut scene = SoundScene {
                struct_size: size_of::<SoundScene>() as u32,
                count: 0,
                bindings: [EMPTY_BINDING; 128],
            };
            assert_eq!((api.snapshot)(instance, &mut scene), 1);
            assert_eq!(scene.count, (api.key_count)());

            let mut a = 128;
            let mut b = 128;
            for index in 0..scene.count {
                let mut key = SoundKey {
                    id: [0; 32],
                    label: [0; 16],
                    x: 0.0,
                    width: 0.0,
                    row: 0,
                };
                assert_eq!((api.key)(index, &mut key), 1);
                assert!(key.row < 6 && key.x + key.width <= 23.0);
                if text(&key.id) == b"KeyA" {
                    a = index;
                }
                if text(&key.id) == b"KeyB" {
                    b = index;
                }
                for other_index in 0..index {
                    let mut other = key;
                    assert_eq!((api.key)(other_index, &mut other), 1);
                    assert_ne!(key.id, other.id);
                }
            }
            assert!(a < scene.count && b < scene.count);

            let binding = binding();
            assert_eq!((api.bind)(instance, a, &binding), 1);
            assert_eq!((api.bind)(instance, b, &binding), 1);
            let mut output = EMPTY_BINDING;
            let key_a = CString::new("KeyA").unwrap();
            let key_b = CString::new("KeyB").unwrap();
            assert_eq!((api.press)(instance, key_a.as_ptr(), 1, &mut output), 1);
            assert_eq!(text(&output.name), b"click.wav");
            assert_eq!((api.press)(instance, key_a.as_ptr(), 1, &mut output), 0);
            assert_eq!((api.press)(instance, key_b.as_ptr(), 1, &mut output), 1);
            assert_eq!((api.press)(instance, key_a.as_ptr(), 0, &mut output), 0);
            assert_eq!((api.press)(instance, key_a.as_ptr(), 1, &mut output), 1);
            let unknown = CString::new("Unknown").unwrap();
            assert_eq!((api.press)(instance, unknown.as_ptr(), 1, &mut output), 0);

            let mut bad = binding;
            bad.asset[..11].copy_from_slice(b"../bad.wav\0");
            assert_eq!((api.bind)(instance, a, &bad), 0);
            assert_eq!((api.snapshot)(instance, &mut scene), 1);
            scene.bindings[b as usize] = bad;
            assert_eq!((api.restore)(instance, &scene), 0);
            assert_eq!((api.snapshot)(instance, &mut scene), 1);
            assert_eq!(text(&scene.bindings[b as usize].name), b"click.wav");

            assert_eq!((api.bind)(instance, a, &EMPTY_BINDING), 1);
            assert_eq!((api.press)(instance, key_a.as_ptr(), 0, &mut output), 0);
            assert_eq!((api.press)(instance, key_a.as_ptr(), 1, &mut output), 0);
            (api.destroy)(instance);
        }
    }
}
