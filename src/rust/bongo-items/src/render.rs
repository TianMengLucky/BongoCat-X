use crate::{Item, ItemScene, CAPACITY};
#[repr(C)]
pub struct Viewport {
    width: i32,
    height: i32,
    x: i32,
    y: i32,
    content_width: i32,
    content_height: i32,
    mirror: u32,
    vertical_flip: u32,
}
#[repr(C)]
#[derive(Default)]
pub struct Placement {
    x: f32,
    y: f32,
    width: f32,
    height: f32,
    angle: f64,
    flip: u32,
}
unsafe extern "C" fn order(
    scene: *const ItemScene,
    front: u32,
    out: *mut u32,
    capacity: u32,
) -> u32 {
    bongo_plugin::guard::guard(0, || {
        let Some(scene) = scene.as_ref() else {
            return 0;
        };
        if out.is_null() || scene.count as usize > CAPACITY || capacity < scene.count {
            return 0;
        }
        let mut indices = [0u32; CAPACITY];
        let mut count = 0;
        for i in 0..scene.count {
            let item = &scene.items[i as usize];
            if item.visible != 0 && item.opacity > 0.0 && (item.layer >= 0) == (front != 0) {
                indices[count] = i;
                count += 1;
            }
        }
        indices[..count].sort_by_key(|&i| scene.items[i as usize].layer);
        std::ptr::copy_nonoverlapping(indices.as_ptr(), out, count);
        count as u32
    })
}
fn place(item: &Item, v: &Viewport, iw: i32, ih: i32) -> Option<Placement> {
    if !crate::valid(item) || iw <= 0 || ih <= 0 || v.width <= 0 || v.height <= 0 {
        return None;
    }
    let follow = item.follow_model != 0;
    let (vx, vy, vw, vh) = if follow {
        (
            v.x as f32,
            (v.height as f32 - v.y as f32 - v.content_height as f32),
            v.content_width as f32,
            v.content_height as f32,
        )
    } else {
        (0.0, 0.0, v.width as f32, v.height as f32)
    };
    if vw <= 0.0 || vh <= 0.0 {
        return None;
    }
    let mirror = follow && v.mirror != 0;
    let vertical = follow && v.vertical_flip != 0;
    let (mut px, mut py) = (item.x / 100.0, item.y / 100.0);
    if mirror {
        px = 1.0 - px;
    }
    if vertical {
        py = 1.0 - py;
    }
    let scale = vw.min(vh) * item.scale / 100.0 / iw.max(ih) as f32;
    Some(Placement {
        x: vx + px * vw - iw as f32 * scale / 2.0,
        y: vy + py * vh - ih as f32 * scale / 2.0,
        width: iw as f32 * scale,
        height: ih as f32 * scale,
        angle: if mirror != vertical {
            -(item.rotation as f64)
        } else {
            item.rotation as f64
        },
        flip: u32::from(mirror) | (u32::from(vertical) << 1),
    })
}
unsafe extern "C" fn placement(
    item: *const Item,
    v: *const Viewport,
    iw: i32,
    ih: i32,
    out: *mut Placement,
) -> u32 {
    bongo_plugin::guard::guard(0, || {
        let (Some(item), Some(v)) = (item.as_ref(), v.as_ref()) else {
            return 0;
        };
        if out.is_null() {
            return 0;
        }
        let Some(value) = place(item, v, iw, ih) else {
            return 0;
        };
        out.write(value);
        1
    })
}
#[repr(C)]
pub struct Api {
    abi_version: u32,
    struct_size: u32,
    order: unsafe extern "C" fn(*const ItemScene, u32, *mut u32, u32) -> u32,
    placement: unsafe extern "C" fn(*const Item, *const Viewport, i32, i32, *mut Placement) -> u32,
}
static API: Api = Api {
    abi_version: 1,
    struct_size: std::mem::size_of::<Api>() as u32,
    order,
    placement,
};
#[no_mangle]
pub extern "C" fn bongo_cat_items_render_query_v1(abi: u32) -> *const Api {
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
    fn placement_tracks_model_mirroring_and_aspect() {
        let item = Item {
            asset: bongo_plugin::text::buffer(&format!("{}.img", "a".repeat(64))),
            name: bongo_plugin::text::buffer("cat"),
            x: 25.0,
            y: 50.0,
            scale: 50.0,
            rotation: 30.0,
            opacity: 100.0,
            follow_model: 1,
            ..crate::EMPTY_ITEM
        };
        let mut v = Viewport {
            width: 400,
            height: 300,
            x: 100,
            y: 50,
            content_width: 200,
            content_height: 200,
            mirror: 0,
            vertical_flip: 0,
        };
        let first = place(&item, &v, 200, 100).unwrap();
        assert_eq!(
            (
                first.x,
                first.y,
                first.width,
                first.height,
                first.angle,
                first.flip
            ),
            (100.0, 125.0, 100.0, 50.0, 30.0, 0)
        );
        v.mirror = 1;
        let second = place(&item, &v, 200, 100).unwrap();
        assert_eq!((second.x, second.angle, second.flip), (200.0, -30.0, 1));
        assert!(place(&item, &v, 0, 100).is_none());
        assert_eq!(std::mem::size_of::<Viewport>(), 32);
        assert_eq!(std::mem::size_of::<Placement>(), 32);
    }
}
