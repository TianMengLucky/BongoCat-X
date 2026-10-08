//! Native shaders are translated once at build time using Naga.
#[allow(dead_code)]
mod compiled {
    include!(concat!(env!("OUT_DIR"), "/native_shaders.rs"));
}
#[cfg(any(target_os = "windows", target_os = "linux"))]
pub fn spirv(stage: naga::ShaderStage, entry: &str) -> Result<Vec<u32>, String> {
    match (stage, entry) {
        (naga::ShaderStage::Vertex, "vertex") => Ok(compiled::VERTEX_SPIRV.to_vec()),
        (naga::ShaderStage::Fragment, "fragment") => Ok(compiled::FRAGMENT_SPIRV.to_vec()),
        _ => Err("Unknown Inox2D shader entry point".into()),
    }
}
#[cfg(any(target_os = "macos", test))]
pub fn msl() -> Result<(String, String, String), String> {
    Ok((compiled::METAL_SOURCE.into(), compiled::METAL_VERTEX.into(), compiled::METAL_FRAGMENT.into()))
}
#[cfg(test)]
#[path = "shader_compile.rs"]
mod compiler;
