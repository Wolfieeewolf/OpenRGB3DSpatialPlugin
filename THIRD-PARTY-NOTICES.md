# Third-party notices

OpenRGB 3D Spatial LED Control is licensed under the GNU General Public License, version 2 or (at your option) any later version (GPL-2.0-or-later); see [LICENSE](LICENSE). It includes, links to or adapts the work listed below. Each item keeps its own licence.

## Included in the plugin binary

| Component | Licence | Notes |
| --- | --- | --- |
| [OpenRGB](https://gitlab.com/CalcProgrammer1/OpenRGB) | GPL-2.0-or-later | Plugin API and shared source files (`OpenRGB/` submodule) |
| [Qt-Plus ColorWheel](https://github.com/liuyanghejerry/Qt-Plus) (via OpenRGB) | As distributed with OpenRGB (GPL-2.0-or-later tree) | Colour wheel widget compiled from `OpenRGB/dependencies/ColorWheel` for effect stop picking |
| [nlohmann/json](https://github.com/nlohmann/json) | MIT | JSON library, via OpenRGB's `dependencies/json` |
| [Open Asset Import Library (Assimp)](https://github.com/assimp/assimp) | BSD-3-Clause | Mesh import; linked statically (`dependencies/assimp` submodule) |
| Libraries built into Assimp | Various permissive | pugixml (MIT), RapidJSON (MIT), Clipper (Boost Software License 1.0), poly2tri (BSD-3-Clause), Open3DGC (MIT), OpenDDL Parser (MIT), utf8cpp (BSL-1.0), stb (MIT or public domain), minizip (zlib), kuba--/zip (Unlicense) and miniz (MIT). Licence texts are in `dependencies/assimp/contrib/`. |
| [zlib](https://zlib.net/) | zlib | Linked statically on Windows; the system zlib is used on Linux and macOS |
| [PipeWire](https://pipewire.org/) and SPA headers | MIT | Linux only: inline helpers from the headers, for Screen Mirror on Wayland. The system's libpipewire is loaded at run time and not shipped. |

The Assimp licence requires this notice: *Copyright (c) 2006-2026, assimp team. All rights reserved.* The full text is in [`dependencies/assimp/LICENSE`](dependencies/assimp/LICENSE).

## Distributed with releases

| Component | Licence | Notes |
| --- | --- | --- |
| [Qt 6](https://www.qt.io/) Network and OpenGLWidgets modules | LGPL-3.0-only | Unmodified Qt 6.8.3 libraries, shipped because OpenRGB does not include them. The licence text is at [gnu.org/licenses/lgpl-3.0](https://www.gnu.org/licenses/lgpl-3.0.html); the matching source code is available from [download.qt.io](https://download.qt.io/archive/qt/6.8/6.8.3/single/) and [code.qt.io](https://code.qt.io/). You may replace these libraries with your own build of the same Qt version. |

The plugin also uses Qt Core, Gui, Widgets, OpenGL and (on Linux) DBus from the OpenRGB installation; those are provided by OpenRGB under the same Qt licence.

## Minecraft integration

The Fabric mod in [`integrations/minecraft/`](integrations/minecraft/) is part of this project and licensed GPL-2.0-or-later. It runs on Minecraft and the Fabric toolchain, which are not distributed with it.

## Ideas and adapted work

Many effects, patterns and techniques here are based on ideas and code from other projects, rewritten to work in 3D space with this plugin. Credit goes to:

Some of these provided ideas and techniques only; from others, code was adapted.

| Project | Licence |
| --- | --- |
| [OpenRGB](https://gitlab.com/CalcProgrammer1/OpenRGB) ([openrgb.org](https://openrgb.org/)) | GPL-2.0-or-later |
| [OpenRGB Effects Plugin](https://gitlab.com/OpenRGBDevelopers/OpenRGBEffectsPlugin) | GPL-2.0-or-later |
| [WLED](https://github.com/wled/WLED) | EUPL-1.2 |
| [FastLED](https://github.com/FastLED/FastLED) | MIT |
| [LedFx](https://github.com/LedFx/LedFx) | GPL-3.0 |
| [ColorChord](https://www.colorchord.net/) ([cnlohr/colorchord](https://github.com/cnlohr/colorchord)) | BSD-3-Clause, MIT or CC0 |
| [ColorChord.NET](https://github.com/CaiB/ColorChord.NET) | Apache-2.0 |
| [Effekseer](https://effekseer.github.io/en/index.html) | MIT |
| [Godot VFX Library](https://github.com/haowg/GODOT-VFX-LIBRARY) | MIT |
| [ShaderParticleEngine](https://github.com/squarefeet/ShaderParticleEngine) | MIT |
| [tsParticles](https://github.com/tsparticles/tsparticles) | MIT |
| [Pixelblaze](https://github.com/simap/pixelblaze) patterns, pattern library and LED mapping | No licence stated (ideas only) |
| Arduino 8×8×8 LED cube sketch by Chris | No licence stated (ideas only) |
| 16×16×16 (4096 LED) cube project | No licence stated (ideas only) |
| [Effekseer](https://effekseer.github.io/en/index.html) sample effects and community effect packs | Per pack (ideas only) |
| Blood: Coagulated voxel mod by Cheello and Adam Matthews | Ideas only |
| [Vixen](https://github.com/VixenLights/Vixen) sequencer and editor layout | Ideas only |
| [xLights](https://github.com/xLightsSequencer/xLights) sequencer timeline and 3D layout | Ideas only |
| [LightTrackPlugin](https://github.com/qiangqiang101/LightTrackPlugin/tree/devdev) timeline | Ideas only |
| [Particleify](https://particleify.talizen.com/generator) | Website (ideas only) |
| [Shadertoy](https://www.shadertoy.com/) and its shader authors | Per shader; CC BY-NC-SA 3.0 by default |
| [Sketchfab](https://sketchfab.com/feed) | Per model (ideas only) |
| [ILDA file format](https://laserboy.org/ilda_file_format.html) reference at LaserBoy | Specification |

Code from the GPL-3.0 and Apache-2.0 projects means a release that contains it is distributed under GPL-3.0, which the "or later" licence allows.

Ideas and techniques are free to reuse; code copied from another project keeps that project's licence and must credit its author. If you recognise your own work here and it is not credited, or not used as your licence allows, please [open an issue](https://github.com/Wolfieeewolf/OpenRGB3DSpatialPlugin/issues) and it will be corrected or removed.
