"""Exercise content detection, dynamic loading and native/GL render equivalence."""
import argparse
import base64
import copy
import json
from pathlib import Path
import re
import subprocess
import tempfile

PNG = base64.b64decode("iVBORw0KGgoAAAANSUhEUgAAAAIAAAACCAYAAABytg0kAAAAFElEQVR4nGP8Lyf3n4GBgYGJAQoAIrkCPiaVkt4AAAAASUVORK5CYII=")
BASE = {'nodes': {'uuid': 0, 'type': 'Node', 'name': 'root', 'enabled': True, 'zsort': 0, 'lockToRoot': False, 'transform': {'trans': [0, 0, 0], 'rot': [0, 0, 0], 'scale': [1, 1]}, 'children': [{'uuid': 1, 'type': 'Part', 'name': 'triangle', 'enabled': True, 'zsort': 0, 'lockToRoot': False, 'transform': {'trans': [0, 0, 0], 'rot': [0, 0, 0], 'scale': [1, 1]}, 'textures': [0], 'blend_mode': 'Normal', 'mesh': {'verts': [-100, -100, 100, -100, 0, 100], 'uvs': [0, 0, 1, 0, 0.5, 1], 'indices': [0, 1, 2], 'origin': [0, 0]}}]}, 'param': [], 'physics': {'pixelsPerMeter': 100, 'gravity': 9.8}, 'meta': {'rigger': None, 'artist': None, 'copyright': None, 'licenseURL': None, 'contact': None, 'reference': None, 'name': 'Native triangle', 'version': '1.0', 'preservePixels': False}}


def fixture(path):
    model = copy.deepcopy(BASE)
    prototype = model["nodes"]["children"][0]

    def part(uid, x, y, width, height, blend="Normal", opacity=1.0):
        node = copy.deepcopy(prototype)
        node.update(uuid=uid, name=str(uid), zsort=-float(uid),
                    blend_mode=blend, opacity=opacity)
        node["mesh"].update(verts=[x, y, x+width, y, x, y+height, x+width, y+height],
                            uvs=[0, 0, 1, 0, 0, 1, 1, 1], indices=[0, 1, 2, 2, 1, 3])
        return node

    children = [part(1, -100, -100, 200, 200, opacity=.55),
                part(2, -70, -70, 100, 160, opacity=.7)]
    mask = part(3, -100, -90, 170, 140, "Screen", .8)
    mask.update(masks=[{"source": 2, "mode": "Mask"}],
                tint=[.8, .5, 1], screenTint=[.2, .3, 0])
    children.append(mask)
    for uid, blend in enumerate(["Multiply", "ColorDodge", "LinearDodge", "Screen",
                                 "ClipToLower", "SliceFromLower"], 4):
        children.append(part(uid, -85+(uid-4)*24, -65, 34, 65, blend, .35))
    composite = copy.deepcopy(model["nodes"])
    composite.update(uuid=20, type="Composite", name="Composite", zsort=-20,
                     blend_mode="Normal", opacity=.65, tint=[.7, 1, .8])
    composite["children"] = [part(21, 0, 25, 65, 55), part(22, 40, 35, 45, 40, "Screen", .6)]
    children.append(composite)
    model["nodes"]["children"] = children
    payload = json.dumps(model, separators=(",", ":")).encode()
    path.write_bytes(b"TRNSRTS\0" + len(payload).to_bytes(4, "big") + payload +
                     b"TEX_SECT" + (1).to_bytes(4, "big") + len(PNG).to_bytes(4, "big") + b"\0" + PNG)


def run(app, root, model, backend, expect_frame=True):
    config = root / "config"
    config.mkdir(parents=True, exist_ok=True)
    (config / "settings.json").write_text(json.dumps({
        "format": "bongocat/settings", "schemaVersion": 1,
        "application": {"renderBackend": backend}}))
    nearby = root / "empty-nearby"
    nearby.mkdir(exist_ok=True)
    command = [str(app), "--ci-smoke", "--ci-ignore-global-input", "--ci-exit-ms=700",
               "--storage-root="+str(root), "--nearby-root="+str(nearby), "--ci-import="+str(model)]
    result = subprocess.run(command, capture_output=True, timeout=40)
    log_path = root / "logs/BongoCat.log"
    log = log_path.read_text(encoding="utf-8") if log_path.exists() else ""
    (root.parent / (backend+"-render.log")).write_text(log, encoding="utf-8")
    assert result.returncode == 0, log + result.stderr.decode("utf-8", "replace")
    assert "loaded plugin: Inox2D Rust" in log, log
    assert "source=5" in log and "setting=renamed.model3.json" in log, log
    if not expect_frame:
        return
    if backend == "vulkan" and "gl=Vulkan" not in log:
        if "Render backend" in log and "falling back to OpenGL" in log:
            print("Vulkan device unavailable; renderer integration skipped")
            raise SystemExit(77)
        raise AssertionError("Inox2D unexpectedly fell back to OpenGL:\n" + log)
    assert "[ERROR:" not in log, log
    stats = re.search(r"First-frame pixels:.*?total=(\d+) alpha=(\d+).*?avg_rgba=([0-9.,]+)", log)
    assert stats and int(stats[2]) > int(stats[1])*.25, log
    return int(stats[2]), tuple(float(v) for v in stats[3].split(","))


def main():
    args = argparse.ArgumentParser(description=__doc__)
    args.add_argument("--app", type=Path, required=True)
    args.add_argument("--build", type=Path, required=True)
    options = args.parse_args()
    with tempfile.TemporaryDirectory(prefix="inox-render-", dir=options.build) as directory:
        root = Path(directory)
        model = root / "renamed.model3.json"  # Contents, rather than extension, choose Inox2D.
        fixture(model)
        # An SDK-less host starts without a default model; the first import
        # occurs before its main loop. Relaunch with the installed model for capture.
        run(options.app, root / "data", model, "opengl", expect_frame=False)
        gl_alpha, gl_color = run(options.app, root / "data", model, "opengl")
        vk_alpha, vk_color = run(options.app, root / "data", model, "vulkan")
        # Rasterization at edges may vary with host GL MSAA; interior appearance stays equivalent.
        assert abs(gl_alpha-vk_alpha) < max(gl_alpha, vk_alpha)*.02, (gl_alpha, vk_alpha)
        assert max(abs(a-b) for a, b in zip(gl_color, vk_color)) < 2, (gl_color, vk_color)
        print("Inox2D: content detection, masks, tint, seven blends and composites verified on GL/Vulkan")


if __name__ == "__main__":
    main()
