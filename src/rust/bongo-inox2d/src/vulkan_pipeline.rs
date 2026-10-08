use crate::{native_plan::Vertex, vulkan_resources::Gpu};
use ash::vk;
use std::rc::Rc;
pub struct Pipelines {
    pub gpu: Rc<Gpu>,
    pub pass: vk::RenderPass,
    pub layout: vk::PipelineLayout,
    pub descriptors: vk::DescriptorSetLayout,
    pub sampler: vk::Sampler,
    pub pipelines: Vec<vk::Pipeline>,
}
impl Pipelines {
    pub unsafe fn new(gpu: Rc<Gpu>, depth: vk::Format) -> Result<Self, String> {
        let mut result = Self {
            gpu,
            pass: vk::RenderPass::null(),
            layout: vk::PipelineLayout::null(),
            descriptors: vk::DescriptorSetLayout::null(),
            sampler: vk::Sampler::null(),
            pipelines: Vec::new(),
        };
        let d = result.gpu.device.clone();
        let color = vk::AttachmentDescription::builder()
            .format(vk::Format::from_raw(result.gpu.info.color_format))
            .samples(vk::SampleCountFlags::TYPE_1)
            .load_op(vk::AttachmentLoadOp::LOAD)
            .store_op(vk::AttachmentStoreOp::STORE)
            .initial_layout(vk::ImageLayout::COLOR_ATTACHMENT_OPTIMAL)
            .final_layout(vk::ImageLayout::COLOR_ATTACHMENT_OPTIMAL)
            .build();
        let stencil = vk::AttachmentDescription::builder()
            .format(depth)
            .samples(vk::SampleCountFlags::TYPE_1)
            .load_op(vk::AttachmentLoadOp::DONT_CARE)
            .store_op(vk::AttachmentStoreOp::DONT_CARE)
            .stencil_load_op(vk::AttachmentLoadOp::LOAD)
            .stencil_store_op(vk::AttachmentStoreOp::STORE)
            .initial_layout(vk::ImageLayout::DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
            .final_layout(vk::ImageLayout::DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
            .build();
        let color_refs = [vk::AttachmentReference {
            attachment: 0,
            layout: vk::ImageLayout::COLOR_ATTACHMENT_OPTIMAL,
        }];
        let depth_ref = vk::AttachmentReference {
            attachment: 1,
            layout: vk::ImageLayout::DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        };
        let subpass = vk::SubpassDescription::builder()
            .pipeline_bind_point(vk::PipelineBindPoint::GRAPHICS)
            .color_attachments(&color_refs)
            .depth_stencil_attachment(&depth_ref)
            .build();
        result.pass = d
            .create_render_pass(
                &vk::RenderPassCreateInfo::builder()
                    .attachments(&[color, stencil])
                    .subpasses(&[subpass]),
                None,
            )
            .map_err(|e| e.to_string())?;
        let bindings = [
            vk::DescriptorType::SAMPLED_IMAGE,
            vk::DescriptorType::SAMPLER,
            vk::DescriptorType::UNIFORM_BUFFER,
        ]
        .into_iter()
        .enumerate()
        .map(|(i, t)| {
            vk::DescriptorSetLayoutBinding::builder()
                .binding(i as u32)
                .descriptor_count(1)
                .descriptor_type(t)
                .stage_flags(vk::ShaderStageFlags::FRAGMENT)
                .build()
        })
        .collect::<Vec<_>>();
        result.descriptors = d
            .create_descriptor_set_layout(
                &vk::DescriptorSetLayoutCreateInfo::builder().bindings(&bindings),
                None,
            )
            .map_err(|e| e.to_string())?;
        result.layout = d
            .create_pipeline_layout(
                &vk::PipelineLayoutCreateInfo::builder().set_layouts(&[result.descriptors]),
                None,
            )
            .map_err(|e| e.to_string())?;
        result.sampler = d
            .create_sampler(
                &vk::SamplerCreateInfo::builder()
                    .mag_filter(vk::Filter::LINEAR)
                    .min_filter(vk::Filter::LINEAR)
                    .address_mode_u(vk::SamplerAddressMode::CLAMP_TO_BORDER)
                    .address_mode_v(vk::SamplerAddressMode::CLAMP_TO_BORDER),
                None,
            )
            .map_err(|e| e.to_string())?;
        let vertex = crate::native_shader::spirv(naga::ShaderStage::Vertex, "vertex")?;
        let fragment = crate::native_shader::spirv(naga::ShaderStage::Fragment, "fragment")?;
        let vs = d
            .create_shader_module(&vk::ShaderModuleCreateInfo::builder().code(&vertex), None)
            .map_err(|e| e.to_string())?;
        let fs = match d
            .create_shader_module(&vk::ShaderModuleCreateInfo::builder().code(&fragment), None)
        {
            Ok(v) => v,
            Err(e) => {
                d.destroy_shader_module(vs, None);
                return Err(e.to_string());
            }
        };
        let build = result.build(vs, fs);
        d.destroy_shader_module(vs, None);
        d.destroy_shader_module(fs, None);
        build?;
        Ok(result)
    }
    unsafe fn build(&mut self, vs: vk::ShaderModule, fs: vk::ShaderModule) -> Result<(), String> {
        let vertex_name = std::ffi::CString::new("vertex").unwrap();
        let fragment_name = std::ffi::CString::new("fragment").unwrap();
        let stages = [
            vk::PipelineShaderStageCreateInfo::builder()
                .stage(vk::ShaderStageFlags::VERTEX)
                .module(vs)
                .name(&vertex_name)
                .build(),
            vk::PipelineShaderStageCreateInfo::builder()
                .stage(vk::ShaderStageFlags::FRAGMENT)
                .module(fs)
                .name(&fragment_name)
                .build(),
        ];
        let binding = [vk::VertexInputBindingDescription {
            binding: 0,
            stride: std::mem::size_of::<Vertex>() as u32,
            input_rate: vk::VertexInputRate::VERTEX,
        }];
        let attrs = [
            vk::VertexInputAttributeDescription {
                location: 0,
                binding: 0,
                format: vk::Format::R32G32B32A32_SFLOAT,
                offset: 0,
            },
            vk::VertexInputAttributeDescription {
                location: 1,
                binding: 0,
                format: vk::Format::R32G32_SFLOAT,
                offset: 16,
            },
        ];
        let vertex = vk::PipelineVertexInputStateCreateInfo::builder()
            .vertex_binding_descriptions(&binding)
            .vertex_attribute_descriptions(&attrs);
        let assembly = vk::PipelineInputAssemblyStateCreateInfo::builder()
            .topology(vk::PrimitiveTopology::TRIANGLE_LIST);
        let viewport = vk::PipelineViewportStateCreateInfo::builder()
            .viewport_count(1)
            .scissor_count(1);
        let raster = vk::PipelineRasterizationStateCreateInfo::builder()
            .polygon_mode(vk::PolygonMode::FILL)
            .line_width(1.0)
            .cull_mode(vk::CullModeFlags::NONE);
        let multisample = vk::PipelineMultisampleStateCreateInfo::builder()
            .rasterization_samples(vk::SampleCountFlags::TYPE_1);
        let dynamic = vk::PipelineDynamicStateCreateInfo::builder().dynamic_states(&[
            vk::DynamicState::VIEWPORT,
            vk::DynamicState::SCISSOR,
            vk::DynamicState::STENCIL_REFERENCE,
        ]);
        for blend in 0..7 {
            for mode in 0..3 {
                let (src, dst, op) = blend_factors(blend);
                let attachment = [vk::PipelineColorBlendAttachmentState::builder()
                    .blend_enable(true)
                    .src_color_blend_factor(src)
                    .dst_color_blend_factor(dst)
                    .color_blend_op(op)
                    .src_alpha_blend_factor(src)
                    .dst_alpha_blend_factor(dst)
                    .alpha_blend_op(op)
                    .color_write_mask(if mode == 1 {
                        vk::ColorComponentFlags::empty()
                    } else {
                        vk::ColorComponentFlags::RGBA
                    })
                    .build()];
                let color =
                    vk::PipelineColorBlendStateCreateInfo::builder().attachments(&attachment);
                let face = vk::StencilOpState::builder()
                    .compare_op(if mode == 2 {
                        vk::CompareOp::EQUAL
                    } else {
                        vk::CompareOp::ALWAYS
                    })
                    .pass_op(if mode == 1 {
                        vk::StencilOp::REPLACE
                    } else {
                        vk::StencilOp::KEEP
                    })
                    .compare_mask(255)
                    .write_mask(if mode == 1 { 255 } else { 0 })
                    .build();
                let depth = vk::PipelineDepthStencilStateCreateInfo::builder()
                    .stencil_test_enable(mode != 0)
                    .front(face)
                    .back(face);
                let info = vk::GraphicsPipelineCreateInfo::builder()
                    .stages(&stages)
                    .vertex_input_state(&vertex)
                    .input_assembly_state(&assembly)
                    .viewport_state(&viewport)
                    .rasterization_state(&raster)
                    .multisample_state(&multisample)
                    .depth_stencil_state(&depth)
                    .color_blend_state(&color)
                    .dynamic_state(&dynamic)
                    .layout(self.layout)
                    .render_pass(self.pass)
                    .build();
                match self.gpu.device.create_graphics_pipelines(
                    vk::PipelineCache::null(),
                    &[info],
                    None,
                ) {
                    Ok(p) => self.pipelines.extend(p),
                    Err((partial, e)) => {
                        self.pipelines.extend(partial);
                        return Err(e.to_string());
                    }
                }
            }
        }
        Ok(())
    }
}
pub fn blend_factors(blend: usize) -> (vk::BlendFactor, vk::BlendFactor, vk::BlendOp) {
    use vk::{BlendFactor as F, BlendOp as O};
    match blend {
        1 => (F::DST_COLOR, F::ONE_MINUS_SRC_ALPHA, O::ADD),
        2 => (F::DST_COLOR, F::ONE, O::ADD),
        3 => (F::ONE, F::ONE, O::ADD),
        4 => (F::ONE, F::ONE_MINUS_SRC_COLOR, O::ADD),
        5 => (F::DST_ALPHA, F::ONE_MINUS_SRC_ALPHA, O::ADD),
        6 => (F::ONE_MINUS_DST_ALPHA, F::ONE_MINUS_SRC_ALPHA, O::SUBTRACT),
        _ => (F::ONE, F::ONE_MINUS_SRC_ALPHA, O::ADD),
    }
}
impl Drop for Pipelines {
    fn drop(&mut self) {
        unsafe {
            let d = &self.gpu.device;
            for p in self.pipelines.drain(..) {
                d.destroy_pipeline(p, None);
            }
            d.destroy_sampler(self.sampler, None);
            d.destroy_pipeline_layout(self.layout, None);
            d.destroy_descriptor_set_layout(self.descriptors, None);
            d.destroy_render_pass(self.pass, None);
        }
    }
}
