#[path = "src/shader_compile.rs"]
mod shader;

fn main() {
    println!("cargo:rerun-if-changed=src/native.wgsl");
    println!("cargo:rerun-if-changed=src/shader_compile.rs");
    let vertex = shader::spirv(naga::ShaderStage::Vertex, "vertex").expect("vertex SPIR-V");
    let fragment = shader::spirv(naga::ShaderStage::Fragment, "fragment").expect("fragment SPIR-V");
    let (metal, metal_vertex, metal_fragment) = shader::msl().expect("Metal shader");
    let output = format!(
        "pub const VERTEX_SPIRV: &[u32] = &{vertex:?};\n\
         pub const FRAGMENT_SPIRV: &[u32] = &{fragment:?};\n\
         pub const METAL_SOURCE: &str = {metal:?};\n\
         pub const METAL_VERTEX: &str = {metal_vertex:?};\n\
         pub const METAL_FRAGMENT: &str = {metal_fragment:?};\n");
    let directory = std::path::PathBuf::from(std::env::var_os("OUT_DIR").unwrap());
    std::fs::write(directory.join("native_shaders.rs"), output).expect("write embedded shaders");
}
