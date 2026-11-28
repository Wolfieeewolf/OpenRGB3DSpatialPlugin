#!/usr/bin/env bash
# Builds the Movie Sync extension for every major browser, into dist/:
#   dist/chromium/                 Chrome, Edge, Brave, Opera, Vivaldi (load unpacked)
#   dist/firefox/                  Firefox 128+ (about:debugging -> Load Temporary Add-on)
#   dist/movie-sync-chromium-<v>.zip   for the Chrome Web Store / Edge Add-ons, or to share
#   dist/movie-sync-firefox-<v>.zip    for addons.mozilla.org (signing), or to share
# Firefox declares the background script as "scripts" instead of a service worker; nothing
# else differs. Needs python3 (Linux / macOS).
set -euo pipefail
root="$(cd "$(dirname "$0")" && pwd)"
python3 - "$root" <<'PY'
import json, os, shutil, sys, zipfile
root = sys.argv[1]
dist = os.path.join(root, "dist")
base = json.load(open(os.path.join(root, "manifest.json"), encoding="utf-8"))
version = base["version"]
scripts = sorted(f for f in os.listdir(root) if f.endswith(".js"))

def build(name, manifest):
    out = os.path.join(dist, name)
    shutil.rmtree(out, ignore_errors=True)
    os.makedirs(out)
    for f in scripts:
        shutil.copy2(os.path.join(root, f), out)
    shutil.copytree(os.path.join(root, "icons"), os.path.join(out, "icons"))
    with open(os.path.join(out, "manifest.json"), "w", encoding="utf-8") as fh:
        json.dump(manifest, fh, indent=2, ensure_ascii=False)
    zpath = os.path.join(dist, f"movie-sync-{name}-{version}.zip")
    with zipfile.ZipFile(zpath, "w", zipfile.ZIP_DEFLATED) as z:
        for folder, _, files in os.walk(out):
            for f in sorted(files):
                full = os.path.join(folder, f)
                z.write(full, os.path.relpath(full, out).replace(os.sep, "/"))
    print(f"{name:<9} {out}\n          {zpath}")

os.makedirs(dist, exist_ok=True)
chromium = dict(base)
chromium.pop("browser_specific_settings", None)
build("chromium", chromium)
firefox = dict(base)
firefox["background"] = json.load(open(os.path.join(root, "manifest-firefox-overrides.json")))["background"]
build("firefox", firefox)
PY
