use super::*;
use std::ffi::c_void;
#[repr(C)]
pub struct Options {
    projection: bool,
    auto_frame: bool,
    mirror: bool,
    bounds: bool,
    left_handed: bool,
    force_move: bool,
    speed: f32,
    scale: f32,
    x: f32,
    y: f32,
    width: i32,
    height: i32,
    left: i32,
    top: i32,
    right: i32,
    bottom: i32,
}
fn number(v: &Value, fallback: f64) -> f64 {
    v.as_f64().filter(|n| n.is_finite()).unwrap_or(fallback)
}
fn coordinate(v: &Value, fallback: i32) -> i32 {
    let value = number(v, f64::from(fallback));
    if value >= f64::from(i32::MIN) && value <= f64::from(i32::MAX) {
        value as i32
    } else {
        fallback
    }
}
fn options(config: &Value) -> Options {
    let d = &config["decoration"];
    let a = &config["workarea"];
    let mut result = Options {
        projection: true,
        auto_frame: d["l2d_auto_frame"].as_bool().unwrap_or(false),
        mirror: d["l2d_horizontal_flip"].as_bool().unwrap_or(false),
        bounds: a["workarea"].as_bool().unwrap_or(false),
        left_handed: d["leftHanded"].as_bool().unwrap_or(false),
        force_move: d["mouse_force_move"].as_bool().unwrap_or(false),
        speed: number(&d["mouse_speed"], 1.0) as f32,
        scale: number(&d["l2d_correct"], 1.1) as f32,
        x: number(&d["l2d_offset"][0], 0.0) as f32,
        y: number(&d["l2d_offset"][1], 0.0) as f32,
        width: coordinate(&d["window_size"][0], 612),
        height: coordinate(&d["window_size"][1], 352),
        left: coordinate(&a["top_left"][0], 0),
        top: coordinate(&a["top_left"][1], 0),
        right: coordinate(&a["right_bottom"][0], 0),
        bottom: coordinate(&a["right_bottom"][1], 0),
    };
    if !(0.0..=100.0).contains(&result.scale) || result.scale == 0.0 {
        result.scale = 1.1;
    }
    if result.width <= 0 || result.height <= 0 {
        result.width = 612;
        result.height = 352;
    }
    if result.right <= result.left || result.bottom <= result.top {
        result.bounds = false;
    }
    result
}
pub(super) fn metadata(package: &Package<'_>, config: &Value, output: &mut Value) {
    let o = options(config);
    output["render"] = json!({"profile":"mver-0.1.6","autoFrame":o.auto_frame,
        "projectionScale":o.scale,"offsetX":o.x,"offsetY":o.y,"referenceWidth":o.width,"referenceHeight":o.height,
        "mirror":o.mirror,"pointerLeftHanded":o.left_handed,"mouseForceMove":o.force_move,"mouseSpeed":o.speed,
        "customPointerBounds":o.bounds,"pointerLeft":o.left,"pointerTop":o.top,"pointerRight":o.right,"pointerBottom":o.bottom});
    if package.mode != "standard" {
        return;
    }
    let d = &config["decoration"];
    let standard = &config["standard"];
    let mouse = standard["mouse"].as_bool().unwrap_or(false);
    let live2d = standard["l2d"].as_bool().unwrap_or(true);
    let usable = |name: &str| {
        asset(package, Path::new(name))
            .and_then(|p| dimensions(&p))
            .is_some_and(|(w, h)| w > 1 || h > 1)
    };
    let enabled = (!standard["l2d"].as_bool().unwrap_or(false)
        || config["mode"].as_i64() == Some(98))
        && usable("arm.png")
        && usable(if mouse { "mouse.png" } else { "tablet.png" });
    let index = usize::from(!mouse);
    let hand = if live2d {
        &standard["hand_offset"]
    } else {
        &d["hand_offset"]
    };
    let device = if mouse { "mouse" } else { "tablet" };
    output["standardPointer"] = json!({"enabled":enabled,"mouse":mouse,"leftHanded":d["leftHanded"].as_bool().unwrap_or(false),
        "arm":"resources/mver-pointer/arm.png","device":format!("resources/mver-pointer/{device}.png"),
        "left":format!("resources/mver-pointer/{device}_left.png"),"right":format!("resources/mver-pointer/{device}_right.png"),
        "side":if mouse { "resources/mver-pointer/mouse_side.png" } else { "" },
        "offsetX":number(&d["offsetX"][index],0.0),"offsetY":number(&d["offsetY"][index],0.0),
        "scale":number(&d["scalar"][index],1.0),"handOffsetX":number(&hand[0],0.0),"handOffsetY":number(&hand[1],0.0),
        "lineRed":coordinate(&d["armLineColor"][0],0),"lineGreen":coordinate(&d["armLineColor"][1],0),"lineBlue":coordinate(&d["armLineColor"][2],0)});
}
#[no_mangle]
pub unsafe extern "C" fn bongo_safe_mver_render_options(
    config: *const crate::json_dom::Node,
    output: *mut c_void,
    size: usize,
) -> bool {
    crate::guard(false, || {
        if output.is_null() || size != std::mem::size_of::<Options>() {
            return false;
        }
        let Ok(config) = crate::json_dom::to_value(config, 0) else {
            return false;
        };
        output.cast::<Options>().write(options(&config));
        true
    })
}
