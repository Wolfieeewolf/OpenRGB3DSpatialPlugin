# DualSense controller torch — parking notes

Parked feature branch: `feat/dualsense-controller-torch` (GitLab). Not on `main`.  
Goal when revived: DualSense as a **room torch / aim** (IMU → RoomGrid beam), not a full gamepad SDK.

## Status

| Item | State |
|------|--------|
| Branch | `feat/dualsense-controller-torch` (rebased onto current `main`) |
| Main | Not merged yet |
| Fusion | **Madgwick IMU** in `Input/MadgwickImu.h` via `ControllerTiltManager` (β≈0.05, rad/s, sensor `dt`) |
| Recenter | Resets Madgwick quat to identity + restarts residual gyro bias |
| Pitch / roll | Gravity-stabilized — better than pre-Madgwick |
| Yaw / heading | Still drifts a lot on DualSense (no mag) — **playtest: better, not good enough for torch aim** |
| Hardware today | 6-axis (gyro + accel); **no magnetometer** |
| Hardware later | **Need 9-axis + mag** for usable absolute heading; DualSense alone is parked for that reason |
| Product call | Don’t keep polishing 6-axis yaw — wait for a mag-capable controller / backend |

## What our branch already does

| Piece | Notes |
|-------|--------|
| HID report parse | Gyro + accel from DualSense input report |
| Factory calib flash | Reads pad calibration block; scales toward °/s and g |
| Pitch / roll | Gravity / accel tilt |
| Yaw | Gyro integrated from **Recenter** (no mag → will wander) |
| Layout | DualSense controller transform in the scene as torch apex |

## Primary fusion references (prefer these over Unreal)

| Source | Why it matters |
|--------|----------------|
| [kdienes/madgwick-ahrs — MadgwickAHRS.c](https://github.com/kdienes/madgwick-ahrs/blob/master/MadgwickAHRS/MadgwickAHRS.c) | Canonical small C port (x-io / Sebastian Madgwick). Prefer as the **implementation** source. |
| Same repo `.h` | `MadgwickAHRSupdate` (MARG) + `MadgwickAHRSupdateIMU` (6-axis) |
| [ahrs.readthedocs — Madgwick](https://ahrs.readthedocs.io/en/latest/filters/madgwick.html) | Clear **theory + API notes**: IMU vs MARG, units, `β` / `Dt`, why yaw needs mag |
| [Teddy Lai — IMU Madgwick filter explanation](https://medium.com/@k66115704/imu-madgwick-filter-explanation-556fbe7f02e3) | Readable walkthrough: quat from gyro, gradient-descent correction from accel (± mag), **ENU vs NED** body/nav frames, init roll/pitch from accel + yaw from mag, normalize before Jacobian; mag = magnetic north (declination ≠ true north) |
| [Reefwing-AHRS](https://github.com/Reefwing-Software/Reefwing-AHRS) | MIT Arduino **C++** AHRS: fusion separated from IMU drivers (good model for DualSense HID → filter). Madgwick / Mahony / complementary / EKF; practical **β vs lag** guidance; fixed known open-source Madgwick bug ([Uckermann analysis](https://github.com/RideBeeline/madgwick-investigation)) |

### DualSense wiring notes

| Detail | What to do |
|--------|------------|
| API for DualSense today | **`MadgwickAHRSupdateIMU`** / `updateIMU` — gyro + accel only |
| Mag path later | MARG update (`MadgwickAHRSupdate` / `updateMARG`); docs: IMU alone cannot hold heading |
| Gyro units | **rad/s** (docs + C) — convert from factory °/s (`* π/180`) |
| Accel | Docs use **m/s²**; C normalizes inside — be consistent (don’t mix g and m/s² across samples) |
| Mag (later) | Docs use **mT**; normalize before / inside filter |
| `sampleFreq` / `Dt` | C sample hardcodes `512` Hz — use measured DualSense `Dt` (or `frequency = 1/Dt`) |
| `beta` / gain | Gyro-error magnitude: `β ≈ √(3/4)·ω̄_β`. kdienes default `0.1`; ahrs Python defaults **`0.033` IMU / `0.041` MARG**. Reefwing: Madgwick study `β≈0.041` is accurate but **~10 s** to settle; ~15× larger β → ~2 s (drone-like) — tune lag vs stickiness for torch |
| Units check | Classic Madgwick C / ahrs docs: gyro **rad/s**. Reefwing API takes gyro **DPS** + accel **g** and converts internally — don’t mix conventions |
| Mahony alt | Reefwing: Mahony often **faster settle** than Madgwick (Kp/Ki); worth A/B if Madgwick feels sluggish |
| Recenter | Reset quat to identity (or stored reference versor); `q0` must stay **unit length** |
| Quaternion → torch | Convert `q` to look / up basis for the RoomGrid beam apex |
| Axis frame | Align DualSense HID gyro/accel axes to one room convention before fusion (Reefwing / Lai: ENU vs NED and body≠nav transforms matter) |
| Init pose | Prefer accel-based roll/pitch at rest (Lai); yaw only if mag present — else identity + Recenter |

Docs + Reefwing: gravity fixes roll/pitch; **heading (yaw) needs magnetometer** (or accept relative aim + recenter). Madgwick IMU slows but does not kill yaw drift.

### Tilt vs horizontal (no mag) — [Robotics.SE #22642](https://robotics.stackexchange.com/questions/22642/can-a-madgwick-imu-filter-be-used-to-determinate-the-tilt-angle-with-respect-to)

| Claim | Takeaway for torch |
|-------|--------------------|
| Goal: tilt of sensor X vs **horizontal plane** with 6-axis only | That’s **roll/pitch vs gravity** — Madgwick **IMU** (no mag) is the right class of filter |
| Accel alone OK at rest; fails once moving | Fuse gyro + accel (Madgwick/Mahony/complementary); don’t re-derive tilt from raw accel while waving the pad |
| Sustained linear accel ~1g | Contaminates the gravity reference → brief tilt bias while accelerating; DualSense swings will see some of this |
| Free fall / zero net accel | Accel gives no tilt cue; gyro-only until gravity returns |
| Full free-space rotation | Pitch/roll stay observable from gravity; **yaw around vertical still drifts** without mag |

So: Madgwick IMU is fine for “aim up/down / bank” relative to the room floor. Absolute compass heading is a separate problem (9-axis or recenter). Matches `dualsense-ts` `tiltRoll` (accel tilt, no yaw) vs fused `orientation`.

## Other references (ideas only — do not vendor their code)

| Source | Why it matters |
|--------|----------------|
| [dualsense-ts](https://classic.yarnpkg.com/en/package/dualsense-ts) ([npm](https://www.npmjs.com/package/dualsense-ts), [repo](https://github.com/nsfm/dualsense-ts)) | **TS / WebHID / node-hid** DualSense stack — not drop-in for our Qt plugin, but the UX we want: factory IMU calib → Madgwick → `orientation.{pitch,yaw,roll,quaternion}`, `tiltRoll` (accel-only, no yaw drift), `orientation.beta`, `orientation.reset()`, plus `lightbar.set({r,g,b})` |
| [IglooVision/ofxDualshock4](https://github.com/IglooVision/ofxDualshock4) | openFrameworks addon: **PS4 DualShock HID** gyro/accel → **Madgwick IMU** orientation (same torch shape as us). HIDAPI, BT-focused, Windows; VR “point to explore” use case. Ops note: Windows **balanced power** can starve HID update rate — use high performance when debugging drift/lag |
| [rafaelvaloto/Unreal-Dualsense](https://github.com/rafaelvaloto/Unreal-Dualsense) | Game/C++ stack: factory IMU calib, °/s, Madgwick, `ResetGyroOrientation` |
| [szyrain/Unreal-Dualsense](https://github.com/szyrain/Unreal-Dualsense) | Related fork of the same family |
| [LeoCodx62/WindowsDualsenseUnreal5](https://github.com/LeoCodx62/WindowsDualsenseUnreal5) | UE5 fork; same stack |
| Arena Shooter sample (same plugins) | **LED on hit** / rumble = lightbar **output** feedback — not IMU aim |

### dualsense-ts patterns worth mirroring in C++

| Pattern | Torch takeaway |
|---------|----------------|
| Factory calib on gyro/accel | Same path our branch already starts; keep it |
| Madgwick built-in | Aligns with kdienes C + ahrs docs |
| `tiltRoll` | Optional “steering only” mode — gravity tilt, **no yaw**, never drifts heading |
| `orientation.reset()` | Map to our Recenter button |
| `beta` exposed | Let users / presets tune smooth vs sticky |
| Lightbar RGB | Status / hit-flash later; not aiming |

## Lessons for a revisit

| Problem | Direction |
|---------|-----------|
| Drift / lost pose | Drop naive yaw integrate; run Madgwick IMU on HID + factory calib; recenter resets quat |
| Absolute heading | Still need **9-axis + mag**, or accept relative aim + frequent recenter |
| Games “don’t lose tracking” | Relative aim + soft recenter / filter — not free absolute orientation |
| Lightbar | Optional later: status / hit-flash; separate from torch aiming |
| Adaptive triggers / haptics | Out of scope for torch unless we expand into a full DualSense output device |

## Suggested next steps (after Madgwick land)

1. Playtest: Recenter flat → tip / roll should stay honest; yaw wander is expected — Recenter clears it.
2. Optional: expose `beta` in Controller Torch UI; `tiltRoll`-only mode (no yaw).
3. Only then: lightbar polish or 9-axis mag hardware path.
