#!/usr/bin/env bash
set -euo pipefail

# Usage: check-appimage.sh APPIMAGE [--diagnostic]
# The build that produced the AppImage knows its shape: runtime-Core/full
# builds include the Live2D plugin, whose shaders are embedded.
appimage=$(realpath "${1:?Usage: check-appimage.sh APPIMAGE [--diagnostic]}")
shift || true
expect_live2d=1
[[ ${1:-} == '--diagnostic' ]] && expect_live2d=0
test_runner=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/test-unix.sh
# The restored Cubism SDK lives in the build workspace next to .github/.
script_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
core_lib="$script_dir/../../vendor/CubismSdkForNative/Core/dll/linux/x86_64/libLive2DCubismCore.so"
work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT
cd "$work"
chmod +x "$appimage"
"$appimage" --appimage-extract > /dev/null
root="$work/squashfs-root"
required=(AppRun bongocat.desktop bongocat.png usr/bin/BongoCat
  usr/bin/assets/bongocat.png usr/bin/assets/locales/en-US.json
  usr/bin/assets/models/standard/cat.model3.json
  usr/bin/assets/models/standard/demomodel.moc3
  usr/bin/assets/models/standard/demomodel.1024/texture_00.png)
if [[ $expect_live2d == 1 ]]; then
  required+=(usr/bin/plugins/libbongo_live2d.so)
fi
# Default builds include Inox2D; check its redistribution terms when present.
if [[ -e "$root/usr/bin/plugins/libbongo_inox2d.so" ]]; then
  required+=(usr/bin/plugins/libbongo_inox2d.so usr/bin/licenses/Inox2D-LICENSE)
fi
for file in "${required[@]}"; do
  test -s "$root/$file" || { echo "Missing AppImage resource: $file" >&2; exit 1; }
done
# No build ships the Core binary - users supply it at runtime.
if find "$root" -name 'Live2DCubismCore.*' | grep -q .; then
  echo 'Core binary leaked into the AppImage' >&2
  exit 1
fi
if [[ $expect_live2d == 0 ]]; then
  echo 'AppImage layout verified (diagnostic package, smoke test skipped).'
  exit 0
fi
# Runtime-Core packages render only once the Core is supplied; drop it in
# the same way an end user would (data-dir live2d folder) so the smoke
# test exercises the real runtime import path.
if [[ ! -s "$core_lib" ]]; then
  echo "Cubism Core library not found in the restored SDK: $core_lib" >&2
  exit 1
fi
mkdir -p "$work/smoke-data/data/live2d"
cp "$core_lib" "$work/smoke-data/data/live2d/"
# Exercise the actual AppImage entry point without requiring a FUSE mount.
bash "$test_runner" env APPIMAGE_EXTRACT_AND_RUN=1 \
  BONGO_CAT_DISABLE_NEARBY_MODEL_SCAN=1 "$appimage" \
  --ci-smoke --ci-ignore-global-input \
  --ci-live2d-scenario=visual-consistency "--storage-root=$work/smoke-data"
mapfile -d '' audits < <(find "$work/smoke-data" -name live2d-audit.txt -print0)
[[ ${#audits[@]} == 1 ]]
grep -qx 'renderer=cubism-native' "${audits[0]}"
grep -qx 'assertions=passed' "${audits[0]}"
echo 'AppImage layout and native Live2D smoke test passed.'
