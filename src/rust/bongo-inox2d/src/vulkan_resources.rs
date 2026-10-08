//! RAII for plugin-owned resources. The host retains ownership of instance/device/queue.
use crate::abi::RhiInfo;
use ash::{vk, vk::Handle};
use gpu_allocator::{
    vulkan::{Allocation, AllocationCreateDesc, AllocationScheme, Allocator, AllocatorCreateDesc},
    MemoryLocation,
};
use std::{cell::RefCell, rc::Rc};
pub struct Gpu {
    pub device: ash::Device,
    pub instance: ash::Instance,
    pub allocator: RefCell<Allocator>,
    pub info: RhiInfo,
    _entry: ash::Entry,
}
impl Gpu {
    pub unsafe fn new(info: RhiInfo) -> Result<Rc<Self>, String> {
        if info.vulkan_instance.is_null()
            || info.vulkan_device.is_null()
            || info.rhi_handle.is_null()
        {
            return Err("Missing Vulkan host handles".into());
        }
        let entry = ash::Entry::load().map_err(|e| e.to_string())?;
        let instance = ash::Instance::load(
            entry.static_fn(),
            vk::Instance::from_raw(info.vulkan_instance as usize as u64),
        );
        let device = ash::Device::load(
            instance.fp_v1_0(),
            vk::Device::from_raw(info.vulkan_device as usize as u64),
        );
        let allocator = Allocator::new(&AllocatorCreateDesc {
            instance: instance.clone(),
            device: device.clone(),
            physical_device: vk::PhysicalDevice::from_raw(
                info.vulkan_physical_device as usize as u64,
            ),
            debug_settings: Default::default(),
            buffer_device_address: false,
            allocation_sizes: Default::default(),
        })
        .map_err(|e| e.to_string())?;
        Ok(Rc::new(Self {
            device,
            instance,
            allocator: RefCell::new(allocator),
            info,
            _entry: entry,
        }))
    }
    pub unsafe fn buffer(
        self: &Rc<Self>,
        bytes: &[u8],
        usage: vk::BufferUsageFlags,
    ) -> Result<Buffer, String> {
        let raw = self
            .device
            .create_buffer(
                &vk::BufferCreateInfo::builder()
                    .size(bytes.len().max(1) as u64)
                    .usage(usage),
                None,
            )
            .map_err(|e| e.to_string())?;
        let mut result = Buffer {
            gpu: self.clone(),
            raw,
            allocation: None,
        };
        let allocation = self
            .allocator
            .borrow_mut()
            .allocate(&AllocationCreateDesc {
                name: "Inox2D buffer",
                requirements: self.device.get_buffer_memory_requirements(raw),
                location: MemoryLocation::CpuToGpu,
                linear: true,
                allocation_scheme: AllocationScheme::GpuAllocatorManaged,
            })
            .map_err(|e| e.to_string())?;
        result.allocation = Some(allocation);
        let a = result.allocation.as_ref().unwrap();
        self.device
            .bind_buffer_memory(raw, a.memory(), a.offset())
            .map_err(|e| e.to_string())?;
        let p = a.mapped_ptr().ok_or("Vulkan buffer has no mapping")?;
        std::ptr::copy_nonoverlapping(bytes.as_ptr(), p.as_ptr().cast(), bytes.len());
        if !a
            .memory_properties()
            .contains(vk::MemoryPropertyFlags::HOST_COHERENT)
        {
            self.device
                .flush_mapped_memory_ranges(&[vk::MappedMemoryRange::builder()
                    .memory(a.memory())
                    .offset(0)
                    .size(vk::WHOLE_SIZE)
                    .build()])
                .map_err(|e| e.to_string())?;
        }
        Ok(result)
    }
    pub unsafe fn image(
        self: &Rc<Self>,
        w: u32,
        h: u32,
        format: vk::Format,
        usage: vk::ImageUsageFlags,
        aspect: vk::ImageAspectFlags,
    ) -> Result<Image, String> {
        let raw = self
            .device
            .create_image(
                &vk::ImageCreateInfo::builder()
                    .image_type(vk::ImageType::TYPE_2D)
                    .format(format)
                    .extent(vk::Extent3D {
                        width: w,
                        height: h,
                        depth: 1,
                    })
                    .mip_levels(1)
                    .array_layers(1)
                    .samples(vk::SampleCountFlags::TYPE_1)
                    .tiling(vk::ImageTiling::OPTIMAL)
                    .usage(usage),
                None,
            )
            .map_err(|e| e.to_string())?;
        let mut result = Image {
            gpu: self.clone(),
            raw,
            view: vk::ImageView::null(),
            allocation: None,
        };
        result.allocation = Some(
            self.allocator
                .borrow_mut()
                .allocate(&AllocationCreateDesc {
                    name: "Inox2D image",
                    requirements: self.device.get_image_memory_requirements(raw),
                    location: MemoryLocation::GpuOnly,
                    linear: false,
                    allocation_scheme: AllocationScheme::GpuAllocatorManaged,
                })
                .map_err(|e| e.to_string())?,
        );
        let a = result.allocation.as_ref().unwrap();
        self.device
            .bind_image_memory(raw, a.memory(), a.offset())
            .map_err(|e| e.to_string())?;
        result.view = self
            .device
            .create_image_view(
                &vk::ImageViewCreateInfo::builder()
                    .image(raw)
                    .view_type(vk::ImageViewType::TYPE_2D)
                    .format(format)
                    .subresource_range(range(aspect)),
                None,
            )
            .map_err(|e| e.to_string())?;
        Ok(result)
    }
    pub unsafe fn commands(&self) -> Result<Commands, String> {
        let begin = crate::host()
            .begin_commands
            .ok_or("Missing host command service")?;
        let raw = begin(self.info.rhi_handle);
        if raw.is_null() {
            return Err("Cannot allocate host Vulkan commands".into());
        }
        Ok(Commands {
            raw: vk::CommandBuffer::from_raw(raw as usize as u64),
            rhi: self.info.rhi_handle,
            submitted: false,
        })
    }
}
pub struct Buffer {
    pub gpu: Rc<Gpu>,
    pub raw: vk::Buffer,
    allocation: Option<Allocation>,
}
impl Drop for Buffer {
    fn drop(&mut self) {
        unsafe {
            self.gpu.device.destroy_buffer(self.raw, None);
        }
        if let Some(a) = self.allocation.take() {
            let _ = self.gpu.allocator.borrow_mut().free(a);
        }
    }
}
pub struct Image {
    pub gpu: Rc<Gpu>,
    pub raw: vk::Image,
    pub view: vk::ImageView,
    allocation: Option<Allocation>,
}
impl Drop for Image {
    fn drop(&mut self) {
        unsafe {
            self.gpu.device.destroy_image_view(self.view, None);
            self.gpu.device.destroy_image(self.raw, None);
        }
        if let Some(a) = self.allocation.take() {
            let _ = self.gpu.allocator.borrow_mut().free(a);
        }
    }
}
pub fn range(aspect: vk::ImageAspectFlags) -> vk::ImageSubresourceRange {
    vk::ImageSubresourceRange {
        aspect_mask: aspect,
        base_mip_level: 0,
        level_count: 1,
        base_array_layer: 0,
        layer_count: 1,
    }
}
pub unsafe fn barrier(
    gpu: &Gpu,
    cmd: vk::CommandBuffer,
    image: vk::Image,
    aspect: vk::ImageAspectFlags,
    old: vk::ImageLayout,
    new: vk::ImageLayout,
) {
    let b = vk::ImageMemoryBarrier::builder()
        .image(image)
        .old_layout(old)
        .new_layout(new)
        .src_queue_family_index(vk::QUEUE_FAMILY_IGNORED)
        .dst_queue_family_index(vk::QUEUE_FAMILY_IGNORED)
        .src_access_mask(if old == vk::ImageLayout::UNDEFINED {
            vk::AccessFlags::empty()
        } else {
            vk::AccessFlags::MEMORY_READ | vk::AccessFlags::MEMORY_WRITE
        })
        .dst_access_mask(vk::AccessFlags::MEMORY_READ | vk::AccessFlags::MEMORY_WRITE)
        .subresource_range(range(aspect));
    gpu.device.cmd_pipeline_barrier(
        cmd,
        vk::PipelineStageFlags::ALL_COMMANDS,
        vk::PipelineStageFlags::ALL_COMMANDS,
        vk::DependencyFlags::empty(),
        &[],
        &[],
        &[*b],
    );
}
pub struct Commands {
    pub raw: vk::CommandBuffer,
    rhi: *const std::ffi::c_void,
    submitted: bool,
}
impl Commands {
    pub unsafe fn submit(mut self) -> Result<(), String> {
        self.submitted = true;
        if crate::host()
            .submit_commands
            .ok_or("Missing Vulkan submit service")?(
            self.rhi, self.raw.as_raw() as usize as *mut _
        ) {
            Ok(())
        } else {
            Err("Vulkan command submission failed".into())
        }
    }
}
impl Drop for Commands {
    fn drop(&mut self) {
        if !self.submitted {
            if let Some(submit) = crate::host().submit_commands {
                unsafe {
                    submit(self.rhi, self.raw.as_raw() as usize as *mut _);
                }
            }
        }
    }
}
