# OpenRGB host-owned QRhi (parked)

**Status:** Design parked. Stack effect-volume boxes landed on `feat/effect-volume-boxes`; QRhi spike is next when unparked.  
**Not in scope now:** Implementing QRhi, forking OpenRGB for this, or a new plugin API repo.

## Goal

OpenRGB **owns** one `QRhi` (same ownership model as ResourceManager / config paths). Plugins **hook in** optionally. No second graphics stack per plugin. No Qt Quick 3D. No drive-by rewrites of OpenRGB UI.

Calc is picky: **smallest clean change that works**, no unused helpers.

## Why (3D Spatial)

[OpenRGB 3D Spatial](https://github.com/Wolfieeewolf/OpenRGB3DSpatialPlugin) already uses GPU for the viewport and effect passes (`QOpenGLWidget` / shaders / offscreen). Portable **OpenGL 4.1** keeps Win/Linux/macOS on one path but caps effects (especially macOS).

**QRhi** is the Qt-native path to Vulkan / Metal / D3D / OpenGL. Plugin API **v5** has no graphics/window hook.

Without a host hook, the practical fork is: stay on 4.1 everywhere, or put stronger GPU features on Windows/Linux only and leave macOS on a lesser path. Host-owned QRhi avoids that split.

## This could also help

- **Effects** — ShaderToy OpenGL path → Metal / modern backends  
- **Wallpaper / Pixel Screen-style plugins** — smoother framebuffer updates if they opt in  
- **OpenRGB later (optional)** — denser device/LED previews with less CPU paint; one graphics path for host + plugins  

CPU/Widgets-only plugins ignore the hook.

## API shape (preferred)

Host-owned, minimal surface (names illustrative):

1. OpenRGB creates **one** `QRhi` at app start (backend by platform: Metal / Vulkan / D3D / GL), destroys on shutdown.  
2. Plugin API **v6** (or optional extension) adds only what is required, e.g.:  
   - `QRhi* GetRhi()`  
   - bind plugin tab → host-provided render target / `QRhiWidget` (lifetime documented)  
3. Plugins that never call it behave exactly as today (v5 unchanged for existing plugins).

**Not for v1 of the spike:** shared convenience render helpers, Quick 3D, rewriting DeviceView onto RHI, plugin-owned second `QRhi`.

### Packaging

- QRhi core lives in **Qt Gui** (already shipped).  
- May need **Qt Shader Tools** (extra Qt module DLL, same class of issue as Network / OpenGLWidgets for this plugin).  
- Official OpenRGB builds must include the RHI backends they care about (esp. Metal on macOS).

## Work plan (when unparked)

1. Fork/branch **OpenRGB** — init/shutdown `QRhi` only + tiny API bump.  
2. New **plugin branch/repo** targeting that API — port viewport/effects onto host RHI (full quality on our side).  
3. Point this repo’s OpenRGB **submodule** at the QRhi fork; build + test together.  
4. Upstream: issue + MR with the **minimal** host patch; keep plugin on a parallel track until merged.

### Suggested first proof

Host: create `QRhi`, expose getter + tab target.  
Plugin: clear color / triangle on that target.  
Then port real viewport/effects — do not leave a permanent half-GL / half-RHI mess.

## Upstream issue draft (short)

**Title:** Plugin API: optional host-owned Qt QRhi hook for GPU plugins

**Ask:** OpenRGB owns one `QRhi`; plugins optionally get `GetRhi()` + a tab render target. New API version / extension. Not Quick 3D; not a full UI rewrite.

**This is why:** 3D Spatial already GPU-renders a 3D room + effects but must stay on OpenGL 4.1 for macOS. QRhi is the Qt-native path to Metal/Vulkan. API v5 has no graphics hook. Without it, stronger GPU features split Win/Linux vs a lesser macOS path.

**This could also help:** Effects shaders; wallpaper/pixel-screen-style plugins; optional later OpenRGB previews.

**Questions:** Consider for a future API? Prefer host-owned shared `QRhi` (plugins hook in)? Packaging OK for Metal/Vulkan backends / ShaderTools if needed? Can prototype on a fork.

File on: https://gitlab.com/CalcProgrammer1/OpenRGB/-/issues

## Decision log

| Date | Decision |
|------|----------|
| 2026-10-29 | Prefer **host-owned** QRhi over plugin-owned / window-only second stack. |
| 2026-10-29 | **Park** implementation until volume-box / gizmo work on `feat/effect-volume-boxes` is finished. |
| 2026-10-29 | Documented here; do not start OpenRGB QRhi fork in this chat pass. |
| 2026-10-30 | Volume-box / gizmo stack work landed; QRhi remains parked until an explicit spike. |
