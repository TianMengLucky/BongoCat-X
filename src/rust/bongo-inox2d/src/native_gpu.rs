//! Native backends borrow the host device; there is no frame transfer through CPU memory.
use glam::Mat4;
use inox2d::model::Model;
use std::collections::HashSet;
pub enum NativeGpu {
    #[cfg(any(target_os = "windows", target_os = "linux"))]
    Vulkan(crate::native_vulkan::Vulkan),
    #[cfg(target_os = "macos")]
    Metal(crate::native_metal::Metal),
}
impl NativeGpu {
    pub unsafe fn new(
        backend: i32,
        model: &Model,
        hidden: HashSet<u32>,
        large_allocation: bool,
        parallel_recording: bool,
    ) -> Result<Self, String> {
        let mut info = crate::abi::RhiInfo::default();
        if !crate::host()
            .device_info
            .ok_or("Missing host device service")?(&mut info)
            || info.backend != backend
        {
            return Err("Host graphics device is unavailable".into());
        }
        let _ = (large_allocation, parallel_recording); // Metal keeps its native allocator.
        match backend {
            #[cfg(any(target_os = "windows", target_os = "linux"))]
            1 => Ok(Self::Vulkan(crate::native_vulkan::Vulkan::new(
                info,
                model,
                hidden,
                large_allocation,
                parallel_recording,
            )?)),
            #[cfg(target_os = "macos")]
            2 => Ok(Self::Metal(crate::native_metal::Metal::new(
                info, model, hidden,
            )?)),
            _ => Err("Native backend is unavailable on this platform".into()),
        }
    }
    pub unsafe fn draw(
        &self,
        model: &Model,
        width: i32,
        height: i32,
        matrix: Mat4,
    ) -> Result<(), String> {
        match self {
            #[cfg(any(target_os = "windows", target_os = "linux"))]
            Self::Vulkan(renderer) => renderer.draw(model, width, height, matrix),
            #[cfg(target_os = "macos")]
            Self::Metal(renderer) => renderer.draw(model, width, height, matrix),
        }
    }
}
