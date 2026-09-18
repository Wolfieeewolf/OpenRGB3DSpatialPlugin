// SPDX-License-Identifier: GPL-2.0-only

#include "ControllerTiltManager.h"
#include "PluginLog.h"

#include <QMutex>
#include <QMutexLocker>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <chrono>

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

constexpr float kPi = 3.14159265358979323846f;
constexpr float kDegToRad = kPi / 180.0f;
/**
 * Raw DualSense HID gyro (USB/BT report) is ~16.384 LSB per deg/s.
 * Linux hid-playstation's 1024 LSB/deg/s is the *post-calibration* ABS scale —
 * using that on raw reports made the torch ~62× too stiff.
 */
constexpr float kGyroLsbPerDegS = 16.384f;
constexpr float kAccelLsbPerG = 8192.0f;

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

float DeadzoneScalar(float v, float dz)
{
    if(v > -dz && v < dz)
    {
        return 0.0f;
    }
    return v;
}

Vector3D RotateAroundAxis(Vector3D v, Vector3D axis, float angle_rad)
{
    axis = Normalize3(axis);
    const float c = std::cos(angle_rad);
    const float s = std::sin(angle_rad);
    const float d = Dot3(axis, v);
    const Vector3D cross = Cross3(axis, v);
    return {
        v.x * c + cross.x * s + axis.x * d * (1.0f - c),
        v.y * c + cross.y * s + axis.y * d * (1.0f - c),
        v.z * c + cross.z * s + axis.z * d * (1.0f - c)
    };
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

/** Room home: DualSense flat on desk, light bar toward the front wall (−Z). */
void ResetRestBasis(Vector3D* aim_out, Vector3D* up_out)
{
    *aim_out = {0.0f, 0.0f, -1.0f};
    *up_out = {0.0f, 1.0f, 0.0f};
}

/**
 * Rigid-body IMU step. Torch aim is always DualSense forward (light bar / front).
 *
 * Two stages so a left/right roll cannot yank heading:
 *  1) Gyro only — the whole pad rotates as a rigid body (pitch/yaw/roll).
 *  2) Gravity tilt — rotate about a *horizontal* axis to match pitch/roll.
 *     World-Y (heading) is then restored so accel never changes yaw.
 */
void IntegrateRigidImu(float gx_rad, float gy_rad, float gz_rad,
                       float ax, float ay, float az, float dt,
                       Vector3D* mahony_i, Vector3D* aim_inout, Vector3D* up_inout)
{
    OrthonormalizeBasis(aim_inout, up_inout);
    Vector3D right = Cross3(*up_inout, *aim_inout);
    if(Length3(right) < 1e-5f)
    {
        right = {1.0f, 0.0f, 0.0f};
    }
    else
    {
        right = Normalize3(right);
    }

    // 1) Gyro: DualSense X=pitch, Y=yaw, Z=roll, applied in the pad's body frame.
    const Vector3D omega{
        right.x * gx_rad + up_inout->x * gy_rad + aim_inout->x * gz_rad,
        right.y * gx_rad + up_inout->y * gy_rad + aim_inout->y * gz_rad,
        right.z * gx_rad + up_inout->z * gy_rad + aim_inout->z * gz_rad
    };
    const float wlen = Length3(omega);
    if(wlen > 1e-6f)
    {
        const Vector3D axis = Normalize3(omega);
        const float angle = wlen * dt;
        *aim_inout = RotateAroundAxis(*aim_inout, axis, angle);
        *up_inout = RotateAroundAxis(*up_inout, axis, angle);
        OrthonormalizeBasis(aim_inout, up_inout);
        right = Normalize3(Cross3(*up_inout, *aim_inout));
    }

    // Integrator from the old mixed filter — decay so it cannot dump leftover yaw.
    mahony_i->x *= 0.90f;
    mahony_i->y *= 0.90f;
    mahony_i->z *= 0.90f;

    const float amag = std::sqrt(ax * ax + ay * ay + az * az);
    if(amag < 1e-4f)
    {
        return;
    }
    const float dev = std::fabs(amag - 1.0f);
    constexpr float k_lin_reject = 0.18f;
    if(dev >= k_lin_reject)
    {
        return;
    }

    // Gyro owns fast combined motion; gravity only assists slow tilt.
    const float gyro_mag = wlen;
    constexpr float k_gyro_fade_rad = 1.2f; // ~70 deg/s → half weight
    float grav_w = (1.0f - (dev / k_lin_reject)) * (1.0f / (1.0f + gyro_mag / k_gyro_fade_rad));
    if(grav_w < 0.02f)
    {
        return;
    }

    // Specific force → room "up". DualSense at rest is +Y through the face.
    Vector3D up_meas = {
        right.x * ax + up_inout->x * ay + aim_inout->x * az,
        right.y * ax + up_inout->y * ay + aim_inout->y * az,
        right.z * ax + up_inout->z * ay + aim_inout->z * az
    };
    up_meas = Normalize3(up_meas);

    Vector3D tilt = Cross3(*up_inout, up_meas);
    tilt.y = 0.0f; // no rotation about world up — heading stays gyro-only
    const float tlen = Length3(tilt);
    if(tlen < 1e-5f)
    {
        return;
    }

    const float hx = aim_inout->x;
    const float hz = aim_inout->z;
    const float hlen = std::sqrt(hx * hx + hz * hz);

    constexpr float k_kp = 1.6f;
    constexpr float k_max_step = 0.025f; // ~1.4° per frame
    float step = k_kp * tlen * dt * grav_w;
    if(step > k_max_step)
    {
        step = k_max_step;
    }
    const Vector3D tilt_axis = Normalize3(tilt);
    *aim_inout = RotateAroundAxis(*aim_inout, tilt_axis, step);
    *up_inout = RotateAroundAxis(*up_inout, tilt_axis, step);
    OrthonormalizeBasis(aim_inout, up_inout);

    // Belt-and-braces: put compass heading back if the tilt step leaked any yaw.
    if(hlen > 0.20f)
    {
        const float hx2 = aim_inout->x;
        const float hz2 = aim_inout->z;
        const float hlen2 = std::sqrt(hx2 * hx2 + hz2 * hz2);
        if(hlen2 > 0.20f)
        {
            const float h0 = std::atan2(hx, -hz);
            const float h1 = std::atan2(hx2, -hz2);
            float dyaw = h0 - h1;
            while(dyaw > kPi)
            {
                dyaw -= 2.0f * kPi;
            }
            while(dyaw < -kPi)
            {
                dyaw += 2.0f * kPi;
            }
            if(std::fabs(dyaw) > 1e-5f)
            {
                const Vector3D world_up{0.0f, 1.0f, 0.0f};
                *aim_inout = RotateAroundAxis(*aim_inout, world_up, dyaw);
                *up_inout = RotateAroundAxis(*up_inout, world_up, dyaw);
                OrthonormalizeBasis(aim_inout, up_inout);
            }
        }
    }
}

std::int16_t ReadI16LE(const unsigned char* p)
{
    return (std::int16_t)((unsigned)p[0] | ((unsigned)p[1] << 8));
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
    bool open_failed_logged = false;
    bool open_ok_logged = false;
    bool enhanced_logged = false;
    bool limited_bt_logged = false;
    bool has_dualsense_origin = false;
    Vector3D dualsense_origin{0.0f, 0.0f, 0.0f};
    Rotation3D layout_rotation{0.0f, 0.0f, 0.0f};
    bool is_bluetooth = false;
    std::uint64_t last_enhanced_request_ms = 0;

    /** Rigid IMU: aim is DualSense forward; gravity holds pitch/roll. */
    bool have_pose = false;
    Vector3D fused_aim{0.0f, 0.0f, -1.0f};
    Vector3D fused_up{0.0f, 1.0f, 0.0f};
    Vector3D mahony_i{0.0f, 0.0f, 0.0f};
    std::uint64_t last_fusion_ms = 0;
    bool gyro_bias_ready = false;
    Vector3D gyro_bias_dps{0.0f, 0.0f, 0.0f};
    int gyro_bias_samples = 0;
    int still_frames = 0;
    bool pad_moving = false;
    bool fusion_logged = false;

    void ResetFusion()
    {
        have_pose = false;
        fused_aim = {0.0f, 0.0f, -1.0f};
        fused_up = {0.0f, 1.0f, 0.0f};
        mahony_i = {0.0f, 0.0f, 0.0f};
        last_fusion_ms = 0;
        gyro_bias_ready = false;
        gyro_bias_dps = {0.0f, 0.0f, 0.0f};
        gyro_bias_samples = 0;
        still_frames = 0;
        pad_moving = false;
        fusion_logged = false;
    }

#if defined(CONTROLLER_TILT_HAS_HIDAPI)
    hid_device* device = nullptr;
#endif

    void CloseDevice()
    {
#if defined(CONTROLLER_TILT_HAS_HIDAPI)
        if(device)
        {
            hid_close(device);
            device = nullptr;
        }
#endif
        is_bluetooth = false;
        enhanced_logged = false;
        limited_bt_logged = false;
        last_enhanced_request_ms = 0;
        ResetFusion();
    }

    /** DualSense over BT stays on limited 0x01 until calibration feature 0x05 is read. */
    void RequestBluetoothEnhancedReports()
    {
#if defined(CONTROLLER_TILT_HAS_HIDAPI)
        if(!device || !is_bluetooth)
        {
            return;
        }
        const std::uint64_t now = NowMs();
        if(last_enhanced_request_ms != 0 && (now - last_enhanced_request_ms) < 1500u)
        {
            return;
        }
        last_enhanced_request_ms = now;

        unsigned char calib[64];
        std::memset(calib, 0, sizeof(calib));
        calib[0] = kFeatureCalibrationId;
        const int got = hid_get_feature_report(device, calib, kFeatureCalibrationSize);
        if(got >= 1 && !enhanced_logged)
        {
            LOG_INFO("[3DSpatial] ControllerTilt: DualSense BT enhanced reports requested "
                     "(feature 0x05, %d bytes)",
                     got);
            enhanced_logged = true;
        }
        else if(got < 1 && !limited_bt_logged)
        {
            LOG_WARNING("[3DSpatial] ControllerTilt: DualSense BT calibration feature failed — "
                        "IMU needs enhanced report 0x31 (got %d)",
                        got);
            limited_bt_logged = true;
        }
#endif
    }

    bool EnsureDevice()
    {
#if defined(CONTROLLER_TILT_HAS_HIDAPI)
        if(device)
        {
            return true;
        }

        hid_device_info* list = hid_enumerate(kSonyVid, 0);
        hid_device_info* it = list;
        const char* open_path = nullptr;
        bool path_is_bt = false;

        // Prefer Bluetooth when present (same report layout Win/Linux once enhanced).
        while(it)
        {
            if(it->product_id == kDualSensePid || it->product_id == kDualSenseEdgePid)
            {
                const bool bt = DualSenseInfoIsBluetooth(it);
                if(!open_path || (bt && !path_is_bt))
                {
                    open_path = it->path;
                    path_is_bt = bt;
                }
                if(bt)
                {
                    break;
                }
            }
            it = it->next;
        }

        if(open_path)
        {
            device = hid_open_path(open_path);
            is_bluetooth = path_is_bt;
        }
        hid_free_enumeration(list);

        if(!device)
        {
            device = hid_open(kSonyVid, kDualSensePid, nullptr);
            if(!device)
            {
                device = hid_open(kSonyVid, kDualSenseEdgePid, nullptr);
            }
            is_bluetooth = false;
        }

        if(!device)
        {
            if(!open_failed_logged)
            {
                LOG_WARNING("[3DSpatial] ControllerTilt: DualSense HID open failed "
                            "(another app may hold the device, or Linux hidraw permissions)");
                open_failed_logged = true;
            }
            return false;
        }

        hid_set_nonblocking(device, 1);
        if(!open_ok_logged)
        {
            LOG_INFO("[3DSpatial] ControllerTilt: DualSense opened (%s) for IMU torch",
                     is_bluetooth ? "Bluetooth" : "USB");
            open_ok_logged = true;
        }
        open_failed_logged = false;

        if(is_bluetooth)
        {
            RequestBluetoothEnhancedReports();
        }
        return true;
#else
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
#if defined(CONTROLLER_TILT_HAS_HIDAPI)
        if(!EnsureDevice())
        {
            QMutexLocker lock(&mutex);
            sample.valid = false;
            sample.kind = ControllerTiltDeviceKind::None;
            return;
        }

        if(is_bluetooth)
        {
            RequestBluetoothEnhancedReports();
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

        for(;;)
        {
            const int got = hid_read(device, buf, sizeof(buf));
            if(got <= 0)
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

            sum_ax += (float)ReadI16LE(buf + accel_off) / kAccelLsbPerG;
            sum_ay += (float)ReadI16LE(buf + accel_off + 2) / kAccelLsbPerG;
            sum_az += (float)ReadI16LE(buf + accel_off + 4) / kAccelLsbPerG;
            sum_gx += (float)ReadI16LE(buf + gyro_off) / kGyroLsbPerDegS;
            sum_gy += (float)ReadI16LE(buf + gyro_off + 2) / kGyroLsbPerDegS;
            sum_gz += (float)ReadI16LE(buf + gyro_off + 4) / kGyroLsbPerDegS;
            ++packet_count;
        }

        if(packet_count <= 0)
        {
            if(saw_limited_bt)
            {
                if(!limited_bt_logged)
                {
                    LOG_WARNING("[3DSpatial] ControllerTilt: DualSense BT still on limited report 0x01 "
                                "(no IMU). Waiting for enhanced 0x31 after calibration request.");
                    limited_bt_logged = true;
                }
                RequestBluetoothEnhancedReports();
            }
            return;
        }

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
                    LOG_INFO("[3DSpatial] ControllerTilt: DualSense gyro bias ready "
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

        // Idle noise floor after bias subtract.
        constexpr float k_gyro_deadzone_dps = 0.8f;
        gx_dps = DeadzoneScalar(gx_dps, k_gyro_deadzone_dps);
        gy_dps = DeadzoneScalar(gy_dps, k_gyro_deadzone_dps);
        gz_dps = DeadzoneScalar(gz_dps, k_gyro_deadzone_dps);

        // Real-torch behaviour: nothing moves the beam unless the pad is actually
        // turning. Detect rotation with hysteresis (enter high, exit low) so a still
        // pad is fully frozen — no gyro, no gravity nudging — and there is no flicker
        // at the threshold. When frozen the pose is held exactly; only real rotation
        // (or the pod being picked up / tilted, |a| off 1g) wakes the filter.
        const float gyro_mag_dz =
            std::sqrt(gx_dps * gx_dps + gy_dps * gy_dps + gz_dps * gz_dps);
        const float accel_dev = std::fabs(accel_mag - 1.0f);
        constexpr float k_move_enter_dps = 3.0f;
        constexpr float k_move_exit_dps = 1.2f;
        constexpr float k_move_accel_g = 0.06f; // translation / tilt-in-hand wakes it
        if(gyro_mag_dz > k_move_enter_dps || accel_dev > k_move_accel_g)
        {
            pad_moving = true;
        }
        else if(gyro_mag_dz < k_move_exit_dps && accel_dev < k_move_accel_g)
        {
            pad_moving = false;
        }

        if(!pad_moving)
        {
            gx_dps = 0.0f;
            gy_dps = 0.0f;
            gz_dps = 0.0f;
        }

        const std::uint64_t now = NowMs();
        float dt = 0.0f;
        if(last_fusion_ms != 0 && now > last_fusion_ms)
        {
            dt = (float)(now - last_fusion_ms) * 0.001f;
        }
        if(dt < 0.001f || dt > 0.12f)
        {
            dt = 1.0f / 60.0f;
        }
        last_fusion_ms = now;

        if(!have_pose)
        {
            ResetRestBasis(&fused_aim, &fused_up);
            have_pose = true;
        }
        else if(gyro_bias_ready && pad_moving)
        {
            // Only touch the pose while the pad is actually moving. A still pad is
            // frozen: gyro AND gravity are skipped, so the torch holds dead still.
            IntegrateRigidImu(
                gx_dps * kDegToRad,
                gy_dps * kDegToRad,
                gz_dps * kDegToRad,
                ax,
                ay,
                az,
                dt,
                &mahony_i,
                &fused_aim,
                &fused_up);
        }

        if(!fusion_logged)
        {
            LOG_INFO("[3DSpatial] ControllerTilt: DualSense rigid IMU "
                     "(torch = pad forward; frozen when still, follows only real motion)");
            fusion_logged = true;
        }

        ControllerTiltSample next{};
        next.kind = ControllerTiltDeviceKind::DualSense;
        next.valid = true;
        next.accel = Normalize3({ax, ay, az}); // debug only
        next.gyro_dps = {gx_dps, gy_dps, gz_dps};
        next.aim = fused_aim;
        next.up = fused_up;
        next.timestamp_ms = now;

        QMutexLocker lock(&mutex);
        sample = next;
#else
        QMutexLocker lock(&mutex);
        sample.valid = false;
        sample.kind = ControllerTiltDeviceKind::None;
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
#if defined(CONTROLLER_TILT_HAS_HIDAPI)
    hid_exit();
#endif
    delete impl_;
    impl_ = nullptr;
}

void ControllerTiltManager::start()
{
    if(!impl_ || impl_->running)
    {
        return;
    }
    impl_->running = true;
    impl_->open_failed_logged = false;
    impl_->open_ok_logged = false;
    impl_->enhanced_logged = false;
    impl_->limited_bt_logged = false;
    impl_->last_enhanced_request_ms = 0;
    impl_->ResetFusion();
}

void ControllerTiltManager::stop()
{
    if(!impl_)
    {
        return;
    }
    impl_->running = false;
    impl_->CloseDevice();
    QMutexLocker lock(&impl_->mutex);
    impl_->sample = ControllerTiltSample{};
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

void ControllerTiltManager::RecenterHeading()
{
    if(!impl_)
    {
        return;
    }
    QMutexLocker lock(&impl_->mutex);
    impl_->gyro_bias_ready = false;
    impl_->gyro_bias_dps = {0.0f, 0.0f, 0.0f};
    impl_->gyro_bias_samples = 0;
    impl_->mahony_i = {0.0f, 0.0f, 0.0f};
    impl_->still_frames = 0;
    ResetRestBasis(&impl_->fused_aim, &impl_->fused_up);
    impl_->have_pose = true;
    impl_->sample.aim = impl_->fused_aim;
    impl_->sample.up = impl_->fused_up;
    LOG_INFO("[3DSpatial] ControllerTilt: pose recentered (front-wall home + bias resampling)");
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
