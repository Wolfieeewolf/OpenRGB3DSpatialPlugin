# OpenRGB 3D Spatial LED Control

A plugin for [OpenRGB](https://openrgb.org/) that lights your setup **as a 3D room** instead of one strip at a time.

You build a model of your real room — desk, PC case, monitor, light strips — and place each RGB device where it really sits. Effects then play across the **whole room**: a wave rolls from left to right through every device in its path, the ceiling glows when you walk under a pink tree in Minecraft, your screen colours spill onto the walls around it.

The idea started as a modern take on the old **Philips amBX** ambient lighting system.

> **Honest status: personal-project alpha.** It works well for daily use on **Windows**. **Linux has known problems** and **macOS is untested**. Some effects and UI bits are rough or broken. Expect to tinker.

---

## Install

You need **OpenRGB 1.0** (Plugin API 5, Qt 6). Older or Qt5 builds will not load the plugin. A GPU with **OpenGL 4.1** is needed for the 3D view.

Download from the **[latest release](https://github.com/Wolfieeewolf/OpenRGB3DSpatialPlugin/releases/latest)**:

| OS | Files |
| --- | --- |
| **Windows** | `OpenRGB3DSpatialPlugin.dll` + `Qt6Network.dll` + `Qt6OpenGLWidgets.dll` |
| **Linux** | `libOpenRGB3DSpatialPlugin.so` |
| **macOS** | `libOpenRGB3DSpatialPlugin.dylib` |

1. **Windows only:** copy `Qt6Network.dll` and `Qt6OpenGLWidgets.dll` into the folder that contains `OpenRGB.exe`. Stock OpenRGB doesn't include them, and **without them the plugin silently won't load**.
2. In OpenRGB: **Settings → Plugins → Install Plugin**, pick the plugin file, then **restart OpenRGB**.
3. **Get the effects.** The plugin ships with *no* looks of its own. Download [OpenRGB3DSpatialPresets](https://github.com/Wolfieeewolf/OpenRGB3DSpatialPresets) (green **Code → Download ZIP**) and copy its folders into:

   | OS | Folder |
   | --- | --- |
   | Windows | `%AppData%\OpenRGB\plugins\settings\OpenRGB3DSpatialPlugin\` |
   | Linux / macOS | `~/.config/OpenRGB/plugins/settings/OpenRGB3DSpatialPlugin/` |

   Restart OpenRGB again. If the **Effect Library** is empty, this step is what's missing.

**Linux notes:** the release `.so` targets the OpenRGB **AppImage**. If you use your distro's OpenRGB package (Arch, CachyOS, Fedora…), build against your system Qt with `scripts/install-linux-user.sh`. Flatpak OpenRGB can't load outside plugins.

---

## Your first 10 minutes

**Think of the 3D view as your actual room.** If you've used Blender or 3ds Max it will feel familiar. The more closely the layout matches reality, the better effects look. You only set this up once.

1. **Size the room** — *Grid Settings* tab: enter your room (or desk area) in millimetres.
2. **Add your devices** — *Setup* tab: pick from **Available Controllers** → **Add to 3D scene**.
3. **Put each device where it really is** — click it in the 3D view and drag the gizmo arrows, or type positions in the side panel.
4. **Play an effect** — *Run* tab: choose a look in the **Effect Library** → **Add to stack** → **Start all**.

### Moving around the 3D view

| Do this | To |
| --- | --- |
| Right-drag | Orbit the camera |
| Middle-drag | Pan |
| Mouse wheel | Zoom |
| Left-click | Select a device |
| **W / E / R** | Gizmo: move / rotate / scale |
| Click the **centre sphere** of the gizmo | Cycle gizmo mode (move → rotate → scale → …) |
| **Q** | Toggle snap to grid |
| **F** / **Home** | Frame selection / reset camera |
| Delete | Remove selected device |

### Tip: group a PC case with a custom controller

In **Object Creator** you can build a **custom controller** — e.g. your PC case with the fans, RAM, and GPU inside it in the right places (great for layered GPU lighting too). Move the case and everything inside moves with it, instead of dragging each part on its own.

---

## How effects work

Every LED gets a real position in the room, based on real-world sizes (if you don't set LED spacing, controllers default to **10 mm** between LEDs).

An effect is just a rule that answers: **"what colour should the light be at this spot, right now?"** The plugin asks that question for every LED, every frame. So effects are mapped onto the **room**, not onto individual devices — that's why a wave moves smoothly across your keyboard, case, and wall strip as one.

The approach is adapted from [Pixelblaze LED mapping](https://electromage.com/docs/intro-to-mapping), scaled up to a whole room.

Want to write your own effects? Start with **[How It Works](https://github.com/Wolfieeewolf/OpenRGB3DSpatialPlugin/wiki/How-It-Works)** and **[Writing Effects](https://github.com/Wolfieeewolf/OpenRGB3DSpatialPlugin/wiki/Writing-Effects)** on the wiki.

---

## What's in it

| Feature | What it does | Where |
| --- | --- | --- |
| **Spatial effects** | Waves, fields, noise, audio-reactive looks played across the room | Run → Effect Library |
| **Screen Mirror / Ambilight** | Screen colours projected outward from your monitor onto the room (add a **Display Plane** in Object Creator first) | Run → Effect Library |
| **Actions** | Timeline editor for your own light shows | Object Creator → object type **Actions** |
| **Triggers** | Play an action when something happens (OS events, games) | Object Creator → object type **Triggers** |
| **Movie Sync** | Lighting cues synced to video playback (VLC) | Effect Library + presets `movie-sync/` |
| **Reactive** | Keyboard / mouse / gamepad pulses at the device's position (no key logging) | Effect Library |
| **Minecraft mod** | The room reflects the game in 3D: under a pink tree → ceiling glows pink; in water → floor goes blue | [integrations/minecraft/](integrations/minecraft/) |

Layouts and effect stacks are saved in your normal OpenRGB profiles.

---

## Known rough edges

- **Linux:** works less well than Windows — known issues are being worked on.
- **macOS:** builds, but nobody has tested it on real hardware yet.
- Some effects don't show on the grid overlay, misbehave, or are just broken.
- The UI is dense, and some options are only discoverable by poking around.

Bug reports are welcome — please include your OS, OpenRGB version, plugin version, and steps to reproduce.

---

## More help

- **[Wiki](https://github.com/Wolfieeewolf/OpenRGB3DSpatialPlugin/wiki)** — step-by-step [Getting Started](https://github.com/Wolfieeewolf/OpenRGB3DSpatialPlugin/wiki/Getting-Started), [FAQ](https://github.com/Wolfieeewolf/OpenRGB3DSpatialPlugin/wiki/Frequently-Asked-Questions), Screen Mirror, Minecraft.
- **[OpenRGB3DSpatialPresets](https://github.com/Wolfieeewolf/OpenRGB3DSpatialPresets)** — the effects, tools, and controller layouts.

## For developers

Everything technical — how a frame is rendered, the coordinate spaces and grid maths, what each engine does, how to write effects, building from source — lives in the wiki's **developer section**, starting at **[How It Works](https://github.com/Wolfieeewolf/OpenRGB3DSpatialPlugin/wiki/How-It-Works)**. Contracts that ship with the code are in [Documentation/](Documentation/); contribution rules in [CONTRIBUTING.md](CONTRIBUTING.md). GitHub is the source of truth; GitLab is a mirror.

## License

GPL-2.0-only — see [LICENSE](LICENSE).

## Support

This is a for-me project shared because it's fun. If you want to throw something in the tip jar anyway, I like **pizza** more than coffee:

<a href="https://buymeacoffee.com/wolfieee"><img src="https://img.buymeacoffee.com/button-api/?text=Buy%20me%20a%20pizza&emoji=%F0%9F%8D%95&slug=wolfieee&button_colour=FFDD00&font_colour=000000&font_family=Cookie&outline_colour=000000&coffee_colour=ffffff" alt="Buy me a pizza" /></a>
