// SPDX-License-Identifier: GPL-2.0-only
//
// Madgwick IMU (6-axis) orientation filter — algorithm from Sebastian Madgwick /
// x-io open AHRS (see Documentation/dualsense-torch-notes.md). Gyro in rad/s;
// accel in any consistent units (normalized inside). No magnetometer.

#ifndef MADGWICKIMU_H
#define MADGWICKIMU_H

#include <cmath>

class MadgwickImu
{
public:
    /** Default ~ahrs IMU gain; raise toward 0.1 for faster settle / more lag. */
    explicit MadgwickImu(float beta = 0.033f)
        : beta_(beta)
    {
        Reset();
    }

    void SetBeta(float beta)
    {
        beta_ = beta;
    }

    float Beta() const
    {
        return beta_;
    }

    void Reset()
    {
        q0_ = 1.0f;
        q1_ = 0.0f;
        q2_ = 0.0f;
        q3_ = 0.0f;
    }

    /** Set unit quaternion (w, x, y, z). Non-unit input is normalized. */
    void SetQuaternion(float w, float x, float y, float z)
    {
        const float n2 = w * w + x * x + y * y + z * z;
        if(n2 < 1e-12f)
        {
            Reset();
            return;
        }
        const float inv = 1.0f / std::sqrt(n2);
        q0_ = w * inv;
        q1_ = x * inv;
        q2_ = y * inv;
        q3_ = z * inv;
    }

    void GetQuaternion(float* w, float* x, float* y, float* z) const
    {
        if(w) { *w = q0_; }
        if(x) { *x = q1_; }
        if(y) { *y = q2_; }
        if(z) { *z = q3_; }
    }

    /**
     * One Madgwick IMU step.
     * @param gx,gy,gz  angular rate (rad/s) in the filter sensor frame
     * @param ax,ay,az  accelerometer (any scale; zero vector skips accel correction)
     * @param dt        seconds since last sample (must be > 0)
     */
    void UpdateIMU(float gx, float gy, float gz, float ax, float ay, float az, float dt)
    {
        if(!(dt > 0.0f) || !std::isfinite(dt))
        {
            return;
        }

        float q0 = q0_;
        float q1 = q1_;
        float q2 = q2_;
        float q3 = q3_;

        // Rate of change of quaternion from gyroscope
        float qDot1 = 0.5f * (-q1 * gx - q2 * gy - q3 * gz);
        float qDot2 = 0.5f * (q0 * gx + q2 * gz - q3 * gy);
        float qDot3 = 0.5f * (q0 * gy - q1 * gz + q3 * gx);
        float qDot4 = 0.5f * (q0 * gz + q1 * gy - q2 * gx);

        // Compute feedback only if accelerometer measurement valid
        if(!((ax == 0.0f) && (ay == 0.0f) && (az == 0.0f)))
        {
            float recipNorm = InvSqrt(ax * ax + ay * ay + az * az);
            ax *= recipNorm;
            ay *= recipNorm;
            az *= recipNorm;

            const float _2q0 = 2.0f * q0;
            const float _2q1 = 2.0f * q1;
            const float _2q2 = 2.0f * q2;
            const float _2q3 = 2.0f * q3;
            const float _4q0 = 4.0f * q0;
            const float _4q1 = 4.0f * q1;
            const float _4q2 = 4.0f * q2;
            const float _8q1 = 8.0f * q1;
            const float _8q2 = 8.0f * q2;
            const float q0q0 = q0 * q0;
            const float q1q1 = q1 * q1;
            const float q2q2 = q2 * q2;
            const float q3q3 = q3 * q3;

            // Gradient descent corrective step
            float s0 = _4q0 * q2q2 + _2q2 * ax + _4q0 * q1q1 - _2q1 * ay;
            float s1 = _4q1 * q3q3 - _2q3 * ax + 4.0f * q0q0 * q1 - _2q0 * ay - _4q1
                       + _8q1 * q1q1 + _8q1 * q2q2 + _4q1 * az;
            float s2 = 4.0f * q0q0 * q2 + _2q0 * ax + _4q2 * q3q3 - _2q3 * ay - _4q2
                       + _8q2 * q1q1 + _8q2 * q2q2 + _4q2 * az;
            float s3 = 4.0f * q1q1 * q3 - _2q1 * ax + 4.0f * q2q2 * q3 - _2q2 * ay;
            recipNorm = InvSqrt(s0 * s0 + s1 * s1 + s2 * s2 + s3 * s3);
            s0 *= recipNorm;
            s1 *= recipNorm;
            s2 *= recipNorm;
            s3 *= recipNorm;

            qDot1 -= beta_ * s0;
            qDot2 -= beta_ * s1;
            qDot3 -= beta_ * s2;
            qDot4 -= beta_ * s3;
        }

        q0 += qDot1 * dt;
        q1 += qDot2 * dt;
        q2 += qDot3 * dt;
        q3 += qDot4 * dt;

        const float recipNorm = InvSqrt(q0 * q0 + q1 * q1 + q2 * q2 + q3 * q3);
        q0_ = q0 * recipNorm;
        q1_ = q1 * recipNorm;
        q2_ = q2 * recipNorm;
        q3_ = q3 * recipNorm;
    }

    /** Rotate a vector from filter/sensor frame into earth/nav frame. */
    void RotateBodyToEarth(float bx, float by, float bz, float* ex, float* ey, float* ez) const
    {
        // R maps body→earth for Hamilton quat (w,x,y,z)
        const float q0 = q0_;
        const float q1 = q1_;
        const float q2 = q2_;
        const float q3 = q3_;
        const float r00 = 1.0f - 2.0f * (q2 * q2 + q3 * q3);
        const float r01 = 2.0f * (q1 * q2 - q0 * q3);
        const float r02 = 2.0f * (q1 * q3 + q0 * q2);
        const float r10 = 2.0f * (q1 * q2 + q0 * q3);
        const float r11 = 1.0f - 2.0f * (q1 * q1 + q3 * q3);
        const float r12 = 2.0f * (q2 * q3 - q0 * q1);
        const float r20 = 2.0f * (q1 * q3 - q0 * q2);
        const float r21 = 2.0f * (q2 * q3 + q0 * q1);
        const float r22 = 1.0f - 2.0f * (q1 * q1 + q2 * q2);
        if(ex) { *ex = r00 * bx + r01 * by + r02 * bz; }
        if(ey) { *ey = r10 * bx + r11 * by + r12 * bz; }
        if(ez) { *ez = r20 * bx + r21 * by + r22 * bz; }
    }

private:
    static float InvSqrt(float x)
    {
        if(!(x > 0.0f) || !std::isfinite(x))
        {
            return 0.0f;
        }
        return 1.0f / std::sqrt(x);
    }

    float beta_;
    float q0_;
    float q1_;
    float q2_;
    float q3_;
};

#endif
