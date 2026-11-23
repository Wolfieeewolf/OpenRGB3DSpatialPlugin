# Third-party notices

OpenRGB 3D Spatial LED Control is licensed under the GNU General Public License, version 2 only (GPL-2.0-only); see [LICENSE](LICENSE). It includes, links to or adapts the work listed below. Each item keeps its own licence.

## Included in the plugin binary

| Component | Licence | Notes |
| --- | --- | --- |
| [OpenRGB](https://gitlab.com/CalcProgrammer1/OpenRGB) | GPL-2.0-only | Plugin API and shared source files (`OpenRGB/` submodule) |
| [nlohmann/json](https://github.com/nlohmann/json) | MIT | JSON library, via OpenRGB's `dependencies/json` |
| [Open Asset Import Library (Assimp)](https://github.com/assimp/assimp) | BSD-3-Clause | Mesh import; linked statically (`dependencies/assimp` submodule) |
| Libraries built into Assimp | Various permissive | pugixml (MIT), RapidJSON (MIT), Clipper (Boost Software License 1.0), poly2tri (BSD-3-Clause), Open3DGC (MIT), OpenDDL Parser (MIT), utf8cpp (BSL-1.0), stb (MIT or public domain), minizip (zlib), kuba--/zip (Unlicense) and miniz (MIT). Licence texts are in `dependencies/assimp/contrib/`. |
| [zlib](https://zlib.net/) | zlib | Linked statically on Windows; the system zlib is used on Linux and macOS |

The Assimp licence requires this notice: *Copyright (c) 2006-2026, assimp team. All rights reserved.* The full text is in [`dependencies/assimp/LICENSE`](dependencies/assimp/LICENSE).

## Distributed with releases

| Component | Licence | Notes |
| --- | --- | --- |
| [Qt 6](https://www.qt.io/) Network and OpenGLWidgets modules | LGPL-3.0-only | Unmodified Qt 6.8.3 libraries, shipped because OpenRGB does not include them. The licence text is at [gnu.org/licenses/lgpl-3.0](https://www.gnu.org/licenses/lgpl-3.0.html); the matching source code is available from [download.qt.io](https://download.qt.io/archive/qt/6.8/6.8.3/single/) and [code.qt.io](https://code.qt.io/). You may replace these libraries with your own build of the same Qt version. |

The plugin also uses Qt Core, Gui, Widgets, OpenGL and (on Linux) DBus from the OpenRGB installation; those are provided by OpenRGB under the same Qt licence.

## Minecraft integration

The Fabric mod in [`integrations/minecraft/`](integrations/minecraft/) is part of this project and licensed GPL-2.0-only. It runs on Minecraft and the Fabric toolchain, which are not distributed with it.

## Ideas and adapted work

Many effects, patterns and techniques here are based on ideas and code from other projects, rewritten to work in 3D space with this plugin. Credit goes to:

* [OpenRGB](https://gitlab.com/CalcProgrammer1/OpenRGB) and the [OpenRGB Effects Plugin](https://gitlab.com/OpenRGBDevelopers/OpenRGBEffectsPlugin) (GPL)
* [WLED](https://github.com/wled/WLED) (EUPL-1.2, which allows use under GPL-2.0)
* [FastLED](https://github.com/FastLED/FastLED) (MIT)
* [Pixelblaze](https://electromage.com/) patterns and LED mapping
* [Shadertoy](https://www.shadertoy.com/) and other shader authors

Ideas and techniques are free to reuse; code copied from another project keeps that project's licence and must credit its author. If you recognise your own work here and it is not credited, or not used as your licence allows, please [open an issue](https://github.com/Wolfieeewolf/OpenRGB3DSpatialPlugin/issues) and it will be corrected or removed.
