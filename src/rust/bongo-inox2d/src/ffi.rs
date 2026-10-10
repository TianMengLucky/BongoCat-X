use crate::{abi::*, guard, runtime::Runtime};
use std::{
    ffi::{c_char, c_void, CStr},
    path::Path,
};

unsafe fn text<'a>(value: *const c_char) -> Option<&'a str> {
    if value.is_null() {
        None
    } else {
        CStr::from_ptr(value).to_str().ok()
    }
}
unsafe fn runtime<'a>(value: *mut RuntimeHandle) -> Option<&'a mut Runtime> {
    value.cast::<Runtime>().as_mut()
}
unsafe fn error(target: *mut Error, message: &str) {
    if let Some(target) = target.as_mut() {
        target.code = 3;
        target.message.fill(0);
        for (dest, src) in target.message.iter_mut().take(255).zip(message.bytes()) {
            *dest = src as c_char;
        }
    }
}
/// Optional v1 policy extension; returns only features implemented here.
/// # Safety
/// `value` is an instance owned by this plugin; configure before loading.
#[no_mangle]
pub unsafe extern "C" fn bongo_cat_model_plugin_optimize_v1(
    value: *mut RuntimeHandle,
    requested: u32,
    backend: u32,
) -> u32 {
    guard(0, || {
        let Some(r) = runtime(value) else {
            return 0;
        };
        if r.loaded.is_some() {
            return 0;
        }
        let native_backend = (cfg!(any(target_os = "windows", target_os = "linux"))
            && backend == 1)
            || (cfg!(target_os = "macos") && backend == 2);
        r.large_allocation = native_backend && requested & 8 != 0;
        r.parallel_recording = cfg!(any(target_os = "windows", target_os = "linux"))
            && backend == 1
            && requested & 1 != 0
            && std::thread::available_parallelism().is_ok_and(|n| n.get() > 1);
        u32::from(r.large_allocation) * 8 | u32::from(r.parallel_recording)
    })
}

pub unsafe extern "C" fn create(_: *const c_char, _: *mut Error) -> *mut RuntimeHandle {
    guard(std::ptr::null_mut(), || {
        Box::into_raw(Box::new(Runtime::new())).cast()
    })
}
pub unsafe extern "C" fn destroy(value: *mut RuntimeHandle) {
    guard((), || {
        if !value.is_null() {
            drop(Box::from_raw(value.cast::<Runtime>()));
        }
    });
}
pub unsafe extern "C" fn set_rhi_info(value: *mut RuntimeHandle, info: *const RhiInfo) {
    guard((), || {
        if let (Some(r), Some(info)) = (runtime(value), info.as_ref()) {
            r.backend = info.backend;
        }
    });
}
pub unsafe extern "C" fn load_ex(
    value: *mut RuntimeHandle,
    directory: *const c_char,
    setting: *const c_char,
    _: bool,
    _: *const RenderOptions,
    textures: *const TextureOptions,
    progress: Progress,
    userdata: *mut c_void,
    failure: *mut Error,
) -> i32 {
    let result = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        let r = runtime(value).ok_or("Missing Inox2D instance")?;
        let dir = text(directory).ok_or("Invalid UTF-8 model directory")?;
        let setting = text(setting).ok_or("Invalid UTF-8 model filename")?;
        if let Some(progress) = progress {
            progress(userdata, 0.1);
        }
        let quality = textures
            .as_ref()
            .map(|t| t.render_quality_percent)
            .unwrap_or(100.0);
        r.load(
            Path::new(dir),
            setting,
            if quality.is_finite() { quality } else { 100.0 },
        )?;
        if let Some(progress) = progress {
            progress(userdata, 1.0);
        }
        Ok::<(), String>(())
    }));
    match result {
        Ok(Ok(())) => 0,
        Ok(Err(message)) => {
            error(failure, &message);
            3
        }
        Err(_) => {
            error(failure, "Inox2D rejected the model during loading");
            3
        }
    }
}
pub unsafe extern "C" fn load(
    value: *mut RuntimeHandle,
    directory: *const c_char,
    setting: *const c_char,
    preset: bool,
    options: *const RenderOptions,
    progress: Progress,
    userdata: *mut c_void,
    failure: *mut Error,
) -> i32 {
    load_ex(
        value,
        directory,
        setting,
        preset,
        options,
        std::ptr::null(),
        progress,
        userdata,
        failure,
    )
}
pub unsafe extern "C" fn ready(value: *const RuntimeHandle) -> bool {
    guard(false, || {
        value
            .cast::<Runtime>()
            .as_ref()
            .is_some_and(|r| r.loaded.is_some())
    })
}
pub unsafe extern "C" fn canvas_size(
    value: *const RuntimeHandle,
    width: *mut i32,
    height: *mut i32,
) -> bool {
    guard(false, || {
        let Some(l) = value
            .cast::<Runtime>()
            .as_ref()
            .and_then(|r| r.loaded.as_ref())
        else {
            return false;
        };
        if let Some(w) = width.as_mut() {
            *w = l.base.size().x.ceil() as i32;
        }
        if let Some(h) = height.as_mut() {
            *h = l.base.size().y.ceil() as i32;
        }
        true
    })
}
pub unsafe extern "C" fn frame(value: *const RuntimeHandle, target: *mut Frame) -> bool {
    guard(false, || {
        if let (Some(r), Some(t)) = (value.cast::<Runtime>().as_ref(), target.as_mut()) {
            *t = r.frame;
            return r.loaded.is_some();
        }
        false
    })
}
pub unsafe extern "C" fn measure_frame(value: *mut RuntimeHandle, target: *mut Frame) -> bool {
    guard(false, || {
        if let (Some(r), Some(t)) = (runtime(value), target.as_mut()) {
            *t = r.required_frame();
            return r.loaded.is_some();
        }
        false
    })
}
pub unsafe extern "C" fn set_frame(value: *mut RuntimeHandle, target: *const Frame) {
    guard((), || {
        if let (Some(r), Some(t)) = (runtime(value), target.as_ref()) {
            if [t.left, t.top, t.right, t.bottom]
                .iter()
                .all(|v| v.is_finite() && *v > -1.0)
                && 1.0 + t.left + t.right >= 0.01
                && 1.0 + t.top + t.bottom >= 0.01
            {
                r.frame = *t;
            }
        }
    });
}
pub unsafe extern "C" fn viewport(
    value: *const RuntimeHandle,
    x: *mut i32,
    y: *mut i32,
    width: *mut i32,
    height: *mut i32,
) -> bool {
    guard(false, || {
        let Some(r) = value.cast::<Runtime>().as_ref() else {
            return false;
        };
        for (target, v) in [x, y, width, height].into_iter().zip(r.viewport()) {
            if let Some(t) = target.as_mut() {
                *t = v;
            }
        }
        r.loaded.is_some()
    })
}
pub unsafe extern "C" fn overlay_viewport(
    value: *const RuntimeHandle,
    x: *mut i32,
    y: *mut i32,
    width: *mut i32,
    height: *mut i32,
) -> bool {
    viewport(value, x, y, width, height)
}
pub unsafe extern "C" fn resize(value: *mut RuntimeHandle, width: i32, height: i32) {
    guard((), || {
        if let Some(r) = runtime(value) {
            r.width = width.max(1);
            r.height = height.max(1);
        }
    });
}
pub unsafe extern "C" fn reshape(value: *mut RuntimeHandle, width: i32, height: i32) {
    resize(value, width, height);
}
pub unsafe extern "C" fn update(value: *mut RuntimeHandle, dt: f32) -> bool {
    guard(false, || runtime(value).is_some_and(|r| r.update(dt)))
}
pub unsafe extern "C" fn draw_checked(value: *mut RuntimeHandle) -> bool {
    guard(false, || {
        let Some(r) = runtime(value) else {
            return false;
        };
        match r.draw() {
            Ok(()) => true,
            Err(message) => {
                crate::log(&message);
                false
            }
        }
    })
}
pub unsafe extern "C" fn draw(value: *mut RuntimeHandle) {
    draw_checked(value);
}
pub unsafe extern "C" fn set_mirror(value: *mut RuntimeHandle, mirror: bool) {
    guard((), || {
        if let Some(r) = runtime(value) {
            r.mirror = mirror;
        }
    });
}
pub unsafe extern "C" fn set_vertical_flip(value: *mut RuntimeHandle, flipped: bool) {
    guard((), || {
        if let Some(r) = runtime(value) {
            r.flipped = flipped;
        }
    });
}
pub unsafe extern "C" fn set_tight_frame(value: *mut RuntimeHandle, tight: bool) {
    guard((), || {
        if let Some(r) = runtime(value) {
            r.tight = tight;
        }
    });
}
pub unsafe extern "C" fn set_parameter(
    value: *mut RuntimeHandle,
    id: *const c_char,
    v: f32,
) -> bool {
    guard(false, || {
        if let (Some(l), Some(id)) = (runtime(value).and_then(|r| r.loaded.as_mut()), text(id)) {
            return l.parameters.set(id, v);
        }
        false
    })
}
pub unsafe extern "C" fn parameter(
    value: *mut RuntimeHandle,
    id: *const c_char,
    range: *mut Range,
) -> bool {
    guard(false, || {
        if let (Some(l), Some(id), Some(target)) = (
            runtime(value).and_then(|r| r.loaded.as_ref()),
            text(id),
            range.as_mut(),
        ) {
            if let Some(value) = l.parameters.range(id) {
                *target = value;
                return true;
            }
        }
        false
    })
}
pub unsafe extern "C" fn set_dragging(value: *mut RuntimeHandle, x: f32, y: f32) {
    guard((), || {
        if let Some(l) = runtime(value).and_then(|r| r.loaded.as_mut()) {
            l.parameters.set("ParamAngleX", x * 30.0);
            l.parameters.set("ParamAngleY", y * 30.0);
        }
    });
}
pub unsafe extern "C" fn set_centered_dragging(value: *mut RuntimeHandle, x: f32, y: f32) {
    set_dragging(value, x, y);
}
pub unsafe extern "C" fn expression(_: *const RuntimeHandle) -> i32 {
    -1
}
pub unsafe extern "C" fn prepare_cover_capture(value: *mut RuntimeHandle) -> bool {
    ready(value)
}
pub unsafe extern "C" fn visual_state(
    value: *const RuntimeHandle,
    target: *mut VisualState,
) -> bool {
    guard(false, || {
        let Some(l) = value
            .cast::<Runtime>()
            .as_ref()
            .and_then(|r| r.loaded.as_ref())
        else {
            return false;
        };
        let Some(t) = target.as_mut() else {
            return false;
        };
        *t = std::mem::zeroed();
        t.fit_scale = 1.0;
        t.visible_min_x = l.current.min.x;
        t.visible_min_y = l.current.min.y;
        t.visible_max_x = l.current.max.x;
        t.visible_max_y = l.current.max.y;
        t.drawable_count = l.current.count;
        t.drawable_visible = l.current.count;
        t.fitted = true;
        t.visible = true;
        true
    })
}

#[cfg(test)]
mod optimization_tests {
    use super::*;
    #[test]
    fn reports_only_enabled_features_and_clears_them_when_disabled() {
        let mut r = Runtime::new();
        let pointer = (&mut r as *mut Runtime).cast::<RuntimeHandle>();
        unsafe {
            let expected = if cfg!(any(target_os = "windows", target_os = "linux")) {
                8 | u32::from(std::thread::available_parallelism().is_ok_and(|n| n.get() > 1))
            } else {
                0
            };
            assert_eq!(bongo_cat_model_plugin_optimize_v1(pointer, 15, 1), expected);
            assert_eq!(r.large_allocation, expected & 8 != 0);
            assert_eq!(r.parallel_recording, expected & 1 != 0);
            assert_eq!(bongo_cat_model_plugin_optimize_v1(pointer, 0, 1), 0);
            assert!(!r.large_allocation);
            assert!(!r.parallel_recording);
            assert_eq!(bongo_cat_model_plugin_optimize_v1(pointer, 15, 0), 0);
            let metal = if cfg!(target_os = "macos") { 8 } else { 0 };
            assert_eq!(bongo_cat_model_plugin_optimize_v1(pointer, 15, 2), metal);
            assert_eq!(r.large_allocation, metal != 0);
            assert!(!r.parallel_recording);
            assert_eq!(bongo_cat_model_plugin_optimize_v1(pointer, 6, 2), 0);
            assert_eq!(bongo_cat_model_plugin_optimize_v1(pointer, 0, 2), 0);
            assert!(!r.large_allocation);
            assert_eq!(bongo_cat_model_plugin_optimize_v1(pointer, 6, 1), 0);
            assert_eq!(bongo_cat_model_plugin_optimize_v1(pointer, 16, 1), 0);
            assert_eq!(
                bongo_cat_model_plugin_optimize_v1(std::ptr::null_mut(), 15, 1),
                0
            );
        }
    }
}
