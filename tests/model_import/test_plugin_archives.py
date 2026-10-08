"""Verify plugin ZIPs, external Core import and traversal/duplicate rejection."""
import argparse
from pathlib import Path
import subprocess
import shutil
import tempfile
from zipfile import ZipFile, ZIP_DEFLATED

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--tester", required=True)
parser.add_argument("--plugin", type=Path, required=True)
parser.add_argument("--build", type=Path, required=True)
args = parser.parse_args()
with tempfile.TemporaryDirectory(prefix="plugin-zip-", dir=args.build) as directory:
    root = Path(directory)
    archives = []
    # A locally supplied SDK enables real Core tests without redistributing it.
    core = args.build.parent / "vendor/CubismSdkForNative/Core/dll/windows/x86_64/Live2DCubismCore.dll"
    native_core = core.is_file() and args.plugin.suffix == ".dll"
    for kind in ("valid", "core", "traversal", "duplicate"):
        path = root / (kind + ".zip")
        with ZipFile(path, "w", ZIP_DEFLATED) as archive:
            archive.write(args.plugin, "nested/plugins/" + args.plugin.name)
            if kind == "core":
                archive.writestr("Live2DCubismCore.dll" if native_core else "Core/README.txt", b"existing Core must not be replaced")
            elif kind == "traversal":
                archive.writestr("../escape.txt", b"must not escape")
            elif kind == "duplicate":
                archive.write(args.plugin, "other/" + args.plugin.name)
        archives.append(str(path))
    if native_core:
        path = root / "auto-core.zip"
        with ZipFile(path, "w", ZIP_DEFLATED) as archive:
            archive.write(args.plugin, "plugins/" + args.plugin.name)
            archive.write(core, "Core/dll/windows/x86_64/Live2DCubismCore.dll")
        archives.append(str(path))
    tester = args.tester
    if native_core:
        # Avoid the development executable's adjacent Core and cached user Core.
        isolated = root / Path(args.tester).name
        shutil.copy2(args.tester, isolated)
        tester = str(isolated)
    result = subprocess.run([tester, str(args.plugin), str(root / "data"), *archives], timeout=30)
    assert not (root / "escape.txt").exists()
    assert not (root / "data/plugins/Live2DCubismCore.dll").exists()
    if native_core:
        imported = root / "data/live2d/auto-core/Core/dll/windows/x86_64/Live2DCubismCore.dll"
        assert imported.read_bytes() == core.read_bytes(), "existing Core was replaced"
    raise SystemExit(result.returncode)
