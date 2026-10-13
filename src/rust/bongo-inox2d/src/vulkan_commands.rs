//! Two bounded submission slots. Only slot reuse and resource mutation wait.
use crate::vulkan_resources::Gpu;
use ash::vk;
use std::rc::Rc;
pub const SLOTS: usize = 2;
#[derive(Default)]
struct Slot {
    pool: vk::CommandPool,
    command: vk::CommandBuffer,
    fence: vk::Fence,
    pending: bool,
    leased: bool,
}
pub struct CommandSlots {
    device: ash::Device,
    queue: vk::Queue,
    slots: [Slot; SLOTS],
    next: usize,
}
impl CommandSlots {
    pub unsafe fn new(device: ash::Device, queue: vk::Queue, family: u32) -> Result<Self, String> {
        let mut result = Self {
            device,
            queue,
            slots: Default::default(),
            next: 0,
        };
        for slot in &mut result.slots {
            slot.pool = result
                .device
                .create_command_pool(
                    &vk::CommandPoolCreateInfo::builder()
                        .queue_family_index(family)
                        .flags(vk::CommandPoolCreateFlags::TRANSIENT),
                    None,
                )
                .map_err(|e| e.to_string())?;
            slot.command = result
                .device
                .allocate_command_buffers(
                    &vk::CommandBufferAllocateInfo::builder()
                        .command_pool(slot.pool)
                        .level(vk::CommandBufferLevel::PRIMARY)
                        .command_buffer_count(1),
                )
                .map_err(|e| e.to_string())?[0];
            slot.fence = result
                .device
                .create_fence(&vk::FenceCreateInfo::default(), None)
                .map_err(|e| e.to_string())?;
        }
        Ok(result)
    }
    unsafe fn wait(&mut self, index: usize) -> Result<(), String> {
        let slot = &mut self.slots[index];
        if slot.pending {
            self.device
                .wait_for_fences(&[slot.fence], true, u64::MAX)
                .map_err(|e| e.to_string())?;
            slot.pending = false;
        }
        Ok(())
    }
    pub unsafe fn wait_all(&mut self) -> Result<(), String> {
        for i in 0..SLOTS {
            self.wait(i)?;
        }
        Ok(())
    }
    unsafe fn acquire(&mut self) -> Result<(usize, vk::CommandBuffer), String> {
        let index = self.next;
        if self.slots[index].leased {
            return Err("Nested Vulkan command slots exhausted".into());
        }
        self.wait(index)?;
        let slot = &mut self.slots[index];
        self.device
            .reset_command_pool(slot.pool, vk::CommandPoolResetFlags::RELEASE_RESOURCES)
            .map_err(|e| e.to_string())?;
        self.device
            .begin_command_buffer(
                slot.command,
                &vk::CommandBufferBeginInfo::builder()
                    .flags(vk::CommandBufferUsageFlags::ONE_TIME_SUBMIT),
            )
            .map_err(|e| e.to_string())?;
        slot.leased = true;
        self.next = (index + 1) % SLOTS;
        Ok((index, slot.command))
    }
    unsafe fn submit(&mut self, index: usize) -> Result<(), String> {
        let slot = &mut self.slots[index];
        self.device
            .end_command_buffer(slot.command)
            .map_err(|e| e.to_string())?;
        self.device
            .reset_fences(&[slot.fence])
            .map_err(|e| e.to_string())?;
        self.device
            .queue_submit(
                self.queue,
                &[vk::SubmitInfo::builder()
                    .command_buffers(&[slot.command])
                    .build()],
                slot.fence,
            )
            .map_err(|e| e.to_string())?;
        slot.pending = true;
        slot.leased = false;
        Ok(()) // Submission succeeded; GPU completion is deliberately deferred.
    }
}
impl Drop for CommandSlots {
    fn drop(&mut self) {
        unsafe {
            // Destruction is an explicit synchronization boundary, also for failed startup.
            if self.wait_all().is_err() {
                let _ = self.device.device_wait_idle();
            }
            for slot in &self.slots {
                if slot.fence != vk::Fence::null() {
                    self.device.destroy_fence(slot.fence, None);
                }
                if slot.pool != vk::CommandPool::null() {
                    self.device.destroy_command_pool(slot.pool, None);
                }
            }
        }
    }
}
pub struct Commands {
    pub raw: vk::CommandBuffer,
    pub slot: usize,
    gpu: Rc<Gpu>,
    submitted: bool,
}
impl Commands {
    pub unsafe fn begin(gpu: Rc<Gpu>) -> Result<Self, String> {
        let (slot, raw) = gpu.command_slots.borrow_mut().acquire()?;
        Ok(Self {
            raw,
            slot,
            gpu,
            submitted: false,
        })
    }
    pub unsafe fn submit(mut self) -> Result<(), String> {
        let result = { self.gpu.command_slots.borrow_mut().submit(self.slot) };
        result?;
        self.submitted = true;
        Ok(())
    }
}
impl Drop for Commands {
    fn drop(&mut self) {
        if !self.submitted {
            // Abandon unsubmitted recording; never submit partially built work on error.
            self.gpu.command_slots.borrow_mut().slots[self.slot].leased = false;
        }
    }
}
