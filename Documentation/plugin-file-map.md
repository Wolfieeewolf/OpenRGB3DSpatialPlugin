# Plugin file map (engine regroup inventory)

Filled during multipass. **Group by runtime engine**, not presets toolbox folders. Content stays in OpenRGB3DSpatialPresets.

Status of regroup: map only until a dedicated branch. Flag **rogue** = unclear owner / wrong tree / merge fodder.

## Engine buckets

| Bucket | Owns |
|--------|------|
| `engines/volume-field` | GPGPU volume sample / Action volume field |
| `engines/particle-field` | GPGPU particles |
| `engines/ribbon-field` | GPGPU ribbons |
| `engines/folder-volume` | FolderVolume content host |
| `engines/shader-field` | ShaderField |
| `engines/screen-mirror` | Screen Mirror |
| `engines/audio` | Audio input + audio-reactive glue |
| `engines/reactive` | Key/button reactive + Input reactive |
| `engines/game` | Minecraft / game telemetry |
| `engines/movie-sync` | Movie Sync |
| `engines/triggers` | Trigger sources/runtime |
| `engines/actions` | Action player/applier/script (calls field engines) |
| `shared` | Clocks, GL pool, room/grid/LED types, cross-engine helpers |
| `ui` | Forms, widgets, viewport, layout |

## Inventory (seed — expand per multipass)

| Path | Role | Engine | Merge | Notes |
|------|------|--------|-------|-------|
| `Effects3D/Actions/*` | Action orchestration | engines/actions | keep/split later | Calls volume/particle/ribbon |
| `Effects3D/Actions/ActionVolumeField.*` | Volume field bridge | engines/volume-field | | |
| `Effects3D/Actions/ActionParticleField.*` | Particle field bridge | engines/particle-field | | |
| `Effects3D/Actions/ActionRibbonField.*` | Ribbon field bridge | engines/ribbon-field | | |
| `Effects3D/FolderVolume/*` | Content host | engines/folder-volume | | |
| `Effects3D/ShaderField/*` | Shader field effect | engines/shader-field | | |
| `Effects3D/ScreenMirror/*` | Ambilight / capture | engines/screen-mirror | | |
| `Effects3D/Reactive/*` | Reactive effect | engines/reactive | | Likely large — judo candidate |
| `Effects3D/Games/*` | Minecraft stack | engines/game | | |
| `Effects3D/MovieSync/*` | Movie Sync | engines/movie-sync | | |
| `Effects3D/Triggers/*` | Triggers | engines/triggers | | |
| `Effects3D/SpatialLighting/*` | Room light UI bits | shared or room | | |
| `Effects3D/SpatialPatternKernels/*` | Strip kernels | shared | | |
| `Audio/*` | Audio capture | engines/audio | | |
| `Input/Reactive*` | Reactive input | engines/reactive | | |
| `Input/ControllerTilt*` | DualSense tilt | parked | | Feature branch |
| `Shaders/*` | GL pool / field engines | shared | | |
| `SpatialLighting/*` `SpatialRoom/*` `SpatialSamplers/*` | Room core | shared / room | | |
| `Game/*` `integrations/*` | Game SHM / telemetry | engines/game | | |
| `ui/*` | Plugin UI | ui | | |
| Root `SpatialEffect3D.*` etc. | Effect base / types | shared | | Map individually as touched |

## Presets → engine (from volume toolbox multipass)

| Engine | Toolbox tools |
|--------|----------------|
| volume-field | `field_aurora`, `field_burst`, `field_dual_vortex`, `field_electric`, `field_flash`, `field_portal`, `field_ripple`, `field_room_glow`, `field_strata` |
| particle-field | `particle_embers`, `particle_fireflies`, `particle_meteors`, `particle_rain`, `spark_burst` |
| ribbon-field | `laser_sweep`, `neon_ribbon`, `ribbon_helix` |
| scripted volume (FolderVolume eval) | remaining volume `.fx` (confetti, helix, orbit, …) |
