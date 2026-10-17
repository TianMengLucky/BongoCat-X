use super::{SoundBinding, SoundScene, Sounds, EMPTY_BINDING, SOUND_KEYS};
use bongo_plugin::io::{self, json, operation as op, Host, Value};
use bongo_plugin::page::text;
use std::ffi::{c_char, c_void};
use std::path::Path;

unsafe fn scene(instance: *mut c_void, host: &Host) -> SoundScene {
    host.locked(|| (*instance.cast::<Sounds>()).scene)
}
fn document(scene: &SoundScene) -> Value {
    let mut bindings = json!({});
    for (key, binding) in SOUND_KEYS.iter().zip(&scene.bindings) {
        if binding.asset[0] != 0 {
            bindings[text(&key.id)] =
                json!({"asset":text(&binding.asset), "name":text(&binding.name)});
        }
    }
    json!({"version":1, "bindings":bindings})
}
fn decode(value: &Value, mut scene: SoundScene) -> Option<SoundScene> {
    if value.get("version")?.as_u64()? != 1 {
        return None;
    }
    let bindings = value.get("bindings")?.as_object()?;
    if bindings.len() > scene.bindings.len() {
        return None;
    }
    for (key, value) in bindings {
        let index = SOUND_KEYS.iter().position(|v| text(&v.id) == key)?;
        let binding = SoundBinding {
            asset: io::fixed(value, "asset")?,
            name: io::fixed(value, "name")?,
        };
        if !super::valid(&binding) {
            return None;
        }
        scene.bindings[index] = binding;
    }
    Some(scene)
}
unsafe fn bind(
    instance: *mut c_void,
    root: &Path,
    host: &Host,
    index: usize,
    binding: SoundBinding,
) -> bool {
    if index >= SOUND_KEYS.len() || !super::valid(&binding) {
        return false;
    }
    let mut next = scene(instance, host);
    let before = next.bindings[index];
    next.bindings[index] = binding;
    // Publish only after the durable save succeeds; no rollback is observable.
    if !host.save(&root.join("bindings.json"), &document(&next)) {
        return false;
    }
    host.locked(|| (*instance.cast::<Sounds>()).scene = next);
    if before.asset[0] != 0 {
        host.stop(&root.join(text(&before.asset)));
    }
    true
}
unsafe fn execute_inner(
    instance: *mut c_void,
    root: &Path,
    host: &Host,
    operation: u32,
    index: u32,
    key: *const c_char,
    source: *const c_char,
    flags: u32,
) -> bool {
    match operation {
        op::LOAD => match io::load(&root.join("bindings.json")) {
            Ok(None) => true,
            Ok(Some(value)) => match decode(&value, scene(instance, host)) {
                Some(next) => {
                    host.locked(|| (*instance.cast::<Sounds>()).scene = next);
                    true
                }
                None => false,
            },
            Err(_) => false,
        },
        op::SAVE => host.save(
            &root.join("bindings.json"),
            &document(&scene(instance, host)),
        ),
        op::CLEAR => bind(instance, root, host, index as usize, EMPTY_BINDING),
        op::IMPORT => {
            let Some(key) = io::text(key) else {
                return false;
            };
            let Some(index) = SOUND_KEYS.iter().position(|v| text(&v.id) == key) else {
                return false;
            };
            let Some(source) = io::text(source).map(Path::new) else {
                return false;
            };
            let Some(asset) = io::Asset::import(host, root, source, "snd", 1) else {
                return false;
            };
            let Some(name) = io::filename(source) else {
                return false;
            };
            let binding = SoundBinding {
                asset: bongo_plugin::text::buffer(&asset.name),
                name: bongo_plugin::text::buffer(&name),
            };
            if !bind(instance, root, host, index, binding) {
                return false;
            }
            let file = asset.file.clone();
            asset.commit();
            // A device failure must not roll back a successfully saved binding.
            host.play(&file)
        }
        op::PREVIEW => {
            let scene = scene(instance, host);
            let Some(binding) = scene
                .bindings
                .get(index as usize)
                .filter(|_| index < scene.count)
            else {
                return false;
            };
            binding.asset[0] == 0 || host.play(&root.join(text(&binding.asset)))
        }
        op::INPUT => {
            let mut binding = EMPTY_BINDING;
            let pressed = host.locked(|| super::press(instance, key, index, &mut binding));
            pressed == 0 || flags == 0 || host.play(&root.join(text(&binding.asset)))
        }
        op::RELEASE => {
            host.locked(|| (*instance.cast::<Sounds>()).held.fill(0));
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
    index: u32,
    key: *const c_char,
    source: *const c_char,
    flags: u32,
) -> u32 {
    bongo_plugin::guard::guard(0, || {
        let (Some(root), Some(host)) = (io::text(root), Host::get(host)) else {
            return 0;
        };
        if instance.is_null() || root.is_empty() {
            return 0;
        }
        execute_inner(
            instance,
            Path::new(root),
            host,
            operation,
            index,
            key,
            source,
            flags,
        ) as u32
    })
}
static API: io::Api = io::Api {
    abi_version: io::ABI,
    struct_size: std::mem::size_of::<io::Api>() as u32,
    execute,
};
#[no_mangle]
pub extern "C" fn bongo_cat_sounds_io_query_v1(abi: u32) -> *const io::Api {
    if abi == io::ABI {
        &API
    } else {
        std::ptr::null()
    }
}
bongo_plugin::export_completion_events!(
    bongo_cat_sounds_events_query_v1,
    c"pages.preference.sounds.saved",
    c"Sound effect saved",
    c"pages.preference.sounds.failed",
    c"Unable to update sound effects"
);
#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn bindings_round_trip_and_reject_invalid_records() {
        unsafe {
            let instance = super::super::create();
            let scene = (*instance.cast::<Sounds>()).scene;
            let value = document(&scene);
            assert!(decode(&value, scene).is_some());
            assert!(decode(
                &json!({"version":1,"bindings":{"Unknown":{"asset":"","name":""}}}),
                scene
            )
            .is_none());
            assert!(decode(
                &json!({"version":1,"bindings":{"KeyA":{"asset":"../x","name":"x"}}}),
                scene
            )
            .is_none());
            super::super::destroy(instance);
        }
    }
}
