# Stack roadmap (best for *this* plugin)

**Status:** Living plan (2026-10-31). Prefer **capability for OpenRGB 3D Spatial** over industry “best stack” lists. Bigger engines stay in race when they unlock more in-plugin work; reject prestige deps that add a second graphics/runtime stack.

**Related:** [qrhi-openrgb-plugin-api.md](qrhi-openrgb-plugin-api.md) (host QRhi, parked), [effects-engines.md](effects-engines.md), [extract-later.md](extract-later.md), [multipass-coverage.md](multipass-coverage.md) (Animated Mesh / viewport).

## Decision bar

Adopt or keep something when it is clearly better for:

- OpenRGB **Qt plugin** (GPL, Win / Linux / macOS)
- **More capability** when we want it (mesh look, fields, capture) — lean is preferred, but not a hard veto on larger pieces
- **Cleaner / faster / less overhead** in practice (not just nicer APIs)

Do **not** add a library that only renames math or duplicates a host concern.

## Ordered backlog

| Priority | Item | Notes |
|----------|------|--------|
| **Now** | MeshUnlit Sketchfab-ish look | HDR emissive → bloom → tonemap; cheap hemisphere IBL; raw glTF UV/emis from JSON. Soft glow = LED product only (LED Off to judge mesh) |
| **1** | Capture / audio backends | WGC (Windows) + DXGI fallback; slim `AudioInputManager` / one loopback path |
| **2** | **Host-owned QRhi** | Unpark [qrhi-openrgb-plugin-api.md](qrhi-openrgb-plugin-api.md); OpenRGB owns one `QRhi`, plugin hooks in |
| **Later** | **fastgltf** dual import | Only if Assimp still fails a real glTF; C++17 OK on v0.9.x |
| **Later** | Real cubemap IBL / full GGX | Same MeshUnlit backend; optional after HDR-bloom path proves out |
| **Later** | **GLM** | Header-only; reimplement `ViewportMath` guts behind same names |
| **Later** | Optional engine extract | Movie Sync / Game / Screen Mirror DLLs per [extract-later.md](extract-later.md) |

## Keep (right class of tool today)

| Area | Choice |
|------|--------|
| UI / viewport | Qt Widgets + `QOpenGLWidget` / GL 4.1 Core |
| Effect fields | Shared volume / shader field assists + author `.fs` packs |
| Settings | nlohmann JSON |
| Viewport math | `ViewportMath` until GLM later |
| Multi-format mesh | Assimp (fastgltf only if Assimp gap appears) |
| Game bridge | UDP + SHM contract (Minecraft pattern) |
| Movie Sync | Sidecar + Script API + external VLC helper |

## In race (bigger OK if it extends the spine)

| Option | When |
|--------|------|
| Host **QRhi** | Need Metal/Vulkan/D3D and stronger GPU effects without forking a second stack in the plugin |
| **fastgltf** | Only when Assimp mishandles a real asset after JSON KHR overlay |
| Filament-class look | Prefer MeshUnlit HDR-bloom + cheap IBL first; Filament only if still too low |
| WGC / leaner capture | Screen Mirror latency and reliability |

## Out / not for this plugin

| Option | Why |
|--------|-----|
| OPENRNDR | Kotlin creative framework — wrong host |
| Bullet | Physics only — we already ray/AABB pick |
| Plugin-owned QRhi / Qt Quick 3D viewport rewrite | Second stack; fights OpenRGB |
| OBS / Unity / Unreal as deps | Install weight and wrong product surface |
| Assimp-only forever *or* fastgltf-only | Prefer Assimp + JSON KHR until a real gap forces dual path |
| Path tracing / cascaded shadows as near-term | Hard ceiling on current GL overlay; revisit with QRhi era |

## Engines & integrations (same bar)

- **Volume / Audio / Media / Shader Field / Actions:** keep the player + content-pack model — do not invent per-look native engines.
- **Screen Mirror:** upgrade **capture backend**, not the effect codec.
- **Audio:** one shared FFT service; slim the manager, don’t add a second analyzer stack.
- **Game:** next titles reuse SHM/UDP-style contracts; don’t pull full game SDKs into OpenRGB.

## Changelog

| Date | Note |
|------|------|
| 2026-10-31 | Captured QRhi + fastgltf(+Assimp) + GLM-later + engine/capture priorities from planning chat. |
| 2026-10-31 | Sketchfab-ish MeshUnlit: RGBA16F HDR scene, bloom then tonemap, cheap hemisphere IBL; fastgltf demoted until Assimp gap. |
