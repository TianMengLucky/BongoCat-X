use super::{Item, ItemScene, CAPACITY, EMPTY_ITEM};
use bongo_plugin::io::{self, json, operation as op, Host, Value};
use bongo_plugin::page::text;
use std::ffi::{c_char, c_void};
use std::path::Path;
unsafe fn scene(instance: *mut c_void, host: &Host) -> ItemScene {
    host.locked(|| *instance.cast::<ItemScene>())
}
fn document(scene: &ItemScene) -> Value {
    let items: Vec<_> = scene.items[..scene.count as usize]
        .iter()
        .map(|v| {
            json!({
                "asset":text(&v.asset),"name":text(&v.name),"x":v.x,"y":v.y,"scale":v.scale,
                "rotation":v.rotation,"opacity":v.opacity,"layer":v.layer,"visible":v.visible != 0,
                "locked":v.locked != 0,"followModel":v.follow_model != 0
            })
        })
        .collect();
    json!({"version":1,"items":items})
}
fn decode(value: &Value) -> Option<ItemScene> {
    if value.get("version")?.as_u64()? != 1 {
        return None;
    }
    let items = value.get("items")?.as_array()?;
    if items.len() > CAPACITY {
        return None;
    }
    let mut scene = ItemScene {
        struct_size: std::mem::size_of::<ItemScene>() as u32,
        count: items.len() as u32,
        items: [EMPTY_ITEM; CAPACITY],
    };
    for (out, v) in scene.items.iter_mut().zip(items) {
        let number = |key: &str| v.get(key)?.as_f64().map(|n| n as f32);
        let flag = |key: &str| v.get(key)?.as_bool().map(u32::from);
        let layer = v.get("layer")?.as_f64()?;
        if layer.fract() != 0.0 || !(-30.0..=30.0).contains(&layer) {
            return None;
        }
        *out = Item {
            asset: io::fixed(v, "asset")?,
            name: io::fixed(v, "name")?,
            x: number("x")?,
            y: number("y")?,
            scale: number("scale")?,
            rotation: number("rotation")?,
            opacity: number("opacity")?,
            layer: layer as i32,
            visible: flag("visible")?,
            locked: flag("locked")?,
            follow_model: flag("followModel")?,
        };
        if !super::valid(out) {
            return None;
        }
    }
    Some(scene)
}
unsafe fn execute_inner(
    instance: *mut c_void,
    root: &Path,
    host: &Host,
    operation: u32,
    source: *const c_char,
) -> bool {
    match operation {
        op::LOAD => match io::load(&root.join("scene.json")) {
            Ok(None) => true,
            Ok(Some(value)) => match decode(&value) {
                Some(next) => {
                    host.locked(|| *instance.cast::<ItemScene>() = next);
                    true
                }
                None => false,
            },
            Err(_) => false,
        },
        op::SAVE => host.save(&root.join("scene.json"), &document(&scene(instance, host))),
        op::IMPORT => {
            let mut next = scene(instance, host);
            if next.count as usize >= CAPACITY {
                return false;
            }
            let Some(source) = io::text(source).map(Path::new) else {
                return false;
            };
            let Some(asset) = io::Asset::import(host, root, source, "img", 0) else {
                return false;
            };
            let Some(name) = io::filename(source) else {
                return false;
            };
            let item = Item {
                asset: bongo_plugin::text::buffer(&asset.name),
                name: bongo_plugin::text::buffer(&name),
                x: 50.0,
                y: 50.0,
                scale: 25.0,
                opacity: 100.0,
                visible: 1,
                follow_model: 1,
                ..EMPTY_ITEM
            };
            if !super::valid(&item) {
                return false;
            }
            next.items[next.count as usize] = item;
            next.count += 1;
            if !host.save(&root.join("scene.json"), &document(&next)) {
                return false;
            }
            host.locked(|| *instance.cast::<ItemScene>() = next);
            asset.commit();
            true
        }
        _ => false,
    }
}
unsafe extern "C" fn execute(
    instance: *mut c_void,
    root: *const c_char,
    host: *const Host,
    operation: u32,
    _index: u32,
    _key: *const c_char,
    source: *const c_char,
    _flags: u32,
) -> u32 {
    bongo_plugin::guard::guard(0, || {
        let (Some(root), Some(host)) = (io::text(root), Host::get(host)) else {
            return 0;
        };
        if instance.is_null() || root.is_empty() {
            return 0;
        }
        execute_inner(instance, Path::new(root), host, operation, source) as u32
    })
}
static API: io::Api = io::Api {
    abi_version: io::ABI,
    struct_size: std::mem::size_of::<io::Api>() as u32,
    execute,
};
#[no_mangle]
pub extern "C" fn bongo_cat_items_io_query_v1(abi: u32) -> *const io::Api {
    if abi == io::ABI {
        &API
    } else {
        std::ptr::null()
    }
}
bongo_plugin::export_completion_events!(
    bongo_cat_items_events_query_v1,
    c"pages.preference.items.saved",
    c"Items saved",
    c"pages.preference.items.failed",
    c"Unable to update items"
);
#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn scenes_round_trip_and_reject_bad_transforms() {
        let item = Item {
            asset: bongo_plugin::text::buffer(&format!("{}.img", "a".repeat(64))),
            name: bongo_plugin::text::buffer("cat"),
            x: 50.0,
            y: 50.0,
            scale: 25.0,
            opacity: 100.0,
            ..EMPTY_ITEM
        };
        let mut scene = ItemScene {
            struct_size: std::mem::size_of::<ItemScene>() as u32,
            count: 1,
            items: [EMPTY_ITEM; CAPACITY],
        };
        scene.items[0] = item;
        let mut value = document(&scene);
        assert_eq!(decode(&value), Some(scene));
        value["items"][0]["x"] = json!(101);
        assert!(decode(&value).is_none());
        value["items"][0]["x"] = json!(50);
        value["items"][0]["layer"] = json!(0.5);
        assert!(decode(&value).is_none());
    }
}
