//! Encode directly into the host's borrowed Metal command buffer and color target.
use crate::{
    abi::{MetalFrame, RhiInfo},
    native_plan::{Command, Plan, Vertex},
};
use foreign_types::ForeignTypeRef;
use glam::Mat4;
use inox2d::model::Model;
use metal::*;
use std::{cell::RefCell, collections::HashSet};
pub struct Metal {
    info: RhiInfo,
    device: Device,
    textures: Vec<Texture>,
    pipelines: Vec<RenderPipelineState>,
    stencil: Vec<DepthStencilState>,
    sampler: SamplerState,
    hidden: HashSet<u32>,
    large_allocation: bool,
    surface: RefCell<Option<Surface>>,
}
struct Surface {
    width: u64,
    height: u64,
    color: Texture,
    stencil: [Texture; 2],
}
impl Metal {
    pub unsafe fn new(
        info: RhiInfo,
        model: &Model,
        hidden: HashSet<u32>,
        large_allocation: bool,
    ) -> Result<Self, String> {
        if info.metal_device.is_null() || info.rhi_handle.is_null() {
            return Err("Missing host Metal device".into());
        }
        let device = DeviceRef::from_ptr(info.metal_device.cast()).to_owned();
        let (source, vs, fs) = crate::native_shader::msl()?;
        let library = device.new_library_with_source(&source, &CompileOptions::new())?;
        let vertex = library.get_function(&vs, None)?;
        let fragment = library.get_function(&fs, None)?;
        let layout = VertexDescriptor::new();
        for (index, format, offset) in [
            (0, MTLVertexFormat::Float4, 0),
            (1, MTLVertexFormat::Float2, 16),
        ] {
            let attribute = layout.attributes().object_at(index).unwrap();
            attribute.set_format(format);
            attribute.set_offset(offset);
            attribute.set_buffer_index(0);
        }
        layout
            .layouts()
            .object_at(0)
            .unwrap()
            .set_stride(std::mem::size_of::<Vertex>() as u64);
        let mut pipelines = Vec::new();
        for blend in 0..7 {
            for mode in 0..3 {
                let descriptor = RenderPipelineDescriptor::new();
                descriptor.set_vertex_function(Some(&vertex));
                descriptor.set_fragment_function(Some(&fragment));
                descriptor.set_vertex_descriptor(Some(layout));
                descriptor.set_stencil_attachment_pixel_format(MTLPixelFormat::Stencil8);
                let color = descriptor.color_attachments().object_at(0).unwrap();
                color.set_pixel_format(MTLPixelFormat::BGRA8Unorm);
                let (src, dst, op) = blend_factors(blend);
                color.set_blending_enabled(true);
                color.set_source_rgb_blend_factor(src);
                color.set_destination_rgb_blend_factor(dst);
                color.set_rgb_blend_operation(op);
                color.set_source_alpha_blend_factor(src);
                color.set_destination_alpha_blend_factor(dst);
                color.set_alpha_blend_operation(op);
                color.set_write_mask(if mode == 1 {
                    MTLColorWriteMask::empty()
                } else {
                    MTLColorWriteMask::All
                });
                pipelines.push(device.new_render_pipeline_state(&descriptor)?);
            }
        }
        let mut stencil = Vec::new();
        for mode in 0..3 {
            let face = StencilDescriptor::new();
            face.set_stencil_compare_function(if mode == 2 {
                MTLCompareFunction::Equal
            } else {
                MTLCompareFunction::Always
            });
            face.set_depth_stencil_pass_operation(if mode == 1 {
                MTLStencilOperation::Replace
            } else {
                MTLStencilOperation::Keep
            });
            face.set_read_mask(255);
            face.set_write_mask(if mode == 1 { 255 } else { 0 });
            let descriptor = DepthStencilDescriptor::new();
            descriptor.set_front_face_stencil(Some(&face));
            descriptor.set_back_face_stencil(Some(&face));
            stencil.push(device.new_depth_stencil_state(&descriptor));
        }
        let sampler = SamplerDescriptor::new();
        sampler.set_min_filter(MTLSamplerMinMagFilter::Linear);
        sampler.set_mag_filter(MTLSamplerMinMagFilter::Linear);
        sampler.set_address_mode_s(MTLSamplerAddressMode::ClampToBorderColor);
        sampler.set_address_mode_t(MTLSamplerAddressMode::ClampToBorderColor);
        let sampler = device.new_sampler(&sampler);
        let mut result = Self {
            info,
            device,
            textures: Vec::new(),
            pipelines,
            stencil,
            sampler,
            hidden,
            large_allocation,
            surface: RefCell::new(None),
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
            let mut image = reader.decode().map_err(|e| e.to_string())?.into_rgba8();
            for pixel in image.pixels_mut() {
                for c in 0..3 {
                    pixel[c] = ((u16::from(pixel[c]) * u16::from(pixel[3]) + 127) / 255) as u8;
                }
            }
            let texture = result.texture(
                image.width() as u64,
                image.height() as u64,
                MTLPixelFormat::RGBA8Unorm,
                MTLTextureUsage::ShaderRead,
                if result.device.has_unified_memory() {
                    MTLStorageMode::Shared
                } else {
                    MTLStorageMode::Managed
                },
            );
            texture.replace_region(
                MTLRegion::new_2d(0, 0, image.width() as u64, image.height() as u64),
                0,
                image.as_raw().as_ptr().cast(),
                image.width() as u64 * 4,
            );
            result.textures.push(texture);
        }
        Ok(result)
    }
    fn texture(
        &self,
        width: u64,
        height: u64,
        format: MTLPixelFormat,
        usage: MTLTextureUsage,
        storage: MTLStorageMode,
    ) -> Texture {
        let descriptor = TextureDescriptor::new();
        descriptor.set_texture_type(MTLTextureType::D2);
        descriptor.set_pixel_format(format);
        descriptor.set_width(width);
        descriptor.set_height(height);
        descriptor.set_usage(usage);
        descriptor.set_storage_mode(storage);
        self.device.new_texture(&descriptor)
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
        let mut frame = MetalFrame::default();
        if !crate::host()
            .metal_frame
            .ok_or("Missing Metal frame service")?(self.info.rhi_handle, &mut frame)
            || frame.command_buffer.is_null()
            || frame.color_texture.is_null()
        {
            return Err("No acquired Metal frame".into());
        }
        let commands = CommandBufferRef::from_ptr(frame.command_buffer.cast());
        let output = TextureRef::from_ptr(frame.color_texture.cast());
        if output.width() != width as u64 || output.height() != height as u64 {
            return Err("Metal frame dimensions changed".into());
        }
        let mut surface = self.surface.borrow_mut();
        if surface
            .as_ref()
            .is_none_or(|s| s.width != width as u64 || s.height != height as u64)
        {
            *surface = Some(Surface {
                width: width as u64,
                height: height as u64,
                color: self.texture(
                    width as u64,
                    height as u64,
                    MTLPixelFormat::BGRA8Unorm,
                    MTLTextureUsage::RenderTarget | MTLTextureUsage::ShaderRead,
                    MTLStorageMode::Private,
                ),
                stencil: std::array::from_fn(|_| {
                    self.texture(
                        width as u64,
                        height as u64,
                        MTLPixelFormat::Stencil8,
                        MTLTextureUsage::RenderTarget,
                        MTLStorageMode::Private,
                    )
                }),
            });
        }
        let s = surface.as_ref().unwrap();
        let plan = Plan::collect(model, &self.hidden, matrix);
        // One allocation per frame, retained by Metal's command buffer. Never
        // overwrite a previous frame's storage while the GPU may still read it.
        let packed = if self.large_allocation {
            let size = plan.iter().try_fold(0usize, |total, command| {
                let count = match command {
                    Command::Paint(p) => p.vertices.len(),
                    Command::Clear { .. } => 0,
                };
                count
                    .checked_mul(std::mem::size_of::<Vertex>())
                    .and_then(|bytes| total.checked_add(bytes))
                    .ok_or("Metal vertex buffer size overflow")
            })?;
            if size == 0 {
                None
            } else {
                let buffer = self
                    .device
                    .new_buffer(size as u64, MTLResourceOptions::StorageModeShared);
                let mut offset = 0;
                for command in &plan {
                    if let Command::Paint(p) = command {
                        let bytes: &[u8] = bytemuck::cast_slice(&p.vertices);
                        std::ptr::copy_nonoverlapping(
                            bytes.as_ptr(),
                            buffer.contents().cast::<u8>().add(offset),
                            bytes.len(),
                        );
                        offset += bytes.len();
                    }
                }
                Some(buffer)
            }
        } else {
            None
        };
        let mut vertex_offset = 0;
        for command in plan {
            let target = match &command {
                Command::Clear { target, .. } => *target,
                Command::Paint(p) => p.target,
            };
            let pass = RenderPassDescriptor::new();
            let color = pass.color_attachments().object_at(0).unwrap();
            color.set_texture(Some(if target == 0 { output } else { &s.color }));
            color.set_load_action(MTLLoadAction::Load);
            color.set_store_action(MTLStoreAction::Store);
            let stencil = pass
                .stencil_attachment()
                .ok_or("Metal stencil attachment is unavailable")?;
            stencil.set_texture(Some(&s.stencil[target]));
            stencil.set_load_action(MTLLoadAction::Load);
            stencil.set_store_action(MTLStoreAction::Store);
            if let Command::Clear {
                color: clear,
                stencil: value,
                ..
            } = &command
            {
                stencil.set_load_action(MTLLoadAction::Clear);
                stencil.set_clear_stencil(*value);
                if *clear && target == 1 {
                    color.set_load_action(MTLLoadAction::Clear);
                    color.set_clear_color(MTLClearColor::new(0.0, 0.0, 0.0, 0.0));
                }
            }
            let encoder = commands.new_render_command_encoder(pass);
            if let Command::Paint(p) = command {
                if !p.vertices.is_empty() {
                    let bytes: &[u8] = bytemuck::cast_slice(&p.vertices);
                    let local = if packed.is_none() {
                        Some(self.device.new_buffer_with_data(
                            bytes.as_ptr().cast(),
                            bytes.len() as u64,
                            MTLResourceOptions::StorageModeShared,
                        ))
                    } else {
                        None
                    };
                    let (buffer, offset) = match &packed {
                        Some(buffer) => (buffer, vertex_offset),
                        None => (local.as_ref().unwrap(), 0),
                    };
                    encoder.set_render_pipeline_state(&self.pipelines[p.blend * 3 + p.mode]);
                    encoder.set_depth_stencil_state(&self.stencil[p.mode]);
                    encoder.set_stencil_reference_value(p.stencil);
                    encoder.set_cull_mode(MTLCullMode::None);
                    encoder.set_vertex_buffer(0, Some(buffer), offset);
                    vertex_offset += bytes.len() as u64;
                    encoder.set_fragment_bytes(
                        1,
                        std::mem::size_of_val(&p.uniform) as u64,
                        bytemuck::bytes_of(&p.uniform).as_ptr().cast(),
                    );
                    encoder.set_fragment_texture(
                        0,
                        Some(p.texture.map(|i| &*self.textures[i]).unwrap_or(&s.color)),
                    );
                    encoder.set_fragment_sampler_state(0, Some(&self.sampler));
                    encoder.draw_primitives(MTLPrimitiveType::Triangle, 0, p.vertices.len() as u64);
                }
            }
            encoder.end_encoding();
        }
        // Submission, capture and presentation belong to the host.
        Ok(())
    }
}
fn blend_factors(blend: usize) -> (MTLBlendFactor, MTLBlendFactor, MTLBlendOperation) {
    use MTLBlendFactor as F;
    use MTLBlendOperation as O;
    match blend {
        1 => (F::DestinationColor, F::OneMinusSourceAlpha, O::Add),
        2 => (F::DestinationColor, F::One, O::Add),
        3 => (F::One, F::One, O::Add),
        4 => (F::One, F::OneMinusSourceColor, O::Add),
        5 => (F::DestinationAlpha, F::OneMinusSourceAlpha, O::Add),
        6 => (
            F::OneMinusDestinationAlpha,
            F::OneMinusSourceAlpha,
            O::Subtract,
        ),
        _ => (F::One, F::OneMinusSourceAlpha, O::Add),
    }
}
