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

# board id | project | env | chip | name | kind
# kind: factory = full image for the web flasher (manifest written);
#       app = the app image only, offered as a download (LEDLINK: flashed through WLED's OTA page).
BOARDS=(
  "costume-guition10|costume|guition-p4-10-1|ESP32-P4|ROGUE.AI costume -- Guition 10.1\" JC8012P4A1C|factory"
  "costume-guition10-oldpanel|costume|guition-p4-10-1-old-panel|ESP32-P4|ROGUE.AI costume -- Guition 10.1\" (older panel)|factory"
  "costume-elecrow10|costume|elecrow-p4-10|ESP32-P4|ROGUE.AI costume -- Elecrow CrowPanel Advanced 10.1\"|factory"
  "costume-tab5|costume|tab5|ESP32-P4|ROGUE.AI costume -- M5Stack Tab5|factory"
  "costume-elecrow7|costume|elecrow-7|ESP32-S3|ROGUE.AI costume -- Elecrow CrowPanel 7.0|factory"
  "costume-sunton7|costume|sunton-7|ESP32-S3|ROGUE.AI costume -- Sunton ESP32-8048S070 7\"|factory"
  "costume-sunton5|costume|sunton-5|ESP32-S3|ROGUE.AI costume -- Sunton ESP32-8048S050 5\"|factory"
  "scanner-tab5|scanner|tab5|ESP32-P4|LAB-7 scanner -- M5Stack Tab5|factory"
  "scanner-waveshare349|scanner|scanner|ESP32-S3|LAB-7 scanner -- Waveshare ESP32-S3-Touch-LCD-3.49|factory"
  "scanner-lilygo|scanner|lilygo|ESP32-S3|LAB-7 scanner -- LilyGO T-Display-S3-Long|factory"
  "scanner-advance35|scanner|advance35|ESP32-S3|LAB-7 scanner -- Elecrow CrowPanel Advance 3.5\"|factory"
  "scanner-advance43|scanner|advance43|ESP32-S3|LAB-7 scanner -- Elecrow CrowPanel Advance 4.3\"|factory"
  "armband-cyd24|armband|armband|ESP32|ARM-7 armband -- Sunton ESP32-2432S024C (2.4\" CYD)|factory"
  "ledlink-gledopto|ledlink|gledopto|ESP32|LEDLINK -- Gledopto GL-MC-001WL|app"
  "pumpkin-eye-round21|pumpkins|rogue-eye-round-2-1|ESP32-S3|ROGUE.AI pumpkin eye -- Elecrow CrowPanel 2.1\" round|factory"
  "pumpkin-wide-4827s043c|pumpkins|rogue-wide-4827s043c|ESP32-S3|ROGUE.AI pumpkin -- Sunton ESP32-4827S043C 4.3\"|factory"
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
  IFS='|' read -r id proj env chip name kind <<<"$b"
  [[ -n "${ONLY:-}" && "$id" != *"$ONLY"* ]] && continue
  if [ ! -f "$proj/platformio.ini" ]; then
    echo "--- $id: $proj not found, skipped (its image in $OUT stays as it was)"; continue
  fi
  echo "=== $id ($proj/$env)"
  (cd "$proj" && pio run -e "$env")
  if [ "$kind" = app ]; then img="$proj/.pio/build/$env/firmware.bin"
  else img="$proj/.pio/build/$env/firmware.factory.bin"; fi
  for v in "${SECRETS[@]}"; do
    if grep -qaF -- "$v" "$img"; then
      echo "!!! $img contains a private value from your secrets -- NOT publishing" >&2
      exit 1
    fi
  done
  cp "$img" "$OUT/$id.bin"
  [ "$kind" = app ] && continue
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
