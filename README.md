# OpenRGB 3D Spatial LED Control

A plugin for [OpenRGB](https://openrgb.org/) that treats your RGB devices as one 3D room rather than separate strips.

Each device is placed in a 3D model of your room at its real position and size. Effects are then calculated for the room as a whole, so a wave or colour field travels across every device in its path: keyboard, PC case, monitor, light strips and walls. The project was inspired by the Philips amBX ambient lighting system.

## Status

The plugin is in alpha.

* **Windows:** tested and in daily use.
* **Linux:** supported, with less testing than Windows. See [Linux notes](#linux-notes).
* **macOS:** builds, but is experimental and untested. Audio effects are not available.

Some effects and parts of the interface are still being worked on. Bug reports are welcome.

## Features

* Place devices in a 3D room model using real-world sizes in millimetres
* Effects that play across the whole room, layered in a stack with blend modes
* Grid overlay in the 3D view that shows the effect exactly as the LEDs do
* Audio-reactive effects
* Screen Mirror (Ambilight): screen colours carried outward from the monitor into the room
* Reactive effects: waves that start at the key, mouse button or gamepad button pressed
* Actions: a timeline editor for your own light shows
* Triggers: play an action on a system or game event
* Movie Sync: lighting cues synchronised with video playback in VLC
* Minecraft integration: the room reflects the game world around the player
* Custom controllers: group devices, for example a PC case with its fans and RAM inside
* Layouts and effect stacks are saved in OpenRGB profiles

## Requirements

* OpenRGB 1.0 with Qt 6 (Plugin API 5). Earlier versions and Qt 5 builds are not supported.
* A GPU with OpenGL 4.1 or newer.

## Installation

Download the files for your system from the [latest release](https://github.com/Wolfieeewolf/OpenRGB3DSpatialPlugin/releases/latest):

| OS | Files |
| --- | --- |
| Windows | `OpenRGB3DSpatialPlugin.dll`, `Qt6Network.dll`, `Qt6OpenGLWidgets.dll` |
| Linux (OpenRGB AppImage) | `libOpenRGB3DSpatialPlugin.so`, `libQt6Network.so.6`, `libQt6OpenGLWidgets.so.6` |
| macOS | `libOpenRGB3DSpatialPlugin.dylib`, `macOS-Qt-frameworks.zip` |

1. Add the two Qt modules OpenRGB does not include. The plugin will not load without them.
   * **Windows:** copy both `.dll` files into the folder that contains `OpenRGB.exe`.
   * **Linux:** copy both `libQt6*.so.6` files into `~/.config/OpenRGB/plugins/`.
   * **macOS:** unzip `macOS-Qt-frameworks.zip` into `~/.config/OpenRGB/plugins/`.
2. In OpenRGB, open **Settings → Plugins → Install Plugin**, select the plugin file and restart OpenRGB.
3. Install the effects. The plugin itself contains no effects; they are in the [OpenRGB3DSpatialPresets](https://github.com/Wolfieeewolf/OpenRGB3DSpatialPresets) repository. Download it (**Code → Download ZIP**) and copy its folders into:

   | OS | Folder |
   | --- | --- |
   | Windows | `%AppData%\OpenRGB\plugins\settings\OpenRGB3DSpatialPlugin\` |
   | Linux / macOS | `~/.config/OpenRGB/plugins/settings/OpenRGB3DSpatialPlugin/` |

   Restart OpenRGB. An empty Effect Library means this step is missing.

### Linux notes

* The release `.so` is built for the OpenRGB AppImage. With a distribution package of OpenRGB (Arch, CachyOS, Fedora and others), build the plugin against your system Qt with `scripts/install-linux-user.sh` and do not add the `libQt6*.so.6` files.
* The Flatpak version of OpenRGB cannot load external plugins.
* Audio effects need `pactl` and `parec` (the `pulseaudio-utils` package, or `libpulse` on Arch). They work with PulseAudio and PipeWire.
* Reactive effects read input devices directly, which requires your user to be in the `input` group.
* Screen Mirror currently requires an X11 session.

## Getting started

1. **Grid Settings:** enter the size of your room or desk area in millimetres.
2. **Setup:** select devices under **Available Controllers** and choose **Add to 3D scene**.
3. **3D view:** select each device and move it to its real position with the gizmo, or enter exact values in the side panel.
4. **Run:** choose an effect in the **Effect Library**, select **Add to stack**, then **Start all**.

The closer the layout matches your room, the better the effects line up. Unless set otherwise, LEDs are spaced 10 mm apart.

### 3D view controls

| Input | Action |
| --- | --- |
| Right-drag | Orbit |
| Middle-drag, or Shift + right-drag | Pan |
| Mouse wheel or trackpad pinch | Zoom |
| Left-click | Select |
| W / E / R | Move / rotate / scale gizmo |
| Q | Snap to grid |
| F / Home | Frame selection / reset camera |
| Delete | Remove the selected device from the scene |

## Documentation

* [Wiki](https://github.com/Wolfieeewolf/OpenRGB3DSpatialPlugin/wiki): [Getting Started](https://github.com/Wolfieeewolf/OpenRGB3DSpatialPlugin/wiki/Getting-Started), [FAQ](https://github.com/Wolfieeewolf/OpenRGB3DSpatialPlugin/wiki/Frequently-Asked-Questions), Screen Mirror and Minecraft guides.
* Developer documentation, including how effects are rendered and how to write your own, starts at [How It Works](https://github.com/Wolfieeewolf/OpenRGB3DSpatialPlugin/wiki/How-It-Works).
* File formats and contracts that ship with the code are in [Documentation/](Documentation/).

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) and [Building from Source](https://github.com/Wolfieeewolf/OpenRGB3DSpatialPlugin/wiki/Building-from-Source). When reporting a bug, please include your OS, OpenRGB version, plugin version, GPU and the OpenRGB log.

GitHub is the main repository; GitLab is a mirror.

## License

GPL-2.0-or-later, the same as OpenRGB. See [LICENSE](LICENSE). Third-party components and credits are listed in [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md).

## Support

If you would like to support development:

<a href="https://buymeacoffee.com/wolfieee"><img src="https://img.buymeacoffee.com/button-api/?text=Buy%20me%20a%20pizza&emoji=%F0%9F%8D%95&slug=wolfieee&button_colour=FFDD00&font_colour=000000&font_family=Cookie&outline_colour=000000&coffee_colour=ffffff" alt="Buy me a pizza" /></a>
