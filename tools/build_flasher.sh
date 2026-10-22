#!/bin/bash
# Builds the public firmware images for the web flasher (docs/, GitHub Pages):
# every board, with PUBLIC_BUILD so src/secrets.h is ignored (no Wi-Fi, no
# photo upload, no private URLs), then checks the images really don't contain
# anything from your secrets.h files before writing them to docs/firmware/.
#
#   tools/build_flasher.sh        then commit docs/ and push
#   ONLY=armband tools/build_flasher.sh   just the boards whose id contains "armband"
set -euo pipefail
cd "$(dirname "$0")/.."
ROOT=$PWD
VERSION=$(cat VERSION)
OUT=docs/firmware
mkdir -p "$OUT"
export PLATFORMIO_BUILD_FLAGS="-DPUBLIC_BUILD"

# board id | project | env | chip | name
BOARDS=(
  "costume-guition10|costume|guition-p4-10-1|ESP32-P4|ROGUE.AI costume -- Guition 10.1\" JC8012P4A1C"
  "costume-guition10-oldpanel|costume|guition-p4-10-1-old-panel|ESP32-P4|ROGUE.AI costume -- Guition 10.1\" (older panel)"
  "scanner-tab5|scanner|tab5|ESP32-P4|LAB-7 scanner -- M5Stack Tab5"
  "scanner-waveshare349|scanner|scanner|ESP32-S3|LAB-7 scanner -- Waveshare ESP32-S3-Touch-LCD-3.49"
  "armband-cyd24|armband|armband|ESP32|ARM-7 armband -- Sunton ESP32-2432S024C (2.4\" CYD)"
)

# Strings that must never be in a public image: every quoted value in the
# secrets.h files that exist on this machine.
SECRETS=()
for f in costume/src/secrets.h scanner/src/secrets.h cloud/.keys; do
  [ -f "$f" ] || continue
  while IFS= read -r v; do [ ${#v} -ge 6 ] && SECRETS+=("$v"); done \
    < <(grep -o '"[^"]*"' "$f" | tr -d '"'; grep -o '=[^ ]*$' "$f" | tr -d '=')
done

for b in "${BOARDS[@]}"; do
  IFS='|' read -r id proj env chip name <<<"$b"
  [[ -n "${ONLY:-}" && "$id" != *"$ONLY"* ]] && continue
  echo "=== $id ($proj/$env)"
  (cd "$proj" && pio run -e "$env")
  img="$proj/.pio/build/$env/firmware.factory.bin"
  for v in "${SECRETS[@]}"; do
    if grep -qaF -- "$v" "$img"; then
      echo "!!! $img contains a private value from your secrets -- NOT publishing" >&2
      exit 1
    fi
  done
  cp "$img" "$OUT/$id.bin"
  jname=$(python3 -c 'import json,sys; print(json.dumps(sys.argv[1]))' "$name")
  cat > "docs/manifest-$id.json" <<JSON
{ "name": $jname, "version": "$VERSION", "new_install_prompt_erase": true,
  "builds": [ { "chipFamily": "$chip", "parts": [ { "path": "firmware/$id.bin", "offset": 0 } ] } ] }
JSON
done
# The public copy of the QR landing page (no photo feed).
python3 - <<'PY'
src = open("web/halloween-landing.html").read()
open("docs/halloween/index.html", "w").write(
    '<!doctype html>\n<html lang="en"><head><meta charset="utf-8">\n'
    '<meta name="viewport" content="width=device-width, initial-scale=1">\n'
    '<title>ROGUE.AI v6.6.6</title>\n<style>body{margin:0;background:#000;padding:16px 0}</style>\n'
    '</head><body>\n<!-- Generated from web/halloween-landing.html (public defaults: no photo feed). -->\n'
    + src + '\n</body></html>\n')
PY
sed -i '' "s/data-version=\"[^\"]*\"/data-version=\"$VERSION\"/" docs/index.html 2>/dev/null || true
echo
echo "Done: $VERSION, no private values found in the images (checked ${#SECRETS[@]} strings)."
echo "Unset PLATFORMIO_BUILD_FLAGS is automatic (this shell only); your next normal"
echo "'pio run' rebuilds with your secrets.h again."
