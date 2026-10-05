# Extract later (standalone engines)

Engines stay **in the plugin** for now. This is a backlog only — do not build a DLL loader until the content library layout is stable and we actually need smaller installs.

## Suggested order

1. **Movie Cue** — single-purpose; clear `library/movie-cue/` content  
2. **Minecraft / Game** — optional for most users  
3. **Screen Mirror / Ambilight** — heavy native deps  
4. **Audio / Shader Field / Media / Volume** — only after a stable host API exists  

## When revisiting

- Prefer normal `or3d-engine-*.dll` / `.so` in a dedicated folder (not OpenRGB’s `plugins/`)  
- Tiny C entry + register API — no custom archive format  
- Content stays in `library/<category>/` either way  

## Not candidates

Room layout, grid math, controllers, stack UI — core brains; stay in the plugin.
