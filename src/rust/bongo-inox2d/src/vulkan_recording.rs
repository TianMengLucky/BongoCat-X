//! Independent command pools for parallel, ordered secondary recording.
use crate::native_plan::Command;
use ash::vk;
use std::ops::Range;

pub fn worker_count() -> usize {
    std::thread::available_parallelism()
        .map_or(1, usize::from)
        .min(4)
}
fn target(command: &Command) -> usize {
    match command {
        Command::Clear { target, .. } => *target,
        Command::Paint(p) => p.target,
    }
}
fn groups(plan: &[Command]) -> Vec<Range<usize>> {
    let mut groups = Vec::new();
    let mut start = 0;
    while start < plan.len() {
        let mut end = start + 1;
        while end < plan.len() && end - start < 64 && target(&plan[end]) == target(&plan[start]) {
            end += 1;
        }
        groups.push(start..end);
        start = end;
    }
    groups
}
struct Worker {
    pool: vk::CommandPool,
    buffers: Vec<vk::CommandBuffer>,
}
pub struct Recorder {
    device: ash::Device,
    workers: Vec<Worker>,
}
impl Recorder {
    pub unsafe fn new(device: ash::Device, family: u32) -> Result<Self, String> {
        let mut result = Self {
            device,
            workers: Vec::new(),
        };
        for _ in 0..worker_count() {
            let pool = result
                .device
                .create_command_pool(
                    &vk::CommandPoolCreateInfo::builder()
                        .queue_family_index(family)
                        .flags(vk::CommandPoolCreateFlags::RESET_COMMAND_BUFFER),
                    None,
                )
                .map_err(|e| e.to_string())?;
            result.workers.push(Worker {
                pool,
                buffers: Vec::new(),
            });
        }
        Ok(result)
    }
    /// Host submission waits for completion before this recorder is reused.
    pub unsafe fn record(
        &mut self,
        plan: &[Command],
        prepared: &[(vk::Buffer, vk::DescriptorSet)],
        pass: vk::RenderPass,
        frames: &[vk::Framebuffer],
        layout: vk::PipelineLayout,
        pipelines: &[vk::Pipeline],
        width: u32,
        height: u32,
    ) -> Result<Option<Vec<Option<vk::CommandBuffer>>>, String> {
        if plan.len() < 128 || self.workers.len() < 2 {
            return Ok(None);
        }
        let groups = groups(plan);
        let mut draws = Vec::with_capacity(plan.len());
        let mut paints = prepared.iter();
        for c in plan {
            draws.push(if matches!(c, Command::Paint(_)) {
                paints.next().copied()
            } else {
                None
            });
        }
        let count = self.workers.len().min(groups.len());
        for (i, worker) in self.workers.iter_mut().take(count).enumerate() {
            self.device
                .reset_command_pool(worker.pool, vk::CommandPoolResetFlags::empty())
                .map_err(|e| e.to_string())?;
            let needed = groups.iter().skip(i).step_by(count).count();
            if needed > worker.buffers.len() {
                let added = self
                    .device
                    .allocate_command_buffers(
                        &vk::CommandBufferAllocateInfo::builder()
                            .command_pool(worker.pool)
                            .level(vk::CommandBufferLevel::SECONDARY)
                            .command_buffer_count((needed - worker.buffers.len()) as u32),
                    )
                    .map_err(|e| e.to_string())?;
                worker.buffers.extend(added);
            }
        }
        let device = &self.device;
        std::thread::scope(|scope| {
            let mut threads = Vec::with_capacity(count);
            for (i, worker) in self.workers.iter().take(count).enumerate() {
                let groups = &groups;
                let draws = &draws;
                threads.push(std::thread::Builder::new().name(format!("inox-record-{i}"))
                    .spawn_scoped(scope, move || -> Result<Vec<(usize, vk::CommandBuffer)>, String> {
                        let mut recorded = Vec::with_capacity(worker.buffers.len());
                        for (slot, range) in groups.iter().skip(i).step_by(count).enumerate() {
                            let cmd = worker.buffers[slot];
                            let t = target(&plan[range.start]);
                            let inheritance = vk::CommandBufferInheritanceInfo::builder()
                                .render_pass(pass).subpass(0).framebuffer(frames[t]);
                            device.begin_command_buffer(cmd, &vk::CommandBufferBeginInfo::builder()
                                .flags(vk::CommandBufferUsageFlags::RENDER_PASS_CONTINUE
                                    | vk::CommandBufferUsageFlags::ONE_TIME_SUBMIT)
                                .inheritance_info(&inheritance)).map_err(|e| e.to_string())?;
                            device.cmd_set_viewport(cmd, 0, &[vk::Viewport {
                                x: 0.0, y: height as f32, width: width as f32,
                                height: -(height as f32), min_depth: 0.0, max_depth: 1.0,
                            }]);
                            let rect = vk::Rect2D { offset: Default::default(), extent: vk::Extent2D { width, height } };
                            device.cmd_set_scissor(cmd, 0, &[rect]);
                            for index in range.clone() {
                                encode_command(device, cmd, &plan[index], draws[index], pipelines, layout, rect);
                            }
                            device.end_command_buffer(cmd).map_err(|e| e.to_string())?;
                            recorded.push((range.start, cmd));
                        }
                        Ok(recorded)
                    }).map_err(|e| e.to_string())?);
            }
            let mut output = vec![None; plan.len()];
            // Join every worker even when an earlier worker failed.
            let mut failure = None;
            for thread in threads {
                match thread.join() {
                    Ok(Ok(recorded)) => {
                        for (start, cmd) in recorded {
                            output[start] = Some(cmd);
                        }
                    }
                    Ok(Err(e)) => {
                        failure = Some(e);
                    }
                    Err(_) => {
                        failure = Some("Vulkan recording worker panicked".into());
                    }
                }
            }
            if let Some(e) = failure {
                Err(e)
            } else {
                Ok(Some(output))
            }
        })
    }
}
impl Drop for Recorder {
    fn drop(&mut self) {
        unsafe {
            for worker in &self.workers {
                self.device.destroy_command_pool(worker.pool, None);
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn groups_preserve_order_and_render_target_boundaries() {
        let plan: Vec<_> = (0..150)
            .map(|i| Command::Clear {
                target: usize::from(i >= 70),
                color: false,
                stencil: i,
            })
            .collect();
        assert_eq!(groups(&plan), vec![0..64, 64..70, 70..134, 134..150]);
        assert!(groups(&[]).is_empty());
    }
}

pub unsafe fn encode_command(
    d: &ash::Device,
    cmd: vk::CommandBuffer,
    command: &Command,
    draw: Option<(vk::Buffer, vk::DescriptorSet)>,
    pipelines: &[vk::Pipeline],
    layout: vk::PipelineLayout,
    rect: vk::Rect2D,
) {
    match command {
        Command::Clear { color, stencil, .. } => {
            let mut clears = [vk::ClearAttachment {
                aspect_mask: vk::ImageAspectFlags::STENCIL,
                color_attachment: 0,
                clear_value: vk::ClearValue {
                    depth_stencil: vk::ClearDepthStencilValue {
                        depth: 1.0,
                        stencil: *stencil,
                    },
                },
            }, vk::ClearAttachment::default()];
            let mut clear_count = 1;
            if *color && matches!(command, Command::Clear { target: 1, .. }) {
                clears[1] = vk::ClearAttachment {
                    aspect_mask: vk::ImageAspectFlags::COLOR,
                    color_attachment: 0,
                    clear_value: vk::ClearValue {
                        color: vk::ClearColorValue { float32: [0.0; 4] },
                    },
                };
                clear_count = 2;
            }
            d.cmd_clear_attachments(
                cmd,
                &clears[..clear_count],
                &[vk::ClearRect {
                    rect,
                    base_array_layer: 0,
                    layer_count: 1,
                }],
            );
        }
        Command::Paint(p) => {
            let (buffer, set) = draw.expect("Prepared Vulkan draw");
            d.cmd_bind_pipeline(
                cmd,
                vk::PipelineBindPoint::GRAPHICS,
                pipelines[p.blend * 3 + p.mode],
            );
            d.cmd_set_stencil_reference(cmd, vk::StencilFaceFlags::FRONT_AND_BACK, p.stencil);
            d.cmd_bind_vertex_buffers(cmd, 0, &[buffer], &[0]);
            d.cmd_bind_descriptor_sets(
                cmd,
                vk::PipelineBindPoint::GRAPHICS,
                layout,
                0,
                &[set],
                &[],
            );
            d.cmd_draw(cmd, p.vertices.len() as u32, 1, 0, 0);
        }
    }
}
