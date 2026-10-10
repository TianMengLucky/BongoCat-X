fn main() {
    println!("cargo:rerun-if-changed=src/corners.wgsl");
    let source = std::fs::read_to_string("src/corners.wgsl").expect("corner shader");
    let module = naga::front::wgsl::parse_str(&source).expect("parse corner shader");
    let info = naga::valid::Validator::new(
        naga::valid::ValidationFlags::all(),
        naga::valid::Capabilities::PUSH_CONSTANT,
    )
    .validate(&module)
    .expect("validate corners");
    let mut output = String::new();
    for (name, stage) in [
        ("vertex", naga::ShaderStage::Vertex),
        ("fragment", naga::ShaderStage::Fragment),
    ] {
        let options = naga::back::spv::Options {
            flags: naga::back::spv::WriterFlags::empty(),
            ..Default::default()
        };
        let words = naga::back::spv::write_vec(
            &module,
            &info,
            &options,
            Some(&naga::back::spv::PipelineOptions {
                shader_stage: stage,
                entry_point: name.into(),
            }),
        )
        .expect("compile corners");
        output.push_str(&format!(
            "static {}: &[u32] = &{:?};\n",
            name.to_uppercase(),
            words
        ));
    }
    let directory = std::path::PathBuf::from(std::env::var_os("OUT_DIR").unwrap());
    std::fs::write(directory.join("corners.rs"), output).expect("write corners");
}
