use super::*;
#[repr(C)]
pub struct Calibration {
    scale: f64,
    x: f64,
    y: f64,
    width: i32,
    height: i32,
    mirror: bool,
    auto_frame: bool,
}
#[no_mangle]
pub unsafe extern "C" fn bongo_safe_tauri_config(
    path: *const c_char,
    mode: *const c_char,
    left: *const i32,
    left_count: usize,
    right: *const i32,
    right_count: usize,
    calibration: *const Calibration,
    size: usize,
) -> bool {
    crate::guard(false, || {
        if size != std::mem::size_of::<Calibration>()
            || left_count > 256
            || right_count > 256
            || (left.is_null() && left_count != 0)
            || (right.is_null() && right_count != 0)
        {
            return false;
        }
        let Some(c) = calibration.as_ref() else {
            return false;
        };
        let (Ok(path), Ok(mode)) = (http::text(path), http::text(mode)) else {
            return false;
        };
        if !matches!(mode, "standard" | "keyboard" | "gamepad") {
            return false;
        }
        let matrix = |p: *const i32, n| {
            if n == 0 {
                Vec::new()
            } else {
                std::slice::from_raw_parts(p, n)
                    .iter()
                    .map(|code| json!([code]))
                    .collect::<Vec<_>>()
            }
        };
        let mut result = json!({"decoration":{"window_size":[c.width,c.height],"l2d_offset":[c.x,c.y],
            "l2d_correct":c.scale,"l2d_horizontal_flip":c.mirror,"l2d_auto_frame":c.auto_frame},
            "mode":match mode {"standard"=>1,"keyboard"=>2,_=>3}});
        let mut section = json!({"l2d":true,"keyboard":[],"face":[],"sounds":[],"l2d_expression":[],"l2d_motion":[],"l2d_motion_lockhand":[]});
        if mode == "standard" {
            section["hand"] = json!(matrix(left, left_count));
            section["mouse"] = json!(false);
        } else {
            section["lefthand"] = json!(matrix(left, left_count));
            section["righthand"] = json!(matrix(right, right_count));
        }
        if mode == "gamepad" {
            section["input_mode"] = json!(1);
        }
        result[mode] = section;
        write(Path::new(path), &result).is_ok()
    })
}
#[no_mangle]
pub unsafe extern "C" fn bongo_safe_tauri_calibration(
    path: *const c_char,
    mode: *const c_char,
    calibration: *mut Calibration,
    size: usize,
) -> bool {
    crate::guard(false, || {
        if size != std::mem::size_of::<Calibration>() {
            return false;
        }
        let Some(c) = calibration.as_mut() else {
            return false;
        };
        let (Ok(path), Ok(mode)) = (http::text(path), http::text(mode)) else {
            return false;
        };
        let Ok(root) = read(Path::new(path)).and_then(|text| parse(&text)) else {
            return false;
        };
        if root["schemaVersion"].as_i64() != Some(1)
            || root["kind"].as_str() != Some("bongo-cat-mver-source")
            || root["mode"].as_str().is_some_and(|m| m != mode)
            || !root["decoration"].is_object()
        {
            return false;
        }
        let d = &root["decoration"];
        c.x = 0.0;
        c.y = 0.0;
        c.mirror = false;
        let scale = d["l2d_correct"]
            .as_f64()
            .filter(|v| v.is_finite() && *v > 0.01 && *v <= 100.0);
        let window = d["window_size"]
            .as_array()
            .filter(|a| a.len() >= 2)
            .and_then(|a| {
                let (w, h) = (a[0].as_f64()?, a[1].as_f64()?);
                ((64.0..=8192.0).contains(&w)
                    && (64.0..=8192.0).contains(&h)
                    && w.fract() == 0.0
                    && h.fract() == 0.0)
                    .then_some((w as i32, h as i32))
            });
        if let Some(scale) = scale {
            c.scale = scale;
        }
        if let Some((w, h)) = window {
            c.width = w;
            c.height = h;
        }
        if let Some(offset) = d["l2d_offset"].as_array().filter(|a| a.len() >= 2) {
            if let (Some(x), Some(y)) = (offset[0].as_f64(), offset[1].as_f64()) {
                if (-100.0..=100.0).contains(&x) && (-100.0..=100.0).contains(&y) {
                    c.x = x;
                    c.y = y;
                }
            }
        }
        c.auto_frame = scale.is_none() || window.is_none();
        if let Some(mirror) = d["l2d_horizontal_flip"].as_bool() {
            c.mirror = mirror;
        }
        true
    })
}
