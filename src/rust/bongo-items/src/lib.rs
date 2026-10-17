//! Image items plugin for BongoCat.
//!
//! Mirrors `include/bongo_cat/items_plugin.h`: the instance owns a
//! [`ItemScene`], validation stays inside the plugin, and every entry point is
//! routed through [`bongo_plugin::guard`] so a panic can never unwind across
//! the C boundary.

mod locale;
pub mod ui;
mod page_ui;

use bongo_plugin::guard::guard;
use bongo_plugin::text::buffer;
use std::ffi::c_void;
use std::mem::size_of;

const CAPACITY: usize = 16;

#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Item {
    pub asset: [u8; 80],
    pub name: [u8; 128],
    pub x: f32,
    pub y: f32,
    pub scale: f32,
    pub rotation: f32,
    pub opacity: f32,
    pub layer: i32,
    pub visible: u32,
    pub locked: u32,
    pub follow_model: u32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct ItemScene {
    pub struct_size: u32,
    pub count: u32,
    pub items: [Item; CAPACITY],
}

#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct ItemsPage {
    pub struct_size: u32,
    pub id: [u8; 32],
    pub title_key: [u8; 128],
    pub title: [u8; 64],
}

const EMPTY_ITEM: Item = Item {
    asset: [0; 80],
    name: [0; 128],
    x: 0.0,
    y: 0.0,
    scale: 0.0,
    rotation: 0.0,
    opacity: 0.0,
    layer: 0,
    visible: 0,
    locked: 0,
    follow_model: 0,
};

fn hex_lower(byte: u8) -> bool {
    byte.is_ascii_digit() || (b'a'..=b'f').contains(&byte)
}

/// Validates an item exactly as the C implementation did: bounded strings, a
/// 68-character hex asset ending in `.img` and finite, range-limited
/// transforms.
fn valid(item: &Item) -> bool {
    if !item.name.contains(&0u8) || item.name[0] == 0 || !item.asset.contains(&0u8) {
        return false;
    }
    let length = item.asset.iter().position(|&b| b == 0).unwrap_or(0);
    if length != 68 || &item.asset[64..68] != b".img" {
        return false;
    }
    if !item.asset[..64].iter().all(|&b| hex_lower(b)) {
        return false;
    }
    item.x.is_finite()
        && item.y.is_finite()
        && item.scale.is_finite()
        && item.rotation.is_finite()
        && item.opacity.is_finite()
        && (0.0..=100.0).contains(&item.x)
        && (0.0..=100.0).contains(&item.y)
        && (1.0..=200.0).contains(&item.scale)
        && (-180.0..=180.0).contains(&item.rotation)
        && (0.0..=100.0).contains(&item.opacity)
        && (-30..=30).contains(&item.layer)
        && item.visible <= 1
        && item.locked <= 1
        && item.follow_model <= 1
}

unsafe fn valid_ptr(item: *const Item) -> bool {
    !item.is_null() && valid(&*item)
}

extern "C" fn create() -> *mut c_void {
    guard(std::ptr::null_mut(), || {
        Box::into_raw(Box::new(ItemScene {
            struct_size: size_of::<ItemScene>() as u32,
            count: 0,
            items: [EMPTY_ITEM; CAPACITY],
        }))
        .cast()
    })
}

unsafe extern "C" fn destroy(instance: *mut c_void) {
    if !instance.is_null() {
        guard((), || drop(Box::from_raw(instance.cast::<ItemScene>())));
    }
}

unsafe extern "C" fn page(instance: *mut c_void, out: *mut ItemsPage) -> u32 {
    guard(0, || {
        if instance.is_null() || out.is_null() || (*out).struct_size != size_of::<ItemsPage>() as u32
        {
            return 0;
        }
        out.write(ItemsPage {
            struct_size: size_of::<ItemsPage>() as u32,
            id: buffer("items"),
            title_key: buffer("pages.preference.items.title"),
            title: buffer("Items"),
        });
        1
    })
}

unsafe extern "C" fn snapshot(instance: *mut c_void, out: *mut ItemScene) -> u32 {
    guard(0, || {
        if instance.is_null() || out.is_null() || (*out).struct_size != size_of::<ItemScene>() as u32
        {
            return 0;
        }
        out.write(*instance.cast::<ItemScene>());
        1
    })
}

unsafe extern "C" fn restore(instance: *mut c_void, scene: *const ItemScene) -> u32 {
    guard(0, || {
        if instance.is_null() || scene.is_null() {
            return 0;
        }
        let scene = &*scene;
        if scene.struct_size != size_of::<ItemScene>() as u32 || scene.count as usize > CAPACITY {
            return 0;
        }
        for index in 0..scene.count as usize {
            if !valid(&scene.items[index]) {
                return 0;
            }
        }
        *instance.cast::<ItemScene>() = *scene;
        1
    })
}

unsafe extern "C" fn add(instance: *mut c_void, item: *const Item) -> u32 {
    guard(0, || {
        if instance.is_null() || !valid_ptr(item) {
            return 0;
        }
        let scene = &mut *instance.cast::<ItemScene>();
        if scene.count as usize >= CAPACITY {
            return 0;
        }
        scene.items[scene.count as usize] = *item;
        scene.count += 1;
        1
    })
}

unsafe extern "C" fn update(instance: *mut c_void, index: u32, item: *const Item) -> u32 {
    guard(0, || {
        if instance.is_null() || !valid_ptr(item) {
            return 0;
        }
        let scene = &mut *instance.cast::<ItemScene>();
        if index >= scene.count {
            return 0;
        }
        scene.items[index as usize] = *item;
        1
    })
}

unsafe extern "C" fn remove(instance: *mut c_void, index: u32) -> u32 {
    guard(0, || {
        if instance.is_null() {
            return 0;
        }
        let scene = &mut *instance.cast::<ItemScene>();
        if index >= scene.count {
            return 0;
        }
        let index = index as usize;
        let count = scene.count as usize;
        scene.items.copy_within(index + 1..count, index);
        scene.count -= 1;
        scene.items[scene.count as usize] = EMPTY_ITEM;
        1
    })
}

#[repr(C)]
pub struct Api {
    abi_version: u32,
    struct_size: u32,
    create: extern "C" fn() -> *mut c_void,
    destroy: unsafe extern "C" fn(*mut c_void),
    page: unsafe extern "C" fn(*mut c_void, *mut ItemsPage) -> u32,
    snapshot: unsafe extern "C" fn(*mut c_void, *mut ItemScene) -> u32,
    restore: unsafe extern "C" fn(*mut c_void, *const ItemScene) -> u32,
    add: unsafe extern "C" fn(*mut c_void, *const Item) -> u32,
    update: unsafe extern "C" fn(*mut c_void, u32, *const Item) -> u32,
    remove: unsafe extern "C" fn(*mut c_void, u32) -> u32,
}

static API: Api = Api {
    abi_version: 1,
    struct_size: size_of::<Api>() as u32,
    create,
    destroy,
    page,
    snapshot,
    restore,
    add,
    update,
    remove,
};

#[no_mangle]
pub extern "C" fn bongo_cat_items_query(abi: u32) -> *const Api {
    if abi == 1 {
        &API
    } else {
        std::ptr::null()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn item() -> Item {
        let mut value = EMPTY_ITEM;
        value.asset[..64].fill(b'a');
        value.asset[64..68].copy_from_slice(b".img");
        value.name[..7].copy_from_slice(b"hat.png");
        value.x = 50.0;
        value.y = 50.0;
        value.scale = 25.0;
        value.opacity = 100.0;
        value.visible = 1;
        value.follow_model = 1;
        value
    }

    fn text(bytes: &[u8]) -> &[u8] {
        let end = bytes.iter().position(|&b| b == 0).unwrap_or(bytes.len());
        &bytes[..end]
    }

    #[test]
    fn layout_matches_the_c_abi() {
        assert_eq!(size_of::<Item>(), 244);
        assert_eq!(size_of::<ItemScene>(), 3912);
        assert_eq!(size_of::<ItemsPage>(), 228);
        assert_eq!(size_of::<Api>(), 8 + 8 * size_of::<usize>());
    }

    #[test]
    fn query_selects_only_the_current_abi() {
        assert!(bongo_cat_items_query(0).is_null());
        let api = unsafe { &*bongo_cat_items_query(1) };
        assert_eq!(api.struct_size, size_of::<Api>() as u32);
    }

    #[test]
    fn scenes_validate_restore_atomically_and_remove_compacts() {
        unsafe {
            let api = &*bongo_cat_items_query(1);
            let mut instance = (api.create)();
            assert!(!instance.is_null());

            let mut page = ItemsPage {
                struct_size: 0,
                id: [0; 32],
                title_key: [0; 128],
                title: [0; 64],
            };
            assert_eq!((api.page)(instance, &mut page), 0);
            page.struct_size = size_of::<ItemsPage>() as u32;
            assert_eq!((api.page)(instance, &mut page), 1);
            assert_eq!(text(&page.id), b"items");

            let mut scene = ItemScene {
                struct_size: size_of::<ItemScene>() as u32,
                count: 0,
                items: [EMPTY_ITEM; CAPACITY],
            };
            assert_eq!((api.snapshot)(instance, &mut scene), 1);
            assert_eq!(scene.count, 0);

            let value = item();
            assert_eq!((api.add)(instance, &value), 1);

            let mut bad = value;
            bad.asset[..15].copy_from_slice(b"../outside.png\0");
            assert_eq!((api.update)(instance, 0, &bad), 0);
            bad = value;
            bad.x = f32::NAN;
            assert_eq!((api.update)(instance, 0, &bad), 0);
            bad = value;
            bad.opacity = 101.0;
            assert_eq!((api.add)(instance, &bad), 0);
            bad = value;
            bad.name.fill(b'x');
            assert_eq!((api.add)(instance, &bad), 0);
            bad = value;
            bad.follow_model = 2;
            assert_eq!((api.add)(instance, &bad), 0);

            assert_eq!((api.snapshot)(instance, &mut scene), 1);
            assert_eq!(scene.count, 1);
            assert_eq!(scene.items[0].x, 50.0);

            scene.count = 2;
            scene.items[1] = value;
            scene.items[1].scale = 0.0;
            assert_eq!((api.restore)(instance, &scene), 0);
            assert_eq!((api.snapshot)(instance, &mut scene), 1);
            assert_eq!(scene.count, 1);

            let mut value = value;
            for index in 1..CAPACITY as i32 {
                value.layer = index;
                assert_eq!((api.add)(instance, &value), 1);
            }
            assert_eq!((api.add)(instance, &value), 0);
            assert_eq!((api.remove)(instance, CAPACITY as u32), 0);
            assert_eq!((api.remove)(instance, 0), 1);
            assert_eq!((api.snapshot)(instance, &mut scene), 1);
            assert_eq!(scene.count, CAPACITY as u32 - 1);
            assert_eq!(scene.items[0].layer, 1);

            (api.destroy)(instance);
            instance = (api.create)();
            assert!(!instance.is_null());
            assert_eq!((api.restore)(instance, &scene), 1);
            let mut restored = ItemScene {
                struct_size: size_of::<ItemScene>() as u32,
                count: 0,
                items: [EMPTY_ITEM; CAPACITY],
            };
            assert_eq!((api.snapshot)(instance, &mut restored), 1);
            assert_eq!(scene, restored);

            let other = (api.create)();
            assert!(!other.is_null());
            assert_eq!((api.snapshot)(other, &mut restored), 1);
            assert_eq!(restored.count, 0);
            (api.destroy)(other);
            (api.destroy)(instance);
        }
    }
}
