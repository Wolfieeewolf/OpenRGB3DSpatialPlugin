#!/usr/bin/env bash
# Builds the Firefox copy of the Movie Sync extension in dist/firefox: the same files, with the
# background script declared the way Firefox wants ("scripts" instead of "service_worker").
# Load dist/firefox/manifest.json from about:debugging -> This Firefox -> Load Temporary Add-on.
set -euo pipefail
root="$(cd "$(dirname "$0")" && pwd)"
out="$root/dist/firefox"
rm -rf "$out"
mkdir -p "$out"
cp "$root"/*.js "$out/"
python3 - "$root" "$out" <<'PY'
import json, sys
root, out = sys.argv[1], sys.argv[2]
m = json.load(open(f"{root}/manifest.json"))
m["background"] = json.load(open(f"{root}/firefox/manifest-overrides.json"))["background"]
json.dump(m, open(f"{out}/manifest.json", "w"), indent=2)
PY
echo "Firefox build: $out"
