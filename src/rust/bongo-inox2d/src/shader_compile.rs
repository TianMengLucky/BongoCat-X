//! Naga translates one shader to SPIR-V and MSL rather than maintaining shader copies.
pub fn module() -> Result<(naga::Module, naga::valid::ModuleInfo), String> {
    let source = include_str!("native.wgsl");
    let module = naga::front::wgsl::parse_str(source).map_err(|e| e.emit_to_string(source))?;
    let info = naga::valid::Validator::new(
        naga::valid::ValidationFlags::all(),
        naga::valid::Capabilities::empty(),
    )
    .validate(&module)
    .map_err(|e| e.to_string())?;
    Ok((module, info))
}
pub fn spirv(stage: naga::ShaderStage, entry: &str) -> Result<Vec<u32>, String> {
    let (module, info) = module()?;
    let options = naga::back::spv::Options {
        flags: naga::back::spv::WriterFlags::empty(),
        ..Default::default()
    };
    naga::back::spv::write_vec(
        &module,
        &info,
        &options,
        Some(&naga::back::spv::PipelineOptions {
            shader_stage: stage,
            entry_point: entry.into(),
        }),
    )
    .map_err(|e| e.to_string())
}
pub fn msl() -> Result<(String, String, String), String> {
    use naga::{back::msl::*, ResourceBinding};
    let (module, info) = module()?;
    let mut resources = EntryPointResources::default();
    resources.resources.insert(
        ResourceBinding {
            group: 0,
            binding: 0,
        },
        BindTarget {
            texture: Some(0),
            ..Default::default()
        },
    );
    resources.resources.insert(
        ResourceBinding {
            group: 0,
            binding: 1,
        },
        BindTarget {
            sampler: Some(BindSamplerTarget::Resource(0)),
            ..Default::default()
        },
    );
    resources.resources.insert(
        ResourceBinding {
            group: 0,
            binding: 2,
        },
        BindTarget {
            buffer: Some(1),
            ..Default::default()
        },
    );
    let mut options = Options {
        lang_version: (2, 0),
        fake_missing_bindings: false,
        ..Default::default()
    };
    options
        .per_entry_point_map
        .insert("vertex".into(), resources.clone());
    options
        .per_entry_point_map
        .insert("fragment".into(), resources);
    let (source, translated) = write_string(
        &module,
        &info,
        &options,
        &PipelineOptions {
            allow_and_force_point_size: false,
        },
    )
    .map_err(|e| e.to_string())?;
    let vertex = translated.entry_point_names[0]
        .as_ref()
        .map_err(|e| e.to_string())?
        .clone();
    let fragment = translated.entry_point_names[1]
        .as_ref()
        .map_err(|e| e.to_string())?
        .clone();
    Ok((source, vertex, fragment))
}
#[cfg(test)]
mod tests {
    #[test]
    fn metal_stages_translate() {
        let (source, vertex, fragment) = super::msl().unwrap();
        assert!(source.contains(&vertex));
        assert!(source.contains(&fragment));
    }
    #[test]
    fn native_shader_is_valid() {
        super::module().unwrap();
    }
        #[test]
    fn spirv_stages_translate() {
        for (stage, name) in [
            (naga::ShaderStage::Vertex, "vertex"),
            (naga::ShaderStage::Fragment, "fragment"),
        ] {
            assert_eq!(super::spirv(stage, name).unwrap()[0], 0x07230203);
        }
    }
}
