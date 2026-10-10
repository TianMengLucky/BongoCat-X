//! Inox2D render traversal recorded directly on the host Vulkan queue.
use crate::{
    abi::{RhiInfo, VulkanFrame},
    native_plan::{Command, Plan},
    vulkan_pipeline::Pipelines,
    vulkan_resources::{barrier, Buffer, Gpu, Image},
};
use ash::{vk, vk::Handle};
use glam::Mat4;
use inox2d::model::Model;
use std::{cell::RefCell, collections::HashSet, rc::Rc};
const COLOR: vk::ImageAspectFlags = vk::ImageAspectFlags::COLOR;
pub struct Vulkan {
    gpu: Rc<Gpu>,
    pipelines: Pipelines,
    textures: Vec<Image>,
    hidden: HashSet<u32>,
    depth_format: vk::Format,
    surface: RefCell<Option<Surface>>,
    recorder: RefCell<Option<crate::vulkan_recording::Recorder>>,
}
struct Surface {
    width: u32,
    height: u32,
    color: Image,
    depth: [Image; 2],
}
struct Frame {
    gpu: Rc<Gpu>,
    pool: vk::DescriptorPool,
    buffers: Vec<Buffer>,
    framebuffers: Vec<vk::Framebuffer>,
}
impl Drop for Frame {
    fn drop(&mut self) {
        unsafe {
            for f in self.framebuffers.drain(..) {
                self.gpu.device.destroy_framebuffer(f, None);
            }
            self.gpu.device.destroy_descriptor_pool(self.pool, None);
        }
    }
}
impl Vulkan {
    pub unsafe fn new(
        info: RhiInfo,
        model: &Model,
        hidden: HashSet<u32>,
        large_allocation: bool,
        parallel_recording: bool,
    ) -> Result<Self, String> {
        let gpu = Gpu::new(info, large_allocation)?;
        let physical = vk::PhysicalDevice::from_raw(info.vulkan_physical_device as usize as u64);
        let depth_format = [
            vk::Format::D24_UNORM_S8_UINT,
            vk::Format::D32_SFLOAT_S8_UINT,
        ]
        .into_iter()
        .find(|f| {
            gpu.instance
                .get_physical_device_format_properties(physical, *f)
                .optimal_tiling_features
                .contains(vk::FormatFeatureFlags::DEPTH_STENCIL_ATTACHMENT)
        })
        .ok_or("Vulkan device has no stencil attachment format")?;
        let pipelines = Pipelines::new(gpu.clone(), depth_format)?;
        let recorder = if parallel_recording {
            Some(crate::vulkan_recording::Recorder::new(
                gpu.device.clone(),
                info.queue_family,
            )?)
        } else {
            None
        };
        let mut result = Self {
            gpu,
            pipelines,
            textures: Vec::new(),
            hidden,
            depth_format,
            surface: RefCell::new(None),
            recorder: RefCell::new(recorder),
        };
        for source in &model.textures {
            let mut reader = image::ImageReader::with_format(
                std::io::Cursor::new(source.data.as_ref()),
                source.format,
            );
            let mut limits = image::Limits::default();
            limits.max_alloc = Some(256 * 1024 * 1024);
            limits.max_image_width = Some(16384);
            limits.max_image_height = Some(16384);
            reader.limits(limits);
            let mut image = reader.decode().map_err(|e| e.to_string())?.to_rgba8();
            for pixel in image.pixels_mut() {
                for c in 0..3 {
                    pixel[c] = ((u16::from(pixel[c]) * u16::from(pixel[3]) + 127) / 255) as u8;
                }
            }
            let texture = result.gpu.image(
                image.width(),
                image.height(),
                vk::Format::R8G8B8A8_UNORM,
                vk::ImageUsageFlags::TRANSFER_DST | vk::ImageUsageFlags::SAMPLED,
                COLOR,
            )?;
            let buffer = result
                .gpu
                .buffer(image.as_raw(), vk::BufferUsageFlags::TRANSFER_SRC)?;
            let commands = result.gpu.commands()?;
            barrier(
                &result.gpu,
                commands.raw,
                texture.raw,
                COLOR,
                vk::ImageLayout::UNDEFINED,
                vk::ImageLayout::TRANSFER_DST_OPTIMAL,
            );
            let copy = vk::BufferImageCopy::builder()
                .image_subresource(vk::ImageSubresourceLayers {
                    aspect_mask: COLOR,
                    mip_level: 0,
                    base_array_layer: 0,
                    layer_count: 1,
                })
                .image_extent(vk::Extent3D {
                    width: image.width(),
                    height: image.height(),
                    depth: 1,
                })
                .build();
            result.gpu.device.cmd_copy_buffer_to_image(
                commands.raw,
                buffer.raw,
                texture.raw,
                vk::ImageLayout::TRANSFER_DST_OPTIMAL,
                &[copy],
            );
            barrier(
                &result.gpu,
                commands.raw,
                texture.raw,
                COLOR,
                vk::ImageLayout::TRANSFER_DST_OPTIMAL,
                vk::ImageLayout::SHADER_READ_ONLY_OPTIMAL,
            );
            commands.submit()?;
            result.textures.push(texture);
        }
        Ok(result)
    }
    unsafe fn surface(&self, width: u32, height: u32) -> Result<Surface, String> {
        let aspect = vk::ImageAspectFlags::DEPTH | vk::ImageAspectFlags::STENCIL;
        let color = self.gpu.image(
            width,
            height,
            vk::Format::from_raw(self.gpu.info.color_format),
            vk::ImageUsageFlags::COLOR_ATTACHMENT | vk::ImageUsageFlags::SAMPLED,
            COLOR,
        )?;
        let depth = [
            self.gpu.image(
                width,
                height,
                self.depth_format,
                vk::ImageUsageFlags::DEPTH_STENCIL_ATTACHMENT,
                aspect,
            )?,
            self.gpu.image(
                width,
                height,
                self.depth_format,
                vk::ImageUsageFlags::DEPTH_STENCIL_ATTACHMENT,
                aspect,
            )?,
        ];
        let commands = self.gpu.commands()?;
        barrier(
            &self.gpu,
            commands.raw,
            color.raw,
            COLOR,
            vk::ImageLayout::UNDEFINED,
            vk::ImageLayout::SHADER_READ_ONLY_OPTIMAL,
        );
        for d in &depth {
            barrier(
                &self.gpu,
                commands.raw,
                d.raw,
                aspect,
                vk::ImageLayout::UNDEFINED,
                vk::ImageLayout::DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            );
        }
        commands.submit()?;
        Ok(Surface {
            width,
            height,
            color,
            depth,
        })
    }
    pub unsafe fn draw(
        &self,
        model: &Model,
        width: i32,
        height: i32,
        matrix: Mat4,
    ) -> Result<(), String> {
        if width <= 0 || height <= 0 || i64::from(width) * i64::from(height) > 4_194_304 {
            return Err("Inox2D render surface exceeds limits".into());
        }
        let mut target = VulkanFrame::default();
        if !crate::host()
            .vulkan_frame
            .ok_or("Missing Vulkan frame service")?(
            self.gpu.info.rhi_handle, &mut target
        ) || target.view == 0
        {
            return Err("No acquired Vulkan frame".into());
        }
        let mut surface = self.surface.borrow_mut();
        if surface
            .as_ref()
            .is_none_or(|s| s.width != width as u32 || s.height != height as u32)
        {
            *surface = Some(self.surface(width as u32, height as u32)?);
        }
        let s = surface.as_ref().unwrap();
        let plan = Plan::collect(model, &self.hidden, matrix);
        let count = plan
            .iter()
            .filter(|c| matches!(c, Command::Paint(_)))
            .count()
            .max(1) as u32;
        let sizes = [
            vk::DescriptorType::SAMPLED_IMAGE,
            vk::DescriptorType::SAMPLER,
            vk::DescriptorType::UNIFORM_BUFFER,
        ]
        .map(|ty| vk::DescriptorPoolSize {
            ty,
            descriptor_count: count,
        });
        let pool = self
            .gpu
            .device
            .create_descriptor_pool(
                &vk::DescriptorPoolCreateInfo::builder()
                    .max_sets(count)
                    .pool_sizes(&sizes),
                None,
            )
            .map_err(|e| e.to_string())?;
        let mut frame = Frame {
            gpu: self.gpu.clone(),
            pool,
            buffers: Vec::new(),
            framebuffers: Vec::new(),
        };
        for (i, view) in [vk::ImageView::from_raw(target.view), s.color.view]
            .into_iter()
            .enumerate()
        {
            frame.framebuffers.push(
                self.gpu
                    .device
                    .create_framebuffer(
                        &vk::FramebufferCreateInfo::builder()
                            .render_pass(self.pipelines.pass)
                            .attachments(&[view, s.depth[i].view])
                            .width(s.width)
                            .height(s.height)
                            .layers(1),
                        None,
                    )
                    .map_err(|e| e.to_string())?,
            );
        }
        let mut prepared = Vec::new();
        for command in &plan {
            if let Command::Paint(p) = command {
                let vertices = self.gpu.buffer(
                    bytemuck::cast_slice(&p.vertices),
                    vk::BufferUsageFlags::VERTEX_BUFFER,
                )?;
                let uniform = self.gpu.buffer(
                    bytemuck::bytes_of(&p.uniform),
                    vk::BufferUsageFlags::UNIFORM_BUFFER,
                )?;
                let set = self
                    .gpu
                    .device
                    .allocate_descriptor_sets(
                        &vk::DescriptorSetAllocateInfo::builder()
                            .descriptor_pool(pool)
                            .set_layouts(&[self.pipelines.descriptors]),
                    )
                    .map_err(|e| e.to_string())?[0];
                let texture = p
                    .texture
                    .map(|i| self.textures[i].view)
                    .unwrap_or(s.color.view);
                let image = [vk::DescriptorImageInfo {
                    sampler: vk::Sampler::null(),
                    image_view: texture,
                    image_layout: vk::ImageLayout::SHADER_READ_ONLY_OPTIMAL,
                }];
                let sampler = [vk::DescriptorImageInfo {
                    sampler: self.pipelines.sampler,
                    ..Default::default()
                }];
                let buffer = [vk::DescriptorBufferInfo {
                    buffer: uniform.raw,
                    offset: 0,
                    range: std::mem::size_of_val(&p.uniform) as u64,
                }];
                let writes = [
                    vk::WriteDescriptorSet::builder()
                        .dst_set(set)
                        .dst_binding(0)
                        .descriptor_type(vk::DescriptorType::SAMPLED_IMAGE)
                        .image_info(&image)
                        .build(),
                    vk::WriteDescriptorSet::builder()
                        .dst_set(set)
                        .dst_binding(1)
                        .descriptor_type(vk::DescriptorType::SAMPLER)
                        .image_info(&sampler)
                        .build(),
                    vk::WriteDescriptorSet::builder()
                        .dst_set(set)
                        .dst_binding(2)
                        .descriptor_type(vk::DescriptorType::UNIFORM_BUFFER)
                        .buffer_info(&buffer)
                        .build(),
                ];
                self.gpu.device.update_descriptor_sets(&writes, &[]);
                prepared.push((vertices.raw, set));
                frame.buffers.extend([vertices, uniform]);
            }
        }
        let secondary = if let Some(recorder) = self.recorder.borrow_mut().as_mut() {
            recorder.record(
                &plan,
                &prepared,
                self.pipelines.pass,
                &frame.framebuffers,
                self.pipelines.layout,
                &self.pipelines.pipelines,
                s.width,
                s.height,
            )?
        } else {
            None
        };
        let commands = self.gpu.commands()?;
        let cmd = commands.raw;
        let d = &self.gpu.device;
        let mut current = None;
        let mut paints = prepared.into_iter();
        for (index, command) in plan.iter().enumerate() {
            let target = match command {
                Command::Clear { target, .. } => *target,
                Command::Paint(p) => p.target,
            };
            if current != Some(target) {
                if let Some(old) = current {
                    d.cmd_end_render_pass(cmd);
                    if old == 1 {
                        barrier(
                            &self.gpu,
                            cmd,
                            s.color.raw,
                            COLOR,
                            vk::ImageLayout::COLOR_ATTACHMENT_OPTIMAL,
                            vk::ImageLayout::SHADER_READ_ONLY_OPTIMAL,
                        );
                    }
                }
                if target == 1 {
                    barrier(
                        &self.gpu,
                        cmd,
                        s.color.raw,
                        COLOR,
                        vk::ImageLayout::SHADER_READ_ONLY_OPTIMAL,
                        vk::ImageLayout::COLOR_ATTACHMENT_OPTIMAL,
                    );
                }
                d.cmd_begin_render_pass(
                    cmd,
                    &vk::RenderPassBeginInfo::builder()
                        .render_pass(self.pipelines.pass)
                        .framebuffer(frame.framebuffers[target])
                        .render_area(vk::Rect2D {
                            offset: Default::default(),
                            extent: vk::Extent2D {
                                width: s.width,
                                height: s.height,
                            },
                        }),
                    if secondary.is_some() {
                        vk::SubpassContents::SECONDARY_COMMAND_BUFFERS
                    } else {
                        vk::SubpassContents::INLINE
                    },
                );
                if secondary.is_none() {
                    d.cmd_set_viewport(
                        cmd,
                        0,
                        &[vk::Viewport {
                            x: 0.0,
                            y: s.height as f32,
                            width: s.width as f32,
                            height: -(s.height as f32),
                            min_depth: 0.0,
                            max_depth: 1.0,
                        }],
                    );
                    d.cmd_set_scissor(
                        cmd,
                        0,
                        &[vk::Rect2D {
                            offset: Default::default(),
                            extent: vk::Extent2D {
                                width: s.width,
                                height: s.height,
                            },
                        }],
                    );
                }
                current = Some(target);
            }
            if let Some(recorded) = &secondary {
                if let Some(buffer) = recorded[index] {
                    d.cmd_execute_commands(cmd, &[buffer]);
                }
                continue;
            }
            let draw = if matches!(command, Command::Paint(_)) {
                paints.next()
            } else {
                None
            };
            crate::vulkan_recording::encode_command(
                d,
                cmd,
                command,
                draw,
                &self.pipelines.pipelines,
                self.pipelines.layout,
                vk::Rect2D {
                    offset: Default::default(),
                    extent: vk::Extent2D {
                        width: s.width,
                        height: s.height,
                    },
                },
            );
        }
        if let Some(old) = current {
            d.cmd_end_render_pass(cmd);
            if old == 1 {
                barrier(
                    &self.gpu,
                    cmd,
                    s.color.raw,
                    COLOR,
                    vk::ImageLayout::COLOR_ATTACHMENT_OPTIMAL,
                    vk::ImageLayout::SHADER_READ_ONLY_OPTIMAL,
                );
            }
        }
        commands.submit()
    }
}
impl Drop for Vulkan {
    fn drop(&mut self) {
        unsafe {
            if let Some(wait) = crate::host().wait_idle {
                wait(self.gpu.info.rhi_handle);
            }
        }
    }
}
