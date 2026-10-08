"""Apply small, checked host-integration fixes to a fetched, pinned dependency.

No third-party sources are committed. The output crate lives in the build tree.
"""
import argparse
import pathlib
import shutil

REV = "d4dd9dd7f16b775042cbda44370570abf9a8cf81"


def replace(text, old, new, minimum=1):
    if text.count(old) < minimum:
        raise RuntimeError("Pinned Inox2D patch anchor changed: " + old)
    return text.replace(old, new)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--upstream", required=True)
    parser.add_argument("--source", required=True)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()
    upstream, source, output = map(pathlib.Path, (args.upstream, args.source, args.output))
    renderer = output / "inox2d-opengl"
    renderer.mkdir(parents=True, exist_ok=True)
    shutil.copytree(upstream / "inox2d-opengl", renderer, dirs_exist_ok=True)
    for path in (renderer / "src").rglob("*.rs"):
        text = path.read_text(encoding="utf-8")
        text = text.replace("&glow::Context", "&crate::TrackedGl")
        path.write_text(text, encoding="utf-8", newline="\n")
    path = renderer / "src/lib.rs"
    text = path.read_text(encoding="utf-8")
    text = replace(text, "gl: glow::Context,", "gl: TrackedGl,", minimum=1)
    # Keep the public constructor accepting a Context, then wrap immediately.
    text = replace(text, "pub fn new(gl: TrackedGl,", "pub fn new(gl: glow::Context,")
    text = replace(text, "\t\tunsafe {\n\t\t\t// Initialize framebuffers", "\t\tlet gl = TrackedGl::new(gl);\n\t\tunsafe {\n\t\t\t// Initialize framebuffers")
    text = "mod resource_owner;\npub use resource_owner::TrackedGl;\n" + text
    text = replace(text, "pub viewport: UVec2,", "pub viewport: UVec2,\n\tbongo_matrix: Option<glam::Mat4>,\n\tbongo_framebuffer: Option<glow::Framebuffer>,")
    text = replace(text, "viewport: UVec2::default(),", "viewport: UVec2::default(),\n\t\t\t\tbongo_matrix: None,\n\t\t\t\tbongo_framebuffer: None,")
    text = replace(text, "self.camera.matrix(self.viewport.as_vec2())", "self.bongo_matrix.unwrap_or_else(|| self.camera.matrix(self.viewport.as_vec2()))")
    # Composite output must return to the host target, not framebuffer zero.
    finish = text.index("fn finish_composite_content(")
    text = text[:finish] + text[finish:].replace("gl.bind_framebuffer(glow::FRAMEBUFFER, None);", "gl.bind_framebuffer(glow::FRAMEBUFFER, self.bongo_framebuffer);", 1)
    # Missing optional material slots use the albedo fallback rather than indexing UINT_MAX.
    for slot in ("tex_bumpmap", "tex_emissive"):
        text = replace(text, f"self.textures[part.{slot}.raw()]", f"self.textures.get(part.{slot}.raw()).unwrap_or(&self.textures[part.tex_albedo.raw()])")
    text += """
impl OpenglRenderer {
    pub fn bongo_begin_frame(&mut self, size: glam::UVec2, matrix: glam::Mat4, target: Option<glow::Framebuffer>) {
        self.bongo_matrix = Some(matrix);
        self.bongo_framebuffer = target;
        if self.viewport != size { self.resize(size.x, size.y); }
        self.cache.borrow_mut().clear();
        self.update_camera();
        unsafe { self.gl.bind_framebuffer(glow::FRAMEBUFFER, target); }

    }
}
"""
    path.write_text(text, encoding="utf-8", newline="\n")
    shutil.copyfile(source / "cmake/inox2d_resource_owner.rs", renderer / "src/resource_owner.rs")
    manifest = renderer / "Cargo.toml"
    text = manifest.read_text(encoding="utf-8")
    # Upstream uses workspace dependencies; make this fetched crate standalone.
    workspace = (upstream / "Cargo.toml").read_text(encoding="utf-8")
    import re
    dependencies = dict(re.findall(r"^([\w-]+)\s*=\s*(.+)$", workspace, re.MULTILINE))
    def dependency(match):
        name = match[1]
        if name == "inox2d":
            return f'{name} = {{ git = "https://github.com/Inochi2D/inox2d", rev = "{REV}" }}'
        return name + " = " + dependencies[name]
    text = re.sub(r"^([\w-]+)\s*=\s*\{\s*workspace\s*=\s*true\s*\}\s*$", dependency, text, flags=re.MULTILINE)
    text = re.sub(r'^inox2d\s*=\s*\{\s*path\s*=.*$', f'inox2d = {{ git = "https://github.com/Inochi2D/inox2d", rev = "{REV}" }}', text, flags=re.MULTILINE)
    manifest.write_text(text, encoding="utf-8", newline="\n")
    plugin = output / "bongo-inox2d"
    shutil.copytree(source / "src/rust/bongo-inox2d", plugin, dirs_exist_ok=True,
                    ignore=shutil.ignore_patterns("target"))
    manifest = plugin / "Cargo.toml"
    text = manifest.read_text(encoding="utf-8")
    text = text.replace('path = "../bongo-safe"', 'path = "' + (source / "src/rust/bongo-safe").as_posix() + '"')
    text = re.sub(r'^inox2d-opengl\s*=.*$', 'inox2d-opengl = { path = "../inox2d-opengl" }', text, flags=re.MULTILINE)
    manifest.write_text(text, encoding="utf-8", newline="\n")
    lock = plugin / "Cargo.lock"
    text = lock.read_text(encoding="utf-8")
    text = re.sub(r'(name = "inox2d-opengl"\nversion = "[^\n]+"\n)source = "[^\n]+"\n', r'\1', text)
    lock.write_text(text, encoding="utf-8", newline="\n")


if __name__ == "__main__":
    main()
