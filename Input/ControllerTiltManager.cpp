// SPDX-License-Identifier: GPL-2.0-only

#include "ControllerTiltManager.h"
#include "PluginLog.h"
#include "VirtualReferencePoint3D.h"

#include <QMutex>
#include <QMutexLocker>
#include <QThread>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

#if !defined(CONTROLLER_TILT_NO_HIDAPI)
#include <hidapi.h>
#define CONTROLLER_TILT_HAS_HIDAPI 1
#endif

namespace
{
constexpr unsigned short kNintendoVid = 0x057Eu;
constexpr unsigned short kWiimotePid = 0x0306u;
constexpr unsigned short kWiimotePlusPid = 0x0330u;

constexpr float kPi = 3.14159265358979323846f;
constexpr float kDegToRad = kPi / 180.0f;
constexpr float kRadToDeg = 180.0f / kPi;

/** Wiimote IR camera FOV (approx) — maps pixel offset → degrees. */
constexpr float kIrHFovDeg = 41.0f;
constexpr float kIrVFovDeg = 31.0f;
constexpr float kIrCenterX = 512.0f;
constexpr float kIrCenterY = 384.0f;
/** Soft pull toward sensor-bar heading when IR is locked (yaw drift kill). */
constexpr float kIrYawCorrect = 0.12f;

/**
 * MotionPlus: Flafla2/Unity-Wiimote calibration.
 * Slow: 0.05 °/s/LSB. Fast: × (2000/440). Rest ~8063 (0x1F7F).
 */
constexpr float kMplusSlowDegPerLsb = 0.05f;
constexpr float kMplusFastMult = 2000.0f / 440.0f;
constexpr float kMplusRest = 8063.0f; // 0x1F7F

/**
 * Soft gravity — only while nearly still. Strong continuous correction snaps the
 * pose back to "buttons up" and kills free 360° aim.
 */
constexpr float kGravityCorrect = 0.03f;
constexpr float kAccelLpf = 0.35f;
/**
 * MotionPlus scale. Keep near 1.0 — responsiveness comes from high FPS / fresh HID,
 * not gain (gain multiplies residual bias and drifts when sitting still).
 */
constexpr float kGyroGain = 1.05f;
/** Below this total |dps| after bias, treat as still — no integrate / allow gravity. */
constexpr float kGyroStillDps = 1.8f;
/** MotionPlus pitch about +X is opposite our RoomGrid tip-up convention. */
constexpr float kPitchSign = -1.0f;

float Clampf(float v, float lo, float hi)
{
    return std::max(lo, std::min(hi, v));
}

float Length3(const Vector3D& v)
{
    return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

Vector3D Normalize3(Vector3D v)
{
    const float len = Length3(v);
    if(len < 1e-5f)
    {
        return {0.0f, 0.0f, -1.0f};
    }
    const float inv = 1.0f / len;
    return {v.x * inv, v.y * inv, v.z * inv};
}

Vector3D Cross3(const Vector3D& a, const Vector3D& b)
{
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

float Dot3(const Vector3D& a, const Vector3D& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

constexpr float kGyroDeadzoneDps = 0.75f;
constexpr float kGyroMaxDps = 1200.0f;

struct Quat
{
    float w = 1.0f;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

Quat QuatNormalize(Quat q)
{
    const float n = std::sqrt(q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z);
    if(n < 1e-8f)
    {
        return {};
    }
    const float inv = 1.0f / n;
    return {q.w * inv, q.x * inv, q.y * inv, q.z * inv};
}

Quat QuatMul(const Quat& a, const Quat& b)
{
    return {
        a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z,
        a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
        a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
        a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w
    };
}

/** Rotate vector by quaternion (device → room). */
Vector3D QuatRotate(const Quat& q, const Vector3D& v)
{
    const Quat p{0.0f, v.x, v.y, v.z};
    const Quat qn = QuatNormalize(q);
    const Quat qi{qn.w, -qn.x, -qn.y, -qn.z};
    const Quat r = QuatMul(QuatMul(qn, p), qi);
    return {r.x, r.y, r.z};
}

/**
 * Integrate body rates (deg/s) in Wiimote frame:
 *   Pitch about +X, Roll about +Y (IR), Yaw about +Z (buttons) — Unity-Wiimote / Wiibrew.
 */
Quat QuatIntegrateGyro(Quat q, float pitch_dps, float roll_dps, float yaw_dps, float dt)
{
    const float wx = pitch_dps * kDegToRad;
    const float wy = roll_dps * kDegToRad;
    const float wz = yaw_dps * kDegToRad;
    const Quat omega{0.0f, wx, wy, wz};
    const Quat qdot = QuatMul(q, omega);
    q.w += 0.5f * qdot.w * dt;
    q.x += 0.5f * qdot.x * dt;
    q.y += 0.5f * qdot.y * dt;
    q.z += 0.5f * qdot.z * dt;
    return QuatNormalize(q);
}

/**
 * Softly align measured accel with home "buttons up" (device +Z in quat space).
 * Reference must be +Z — NOT room +Y. Comparing to room +Y without the home remap
 * leaves a 90° error at identity and spins the pose while sitting still.
 */
Quat QuatCorrectGravity(Quat q, const Vector3D& accel_device)
{
    Vector3D a = Normalize3(accel_device);
    const Vector3D up_est = QuatRotate(q, a);
    const Vector3D up_ref{0.0f, 0.0f, 1.0f}; // buttons-up at Recenter home
    Vector3D err = Cross3(up_est, up_ref);
    const float elen = Length3(err);
    if(elen < 1e-5f)
    {
        return q;
    }
    err = {err.x / elen, err.y / elen, err.z / elen};
    const float angle = std::asin(Clampf(elen, 0.0f, 1.0f)) * kGravityCorrect;
    const float ha = angle * 0.5f;
    const float s = std::sin(ha);
    const Quat corr{std::cos(ha), err.x * s, err.y * s, err.z * s};
    return QuatNormalize(QuatMul(corr, q));
}

float WrapDeg(float deg)
{
    while(deg > 180.0f)
    {
        deg -= 360.0f;
    }
    while(deg < -180.0f)
    {
        deg += 360.0f;
    }
    return deg;
}

/**
 * Soft body-yaw nudge so room heading matches IR (sensor bar) when locked.
 * Positive body +Z aims left in RoomGrid, so correction uses (cur − ir).
 */
Quat QuatCorrectIrYaw(Quat q, float ir_room_yaw_deg, float cur_room_yaw_deg)
{
    const float err = WrapDeg(cur_room_yaw_deg - ir_room_yaw_deg);
    const float angle = err * kIrYawCorrect * kDegToRad;
    if(std::fabs(angle) < 1e-6f)
    {
        return q;
    }
    const float ha = angle * 0.5f;
    const Quat corr{std::cos(ha), 0.0f, 0.0f, std::sin(ha)};
    return QuatNormalize(QuatMul(q, corr));
}

/**
 * Home orientation: IR (−device Y→ wait +Y) toward room −Z, buttons (+Z) toward room +Y.
 * Identity quaternion maps device → room with that basis via fixed remap below.
 */
void BasisFromDeviceQuat(const Quat& q, Vector3D* aim_out, Vector3D* up_out, Rotation3D* euler_out)
{
    // Device axes in room after q (q is incremental from home; home remap applied here).
    // At identity: aim = room −Z, up = room +Y.
    const Vector3D device_forward = QuatRotate(q, {0.0f, 1.0f, 0.0f}); // +Y = IR
    const Vector3D device_buttons = QuatRotate(q, {0.0f, 0.0f, 1.0f}); // +Z = buttons

    // Remap: device forward → room −Z, device buttons → room +Y, device right → room +X
    // R_home columns: right=(1,0,0), up=(0,1,0), back=(0,0,1) with forward=-back.
    // device_forward (Y) → (0,0,-1), device_buttons (Z) → (0,1,0), device_right (X) → (1,0,0)
    // So room_vec = (dx, dz, -dy) when q=I and device vec is (dx,dy,dz)... 
    // At I: forward (0,1,0) → (0, 0, -1) = (dx, dz, -dy) ✓
    // At I: buttons (0,0,1) → (0, 1, 0) = (dx, dz, -dy) ✓
    const Vector3D aim = Normalize3({
        device_forward.x,
        device_forward.z,
        -device_forward.y
    });
    Vector3D up = Normalize3({
        device_buttons.x,
        device_buttons.z,
        -device_buttons.y
    });
    // Re-orthogonalize up against aim.
    const Vector3D right = Normalize3(Cross3(aim, up));
    up = Normalize3(Cross3(right, aim));

    if(aim_out)
    {
        *aim_out = aim;
    }
    if(up_out)
    {
        *up_out = up;
    }
    if(euler_out)
    {
        // Display Euler from aim (same as pad-native: pitch/yaw from aim, roll from up).
        const float pitch = std::asin(Clampf(aim.y, -1.0f, 1.0f)) * kRadToDeg;
        const float yaw = std::atan2(aim.x, -aim.z) * kRadToDeg;
        const Vector3D world_up{0.0f, 1.0f, 0.0f};
        Vector3D flat_right = Cross3(aim, world_up);
        if(Length3(flat_right) < 1e-4f)
        {
            flat_right = {1.0f, 0.0f, 0.0f};
        }
        flat_right = Normalize3(flat_right);
        const Vector3D flat_up = Normalize3(Cross3(flat_right, aim));
        const float roll = std::atan2(Dot3(up, flat_right), Dot3(up, flat_up)) * kRadToDeg;
        *euler_out = {pitch, yaw, roll};
    }
}

std::uint64_t NowMs()
{
    using clock = std::chrono::steady_clock;
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(clock::now().time_since_epoch()).count());
}

#if CONTROLLER_TILT_HAS_HIDAPI

/**
 * DolphinBar / Wiimote output writes (Julian Löhr / Wiimote-HIDAPI):
 *   Native Bluetooth: hid_write first (SetOutputReport can power-off -TR).
 *   DolphinBar USB: try hid_send_output_report first, then hid_write.
 * Do NOT install the HID-Wiimote kernel gamepad driver on top of mode 4 —
 * that replaces the raw Nintendo reports we need.
 */
bool PathLooksLikeBluetoothHid(const std::string& path)
{
    std::string lower = path;
    for(char& ch : lower)
    {
        if(ch >= 'A' && ch <= 'Z')
        {
            ch = static_cast<char>(ch - 'A' + 'a');
        }
    }
    return lower.find("bthenum") != std::string::npos;
}

bool WriteReportEx(hid_device* dev, const unsigned char* data, size_t len, bool dolphinbar_usb)
{
    if(!dev || !data || len == 0)
    {
        return false;
    }

#if defined(_WIN32)
    if(dolphinbar_usb)
    {
        if(hid_send_output_report(dev, data, len) >= 0)
        {
            return true;
        }
        return hid_write(dev, data, len) >= 0;
    }
#endif
    if(hid_write(dev, data, len) >= 0)
    {
        return true;
    }
#if defined(_WIN32)
    return hid_send_output_report(dev, data, len) >= 0;
#else
    return false;
#endif
}

/** Set in TryOpen — DolphinBar USB vs native Bluetooth write preference. */
bool g_wiimote_dolphinbar_usb = false;

bool WriteReport(hid_device* dev, const unsigned char* data, size_t len)
{
    return WriteReportEx(dev, data, len, g_wiimote_dolphinbar_usb);
}

bool WriteRegister(hid_device* dev, unsigned int addr, const unsigned char* payload, unsigned char size)
{
    // Unity-Wiimote / Wiibrew: report 0x16 layout is
    //   [0]=0x16  [1]=RegisterType|rumble (0x04 = CONTROL)  [2..4]=24-bit addr  [5]=size  [6..]=data
    // Writing 0x00 in [1] targets EEPROM — MotionPlus/IR register setup silently does nothing.
    unsigned char buf[22]{};
    buf[0] = 0x16;
    buf[1] = 0x04; // RegisterType::CONTROL
    buf[2] = static_cast<unsigned char>((addr >> 16) & 0xffu);
    buf[3] = static_cast<unsigned char>((addr >> 8) & 0xffu);
    buf[4] = static_cast<unsigned char>(addr & 0xffu);
    buf[5] = size;
    if(payload && size > 0)
    {
        std::memcpy(buf + 6, payload, size);
    }
    return WriteReport(dev, buf, sizeof(buf));
}

bool SetReportMode(hid_device* dev, unsigned char mode)
{
    const unsigned char buf[3] = {0x12, 0x00, mode};
    return WriteReport(dev, buf, sizeof(buf));
}

bool SetPlayerLeds(hid_device* dev, unsigned char led_bits)
{
    const unsigned char buf[2] = {0x11, static_cast<unsigned char>((led_bits << 4) & 0xf0u)};
    return WriteReport(dev, buf, sizeof(buf));
}

bool EnableIrCamera(hid_device* dev, bool on)
{
    const unsigned char v = on ? 0x04u : 0x00u;
    const unsigned char a[2] = {0x13, v};
    const unsigned char b[2] = {0x1a, v};
    return WriteReport(dev, a, sizeof(a)) && WriteReport(dev, b, sizeof(b));
}

bool InitIrCamera(hid_device* dev)
{
    // Sequence from Flafla2/Unity-Wiimote SetupIRCamera (BASIC = 10-byte IR for report 0x37).
    if(!EnableIrCamera(dev, true))
    {
        return false;
    }
    QThread::msleep(25);

    unsigned char eight = 0x08;
    if(!WriteRegister(dev, 0xb00030u, &eight, 1))
    {
        return false;
    }
    QThread::msleep(25);

    // Wii sensitivity level 3 (Unity-Wiimote).
    const unsigned char sens1[9] = {0x02, 0x00, 0x00, 0x71, 0x01, 0x00, 0xaa, 0x00, 0x64};
    if(!WriteRegister(dev, 0xb00000u, sens1, 9))
    {
        return false;
    }
    QThread::msleep(25);

    const unsigned char sens2[2] = {0x63, 0x03};
    if(!WriteRegister(dev, 0xb0001au, sens2, 2))
    {
        return false;
    }
    QThread::msleep(25);

    // Mode 1 = BASIC (10 bytes / 4 objects) — fits report 0x37.
    unsigned char mode = 0x01;
    if(!WriteRegister(dev, 0xb00033u, &mode, 1))
    {
        return false;
    }
    QThread::msleep(25);

    if(!WriteRegister(dev, 0xb00030u, &eight, 1))
    {
        return false;
    }
    QThread::msleep(25);
    return true;
}

bool ActivateMotionPlus(hid_device* dev)
{
    // Flafla2/Unity-Wiimote ActivateWiiMotionPlus:
    //   write 0x55 @ 0xA600F0, then 0x04 @ 0xA600FE (maps M+ into extension port).
    unsigned char v55 = 0x55;
    unsigned char v04 = 0x04;

    if(!WriteRegister(dev, 0xa600f0u, &v55, 1))
    {
        return false;
    }
    QThread::msleep(50);
    if(!WriteRegister(dev, 0xa600feu, &v04, 1))
    {
        return false;
    }
    QThread::msleep(250);
    return true;
}

struct IrDot
{
    bool valid = false;
    float x = 0.0f;
    float y = 0.0f;
};

/** Decode IR basic mode pair packed into 5 bytes → two dots. */
void DecodeIrBasicPair(const unsigned char* p, IrDot* a, IrDot* b)
{
    // Invisible markers report 0xFF in the low X/Y bytes (WiiYourself / wiimote crate).
    a->valid = !(p[0] == 0xffu && p[1] == 0xffu);
    b->valid = !(p[3] == 0xffu && p[4] == 0xffu);
    if(a->valid)
    {
        a->x = static_cast<float>(p[0] | ((p[2] & 0x30u) << 4));
        a->y = static_cast<float>(p[1] | ((p[2] & 0xc0u) << 2));
    }
    if(b->valid)
    {
        b->x = static_cast<float>(p[3] | ((p[2] & 0x03u) << 8));
        b->y = static_cast<float>(p[4] | ((p[2] & 0x0cu) << 6));
    }
}

/** Basic-mode IR (10 bytes) → room yaw/pitch vs camera centre (degrees). */
bool DecodeIrHeading(const unsigned char* ir10, float* yaw_deg_out, float* pitch_deg_out)
{
    IrDot dots[4]{};
    DecodeIrBasicPair(ir10, &dots[0], &dots[1]);
    DecodeIrBasicPair(ir10 + 5, &dots[2], &dots[3]);
    float sx = 0.0f;
    float sy = 0.0f;
    int n = 0;
    for(int i = 0; i < 4; ++i)
    {
        if(!dots[i].valid)
        {
            continue;
        }
        sx += dots[i].x;
        sy += dots[i].y;
        ++n;
    }
    if(n < 1)
    {
        return false;
    }
    const float mx = sx / static_cast<float>(n);
    const float my = sy / static_cast<float>(n);
    // Yaw right → dots slide left on the camera → mid_x drops → positive room yaw.
    *yaw_deg_out = (kIrCenterX - mx) / kIrCenterX * (kIrHFovDeg * 0.5f);
    *pitch_deg_out = (my - kIrCenterY) / kIrCenterY * (kIrVFovDeg * 0.5f);
    return true;
}

void DecodeAccel10(const unsigned char* report, float* ax, float* ay, float* az)
{
    // report[0..] is payload after report ID for 0x31/0x35/0x37.
    const unsigned int x = (static_cast<unsigned int>(report[2]) << 2)
                         | ((report[0] >> 5) & 0x3u);
    const unsigned int y = (static_cast<unsigned int>(report[3]) << 2)
                         | ((report[1] >> 5) & 0x3u);
    const unsigned int z = (static_cast<unsigned int>(report[4]) << 2)
                         | ((report[1] >> 7) & 0x1u)
                         | (((report[0] >> 7) & 0x1u) << 1);
    *ax = (static_cast<float>(x) - 512.0f) / 256.0f;
    *ay = (static_cast<float>(y) - 512.0f) / 256.0f;
    *az = (static_cast<float>(z) - 512.0f) / 256.0f;
}

/**
 * MotionPlus extension packing (Wiibrew / WiiYourself):
 *   b0 yaw_lo, b1 roll_lo, b2 pitch_lo,
 *   b3 yaw_hi[13:8]<<2 | yaw_slow | pitch_slow,
 *   b4 roll_hi[13:8]<<2 | roll_slow | ext_connected,
 *   b5 pitch_hi[13:8]<<2 | 1 | 0
 */
bool DecodeMotionPlus(const unsigned char* ext6,
                      float* yaw_dps, float* roll_dps, float* pitch_dps)
{
    const unsigned int yaw = ext6[0] | (static_cast<unsigned int>(ext6[3] & 0xFCu) << 6);
    const unsigned int roll = ext6[1] | (static_cast<unsigned int>(ext6[4] & 0xFCu) << 6);
    const unsigned int pitch = ext6[2] | (static_cast<unsigned int>(ext6[5] & 0xFCu) << 6);

    // Disconnect / not-ready sentinel (WiiYourself ignores these).
    if(yaw == 0x3fffu && pitch == 0x3fffu && roll == 0x3fffu)
    {
        return false;
    }
    // All-zero / all-FF padding is not a live sample.
    if((ext6[0] == 0 && ext6[1] == 0 && ext6[2] == 0 && ext6[3] == 0 && ext6[4] == 0 && ext6[5] == 0)
       || (ext6[0] == 0xff && ext6[1] == 0xff && ext6[2] == 0xff))
    {
        return false;
    }

    const bool yaw_slow = (ext6[3] & 0x02u) != 0;
    const bool pitch_slow = (ext6[3] & 0x01u) != 0;
    const bool roll_slow = (ext6[4] & 0x02u) != 0;

    // Unity-Wiimote: (raw - zero) * 0.05, then ×(2000/440) when not slow.
    float y = (static_cast<float>(yaw) - kMplusRest) * kMplusSlowDegPerLsb;
    float r = (static_cast<float>(roll) - kMplusRest) * kMplusSlowDegPerLsb;
    float p = (static_cast<float>(pitch) - kMplusRest) * kMplusSlowDegPerLsb;
    if(!yaw_slow)
    {
        y *= kMplusFastMult;
    }
    if(!roll_slow)
    {
        r *= kMplusFastMult;
    }
    if(!pitch_slow)
    {
        p *= kMplusFastMult;
    }
    *yaw_dps = Clampf(y, -kGyroMaxDps, kGyroMaxDps);
    *roll_dps = Clampf(r, -kGyroMaxDps, kGyroMaxDps);
    *pitch_dps = Clampf(p, -kGyroMaxDps, kGyroMaxDps);
    return true;
}

float ApplyDeadzone(float v, float dz)
{
    if(std::fabs(v) < dz)
    {
        return 0.0f;
    }
    return v;
}

#endif // CONTROLLER_TILT_HAS_HIDAPI
} // namespace

class ControllerTiltManager::Impl
{
public:
    mutable QMutex mutex;
    bool running = false;
    ControllerTiltSample sample{};
    ControllerTiltLinkStatus link_status = ControllerTiltLinkStatus::Stopped;
    Vector3D torch_origin{};
    bool have_torch_origin = false;

    float pitch_deg = 0.0f;
    float yaw_deg = 0.0f;
    float roll_deg = 0.0f;
    Quat orient{};
    Vector3D gyro_bias{};
    bool gyro_bias_ready = false;
    int gyro_bias_samples = 0;
    Vector3D gyro_bias_accum{};
    Vector3D accel_filt{0.0f, 0.0f, 1.0f};
    /** IR yaw (deg) captured at Recenter — subtracted so home faces the bar at yaw 0. */
    float ir_yaw_center_deg = 0.0f;
    /** Last decoded IR yaw (before center subtract) for Recenter capture. */
    float last_ir_yaw_deg = 0.0f;
    bool last_ir_ok = false;
    std::uint64_t last_sample_ms = 0;
    bool need_init = true;
    int reconnect_cooldown = 0;
    int frames_without_data = 0;

#if CONTROLLER_TILT_HAS_HIDAPI
    hid_device* dev = nullptr;
    std::string open_path;
#endif

    void SetLink(ControllerTiltLinkStatus s)
    {
        link_status = s;
        sample.link = s;
    }

    void CloseDevice()
    {
#if CONTROLLER_TILT_HAS_HIDAPI
        if(dev)
        {
            hid_close(dev);
            dev = nullptr;
        }
        open_path.clear();
#endif
        need_init = true;
        gyro_bias_ready = false;
        gyro_bias_samples = 0;
        gyro_bias_accum = {};
        ir_yaw_center_deg = 0.0f;
        last_ir_yaw_deg = 0.0f;
        last_ir_ok = false;
        last_sample_ms = 0;
        frames_without_data = 0;
        orient = {};
    }

    void HandleDisconnect(const char* why)
    {
        LOG_WARNING("[3DSpatial] ControllerTilt: Wii Remote HID dropped (%s)", why ? why : "?");
        CloseDevice();
        sample = ControllerTiltSample{};
        sample.kind = ControllerTiltDeviceKind::None;
        SetLink(ControllerTiltLinkStatus::NoDevice);
        reconnect_cooldown = 30;
    }

#if CONTROLLER_TILT_HAS_HIDAPI
    bool TryOpen()
    {
        CloseDevice();
        hid_device_info* list = hid_enumerate(kNintendoVid, 0);
        if(!list)
        {
            SetLink(ControllerTiltLinkStatus::NoDevice);
            return false;
        }

        hid_device_info* pick = nullptr;
        for(hid_device_info* it = list; it; it = it->next)
        {
            if(it->product_id != kWiimotePid && it->product_id != kWiimotePlusPid)
            {
                continue;
            }
            pick = it;
            break;
        }

        if(!pick || !pick->path)
        {
            hid_free_enumeration(list);
            SetLink(ControllerTiltLinkStatus::NoDevice);
            return false;
        }

        open_path = pick->path;
        const unsigned short pid = pick->product_id;
        g_wiimote_dolphinbar_usb = !PathLooksLikeBluetoothHid(open_path);
        hid_free_enumeration(list);

        dev = hid_open_path(open_path.c_str());
        if(!dev)
        {
            SetLink(ControllerTiltLinkStatus::DeviceBusy);
            LOG_WARNING("[3DSpatial] ControllerTilt: Wii Remote HID open failed (busy?) path=%s",
                        open_path.c_str());
            open_path.clear();
            return false;
        }

        hid_set_nonblocking(dev, 1);
        LOG_INFO("[3DSpatial] ControllerTilt: Wii Remote opened (pid=0x%04x, %s writes) for torch",
                 static_cast<unsigned>(pid),
                 g_wiimote_dolphinbar_usb ? "DolphinBar/USB" : "Bluetooth");
        SetLink(ControllerTiltLinkStatus::WaitingInit);
        need_init = true;
        return true;
    }

    bool RunInit()
    {
        if(!dev)
        {
            return false;
        }
        if(!SetPlayerLeds(dev, 0x1))
        {
            LOG_WARNING("[3DSpatial] ControllerTilt: LED write failed — "
                        "DolphinBar/HID writes may be blocked (try SetOutputReport path; "
                        "uninstall HID-Wiimote gamepad driver if installed)");
        }
        QThread::msleep(20);

        if(!ActivateMotionPlus(dev))
        {
            LOG_WARNING("[3DSpatial] ControllerTilt: MotionPlus activate write failed");
        }
        else
        {
            const unsigned char status_req[2] = {0x15, 0x00};
            WriteReport(dev, status_req, sizeof(status_req));
            QThread::msleep(100);
        }

        // IR + MotionPlus on report 0x37: gyro for 360°, IR soft-locks yaw when the
        // sensor bar is in view (cuts drift); outside the bar FOV yaw stays free.
        if(!InitIrCamera(dev))
        {
            LOG_WARNING("[3DSpatial] ControllerTilt: IR camera init failed — "
                        "360° MotionPlus only (no sensor-bar yaw lock)");
        }
        if(!SetReportMode(dev, 0x37))
        {
            LOG_WARNING("[3DSpatial] ControllerTilt: report 0x37 failed — trying 0x35");
            if(!SetReportMode(dev, 0x35))
            {
                HandleDisconnect("set report mode failed");
                return false;
            }
        }

        orient = {};
        need_init = false;
        frames_without_data = 0;
        SetLink(ControllerTiltLinkStatus::Ok);
        LOG_INFO("[3DSpatial] ControllerTilt: Wii Remote init done "
                 "(MotionPlus 360° + IR yaw lock when sensor bar visible)");
        return true;
    }

    void PollWii()
    {
        if(!running)
        {
            SetLink(ControllerTiltLinkStatus::Stopped);
            return;
        }

        if(reconnect_cooldown > 0)
        {
            --reconnect_cooldown;
        }

        if(!dev)
        {
            if(reconnect_cooldown > 0)
            {
                return;
            }
            if(!TryOpen())
            {
                sample.valid = false;
                sample.kind = ControllerTiltDeviceKind::None;
                return;
            }
        }

        if(need_init)
        {
            if(!RunInit())
            {
                return;
            }
        }

        unsigned char buf[64]{};
        int n = 0;
        bool got = false;
        // Drain queued HID reports so we always fuse the newest sample.
        for(int i = 0; i < 24; ++i)
        {
            const int r = hid_read(dev, buf, sizeof(buf));
            if(r == 0)
            {
                break;
            }
            if(r < 0)
            {
                HandleDisconnect("hid_read error");
                return;
            }
            n = r;
            got = true;
        }

        if(!got)
        {
            ++frames_without_data;
            // Re-assert IR+MotionPlus report mode if the remote went quiet after init.
            if(frames_without_data == 90 && dev)
            {
                SetReportMode(dev, 0x37);
            }
            return;
        }
        frames_without_data = 0;

        if(n < 2)
        {
            return;
        }

        // Windows hidapi usually returns report ID as byte 0.
        int off = 0;
        unsigned char rid = buf[0];
        if(rid == 0x37 || rid == 0x35 || rid == 0x31 || rid == 0x33 || rid == 0x34)
        {
            off = 1;
        }
        else if(rid == 0x20 || rid == 0x21 || rid == 0x22)
        {
            // Status / read-ack / write-ack — ignore.
            return;
        }
        else if(n >= 21)
        {
            // Some stacks omit report ID when only one report is active.
            rid = 0x37;
            off = 0;
        }
        else if(n >= 6)
        {
            rid = 0x31;
            off = 0;
        }
        else
        {
            return;
        }

        const unsigned char* payload = buf + off;
        const int plen = n - off;
        if(plen < 5)
        {
            return;
        }

        float ax = 0.0f;
        float ay = 0.0f;
        float az = 0.0f;
        DecodeAccel10(payload, &ax, &ay, &az);
        accel_filt.x += (ax - accel_filt.x) * kAccelLpf;
        accel_filt.y += (ay - accel_filt.y) * kAccelLpf;
        accel_filt.z += (az - accel_filt.z) * kAccelLpf;

        bool ir_ok = false;
        float ir_yaw_deg = 0.0f;
        float ir_pitch_deg = 0.0f;
        float yaw_dps = 0.0f;
        float roll_dps = 0.0f;
        float pitch_dps = 0.0f;
        bool have_mplus = false;
        if(rid == 0x35 && plen >= 11)
        {
            if(DecodeMotionPlus(payload + 5, &yaw_dps, &roll_dps, &pitch_dps))
            {
                have_mplus = true;
            }
        }
        else if(rid == 0x37 && plen >= 21)
        {
            if(DecodeIrHeading(payload + 5, &ir_yaw_deg, &ir_pitch_deg))
            {
                ir_ok = true;
                (void)ir_pitch_deg; // reserved for optional IR pitch assist
            }
            if(DecodeMotionPlus(payload + 15, &yaw_dps, &roll_dps, &pitch_dps))
            {
                have_mplus = true;
            }
        }

        const std::uint64_t now = NowMs();
        float dt = 1.0f / 60.0f;
        if(last_sample_ms != 0 && now > last_sample_ms)
        {
            dt = Clampf(static_cast<float>(now - last_sample_ms) * 0.001f, 0.001f, 0.05f);
        }
        last_sample_ms = now;

        bool nearly_still = true;
        if(have_mplus)
        {
            const float still = std::fabs(pitch_dps) + std::fabs(yaw_dps) + std::fabs(roll_dps);
            if(!gyro_bias_ready && still < 20.0f)
            {
                gyro_bias_accum.x += pitch_dps;
                gyro_bias_accum.y += yaw_dps;
                gyro_bias_accum.z += roll_dps;
                ++gyro_bias_samples;
                if(gyro_bias_samples >= 50)
                {
                    const float inv = 1.0f / static_cast<float>(gyro_bias_samples);
                    gyro_bias = {
                        gyro_bias_accum.x * inv,
                        gyro_bias_accum.y * inv,
                        gyro_bias_accum.z * inv
                    };
                    gyro_bias_ready = true;
                    LOG_INFO("[3DSpatial] ControllerTilt: MotionPlus residual bias ready "
                             "(p=%.2f y=%.2f r=%.2f)",
                             gyro_bias.x, gyro_bias.y, gyro_bias.z);
                }
            }

            // Do not integrate until bias is known — otherwise flat rest drifts hard.
            if(gyro_bias_ready)
            {
                pitch_dps = ApplyDeadzone(pitch_dps - gyro_bias.x, kGyroDeadzoneDps) * kGyroGain;
                yaw_dps = ApplyDeadzone(yaw_dps - gyro_bias.y, kGyroDeadzoneDps) * kGyroGain;
                roll_dps = ApplyDeadzone(roll_dps - gyro_bias.z, kGyroDeadzoneDps) * kGyroGain;

                const float moving = std::fabs(pitch_dps) + std::fabs(yaw_dps) + std::fabs(roll_dps);
                nearly_still = moving < kGyroStillDps;
                if(!nearly_still)
                {
                    orient = QuatIntegrateGyro(
                        orient,
                        pitch_dps * kPitchSign,
                        roll_dps,
                        yaw_dps,
                        dt);
                }
                else
                {
                    pitch_dps = 0.0f;
                    yaw_dps = 0.0f;
                    roll_dps = 0.0f;
                }
            }
            else
            {
                pitch_dps = 0.0f;
                yaw_dps = 0.0f;
                roll_dps = 0.0f;
            }
        }

        // Gravity only while still — otherwise it fights free 360° aim.
        const float accel_mag = Length3(accel_filt);
        if(nearly_still && accel_mag > 0.7f && accel_mag < 1.35f)
        {
            orient = QuatCorrectGravity(orient, accel_filt);
        }

        // Sensor-bar yaw lock when IR dots are visible (optional drift kill).
        last_ir_ok = ir_ok;
        if(ir_ok)
        {
            last_ir_yaw_deg = ir_yaw_deg;
            Vector3D aim_tmp{};
            BasisFromDeviceQuat(orient, &aim_tmp, nullptr, nullptr);
            const float cur_yaw = std::atan2(aim_tmp.x, -aim_tmp.z) * kRadToDeg;
            orient = QuatCorrectIrYaw(orient, ir_yaw_deg - ir_yaw_center_deg, cur_yaw);
        }

        Vector3D aim{};
        Vector3D up{};
        Rotation3D euler{};
        BasisFromDeviceQuat(orient, &aim, &up, &euler);
        pitch_deg = euler.x;
        yaw_deg = euler.y;
        roll_deg = euler.z;

        ControllerTiltSample next{};
        next.kind = ControllerTiltDeviceKind::WiiRemote;
        next.valid = true;
        next.aim = aim;
        next.up = up;
        next.euler_deg = euler;
        next.accel = accel_filt;
        next.gyro_dps = {pitch_dps * kPitchSign, yaw_dps, roll_dps};
        next.ir_locked = ir_ok;
        next.mplus_live = have_mplus;
        next.timestamp_ms = now;
        next.link = ControllerTiltLinkStatus::Ok;
        sample = next;
        SetLink(ControllerTiltLinkStatus::Ok);
    }
#endif
};

ControllerTiltManager* ControllerTiltManager::instance()
{
    static ControllerTiltManager mgr;
    return &mgr;
}

ControllerTiltManager::ControllerTiltManager()
    : impl_(new Impl)
{
#if CONTROLLER_TILT_HAS_HIDAPI
    if(hid_init() != 0)
    {
        LOG_WARNING("[3DSpatial] ControllerTilt: hid_init failed");
    }
#else
    LOG_WARNING("[3DSpatial] ControllerTilt: built without hidapi — Wii Remote disabled");
#endif
}

ControllerTiltManager::~ControllerTiltManager()
{
    stop();
#if CONTROLLER_TILT_HAS_HIDAPI
    // Do not call hid_exit — OpenRGB / other plugin code may still use hidapi.
#endif
    delete impl_;
    impl_ = nullptr;
}

void ControllerTiltManager::start()
{
    QMutexLocker lock(&impl_->mutex);
    if(impl_->running)
    {
        return;
    }
    impl_->running = true;
    impl_->need_init = true;
    impl_->reconnect_cooldown = 0;
    impl_->sample = ControllerTiltSample{};
    impl_->SetLink(ControllerTiltLinkStatus::NoDevice);
}

void ControllerTiltManager::stop()
{
    QMutexLocker lock(&impl_->mutex);
    if(!impl_->running)
    {
        return;
    }
    impl_->running = false;
    impl_->CloseDevice();
    impl_->sample = ControllerTiltSample{};
    impl_->SetLink(ControllerTiltLinkStatus::Stopped);
}

bool ControllerTiltManager::isRunning() const
{
    QMutexLocker lock(&impl_->mutex);
    return impl_->running;
}

void ControllerTiltManager::Poll()
{
    QMutexLocker lock(&impl_->mutex);
#if CONTROLLER_TILT_HAS_HIDAPI
    impl_->PollWii();
#else
    impl_->sample.valid = false;
    impl_->sample.kind = ControllerTiltDeviceKind::None;
    impl_->SetLink(ControllerTiltLinkStatus::NoHidapi);
#endif
}

ControllerTiltSample ControllerTiltManager::Latest() const
{
    QMutexLocker lock(&impl_->mutex);
    return impl_->sample;
}

ControllerTiltLinkStatus ControllerTiltManager::LinkStatus() const
{
    QMutexLocker lock(&impl_->mutex);
    if(!impl_->running)
    {
        return ControllerTiltLinkStatus::Stopped;
    }
    return impl_->link_status;
}

void ControllerTiltManager::RecenterHeading()
{
    QMutexLocker lock(&impl_->mutex);
    impl_->orient = {};
    impl_->pitch_deg = 0.0f;
    impl_->yaw_deg = 0.0f;
    impl_->roll_deg = 0.0f;
    impl_->gyro_bias_ready = false;
    impl_->gyro_bias_samples = 0;
    impl_->gyro_bias_accum = {};
    impl_->gyro_bias = {};
    // Face the sensor bar at click time — that IR reading becomes yaw 0.
    impl_->ir_yaw_center_deg = impl_->last_ir_ok ? impl_->last_ir_yaw_deg : 0.0f;
    LOG_INFO("[3DSpatial] ControllerTilt: recentered (identity quat; IR yaw center=%.1f°)",
             impl_->ir_yaw_center_deg);
}

bool ControllerTiltManager::ResolveTorchOrigin(
    const std::vector<std::unique_ptr<VirtualReferencePoint3D>>& refs,
    Vector3D& origin_out) const
{
    const VirtualReferencePoint3D* named = nullptr;
    const VirtualReferencePoint3D* user = nullptr;
    const VirtualReferencePoint3D* chair = nullptr;

    for(const auto& ref : refs)
    {
        if(!ref)
        {
            continue;
        }
        if(!named && ControllerTiltNameLooksLikeWii(ref->GetName()))
        {
            named = ref.get();
        }
        if(!user && ref->GetType() == REF_POINT_USER)
        {
            user = ref.get();
        }
        if(!chair && ref->GetType() == REF_POINT_CHAIR)
        {
            chair = ref.get();
        }
    }

    const VirtualReferencePoint3D* pick = named ? named : (user ? user : chair);
    if(!pick)
    {
        return false;
    }
    origin_out = pick->GetPosition();
    return true;
}

void ControllerTiltManager::UpdateLayoutOrigins(
    const std::vector<std::unique_ptr<VirtualReferencePoint3D>>& refs)
{
    QMutexLocker lock(&impl_->mutex);
    Vector3D origin{};
    const bool ok = ResolveTorchOrigin(refs, origin);
    impl_->have_torch_origin = ok;
    if(ok)
    {
        impl_->torch_origin = origin;
    }
}

bool ControllerTiltManager::TorchOrigin(Vector3D& origin_out) const
{
    QMutexLocker lock(&impl_->mutex);
    if(!impl_->have_torch_origin)
    {
        return false;
    }
    origin_out = impl_->torch_origin;
    return true;
}
