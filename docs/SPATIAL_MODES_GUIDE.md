# Spatial modes — what does what (user guide)

OpenRGB 3D Spatial stacks several **independent** systems. They can be combined, but they are **not** the same knob. This doc is the split we are standardizing on in the library and UI.

Related (developers): [SPATIAL_ROOM.md](SPATIAL_ROOM.md), [SPATIAL_LIGHTING_ENGINE.md](SPATIAL_LIGHTING_ENGINE.md), [SPATIAL_LIGHTING_TEST.md](SPATIAL_LIGHTING_TEST.md).

---

## The four layers (simple mental model)

| Layer | What you configure | What it changes on each LED |
|-------|-------------------|-----------------------------|
| **1. Room mode** | **Room output** section on each effect layer | Coordinates, emitter, relay shade, occlusion |
| **2. Room sampler** | **Room mapping** panel on some effects | Optional **tint / direction / game voxel** mixed *into* that effect’s color |
| **3. Shader field** | **Shader Field** effect only | GPU animation sampled at LED positions (not lighting rays) |
| **4. Stack blend** | Effect stack order + blend mode | Layers add/multiply on top of each other |

**Viewport “shader room”** (Grid settings) is only the **3D preview** look (walls/floor). It does **not** drive LED colors.

---

## 1. Room modes (library groups)

Each effect belongs to **one** primary mode. We are moving away from one effect with twenty dropdowns that secretly change mode; instead you pick the right **library entry**.

| Library group | Room mode | Plain English | Occlusion / shadows? |
|---------------|-----------|---------------|----------------------|
| **Spatial** | Origin field | Classic 3D effect from an **origin** (waves, wheels, plasma from a point). Reference can move the pattern. | No |
| **Spatial · Mapped** | Room mapped pattern | Pattern is **painted on the room box** (walls/floor bounds), not tied to effect origin. | No — color only |
| **Spatial · Lighting** | Spatial lighting | A **light** in the room; other LEDs see falloff, **occlusion**, ambient occlusion, room fill. | **Yes** |
| **Spatial · Relay** | Emissive relay | Chosen devices run layers **below** this one; all other LEDs are **shaded** from those LED colors. | **Yes** |
| **Spatial · Volume** | Hologram volume | Shapes / particles in space (future family). | TBD |
| **Spatial · Audio** | Audio reactive | Sound drives **where** in the room (planned dedicated path). | No |
| **Media** | Surface media | **Screen mirror**, textures on **display planes**. | No (plane projection) |
| **Game · Room** | Room map | Minecraft / telemetry **compass & voxels** in room space. | No |

### What “room mapped” is *not*

**Color Wheel** with **Room coordinates → Room mapped** is still a **rainbow pattern** on the room box. Occlusion sliders appear when **Room output → Emitter relay** (or relay shade) is selected.

To get **keyboard rainbow + shaded desk strips**, use **one layer** with **Emitter relay**, or two layers (emitter source + relay shade). See the recipe below.

---

## 2. Room sampler (optional add-on)

On many **Spatial** effects you may see **Room mapping** (Compass / Voxel / Subtle tint). That is **not** a room mode change.

| Setting | Meaning |
|---------|---------|
| **Off** | Effect color only. |
| **Subtle tint** | Soft palette crawl from position. |
| **Compass** | Sectors + height bands use **game-style** direction hubs. |
| **Voxel volume** | Sample Minecraft-style voxels in the room grid. |

Samplers **tint or remap** the effect; they do not replace spatial lighting.

---

## 3. Shader field (GPU presets)

**Shader Field** runs a small **GLSL** animation (`resources/spatial_shaders/*.fs`), renders to a texture, then **samples** that field at each LED’s 3D position.

| vs room mode | Difference |
|--------------|------------|
| Origin / mapped | Math in C++ per LED |
| Shader field | Math on GPU; LEDs read the image |
| Spatial lighting | Physical-style falloff + blockers |

Shader Field is its own effect; it does not add occlusion unless you stack a lighting layer.

---

## 4. Working together (examples)

| Goal | Stack idea |
|------|------------|
| Keyboard runs rainbow; desk strips “see” the glow | **Color Wheel** + **Room output → Emitter relay** (keyboard emitters) |
| Rainbow on full room with relay shading | **Color Wheel** + **Room mapped** + **Emitter relay** with occlusion on |
| Game biome colors on a wave effect | **Wave** + Room mapping **Compass** or **Voxel** |
| Monitor colors on LEDs | **Screen mirror** (Media), not spatial lighting |

---

## Product direction (effects in the library)

1. **Freeze** what each room mode means (this doc + [SPATIAL_ROOM.md](SPATIAL_ROOM.md)).
2. **Configure** classics via **Room output** (coordinates, emitter, relay) instead of duplicating library entries per mode.
3. **Hide** irrelevant panels per mode (no occlusion on mapped; no origin offset on room-fixed mapped; full occlusion panel on lighting).
4. **New modes** (e.g. emissive relay: effect on keyboard → shade everywhere else) as **new library entries**, not extra dropdowns on Color Wheel.

---

## Quick FAQ

**When do occlusion sliders appear?**  
When **Room output** is **Emitter relay** or **Relay shade** — not for **Direct** output with origin coordinates only.

**Room mapped vs origin.**  
Set **Room coordinates → Room mapped** on **Color Wheel** to paint the rainbow on room bounds instead of effect origin.

### Emitter + relay (one Color Wheel, stack recipe)

1. **Layer 1** — **Color Wheel** — **Room output → Emitter source** — **Room coordinates → Room mapped** — stack zone = keyboard.
2. **Layer 2** — **Color Wheel** (or any layer) — **Room output → Relay shade** — zone = All — blend **Add**. Occlusion / fill sliders appear under Relay.

Legacy **Room emissive relay** library entry still works; prefer **Room output** on classic effects.

**What are we building next?**  
Polish relay defaults, preset migration, then more room-mode-specific controls on the shared **Room output** panel.
