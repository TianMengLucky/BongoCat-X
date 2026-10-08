//! ABI v1 mirrors include/bongo_cat/model_plugin.h. Nullable slots advertise unsupported features.
use std::ffi::{c_char, c_void};
pub type RuntimeHandle = c_void;
pub type Progress = Option<unsafe extern "C" fn(*mut c_void, f32)>;
#[repr(C)]
pub struct Error {
    pub code: i32,
    pub message: [c_char; 256],
}
#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct RhiInfo {
    pub backend: i32,
    pub vulkan_instance: *mut c_void,
    pub vulkan_device: *mut c_void,
    pub vulkan_physical_device: *mut c_void,
    pub vulkan_command_pool: *mut c_void,
    pub vulkan_queue: *mut c_void,
    pub queue_family: u32,
    pub image_count: u32,
    pub extent_width: u32,
    pub extent_height: u32,
    pub color_format: i32,
    pub depth_format: i32,
    pub swapchain_views: *mut *mut c_void,
    pub current_image: *mut c_void,
    pub current_view: *mut c_void,
    pub metal_device: *mut c_void,
    pub metal_layer: *mut c_void,
    pub rhi_handle: *const c_void,
}
#[repr(C)]
#[derive(Default)]
pub struct VulkanFrame {
    pub image: u64,
    pub view: u64,
}
#[repr(C)]
#[derive(Default)]
pub struct MetalFrame {
    pub command_buffer: *mut c_void,
    pub render_pass: *mut c_void,
    pub color_texture: *mut c_void,
}
#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct Frame {
    pub left: f32,
    pub top: f32,
    pub right: f32,
    pub bottom: f32,
}
#[repr(C)]
pub struct Range {
    pub minimum: f32,
    pub maximum: f32,
    pub value: f32,
}
#[repr(C)]
pub struct RenderOptions {
    pub mver_projection: bool,
    pub auto_frame: bool,
    pub source_mirror: bool,
    pub custom_pointer_bounds: bool,
    pub pointer_left_handed: bool,
    pub mouse_force_move: bool,
    pub mouse_speed: f32,
    pub projection_scale: f32,
    pub offset_x: f32,
    pub offset_y: f32,
    pub reference_width: i32,
    pub reference_height: i32,
    pub pointer_left: i32,
    pub pointer_top: i32,
    pub pointer_right: i32,
    pub pointer_bottom: i32,
}
#[repr(C)]
pub struct TextureOptions {
    pub dynamic_resolution: bool,
    pub render_quality_percent: f32,
    pub display_size: Option<
        unsafe extern "C" fn(
            *mut c_void,
            *const RenderOptions,
            i32,
            i32,
            *mut i32,
            *mut i32,
        ) -> bool,
    >,
    pub display_size_userdata: *mut c_void,
}
#[repr(C)]
pub struct VisualState {
    pub fit_scale: f32,
    pub fit_translate_x: f32,
    pub fit_translate_y: f32,
    pub visible_min_x: f32,
    pub visible_min_y: f32,
    pub visible_max_x: f32,
    pub visible_max_y: f32,
    pub drawable_count: i32,
    pub drawable_visible: i32,
    pub drawable_vertex_changed: i32,
    pub offscreen_count: i32,
    pub offscreen_positive: i32,
    pub part_count: i32,
    pub part_positive: i32,
    pub fitted: bool,
    pub visible: bool,
    pub mver_projection: bool,
}
/// Only this stable prefix is consumed; the host owns additional C++ services.
#[repr(C)]
pub struct Host {
    pub abi_version: u32,
    pub struct_size: u32,
    pub core_symbol: Option<unsafe extern "C" fn(*const c_char) -> *mut c_void>,
    pub gl_proc: Option<unsafe extern "C" fn(*const c_char) -> *mut c_void>,
    pub log: Option<unsafe extern "C" fn(i32, *const c_char)>,
    pub device_info: Option<unsafe extern "C" fn(*mut RhiInfo) -> bool>,
    pub vulkan_frame: Option<unsafe extern "C" fn(*const c_void, *mut VulkanFrame) -> bool>,
    pub metal_frame: Option<unsafe extern "C" fn(*const c_void, *mut MetalFrame) -> bool>,
    pub begin_commands: Option<unsafe extern "C" fn(*const c_void) -> *mut c_void>,
    pub submit_commands: Option<unsafe extern "C" fn(*const c_void, *mut c_void) -> bool>,
    pub wait_idle: Option<unsafe extern "C" fn(*const c_void) -> bool>,
}
#[repr(C)]
pub struct Ops {
    pub create: Option<unsafe extern "C" fn(*const c_char, *mut Error) -> *mut RuntimeHandle>,
    pub destroy: Option<unsafe extern "C" fn(*mut RuntimeHandle) -> ()>,
    pub set_rhi_info: Option<unsafe extern "C" fn(*mut RuntimeHandle, *const RhiInfo) -> ()>,
    pub load: Option<
        unsafe extern "C" fn(
            *mut RuntimeHandle,
            *const c_char,
            *const c_char,
            bool,
            *const RenderOptions,
            Progress,
            *mut c_void,
            *mut Error,
        ) -> i32,
    >,
    pub load_ex: Option<
        unsafe extern "C" fn(
            *mut RuntimeHandle,
            *const c_char,
            *const c_char,
            bool,
            *const RenderOptions,
            *const TextureOptions,
            Progress,
            *mut c_void,
            *mut Error,
        ) -> i32,
    >,
    pub ready: Option<unsafe extern "C" fn(*const RuntimeHandle) -> bool>,
    pub canvas_size: Option<unsafe extern "C" fn(*const RuntimeHandle, *mut i32, *mut i32) -> bool>,
    pub frame: Option<unsafe extern "C" fn(*const RuntimeHandle, *mut Frame) -> bool>,
    pub measure_frame: Option<unsafe extern "C" fn(*mut RuntimeHandle, *mut Frame) -> bool>,
    pub set_frame: Option<unsafe extern "C" fn(*mut RuntimeHandle, *const Frame) -> ()>,
    pub viewport: Option<
        unsafe extern "C" fn(*const RuntimeHandle, *mut i32, *mut i32, *mut i32, *mut i32) -> bool,
    >,
    pub overlay_viewport: Option<
        unsafe extern "C" fn(*const RuntimeHandle, *mut i32, *mut i32, *mut i32, *mut i32) -> bool,
    >,
    pub resize: Option<unsafe extern "C" fn(*mut RuntimeHandle, i32, i32) -> ()>,
    pub reshape: Option<unsafe extern "C" fn(*mut RuntimeHandle, i32, i32) -> ()>,
    pub try_reuse_texture_quality: Option<unsafe extern "C" fn(*mut RuntimeHandle, f32) -> bool>,
    pub texture_refresh_pending: Option<unsafe extern "C" fn(*const RuntimeHandle, bool) -> bool>,
    pub texture_refresh_due: Option<unsafe extern "C" fn(*const RuntimeHandle, bool, bool) -> bool>,
    pub texture_refresh_busy: Option<unsafe extern "C" fn(*const RuntimeHandle) -> bool>,
    pub cancel_texture_refresh: Option<unsafe extern "C" fn(*mut RuntimeHandle) -> ()>,
    pub refresh_textures: Option<unsafe extern "C" fn(*mut RuntimeHandle, bool, bool) -> bool>,
    pub update: Option<unsafe extern "C" fn(*mut RuntimeHandle, f32) -> bool>,
    pub draw: Option<unsafe extern "C" fn(*mut RuntimeHandle) -> ()>,
    pub draw_checked: Option<unsafe extern "C" fn(*mut RuntimeHandle) -> bool>,
    pub set_mirror: Option<unsafe extern "C" fn(*mut RuntimeHandle, bool) -> ()>,
    pub set_vertical_flip: Option<unsafe extern "C" fn(*mut RuntimeHandle, bool) -> ()>,
    pub set_render_options:
        Option<unsafe extern "C" fn(*mut RuntimeHandle, *const RenderOptions) -> ()>,
    pub set_tight_frame: Option<unsafe extern "C" fn(*mut RuntimeHandle, bool) -> ()>,
    pub set_tight_overlay_rect: Option<unsafe extern "C" fn(*mut RuntimeHandle, *const f32) -> ()>,
    pub set_dragging: Option<unsafe extern "C" fn(*mut RuntimeHandle, f32, f32) -> ()>,
    pub set_centered_dragging: Option<unsafe extern "C" fn(*mut RuntimeHandle, f32, f32) -> ()>,
    pub prepare_viewer_audit: Option<unsafe extern "C" fn(*mut RuntimeHandle) -> ()>,
    pub prepare_cover_capture: Option<unsafe extern "C" fn(*mut RuntimeHandle) -> bool>,
    pub set_parameter: Option<unsafe extern "C" fn(*mut RuntimeHandle, *const c_char, f32) -> bool>,
    pub parameter:
        Option<unsafe extern "C" fn(*mut RuntimeHandle, *const c_char, *mut Range) -> bool>,
    pub start_motion: Option<unsafe extern "C" fn(*mut RuntimeHandle, *const c_char, i32) -> bool>,
    pub restore_motion_state:
        Option<unsafe extern "C" fn(*mut RuntimeHandle, *const c_char, i32) -> bool>,
    pub preview_motion:
        Option<unsafe extern "C" fn(*mut RuntimeHandle, *const c_char, i32) -> bool>,
    pub restore_motion_preview: Option<unsafe extern "C" fn(*mut RuntimeHandle) -> bool>,
    pub commit_motion_preview:
        Option<unsafe extern "C" fn(*mut RuntimeHandle, *const c_char, i32) -> bool>,
    pub motion_selected:
        Option<unsafe extern "C" fn(*const RuntimeHandle, *const c_char, i32) -> bool>,
    pub motion_persistent:
        Option<unsafe extern "C" fn(*const RuntimeHandle, *const c_char, i32) -> bool>,
    pub motion_visible:
        Option<unsafe extern "C" fn(*const RuntimeHandle, *const c_char, i32) -> bool>,
    pub motion_same_toggle: Option<
        unsafe extern "C" fn(*const RuntimeHandle, *const c_char, i32, *const c_char, i32) -> bool,
    >,
    pub set_expression: Option<unsafe extern "C" fn(*mut RuntimeHandle, i32) -> bool>,
    pub expression: Option<unsafe extern "C" fn(*const RuntimeHandle) -> i32>,
    pub visual_state: Option<unsafe extern "C" fn(*const RuntimeHandle, *mut VisualState) -> bool>,
}
#[repr(C)]
pub struct Plugin {
    pub abi_version: u32,
    pub struct_size: u32,
    pub engine: u32,
    pub flags: u32,
    pub graphics_backends: u32,
    pub name: *const c_char,
    pub ops: Ops,
}
// Contains only immutable string and code pointers.
unsafe impl Sync for Plugin {}
