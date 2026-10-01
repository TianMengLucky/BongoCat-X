#!/usr/bin/env bash
set -euo pipefail

appimage=$(realpath "${1:?Usage: check-appimage.sh APPIMAGE}")
test_runner=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/test-unix.sh
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
diagnostic=0
[[ $appimage =~ -Diagnostic- ]] && diagnostic=1
if [[ $diagnostic == 0 ]]; then
  required+=(usr/bin/assets/FrameworkShaders/VertShaderSrc.vert
    usr/bin/assets/FrameworkShaders/FragShaderSrc.frag
    usr/bin/assets/FrameworkShaders/VertShaderSrcBlend.vert
    usr/bin/assets/FrameworkShaders/FragShaderSrcBlend.frag)
fi
for file in "${required[@]}"; do
  test -s "$root/$file" || { echo "Missing AppImage resource: $file" >&2; exit 1; }
done
# Diagnostic packages render without the Cubism SDK, so the native
# Live2D audit cannot pass; skip the smoke test for them.
if [[ $diagnostic == 1 ]]; then
  echo 'AppImage layout verified (diagnostic package, smoke test skipped).'
  exit 0
fi
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
