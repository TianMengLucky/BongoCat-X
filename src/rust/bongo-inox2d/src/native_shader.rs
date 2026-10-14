//! Native shaders are translated once at build time using Naga.
#[allow(dead_code)]
mod compiled {
    include!(concat!(env!("OUT_DIR"), "/native_shaders.rs"));
}
#[cfg(any(target_os = "windows", target_os = "linux"))]
pub fn spirv() -> (&'static [u32], &'static [u32]) {
    (compiled::VERTEX_SPIRV, compiled::FRAGMENT_SPIRV)
}
#[cfg(any(target_os = "macos", test))]
pub fn msl() -> (&'static str, &'static str, &'static str) {
    (
        compiled::METAL_SOURCE,
        compiled::METAL_VERTEX,
        compiled::METAL_FRAGMENT,
    )
}
#[cfg(test)]
#[path = "shader_compile.rs"]
mod compiler;

#[cfg(test)]
mod tests {
    #[test]
    fn embedded_shaders_match_the_compiler() {
        let (module, info) = super::compiler::module().unwrap();
        for (stage, name, embedded) in [
            (
                naga::ShaderStage::Vertex,
                "vertex",
                super::compiled::VERTEX_SPIRV,
            ),
            (
                naga::ShaderStage::Fragment,
                "fragment",
                super::compiled::FRAGMENT_SPIRV,
            ),
        ] {
            assert_eq!(
                super::compiler::spirv(&module, &info, stage, name).unwrap(),
                embedded
            );
        }
        let (source, vertex, fragment) = super::compiler::msl(&module, &info).unwrap();
        assert_eq!(
            super::msl(),
            (source.as_str(), vertex.as_str(), fragment.as_str())
        );
    }
}
