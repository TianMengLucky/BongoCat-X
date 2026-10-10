mod abi;
mod bounds;
mod ffi;
mod gl_state;
mod native_gpu;
#[cfg(target_os = "macos")]
mod native_metal;
mod native_plan;
mod native_shader;
#[cfg(any(target_os = "windows", target_os = "linux"))]
mod native_vulkan;
mod parameters;
mod renderer;
mod runtime;
#[cfg(any(target_os = "windows", target_os = "linux"))]
mod vulkan_pipeline;
#[cfg(any(target_os = "windows", target_os = "linux"))]
mod vulkan_recording;
#[cfg(any(target_os = "windows", target_os = "linux"))]
mod vulkan_resources;
use abi::*;
use std::sync::atomic::{AtomicPtr, Ordering};
// The pinned upstream UUID is repr(transparent) over u32 but has no accessor.
pub(crate) fn node_id(id: inox2d::node::InoxNodeUuid) -> u32 {
    unsafe { std::mem::transmute(id) }
}
static HOST: AtomicPtr<Host> = AtomicPtr::new(std::ptr::null_mut());
pub(crate) fn host() -> &'static Host {
    unsafe { &*HOST.load(Ordering::Acquire) }
}
pub(crate) fn guard<T>(fallback: T, action: impl FnOnce() -> T) -> T {
    std::panic::catch_unwind(std::panic::AssertUnwindSafe(action)).unwrap_or(fallback)
}
pub(crate) fn log(message: &str) {
    if let (Some(log), Ok(text)) = (host().log, std::ffi::CString::new(message)) {
        unsafe {
            log(5, text.as_ptr());
        }
    }
}
static PLUGIN: Plugin = Plugin {
    abi_version: 1,
    struct_size: std::mem::size_of::<Plugin>() as u32,
    engine: 2,
    flags: 0,
    graphics_backends: if cfg!(target_os = "macos") { 5 } else { 3 },
    name: b"Inox2D Rust\0".as_ptr().cast(),
    ops: Ops {
        create: Some(ffi::create),
        destroy: Some(ffi::destroy),
        set_rhi_info: Some(ffi::set_rhi_info),
        load: Some(ffi::load),
        load_ex: Some(ffi::load_ex),
        ready: Some(ffi::ready),
        canvas_size: Some(ffi::canvas_size),
        frame: Some(ffi::frame),
        measure_frame: Some(ffi::measure_frame),
        set_frame: Some(ffi::set_frame),
        viewport: Some(ffi::viewport),
        overlay_viewport: Some(ffi::overlay_viewport),
        resize: Some(ffi::resize),
        reshape: Some(ffi::reshape),
        try_reuse_texture_quality: None,
        texture_refresh_pending: None,
        texture_refresh_due: None,
        texture_refresh_busy: None,
        cancel_texture_refresh: None,
        refresh_textures: None,
        update: Some(ffi::update),
        draw: Some(ffi::draw),
        draw_checked: Some(ffi::draw_checked),
        set_mirror: Some(ffi::set_mirror),
        set_vertical_flip: Some(ffi::set_vertical_flip),
        set_render_options: None,
        set_tight_frame: Some(ffi::set_tight_frame),
        set_tight_overlay_rect: None,
        set_dragging: Some(ffi::set_dragging),
        set_centered_dragging: Some(ffi::set_centered_dragging),
        prepare_viewer_audit: None,
        prepare_cover_capture: Some(ffi::prepare_cover_capture),
        set_parameter: Some(ffi::set_parameter),
        parameter: Some(ffi::parameter),
        start_motion: None,
        restore_motion_state: None,
        preview_motion: None,
        restore_motion_preview: None,
        commit_motion_preview: None,
        motion_selected: None,
        motion_persistent: None,
        motion_visible: None,
        motion_same_toggle: None,
        set_expression: None,
        expression: Some(ffi::expression),
        visual_state: Some(ffi::visual_state),
    },
};
/// # Safety
/// The host table lives until all plugin instances and the library are destroyed.
#[no_mangle]
pub unsafe extern "C" fn bongo_cat_model_plugin_query(
    abi: u32,
    host: *const Host,
) -> *const Plugin {
    guard(std::ptr::null(), || {
        if abi != 1
            || host.is_null()
            || (*host).abi_version != 1
            || (*host).struct_size < std::mem::size_of::<Host>() as u32
            || (*host).gl_proc.is_none()
        {
            return std::ptr::null();
        }
        let previous = HOST.compare_exchange(
            std::ptr::null_mut(),
            host.cast_mut(),
            Ordering::AcqRel,
            Ordering::Acquire,
        );
        if previous.is_err_and(|p| p != host.cast_mut()) {
            return std::ptr::null();
        }
        &PLUGIN
    })
}
