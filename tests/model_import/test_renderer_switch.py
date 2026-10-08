"""Exercise plugin unload/reload in both directions on OpenGL and Vulkan."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import tempfile

from test_inochi_render import fixture


def run(app, root, backend, *arguments, engines=("Live2D Cubism", "Inox2D Rust"), persist=False):
    (root / "config/settings.json").write_text(json.dumps({
        "format": "bongocat/settings", "schemaVersion": 1,
        "application": {"renderBackend": backend}}))
    result = subprocess.run([str(app), "--ci-ignore-global-input",
        "--ci-exit-ms=4500", "--storage-root=" + str(root),
        "--nearby-root=" + str(root / "empty-nearby"),
        *([] if persist else ["--ci-smoke"]), *arguments],
        capture_output=True, timeout=35)
    log = (root / "logs/BongoCat.log").read_text(encoding="utf-8")
    assert result.returncode == 0, log + result.stderr.decode("utf-8", "replace")
    for engine in engines:
        assert "loaded plugin: " + engine in log, log
    if not engines:
        assert "loaded plugin:" not in log, log
    assert "Shutdown complete: exit_code=0" in log, log
    if backend == "vulkan" and "Live2D Cubism" in engines and "gl=Vulkan" not in log:
        if "falling back to OpenGL" in log:
            raise SystemExit(77)
        raise AssertionError("Vulkan backend was not used:\n" + log)
    assert "[ERROR:" not in log, log
    if any(a.startswith("--ci-live2d-scenario=") for a in arguments):
        audit = (root / "state/live2d-audit.txt").read_text()
        assert "operation=accepted" in audit and "assertions=passed" in audit, audit


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--app", type=Path, required=True)
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--source", type=Path, required=True)
    options = parser.parse_args()
    if not (options.app.parent / "Live2DCubismCore.dll").is_file():
        print("External test Core unavailable; renderer switching test skipped")
        raise SystemExit(77)
    with tempfile.TemporaryDirectory(prefix="renderer-switch-", dir=options.build) as directory:
        root = Path(directory)
        for name in ("config", "empty-nearby"):
            (root / name).mkdir()
        shutil.copytree(options.source / "resources/assets/models/standard", root / "models/standard")
        (root / "models/standard/.bongo-cat-builtin").touch()
        model = root / "switch.inp"
        fixture(model)
        for backend in ("opengl", "vulkan"):
            run(options.app, root, backend, "--ci-model=standard", "--ci-import=" + str(model))
            run(options.app, root, backend, "--ci-model=switch.inp", "--ci-live2d-scenario=switch:standard")
            if backend == "opengl":
                run(options.app, root, backend, "--ci-model=switch.inp",
                    "--ci-live2d-scenario=deselect", engines=("Inox2D Rust",), persist=True)
                session = json.loads((root / "state/session.json").read_text())
                assert session["activeModelId"] == "", session
                run(options.app, root, backend, engines=(), persist=True)
                run(options.app, root, backend, "--ci-model=standard", engines=("Live2D Cubism",), persist=True)
        print("GL/Vulkan switching and OpenGL empty selection persistence passed")


if __name__ == "__main__":
    main()
