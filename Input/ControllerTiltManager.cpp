// SPDX-License-Identifier: GPL-2.0-only

#include "ControllerTiltManager.h"
#include "MadgwickImu.h"
#include "PluginLog.h"

#include <QMutex>
#include <QMutexLocker>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <chrono>
#include <cstdlib>
#include <string>
#include <vector>

#if !defined(CONTROLLER_TILT_NO_HIDAPI)
#include <hidapi.h>
#define CONTROLLER_TILT_HAS_HIDAPI 1
#endif

namespace
{
constexpr unsigned short kSonyVid = 0x054Cu;
constexpr unsigned short kDualSensePid = 0x0CE6u;
constexpr unsigned short kDualSenseEdgePid = 0x0DF2u;

/** dualsense_input_report (Linux/SDL): sticks/buttons then gyro[3], accel[3] as le16. */
constexpr int kReportGyroOffset = 15;
constexpr int kReportAccelOffset = 21;
constexpr int kUsbReportMinBytes = 28;
constexpr int kBtReportMinBytes = 40;
constexpr unsigned char kBtInputReportId = 0x31;
constexpr unsigned char kUsbInputReportId = 0x01;
constexpr unsigned char kFeatureCalibrationId = 0x05;
constexpr int kFeatureCalibrationSize = 41;
/** dualsense_input_report: le32 sensor clock after accel (Linux: ticks/3 → µs). */
constexpr int kReportSensorTimestampOffset = 27;

constexpr float kPi = 3.14159265358979323846f;
constexpr float kDegToRad = kPi / 180.0f;
/**
 * Raw DualSense HID gyro (USB/BT report) is ~16.384 LSB per deg/s.
 * Linux hid-playstation's 1024 LSB/deg/s is the *post-calibration* ABS scale —
 * using that on raw reports made the torch ~62× too stiff.
 */
constexpr float kGyroLsbPerDegS = 16.384f;
constexpr float kAccelLsbPerG = 8192.0f;

std::int16_t ReadI16LE(const unsigned char* p)
{
    return (std::int16_t)((unsigned)p[0] | ((unsigned)p[1] << 8));
}

std::uint32_t ReadU32LE(const unsigned char* p)
{
    return (std::uint32_t)p[0]
           | ((std::uint32_t)p[1] << 8)
           | ((std::uint32_t)p[2] << 16)
           | ((std::uint32_t)p[3] << 24);
}

struct ImuAxisCalib
{
    /** Subtract from raw le16 before scale (0 for gyro — Linux treats report as pre-biased). */
    float bias_raw = 0.0f;
    /** Multiply (raw - bias) → physical units (deg/s or g). */
    float scale = 1.0f;
};

struct DualSenseImuCalib
{
    ImuAxisCalib gyro[3];  // pitch, yaw, roll
    ImuAxisCalib accel[3]; // x, y, z
    bool loaded = false;
};

void SetDefaultImuCalib(DualSenseImuCalib* c)
{
    for(int i = 0; i < 3; ++i)
    {
        c->gyro[i] = {0.0f, 1.0f / kGyroLsbPerDegS};
        c->accel[i] = {0.0f, 1.0f / kAccelLsbPerG};
    }
    c->loaded = false;
}

/**
 * Parse feature 0x05 (Linux hid-playstation dualsense_get_calibration_data).
 * Gyro: dps = raw * speed_2x / denom (factory bias field unused — firmware pre-biases).
 * Accel: g = 2 * (raw - bias) / range_2g.
 */
bool ParseFeatureCalibration(const unsigned char* buf, int len, DualSenseImuCalib* out)
{
    if(!buf || !out || len < 35 || buf[0] != kFeatureCalibrationId)
    {
        return false;
    }

    const std::int16_t gyro_pitch_bias = ReadI16LE(buf + 1);
    const std::int16_t gyro_yaw_bias = ReadI16LE(buf + 3);
    const std::int16_t gyro_roll_bias = ReadI16LE(buf + 5);
    const std::int16_t gyro_pitch_plus = ReadI16LE(buf + 7);
    const std::int16_t gyro_pitch_minus = ReadI16LE(buf + 9);
    const std::int16_t gyro_yaw_plus = ReadI16LE(buf + 11);
    const std::int16_t gyro_yaw_minus = ReadI16LE(buf + 13);
    const std::int16_t gyro_roll_plus = ReadI16LE(buf + 15);
    const std::int16_t gyro_roll_minus = ReadI16LE(buf + 17);
    const std::int16_t gyro_speed_plus = ReadI16LE(buf + 19);
    const std::int16_t gyro_speed_minus = ReadI16LE(buf + 21);
    const std::int16_t acc_x_plus = ReadI16LE(buf + 23);
    const std::int16_t acc_x_minus = ReadI16LE(buf + 25);
    const std::int16_t acc_y_plus = ReadI16LE(buf + 27);
    const std::int16_t acc_y_minus = ReadI16LE(buf + 29);
    const std::int16_t acc_z_plus = ReadI16LE(buf + 31);
    const std::int16_t acc_z_minus = ReadI16LE(buf + 33);

    const int speed_2x = (int)gyro_speed_plus + (int)gyro_speed_minus;
    const std::int16_t gyro_bias[3] = {gyro_pitch_bias, gyro_yaw_bias, gyro_roll_bias};
    const std::int16_t gyro_plus[3] = {gyro_pitch_plus, gyro_yaw_plus, gyro_roll_plus};
    const std::int16_t gyro_minus[3] = {gyro_pitch_minus, gyro_yaw_minus, gyro_roll_minus};

    DualSenseImuCalib parsed{};
    SetDefaultImuCalib(&parsed);

    for(int i = 0; i < 3; ++i)
    {
        const int denom = std::abs((int)gyro_plus[i] - (int)gyro_bias[i])
                          + std::abs((int)gyro_minus[i] - (int)gyro_bias[i]);
        if(denom > 0 && speed_2x != 0)
        {
            // Matches Linux: (speed_2x * raw / denom) deg/s — gyro bias left at 0.
            parsed.gyro[i].bias_raw = 0.0f;
            parsed.gyro[i].scale = (float)speed_2x / (float)denom;
        }
    }

    const std::int16_t acc_plus[3] = {acc_x_plus, acc_y_plus, acc_z_plus};
    const std::int16_t acc_minus[3] = {acc_x_minus, acc_y_minus, acc_z_minus};
    for(int i = 0; i < 3; ++i)
    {
        const int range_2g = (int)acc_plus[i] - (int)acc_minus[i];
        if(range_2g != 0)
        {
            parsed.accel[i].bias_raw = (float)((int)acc_plus[i] - range_2g / 2);
            parsed.accel[i].scale = 2.0f / (float)range_2g;
        }
    }

    parsed.loaded = true;
    *out = parsed;
    return true;
}

float ApplyAxisCalib(const ImuAxisCalib& c, std::int16_t raw)
{
    return ((float)raw - c.bias_raw) * c.scale;
}

/** DualSense sensor clock delta → seconds (Linux: ticks / 3 = µs). */
float SensorTimestampDeltaSeconds(std::uint32_t prev, std::uint32_t next)
{
    const std::uint32_t delta_ticks = next - prev; // uint wrap-safe
    if(delta_ticks == 0)
    {
        return 0.0f;
    }
    return (float)delta_ticks / (3.0f * 1.0e6f);
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
        return {0.0f, 1.0f, 0.0f};
    }
    const float inv = 1.0f / len;
    return {v.x * inv, v.y * inv, v.z * inv};
}

float Dot3(const Vector3D& a, const Vector3D& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vector3D Cross3(const Vector3D& a, const Vector3D& b)
{
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

float SoftDeadzone(float v, float dz)
{
    if(v > -dz && v < dz)
    {
        return 0.0f;
    }
    return (v > 0.0f) ? (v - dz) : (v + dz);
}

void OrthonormalizeBasis(Vector3D* aim_inout, Vector3D* up_inout)
{
    *aim_inout = Normalize3(*aim_inout);
    const float along = Dot3(*up_inout, *aim_inout);
    *up_inout = Normalize3({
        up_inout->x - aim_inout->x * along,
        up_inout->y - aim_inout->y * along,
        up_inout->z - aim_inout->z * along
    });
    if(Length3(*up_inout) < 1e-4f)
    {
        *up_inout = {0.0f, 1.0f, 0.0f};
    }
}

constexpr float kRadToDeg = 180.0f / kPi;

/**
 * Same Euler XYZ (degrees) as Geometry3DUtils / controller transforms:
 * rotate about X, then Y, then Z.
 */
Vector3D ApplyRoomEulerXYZDeg(Vector3D v, float rx_deg, float ry_deg, float rz_deg)
{
    const float rx = rx_deg * kDegToRad;
    const float ry = ry_deg * kDegToRad;
    const float rz = rz_deg * kDegToRad;

    float temp_y = v.y * std::cos(rx) - v.z * std::sin(rx);
    float temp_z = v.y * std::sin(rx) + v.z * std::cos(rx);
    v.y = temp_y;
    v.z = temp_z;

    float temp_x = v.x * std::cos(ry) + v.z * std::sin(ry);
    temp_z = -v.x * std::sin(ry) + v.z * std::cos(ry);
    v.x = temp_x;
    v.z = temp_z;

    temp_x = v.x * std::cos(rz) - v.y * std::sin(rz);
    temp_y = v.x * std::sin(rz) + v.y * std::cos(rz);
    v.x = temp_x;
    v.y = temp_y;
    return v;
}

/**
 * DualSense body → Madgwick filter frame:
 *   body +X right → filter +X
 *   body +Z light bar → filter +Y (forward)
 *   body +Y face up → filter +Z (up)
 * Madgwick identity then matches flat desk home after FilterEarthToRoom.
 */
void DualSenseBodyToFilter(float bx, float by, float bz, float* fx, float* fy, float* fz)
{
    *fx = bx;
    *fy = bz;
    *fz = by;
}

/** Filter/earth (+X right, +Y forward, +Z up) → RoomGrid (+X right, +Y up, +Z back). */
Vector3D FilterEarthToRoom(float ex, float ey, float ez)
{
    return {ex, ez, -ey};
}

/**
 * Pad-native Euler for status UI from room aim/up (before layout rotation).
 * X tip, Y twist, Z roll about beam — same labels as before.
 */
void PadEulerFromAimUp(const Vector3D& aim, const Vector3D& up,
                       float* pitch_deg, float* yaw_deg, float* roll_deg)
{
    const float aim_y = std::clamp(aim.y, -1.0f, 1.0f);
    *pitch_deg = std::asin(aim_y) * kRadToDeg;
    *yaw_deg = std::atan2(aim.x, -aim.z) * kRadToDeg;

    const Vector3D world_up{0.0f, 1.0f, 0.0f};
    Vector3D right = Cross3(aim, world_up);
    if(Length3(right) < 1e-4f)
    {
        right = Cross3(aim, {0.0f, 0.0f, 1.0f});
    }
    right = Normalize3(right);
    const Vector3D up_ref = Normalize3(Cross3(right, aim));
    *roll_deg = std::atan2(Dot3(up, right), Dot3(up, up_ref)) * kRadToDeg;
}

std::uint64_t NowMs()
{
    using clock = std::chrono::steady_clock;
    return (std::uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
               clock::now().time_since_epoch())
        .count();
}

/**
 * Locate dualsense_input_report start inside a HID read buffer.
 * USB: [report_id=0x01][report…]
 * BT enhanced: [report_id=0x31][seq][report…]
 * Returns byte offset of accel X, or -1 if unusable.
 */
int AccelOffsetInBuffer(const unsigned char* buf, int n, bool prefer_bt)
{
    if(n < kUsbReportMinBytes)
    {
        return -1;
    }

    if(buf[0] == kBtInputReportId && n >= kBtReportMinBytes)
    {
        // Report body starts at index 2.
        return 2 + kReportAccelOffset;
    }
    if(buf[0] == kUsbInputReportId)
    {
        return 1 + kReportAccelOffset;
    }

    // Some stacks omit the report id on USB reads.
    if(!prefer_bt && n >= (1 + kReportAccelOffset + 6))
    {
        return kReportAccelOffset;
    }

    // BT limited report 0x01 has no IMU — reject.
    if(buf[0] == kUsbInputReportId && prefer_bt)
    {
        return -1;
    }

    return -1;
}

int GyroOffsetFromAccelOffset(int accel_off)
{
    return accel_off - (kReportAccelOffset - kReportGyroOffset);
}

#if defined(CONTROLLER_TILT_HAS_HIDAPI)
/** True when this enumerated node is DualSense over Bluetooth. */
bool DualSenseInfoIsBluetooth(const hid_device_info* info)
{
    if(!info)
    {
        return false;
    }
#if defined(HID_API_BUS_BLUETOOTH)
    if(info->bus_type == HID_API_BUS_BLUETOOTH)
    {
        return true;
    }
#endif
    // Windows BT DualSense often reports interface_number == -1.
    return info->interface_number < 0;
}
#endif
} // namespace

class ControllerTiltManager::Impl
{
public:
    mutable QMutex mutex;
    bool running = false;
    ControllerTiltSample sample{};
    ControllerTiltLinkStatus link_status = ControllerTiltLinkStatus::Stopped;
    bool open_failed_logged = false;
    bool open_ok_logged = false;
    bool enhanced_logged = false;
    bool limited_bt_logged = false;
    bool calib_logged = false;
    bool has_dualsense_origin = false;
    Vector3D dualsense_origin{0.0f, 0.0f, 0.0f};
    Rotation3D layout_rotation{0.0f, 0.0f, 0.0f};
    bool is_bluetooth = false;
    std::uint64_t last_enhanced_request_ms = 0;
    std::uint64_t last_open_attempt_ms = 0;
    DualSenseImuCalib imu_calib{};
    bool sensor_ts_ready = false;
    std::uint32_t last_sensor_ts = 0;
    int empty_polls = 0;
    bool ever_got_imu = false;
    bool calib_attempted = false;

    bool have_pose = false;
    float pitch_deg = 0.0f; // X — tip
    float yaw_deg = 0.0f;   // Y — twist
    float roll_deg = 0.0f;  // Z — tilt left/right
    Vector3D fused_aim{0.0f, 0.0f, -1.0f};
    Vector3D fused_up{0.0f, 1.0f, 0.0f};
    /** Light LPF on accel (g) before Madgwick. */
    float filt_ax = 0.0f;
    float filt_ay = 1.0f;
    float filt_az = 0.0f;
    bool filt_accel_ready = false;
    std::uint64_t last_fusion_ms = 0;
    bool gyro_bias_ready = false;
    Vector3D gyro_bias_dps{0.0f, 0.0f, 0.0f};
    int gyro_bias_samples = 0;
    int still_frames = 0;
    bool fusion_logged = false;
    /** Madgwick IMU — beta ~0.05 balances settle vs lag for handheld torch. */
    MadgwickImu madgwick{0.05f};

    Impl()
    {
        SetDefaultImuCalib(&imu_calib);
    }

    void SetLink(ControllerTiltLinkStatus s)
    {
        link_status = s;
        sample.link = s;
    }

    void ResetFusion()
    {
        have_pose = false;
        pitch_deg = 0.0f;
        yaw_deg = 0.0f;
        roll_deg = 0.0f;
        fused_aim = {0.0f, 0.0f, -1.0f};
        fused_up = {0.0f, 1.0f, 0.0f};
        filt_ax = 0.0f;
        filt_ay = 1.0f;
        filt_az = 0.0f;
        filt_accel_ready = false;
        last_fusion_ms = 0;
        gyro_bias_ready = false;
        gyro_bias_dps = {0.0f, 0.0f, 0.0f};
        gyro_bias_samples = 0;
        still_frames = 0;
        fusion_logged = false;
        sensor_ts_ready = false;
        last_sensor_ts = 0;
        empty_polls = 0;
        madgwick.Reset();
    }

    void RebuildBasisFromMadgwick()
    {
        // Filter body: +Y = light-bar forward, +Z = face up → room aim/up.
        float ex = 0.0f;
        float ey = 0.0f;
        float ez = 0.0f;
        madgwick.RotateBodyToEarth(0.0f, 1.0f, 0.0f, &ex, &ey, &ez);
        fused_aim = FilterEarthToRoom(ex, ey, ez);
        madgwick.RotateBodyToEarth(0.0f, 0.0f, 1.0f, &ex, &ey, &ez);
        fused_up = FilterEarthToRoom(ex, ey, ez);
        OrthonormalizeBasis(&fused_aim, &fused_up);
        PadEulerFromAimUp(fused_aim, fused_up, &pitch_deg, &yaw_deg, &roll_deg);

        if(has_dualsense_origin)
        {
            fused_aim = ApplyRoomEulerXYZDeg(fused_aim,
                                             layout_rotation.x,
                                             layout_rotation.y,
                                             layout_rotation.z);
            fused_up = ApplyRoomEulerXYZDeg(fused_up,
                                            layout_rotation.x,
                                            layout_rotation.y,
                                            layout_rotation.z);
            OrthonormalizeBasis(&fused_aim, &fused_up);
        }
    }

    void RecenterPose()
    {
        gyro_bias_ready = false;
        gyro_bias_dps = {0.0f, 0.0f, 0.0f};
        gyro_bias_samples = 0;
        still_frames = 0;
        madgwick.Reset();
        pitch_deg = 0.0f;
        yaw_deg = 0.0f;
        roll_deg = 0.0f;
        filt_ax = 0.0f;
        filt_ay = 1.0f;
        filt_az = 0.0f;
        filt_accel_ready = false;
        RebuildBasisFromMadgwick();
        have_pose = true;
        sample.aim = fused_aim;
        sample.up = fused_up;
        sample.euler_deg = {0.0f, 0.0f, 0.0f};
    }

#if defined(CONTROLLER_TILT_HAS_HIDAPI)
    hid_device* device = nullptr;
    std::string open_path_owned;
#endif

    /** Drop the HID handle immediately — never I/O a dead DualSense after unplug. */
    void CloseDevice()
    {
#if defined(CONTROLLER_TILT_HAS_HIDAPI)
        if(device)
        {
            hid_close(device);
            device = nullptr;
        }
        open_path_owned.clear();
#endif
        is_bluetooth = false;
        enhanced_logged = false;
        limited_bt_logged = false;
        calib_logged = false;
        calib_attempted = false;
        ever_got_imu = false;
        empty_polls = 0;
        last_enhanced_request_ms = 0;
        // Keep last_open_attempt_ms so reconnect is backoff-throttled.
        SetDefaultImuCalib(&imu_calib);
        ResetFusion();
        sample.valid = false;
        sample.kind = ControllerTiltDeviceKind::None;
    }

    void HandleDisconnect(const char* reason)
    {
#if defined(CONTROLLER_TILT_HAS_HIDAPI)
        if(device)
        {
            LOG_WARNING("[3DSpatial] ControllerTilt: DualSense HID dropped (%s) — closing handle",
                        reason ? reason : "unknown");
        }
#else
        (void)reason;
#endif
        CloseDevice();
        open_ok_logged = false;
        last_open_attempt_ms = NowMs(); // throttle reconnect during unplug/rescan storms
        SetLink(ControllerTiltLinkStatus::NoDevice);
        sample.link = link_status;
    }

    /**
     * Read feature 0x05: factory IMU calibration (USB + BT) and BT enhanced unlock.
     * Returns false if the device died during the call (handle already closed).
     */
    bool LoadCalibrationFeature(bool force)
    {
#if defined(CONTROLLER_TILT_HAS_HIDAPI)
        if(!device)
        {
            return true;
        }
        // USB: one attempt is enough. BT: retry while waiting for enhanced 0x31.
        if(!force && calib_attempted && !is_bluetooth)
        {
            return true;
        }
        const std::uint64_t now = NowMs();
        if(!force && last_enhanced_request_ms != 0 && (now - last_enhanced_request_ms) < 1500u)
        {
            return true;
        }
        last_enhanced_request_ms = now;
        calib_attempted = true;

        unsigned char calib[64];
        std::memset(calib, 0, sizeof(calib));
        calib[0] = kFeatureCalibrationId;
        const int got = hid_get_feature_report(device, calib, kFeatureCalibrationSize);
        if(got < 0)
        {
            HandleDisconnect("feature 0x05 failed");
            return false;
        }
        if(got >= 35 && ParseFeatureCalibration(calib, got, &imu_calib))
        {
            if(!calib_logged)
            {
                LOG_INFO("[3DSpatial] ControllerTilt: DualSense factory IMU calib loaded "
                         "(gyro scale %.5f/%.5f/%.5f dps/LSB)",
                         imu_calib.gyro[0].scale,
                         imu_calib.gyro[1].scale,
                         imu_calib.gyro[2].scale);
                calib_logged = true;
            }
            if(is_bluetooth && !enhanced_logged)
            {
                LOG_INFO("[3DSpatial] ControllerTilt: DualSense BT enhanced reports requested "
                         "(feature 0x05, %d bytes)",
                         got);
                enhanced_logged = true;
            }
        }
        else if(got >= 1)
        {
            if(is_bluetooth && !enhanced_logged)
            {
                LOG_INFO("[3DSpatial] ControllerTilt: DualSense BT feature 0x05 ok (%d bytes) "
                         "but calib parse skipped — using default scale",
                         got);
                enhanced_logged = true;
            }
        }
        else if(is_bluetooth && !limited_bt_logged)
        {
            LOG_WARNING("[3DSpatial] ControllerTilt: DualSense BT calibration feature failed — "
                        "IMU needs enhanced report 0x31 (got %d)",
                        got);
            limited_bt_logged = true;
        }
        return true;
#else
        (void)force;
        return true;
#endif
    }

    bool EnsureDevice()
    {
#if defined(CONTROLLER_TILT_HAS_HIDAPI)
        if(device)
        {
            return true;
        }

        // Never re-enumerate every render frame — that freezes OpenRGB's UI when the
        // pad is missing or held by another app / mid-rescan.
        const std::uint64_t now = NowMs();
        constexpr std::uint64_t k_open_retry_ms = 2000u;
        if(last_open_attempt_ms != 0 && (now - last_open_attempt_ms) < k_open_retry_ms)
        {
            return false;
        }
        last_open_attempt_ms = now;

        struct Candidate
        {
            std::string path;
            bool bt = false;
            int interface_number = -1;
        };
        std::vector<Candidate> candidates;
        bool saw_dualsense = false;

        hid_device_info* list = hid_enumerate(kSonyVid, 0);
        for(hid_device_info* it = list; it; it = it->next)
        {
            if(it->product_id != kDualSensePid && it->product_id != kDualSenseEdgePid)
            {
                continue;
            }
            if(!it->path)
            {
                continue;
            }
            saw_dualsense = true;
            Candidate c;
            c.path = it->path; // own a copy — list is freed before open
            c.bt = DualSenseInfoIsBluetooth(it);
            c.interface_number = it->interface_number;
            candidates.push_back(std::move(c));
        }
        hid_free_enumeration(list);

        // Prefer BT, then USB vendor iface 3 (gamepad/IMU) when present.
        std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
            if(a.bt != b.bt)
            {
                return a.bt && !b.bt;
            }
            const int score_a = (a.interface_number == 3) ? 0 : 1;
            const int score_b = (b.interface_number == 3) ? 0 : 1;
            return score_a < score_b;
        });

        for(const Candidate& c : candidates)
        {
            hid_device* try_dev = hid_open_path(c.path.c_str());
            if(!try_dev)
            {
                continue;
            }
            hid_set_nonblocking(try_dev, 1);
            device = try_dev;
            open_path_owned = c.path;
            is_bluetooth = c.bt;
            break;
        }

        // Do NOT fall back to hid_open(vid,pid) — that grabs an arbitrary interface and
        // races OpenRGB's Sony DualSense RGB driver on the same node.

        if(!device)
        {
            SetLink(saw_dualsense ? ControllerTiltLinkStatus::DeviceBusy
                                  : ControllerTiltLinkStatus::NoDevice);
            if(!open_failed_logged)
            {
                if(saw_dualsense)
                {
                    LOG_WARNING("[3DSpatial] ControllerTilt: DualSense present but HID open failed — "
                                "pad may be held by OpenRGB rescan / Steam / another app "
                                "(or Linux hidraw permissions)");
                }
                else
                {
                    LOG_WARNING("[3DSpatial] ControllerTilt: DualSense HID open failed "
                                "(no pad found, or Linux hidraw permissions)");
                }
                open_failed_logged = true;
            }
            return false;
        }

        if(!open_ok_logged)
        {
            LOG_INFO("[3DSpatial] ControllerTilt: DualSense opened (%s, iface path ok) for IMU torch",
                     is_bluetooth ? "Bluetooth" : "USB");
            open_ok_logged = true;
        }
        open_failed_logged = false;
        empty_polls = 0;
        ever_got_imu = false;
        SetLink(is_bluetooth ? ControllerTiltLinkStatus::WaitingEnhancedBt
                             : ControllerTiltLinkStatus::Ok);

        if(!LoadCalibrationFeature(true))
        {
            return false;
        }
        return device != nullptr;
#else
        SetLink(ControllerTiltLinkStatus::NoHidapi);
        if(!open_failed_logged)
        {
            LOG_WARNING("[3DSpatial] ControllerTilt: DualSense IMU disabled "
                        "(built without hidapi)");
            open_failed_logged = true;
        }
        return false;
#endif
    }

    void PollDualSense()
    {
        // Hold mutex for the whole poll so stop() cannot hid_close under us.
        QMutexLocker lock(&mutex);
        if(!running)
        {
            SetLink(ControllerTiltLinkStatus::Stopped);
            return;
        }
#if defined(CONTROLLER_TILT_HAS_HIDAPI)
        if(!EnsureDevice())
        {
            sample.valid = false;
            sample.kind = ControllerTiltDeviceKind::None;
            sample.link = link_status;
            return;
        }

        // Refresh calib / BT unlock only while needed (never on a dead handle).
        if(is_bluetooth || !imu_calib.loaded)
        {
            if(!LoadCalibrationFeature(false))
            {
                return;
            }
            if(!device)
            {
                return;
            }
        }

        unsigned char buf[128];
        float sum_ax = 0.0f;
        float sum_ay = 0.0f;
        float sum_az = 0.0f;
        float sum_gx = 0.0f;
        float sum_gy = 0.0f;
        float sum_gz = 0.0f;
        int packet_count = 0;
        bool saw_limited_bt = false;
        bool have_batch_ts = false;
        std::uint32_t batch_last_ts = 0;

        // Cap drain — a disconnect storm must not spin the UI thread.
        constexpr int k_max_reads_per_poll = 48;
        for(int n = 0; n < k_max_reads_per_poll; ++n)
        {
            const int got = hid_read(device, buf, sizeof(buf));
            if(got < 0)
            {
                HandleDisconnect("hid_read error");
                return;
            }
            if(got == 0)
            {
                break;
            }

            if(buf[0] == kBtInputReportId)
            {
                // Report id is authoritative (Linux hidraw may not flag BT via interface_number).
                is_bluetooth = true;
            }

            if(is_bluetooth && buf[0] == kUsbInputReportId)
            {
                saw_limited_bt = true;
                continue;
            }

            const int accel_off = AccelOffsetInBuffer(buf, got, is_bluetooth);
            if(accel_off < 0 || accel_off + 6 > got)
            {
                continue;
            }
            const int gyro_off = GyroOffsetFromAccelOffset(accel_off);
            if(gyro_off < 0 || gyro_off + 6 > got)
            {
                continue;
            }

            const int body_base = accel_off - kReportAccelOffset;
            const int ts_off = body_base + kReportSensorTimestampOffset;
            if(ts_off >= 0 && ts_off + 4 <= got)
            {
                batch_last_ts = ReadU32LE(buf + ts_off);
                have_batch_ts = true;
            }

            sum_ax += ApplyAxisCalib(imu_calib.accel[0], ReadI16LE(buf + accel_off));
            sum_ay += ApplyAxisCalib(imu_calib.accel[1], ReadI16LE(buf + accel_off + 2));
            sum_az += ApplyAxisCalib(imu_calib.accel[2], ReadI16LE(buf + accel_off + 4));
            sum_gx += ApplyAxisCalib(imu_calib.gyro[0], ReadI16LE(buf + gyro_off));
            sum_gy += ApplyAxisCalib(imu_calib.gyro[1], ReadI16LE(buf + gyro_off + 2));
            sum_gz += ApplyAxisCalib(imu_calib.gyro[2], ReadI16LE(buf + gyro_off + 4));
            ++packet_count;
        }

        if(packet_count <= 0)
        {
            ++empty_polls;
            if(saw_limited_bt)
            {
                SetLink(ControllerTiltLinkStatus::WaitingEnhancedBt);
                if(!limited_bt_logged)
                {
                    LOG_WARNING("[3DSpatial] ControllerTilt: DualSense BT still on limited report 0x01 "
                                "(no IMU). Waiting for enhanced 0x31 after calibration request.");
                    limited_bt_logged = true;
                }
                if(!LoadCalibrationFeature(false))
                {
                    return;
                }
            }
            // After we had IMU, prolonged silence usually means unplug / OpenRGB rescan
            // stole the node — drop the handle instead of I/O on a zombie.
            constexpr int k_zombie_empty_polls = 90; // ~3s at 30 FPS
            if(ever_got_imu && empty_polls >= k_zombie_empty_polls)
            {
                HandleDisconnect("no IMU reports (likely unplugged or rescanned)");
                return;
            }
            sample.valid = false;
            sample.kind = ControllerTiltDeviceKind::None;
            sample.link = link_status;
            return;
        }

        empty_polls = 0;
        ever_got_imu = true;
        SetLink(ControllerTiltLinkStatus::Ok);

        const float inv_n = 1.0f / (float)packet_count;
        const float ax = sum_ax * inv_n;
        const float ay = sum_ay * inv_n;
        const float az = sum_az * inv_n;
        float gx_raw = sum_gx * inv_n;
        float gy_raw = sum_gy * inv_n;
        float gz_raw = sum_gz * inv_n;

        const float accel_mag = std::sqrt(ax * ax + ay * ay + az * az);
        const float gyro_raw_mag =
            std::sqrt(gx_raw * gx_raw + gy_raw * gy_raw + gz_raw * gz_raw);
        // Bias sampling only: quiet gyro + |a|≈1g. Accel fusion has its own gates.
        const bool likely_still =
            (gyro_raw_mag < 12.0f) && (accel_mag > 0.92f) && (accel_mag < 1.08f);

        if(likely_still)
        {
            ++still_frames;
        }
        else
        {
            still_frames = 0;
        }

        if(!gyro_bias_ready)
        {
            // Need a solid rest sample so leftover bias doesn't walk the beam.
            if(likely_still)
            {
                const float n = (float)gyro_bias_samples;
                gyro_bias_dps.x = (gyro_bias_dps.x * n + gx_raw) / (n + 1.0f);
                gyro_bias_dps.y = (gyro_bias_dps.y * n + gy_raw) / (n + 1.0f);
                gyro_bias_dps.z = (gyro_bias_dps.z * n + gz_raw) / (n + 1.0f);
                ++gyro_bias_samples;
                if(gyro_bias_samples >= 40)
                {
                    gyro_bias_ready = true;
                    LOG_INFO("[3DSpatial] ControllerTilt: DualSense residual gyro bias ready "
                             "(%.2f, %.2f, %.2f deg/s)",
                             gyro_bias_dps.x, gyro_bias_dps.y, gyro_bias_dps.z);
                }
            }
        }
        else if(likely_still && still_frames > 8)
        {
            // Slow online bias track while parked — kills long-term walk.
            constexpr float k_bias_track = 0.02f;
            gyro_bias_dps.x += (gx_raw - gyro_bias_dps.x) * k_bias_track;
            gyro_bias_dps.y += (gy_raw - gyro_bias_dps.y) * k_bias_track;
            gyro_bias_dps.z += (gz_raw - gyro_bias_dps.z) * k_bias_track;
        }

        float gx_dps = gx_raw;
        float gy_dps = gy_raw;
        float gz_dps = gz_raw;
        if(gyro_bias_ready)
        {
            gx_dps -= gyro_bias_dps.x;
            gy_dps -= gyro_bias_dps.y;
            gz_dps -= gyro_bias_dps.z;
        }

        // Soft deadzone — Madgwick needs real rates; keep only tremor kill.
        constexpr float k_gyro_deadzone_dps = 1.0f;
        gx_dps = SoftDeadzone(gx_dps, k_gyro_deadzone_dps);
        gy_dps = SoftDeadzone(gy_dps, k_gyro_deadzone_dps);
        gz_dps = SoftDeadzone(gz_dps, k_gyro_deadzone_dps);

        constexpr float k_accel_lpf = 0.20f;
        if(!filt_accel_ready)
        {
            filt_ax = ax;
            filt_ay = ay;
            filt_az = az;
            filt_accel_ready = true;
        }
        else
        {
            filt_ax += (ax - filt_ax) * k_accel_lpf;
            filt_ay += (ay - filt_ay) * k_accel_lpf;
            filt_az += (az - filt_az) * k_accel_lpf;
        }

        const std::uint64_t now = NowMs();
        float dt = 0.0f;
        if(have_batch_ts && sensor_ts_ready)
        {
            dt = SensorTimestampDeltaSeconds(last_sensor_ts, batch_last_ts);
        }
        else if(last_fusion_ms != 0 && now > last_fusion_ms)
        {
            dt = (float)(now - last_fusion_ms) * 0.001f;
        }
        if(dt < 0.001f || dt > 0.12f)
        {
            dt = 1.0f / 60.0f;
        }
        last_fusion_ms = now;
        if(have_batch_ts)
        {
            last_sensor_ts = batch_last_ts;
            sensor_ts_ready = true;
        }

        // DualSense body → filter frame, then Madgwick IMU (no mag).
        float fax = 0.0f;
        float fay = 0.0f;
        float faz = 0.0f;
        float fgx_dps = 0.0f;
        float fgy_dps = 0.0f;
        float fgz_dps = 0.0f;
        DualSenseBodyToFilter(filt_ax, filt_ay, filt_az, &fax, &fay, &faz);
        // Hold gyro at 0 until residual bias is sampled — accel still settles pitch/roll.
        if(gyro_bias_ready)
        {
            DualSenseBodyToFilter(gx_dps, gy_dps, gz_dps, &fgx_dps, &fgy_dps, &fgz_dps);
        }

        // Skip accel correction when |a| far from 1g (linear accel / free fall).
        float ax_in = fax;
        float ay_in = fay;
        float az_in = faz;
        const float filt_mag = std::sqrt(fax * fax + fay * fay + faz * faz);
        if(filt_mag < 0.75f || filt_mag > 1.25f)
        {
            ax_in = 0.0f;
            ay_in = 0.0f;
            az_in = 0.0f;
        }

        madgwick.UpdateIMU(fgx_dps * kDegToRad,
                           fgy_dps * kDegToRad,
                           fgz_dps * kDegToRad,
                           ax_in, ay_in, az_in,
                           dt);
        have_pose = true;
        RebuildBasisFromMadgwick();

        if(!fusion_logged)
        {
            LOG_INFO("[3DSpatial] ControllerTilt: Madgwick IMU fuse "
                     "(pitch/roll vs gravity; yaw relative to Recenter; beta=%.3f)",
                     madgwick.Beta());
            fusion_logged = true;
        }

        ControllerTiltSample next{};
        next.kind = ControllerTiltDeviceKind::DualSense;
        next.valid = true;
        next.accel = Normalize3({filt_ax, filt_ay, filt_az});
        next.gyro_dps = {gx_dps, gy_dps, gz_dps};
        next.aim = fused_aim;
        next.up = fused_up;
        next.euler_deg = {pitch_deg, yaw_deg, roll_deg};
        next.timestamp_ms = now;
        next.link = link_status;

        sample = next;
#else
        sample.valid = false;
        sample.kind = ControllerTiltDeviceKind::None;
        SetLink(ControllerTiltLinkStatus::NoHidapi);
#endif
    }
};

ControllerTiltManager* ControllerTiltManager::instance()
{
    static ControllerTiltManager mgr;
    return &mgr;
}

ControllerTiltManager::ControllerTiltManager()
    : impl_(new Impl)
{
#if defined(CONTROLLER_TILT_HAS_HIDAPI)
    if(hid_init() != 0)
    {
        LOG_WARNING("[3DSpatial] ControllerTilt: hid_init failed");
    }
#endif
}

ControllerTiltManager::~ControllerTiltManager()
{
    stop();
    // Do NOT call hid_exit() — OpenRGB owns the process-wide hidapi lifetime.
    // Exiting from a plugin tears down HID under DetectionManager and crashes the UI.
    delete impl_;
    impl_ = nullptr;
}

void ControllerTiltManager::start()
{
    if(!impl_)
    {
        return;
    }
    QMutexLocker lock(&impl_->mutex);
    if(impl_->running)
    {
        return;
    }
    impl_->running = true;
    impl_->open_failed_logged = false;
    impl_->open_ok_logged = false;
    impl_->enhanced_logged = false;
    impl_->limited_bt_logged = false;
    impl_->calib_logged = false;
    impl_->calib_attempted = false;
    impl_->last_enhanced_request_ms = 0;
    impl_->last_open_attempt_ms = 0;
    SetDefaultImuCalib(&impl_->imu_calib);
    impl_->SetLink(ControllerTiltLinkStatus::NoDevice);
    impl_->ResetFusion();
}

void ControllerTiltManager::stop()
{
    if(!impl_)
    {
        return;
    }
    QMutexLocker lock(&impl_->mutex);
    impl_->running = false;
    impl_->CloseDevice();
    impl_->sample = ControllerTiltSample{};
    impl_->SetLink(ControllerTiltLinkStatus::Stopped);
}

bool ControllerTiltManager::isRunning() const
{
    return impl_ && impl_->running;
}

void ControllerTiltManager::Poll()
{
    if(!impl_ || !impl_->running)
    {
        return;
    }
    // Single backend for now — add Wii / DS4 branches here later.
    impl_->PollDualSense();
}

ControllerTiltSample ControllerTiltManager::Latest() const
{
    if(!impl_)
    {
        return {};
    }
    QMutexLocker lock(&impl_->mutex);
    return impl_->sample;
}

ControllerTiltLinkStatus ControllerTiltManager::LinkStatus() const
{
    if(!impl_)
    {
        return ControllerTiltLinkStatus::Stopped;
    }
    QMutexLocker lock(&impl_->mutex);
    return impl_->link_status;
}

void ControllerTiltManager::RecenterHeading()
{
    if(!impl_)
    {
        return;
    }
    QMutexLocker lock(&impl_->mutex);
    impl_->RecenterPose();
    LOG_INFO("[3DSpatial] ControllerTilt: recentered Madgwick quat "
             "(hold flat, light bar toward front wall)");
}

bool ControllerTiltManager::ResolveDualSenseOrigin(
    const std::vector<std::unique_ptr<ControllerTransform>>& transforms,
    Vector3D& origin_out) const
{
    Rotation3D unused{};
    return ResolveDualSenseLayoutPose(transforms, origin_out, unused);
}

bool ControllerTiltManager::ResolveDualSenseLayoutPose(
    const std::vector<std::unique_ptr<ControllerTransform>>& transforms,
    Vector3D& origin_out,
    Rotation3D& rotation_out) const
{
    for(const std::unique_ptr<ControllerTransform>& transform_ptr : transforms)
    {
        if(!transform_ptr || !transform_ptr->controller)
        {
            continue;
        }
        const std::string name = transform_ptr->controller->GetName();
        if(!ControllerTiltNameLooksLikeDualSense(name))
        {
            continue;
        }

        // Scene object pose: position + rotation of the DualSense controller in RoomGrid.
        origin_out = transform_ptr->transform.position;
        rotation_out = transform_ptr->transform.rotation;
        return true;
    }
    return false;
}

void ControllerTiltManager::UpdateLayoutOrigins(
    const std::vector<std::unique_ptr<ControllerTransform>>& transforms)
{
    if(!impl_)
    {
        return;
    }
    Vector3D origin{};
    Rotation3D rotation{};
    const bool ok = ResolveDualSenseLayoutPose(transforms, origin, rotation);
    QMutexLocker lock(&impl_->mutex);
    impl_->has_dualsense_origin = ok;
    impl_->dualsense_origin = origin;
    if(ok)
    {
        impl_->layout_rotation = rotation;
    }
}

bool ControllerTiltManager::DualSenseOrigin(Vector3D& origin_out) const
{
    if(!impl_)
    {
        return false;
    }
    QMutexLocker lock(&impl_->mutex);
    if(!impl_->has_dualsense_origin)
    {
        return false;
    }
    origin_out = impl_->dualsense_origin;
    return true;
}
