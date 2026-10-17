"""Generation must preserve Cargo fingerprints when inputs are unchanged."""
import importlib.util
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("prepare_inox2d", ROOT / "cmake/prepare_inox2d.py")
prepare = importlib.util.module_from_spec(spec)
spec.loader.exec_module(prepare)


class PrepareInox2DTests(unittest.TestCase):
    def test_preserves_unchanged_files_and_publishes_only_successful_generation(self):
        scratch = ROOT / "build-tests"
        scratch.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="inox-generation-", dir=scratch) as temporary:
            base = Path(temporary).resolve()
            self.assertEqual(base.parent, scratch.resolve())
            upstream, source, output = (base / name for name in ("upstream", "source", "output"))
            renderer = upstream / "inox2d-opengl"
            plugin = source / "src/rust/bongo-inox2d"
            (renderer / "src").mkdir(parents=True)
            (plugin / "src").mkdir(parents=True)
            (plugin / "locales").mkdir()
            (plugin / "locales/en-US.json").write_text('{"name":"first"}\n')
            (source / "cmake").mkdir()
            (upstream / "Cargo.toml").write_text('[workspace.dependencies]\nglow = "0.14.1"\n')
            (renderer / "Cargo.toml").write_text(
                '[dependencies]\ninox2d = { path = "../inox2d" }\nglow = { workspace = true }\n')
            (renderer / "src/lib.rs").write_text(
                'gl: glow::Context,\npub fn new(gl: glow::Context,\npub viewport: UVec2,\n'
                'viewport: UVec2::default(),\n\t\tunsafe {\n\t\t\t// Initialize framebuffers\n'
                'self.camera.matrix(self.viewport.as_vec2())\n'
                'fn finish_composite_content(\ngl.bind_framebuffer(glow::FRAMEBUFFER, None);\n'
                'self.textures[part.tex_bumpmap.raw()]\nself.textures[part.tex_emissive.raw()]\n')
            (source / "cmake/inox2d_resource_owner.rs").write_text('// resource owner\n')
            (plugin / "Cargo.toml").write_text(
                '[dependencies]\nbongo-safe = { path = "../bongo-safe" }\n'
                'bongo-plugin = { path = "../bongo-plugin" }\n'
                'inox2d-opengl = { git = "upstream" }\n')
            (plugin / "Cargo.lock").write_text(
                '[[package]]\nname = "inox2d-opengl"\nversion = "0.1.0"\nsource = "git+upstream"\n')
            (plugin / "src/lib.rs").write_text('// plugin v1\n')
            (plugin / "target").mkdir()
            (plugin / "target/scratch").write_bytes(b'not a source')
            args = ["prepare", "--upstream", str(upstream),
                    "--source", os.path.relpath(source, Path.cwd()),
                    "--output", str(output)]
            with patch.object(sys, "argv", args):
                prepare.main()
                files = [path for path in output.rglob("*") if path.is_file()]
                self.assertFalse((output / "bongo-inox2d/target").exists())
                manifest = (output / "bongo-inox2d/Cargo.toml").read_text()
                for name in ("bongo-safe", "bongo-plugin"):
                    self.assertIn(f'path = "{(source / "src/rust" / name).as_posix()}"', manifest)
                locale = output / "bongo-inox2d/locales/en-US.json"
                self.assertEqual(locale.read_text(), '{"name":"first"}\n')
                self.assertIn("TrackedGl", (output / "inox2d-opengl/src/lib.rs").read_text())
                self.assertNotIn('source =', (output / "bongo-inox2d/Cargo.lock").read_text())
                snapshots = {}
                for path in files:
                    os.utime(path, ns=(1_000_000_000, 1_000_000_000))
                    snapshots[path] = (path.read_bytes(), path.stat().st_mtime_ns)
                prepare.main()
                for path, snapshot in snapshots.items():
                    self.assertEqual((path.read_bytes(), path.stat().st_mtime_ns), snapshot)
                (plugin / "src/lib.rs").write_text('// plugin v2\n')
                (plugin / "locales/en-US.json").write_text('{"name":"second"}\n')
                prepare.main()
                changed = output / "bongo-inox2d/src/lib.rs"
                self.assertEqual(changed.read_text(), '// plugin v2\n')
                self.assertEqual(locale.read_text(), '{"name":"second"}\n')
                for path, snapshot in snapshots.items():
                    if path not in (changed, locale):
                        self.assertEqual((path.read_bytes(), path.stat().st_mtime_ns), snapshot)
                # A rejected upstream patch must not replace working generated files.
                (renderer / "src/lib.rs").write_text('// incompatible upstream\n')
                with self.assertRaises(RuntimeError):
                    prepare.main()
                self.assertEqual(changed.read_text(), '// plugin v2\n')
                self.assertFalse(any(path.name.startswith("prepare-") for path in output.iterdir()))


if __name__ == "__main__":
    unittest.main()
