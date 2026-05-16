// SPDX-License-Identifier: GPL-2.0-only

#ifndef GEOMETRY3DUTILS_H
#define GEOMETRY3DUTILS_H

#include "LEDPosition3D.h"
#include "DisplayPlane3D.h"
#include "GridSpaceUtils.h"
#include <algorithm>
#include <cmath>

namespace Geometry3D
{
    struct PlaneProjection
    {
        float   u;
        float   v;
        float   distance;
        bool    is_valid;
    };

    inline void ComputeRotationMatrix(const Rotation3D& rotation_deg, float matrix[9])
    {
        const float deg_to_rad = 3.14159265359f / 180.0f;
        float rx = rotation_deg.x * deg_to_rad;
        float ry = rotation_deg.y * deg_to_rad;
        float rz = rotation_deg.z * deg_to_rad;

        float cx = cosf(rx), sx = sinf(rx);
        float cy = cosf(ry), sy = sinf(ry);
        float cz = cosf(rz), sz = sinf(rz);

        matrix[0] = cy * cz;
        matrix[1] = -cy * sz;
        matrix[2] = sy;

        matrix[3] = cx * sz + sx * sy * cz;
        matrix[4] = cx * cz - sx * sy * sz;
        matrix[5] = -sx * cy;

        matrix[6] = sx * sz - cx * sy * cz;
        matrix[7] = sx * cz + cx * sy * sz;
        matrix[8] = cx * cy;
    }

    inline Vector3D RotateVector(const Vector3D& v, const float matrix[9])
    {
        Vector3D result;
        result.x = matrix[0] * v.x + matrix[1] * v.y + matrix[2] * v.z;
        result.y = matrix[3] * v.x + matrix[4] * v.y + matrix[5] * v.z;
        result.z = matrix[6] * v.x + matrix[7] * v.y + matrix[8] * v.z;
        return result;
    }

    inline float ComputeFalloff(float distance, float max_range, float feather_percent = 30.0f)
    {
        if (max_range <= 0.0f)
        {
            return 1.0f;
        }

        float feather_width = max_range * (feather_percent / 100.0f);
        float core_range = max_range - feather_width;

        if (distance <= core_range)
        {
            return 1.0f;
        }

        if (distance >= max_range)
        {
            return 0.0f;
        }

        float t = (distance - core_range) / feather_width;
        t = fmaxf(0.0f, fminf(1.0f, t));

        float fade = 1.0f - (t * t * (3.0f - 2.0f * t));

        return fade;
    }

    inline float RadialCornerQuadWeightTL(float u, float v)
    {
        if(u > 0.5f || v > 0.5f) return 0.f;
        return (1.f - 2.f * u) * (1.f - 2.f * v);
    }
    inline float RadialCornerQuadWeightTR(float u, float v)
    {
        if(u < 0.5f || v > 0.5f) return 0.f;
        return (2.f * u - 1.f) * (1.f - 2.f * v);
    }
    inline float RadialCornerQuadWeightBL(float u, float v)
    {
        if(u > 0.5f || v < 0.5f) return 0.f;
        return (1.f - 2.f * u) * (2.f * v - 1.f);
    }
    inline float RadialCornerQuadWeightBR(float u, float v)
    {
        if(u < 0.5f || v < 0.5f) return 0.f;
        return (2.f * u - 1.f) * (2.f * v - 1.f);
    }

    inline void ApplyRadialCornerMapping01(float& u, float& v,
                                          float expansion_pct,
                                          float bias_tl_pct, float bias_tr_pct, float bias_bl_pct, float bias_br_pct)
    {
        const bool idle = (expansion_pct > -0.05f && expansion_pct < 0.05f &&
                           bias_tl_pct > -0.05f && bias_tl_pct < 0.05f &&
                           bias_tr_pct > -0.05f && bias_tr_pct < 0.05f &&
                           bias_bl_pct > -0.05f && bias_bl_pct < 0.05f &&
                           bias_br_pct > -0.05f && bias_br_pct < 0.05f);
        if(idle)
        {
            return;
        }

        float du = u - 0.5f;
        float dv = v - 0.5f;
        const float wexp = 4.0f * std::fabs(du) * std::fabs(dv);
        const float g = expansion_pct / 100.0f;
        u = 0.5f + du * (1.0f + wexp * g);
        v = 0.5f + dv * (1.0f + wexp * g);

        const float k = 0.28f / 100.0f;
        const float wtl = RadialCornerQuadWeightTL(u, v);
        const float wtr = RadialCornerQuadWeightTR(u, v);
        const float wbl = RadialCornerQuadWeightBL(u, v);
        const float wbr = RadialCornerQuadWeightBR(u, v);

        u += k * (-wtl * bias_tl_pct + wtr * bias_tr_pct - wbl * bias_bl_pct + wbr * bias_br_pct);
        v += k * (-wtl * bias_tl_pct - wtr * bias_tr_pct + wbl * bias_bl_pct + wbr * bias_br_pct);

        u = std::clamp(u, 0.0f, 1.0f);
        v = std::clamp(v, 0.0f, 1.0f);
    }

    inline void ApplyUVRotationDegrees01(float& u, float& v, float roll_deg)
    {
        if(roll_deg > -0.005f && roll_deg < 0.005f)
        {
            return;
        }
        const float rad = roll_deg * 3.14159265359f / 180.0f;
        const float c = std::cos(rad);
        const float s = std::sin(rad);
        const float du = u - 0.5f;
        const float dv = v - 0.5f;
        u = std::clamp(0.5f + c * du - s * dv, 0.0f, 1.0f);
        v = std::clamp(0.5f + s * du + c * dv, 0.0f, 1.0f);
    }

    inline PlaneProjection SpatialMapToScreen(const Vector3D& led_position, const DisplayPlane3D& plane, float edge_zone_depth = 0.15f, const Vector3D* user_position = nullptr, float grid_scale_mm = 10.0f)
    {
        PlaneProjection result;
        result.is_valid = false;
        result.u = 0.5f;
        result.v = 0.5f;
        result.distance = 0.0f;

        const Transform3D& transform = plane.GetTransform();
        const Vector3D& ref = user_position ? *user_position : transform.position;

        Vector3D ref_to_led;
        ref_to_led.x = led_position.x - ref.x;
        ref_to_led.y = led_position.y - ref.y;
        ref_to_led.z = led_position.z - ref.z;
        result.distance = sqrtf(ref_to_led.x * ref_to_led.x + ref_to_led.y * ref_to_led.y + ref_to_led.z * ref_to_led.z);
        result.distance = GridUnitsToMM(result.distance, grid_scale_mm);

        float rotation_matrix[9];
        ComputeRotationMatrix(transform.rotation, rotation_matrix);

        Vector3D plane_right  = { rotation_matrix[0], rotation_matrix[3], rotation_matrix[6] };
        Vector3D plane_up     = { rotation_matrix[1], rotation_matrix[4], rotation_matrix[7] };

        float len_sq = ref_to_led.x * ref_to_led.x + ref_to_led.y * ref_to_led.y + ref_to_led.z * ref_to_led.z;
        float len = sqrtf(len_sq);
        const float eps = 1e-6f;
        if (len < eps)
        {
            result.u = 0.5f;
            result.v = 0.5f;
            result.is_valid = true;
            return result;
        }
        float inv_len = 1.0f / len;
        float dir_x = ref_to_led.x * inv_len;
        float dir_y = ref_to_led.y * inv_len;
        float dir_z = ref_to_led.z * inv_len;
        float dir_right = dir_x * plane_right.x + dir_y * plane_right.y + dir_z * plane_right.z;
        float dir_up    = dir_x * plane_up.x    + dir_y * plane_up.y    + dir_z * plane_up.z;
        static constexpr float kDirectionalMapBasisRotationDeg = -25.5f;
        const float calib_rad = kDirectionalMapBasisRotationDeg * 3.14159265359f / 180.0f;
        const float cc = std::cos(calib_rad);
        const float ss = std::sin(calib_rad);
        const float dir_r_rot = dir_right * cc - dir_up * ss;
        const float dir_u_rot = dir_right * ss + dir_up * cc;
        static const float inv_half_sqrt2 = 1.414213562373095f;
        result.u = 0.5f + 0.5f * dir_r_rot * inv_half_sqrt2;
        result.v = 0.5f + 0.5f * dir_u_rot * inv_half_sqrt2;

        if (result.u < 0.0f) result.u = 0.0f;
        if (result.u > 1.0f) result.u = 1.0f;
        if (result.v < 0.0f) result.v = 0.0f;
        if (result.v > 1.0f) result.v = 1.0f;

        float inset = edge_zone_depth;
        if(inset > 0.0f)
        {
            inset = std::clamp(inset, 0.0f, 0.49f);
            float min_uv = inset;
            float max_uv = 1.0f - inset;
            float span = std::max(0.0f, max_uv - min_uv);
            if(span > 0.0f)
            {
                result.u = min_uv + span * result.u;
                result.v = min_uv + span * result.v;
            }
            else
            {
                result.u = result.v = 0.5f;
            }
        }

        if (std::isnan(result.u) || std::isnan(result.v) || !std::isfinite(result.u) || !std::isfinite(result.v))
        {
            result.is_valid = false;
            return result;
        }
        result.is_valid = true;
        return result;
    }

    inline void QuantizeMediaUV01(float& u, float& v, int w, int h, unsigned int resolution_pct)
    {
        if(resolution_pct >= 100u)
        {
            return;
        }
        const float q = resolution_pct / 100.0f;
        const float steps_u = std::max(2.0f, 4.0f + q * q * (float)(std::max(2, w) - 4));
        const float steps_v = std::max(2.0f, 4.0f + q * q * (float)(std::max(2, h) - 4));
        u = std::floor(u * steps_u) / steps_u;
        v = std::floor(v * steps_v) / steps_v;
    }

    inline void QuantizeNormalizedAxis01(float& t, unsigned int resolution_pct, int virtual_cells = 128)
    {
        if(resolution_pct >= 100u)
        {
            return;
        }
        const float q = resolution_pct / 100.0f;
        const float steps = std::max(2.0f, 4.0f + q * q * (float)(std::max(2, virtual_cells) - 4));
        t = std::floor(t * steps) / steps;
    }

    inline RGBColor SampleFrame(const uint8_t* frame_data, int frame_width, int frame_height,
                               float u, float v, bool use_bilinear = true)
    {
        if (!frame_data || frame_width <= 0 || frame_height <= 0)
        {
            return ToRGBColor(0, 0, 0);
        }
        if (u < 0.0f) u = 0.0f;
        if (u > 1.0f) u = 1.0f;
        if (v < 0.0f) v = 0.0f;
        if (v > 1.0f) v = 1.0f;

        float x = u * (frame_width - 1);
        float y = v * (frame_height - 1);

        if (!use_bilinear)
        {
            int ix = (int)(x + 0.5f);
            int iy = (int)(y + 0.5f);
            int index = (iy * frame_width + ix) * 4;

            uint8_t r = frame_data[index + 0];
            uint8_t g = frame_data[index + 1];
            uint8_t b = frame_data[index + 2];
            return ToRGBColor(r, g, b);
        }
        else
        {
            int x0 = (int)x;
            int y0 = (int)y;
            int x1 = (x0 + 1 < frame_width) ? x0 + 1 : x0;
            int y1 = (y0 + 1 < frame_height) ? y0 + 1 : y0;

            float fx = x - x0;
            float fy = y - y0;

            int idx00 = (y0 * frame_width + x0) * 4;
            int idx10 = (y0 * frame_width + x1) * 4;
            int idx01 = (y1 * frame_width + x0) * 4;
            int idx11 = (y1 * frame_width + x1) * 4;

            float r00 = frame_data[idx00 + 0];
            float g00 = frame_data[idx00 + 1];
            float b00 = frame_data[idx00 + 2];

            float r10 = frame_data[idx10 + 0];
            float g10 = frame_data[idx10 + 1];
            float b10 = frame_data[idx10 + 2];

            float r01 = frame_data[idx01 + 0];
            float g01 = frame_data[idx01 + 1];
            float b01 = frame_data[idx01 + 2];

            float r11 = frame_data[idx11 + 0];
            float g11 = frame_data[idx11 + 1];
            float b11 = frame_data[idx11 + 2];

            float r = r00 * (1 - fx) * (1 - fy) + r10 * fx * (1 - fy) + r01 * (1 - fx) * fy + r11 * fx * fy;
            float g = g00 * (1 - fx) * (1 - fy) + g10 * fx * (1 - fy) + g01 * (1 - fx) * fy + g11 * fx * fy;
            float b = b00 * (1 - fx) * (1 - fy) + b10 * fx * (1 - fy) + b01 * (1 - fx) * fy + b11 * fx * fy;

            return ToRGBColor((uint8_t)(r + 0.5f), (uint8_t)(g + 0.5f), (uint8_t)(b + 0.5f));
        }
    }

}

#endif
