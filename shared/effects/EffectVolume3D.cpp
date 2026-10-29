// SPDX-License-Identifier: GPL-2.0-only

#include "EffectVolume3D.h"

#include <algorithm>
#include <cmath>

namespace
{

float ClampOffsetPercent(float v)
{
    return std::clamp(v, -100.0f, 100.0f);
}

unsigned int ClampAxisScalePercent(float half_extent, float base_half)
{
    const float base = std::max(1e-4f, base_half);
    const float pct = (half_extent / base) * 100.0f;
    return (unsigned int)std::clamp((int)std::lround(pct), 1, 400);
}

Vector3D RotateLocalToWorld(float lx, float ly, float lz,
                            float yaw_deg, float pitch_deg, float roll_deg)
{
    const float yaw = yaw_deg * 3.14159265359f / 180.0f;
    const float pitch = pitch_deg * 3.14159265359f / 180.0f;
    const float roll = roll_deg * 3.14159265359f / 180.0f;

    const float cy = std::cos(yaw);
    const float sy = std::sin(yaw);
    float nx = lx * cy - lz * sy;
    float nz = lx * sy + lz * cy;
    float ny = ly;

    const float cp = std::cos(pitch);
    const float sp = std::sin(pitch);
    float py = ny * cp - nz * sp;
    float pz = ny * sp + nz * cp;
    float px = nx;

    const float cr = std::cos(roll);
    const float sr = std::sin(roll);
    return {px * cr - py * sr, px * sr + py * cr, pz};
}

} // namespace

Vector3D EffectVolume3D::HalfExtentsFromEffect(const SpatialEffect3D& effect,
                                               const GridContext3D& grid)
{
    const float scale_pct = std::max(0.01f, effect.GetOccupancyScale());
    Vector3D half;
    half.x = (grid.width * 0.5f) * scale_pct * ((float)effect.GetAxisScaleX() / 100.0f);
    half.y = (grid.height * 0.5f) * scale_pct * ((float)effect.GetAxisScaleY() / 100.0f);
    half.z = (grid.depth * 0.5f) * scale_pct * ((float)effect.GetAxisScaleZ() / 100.0f);
    half.x = std::max(half.x, 0.05f);
    half.y = std::max(half.y, 0.05f);
    half.z = std::max(half.z, 0.05f);
    return half;
}

EffectVolume3D EffectVolume3D::FromEffect(const SpatialEffect3D& effect,
                                          const GridContext3D& grid,
                                          int stack_index)
{
    EffectVolume3D vol;
    vol.stack_index = stack_index;
    const Vector3D origin = effect.GetEffectOriginGrid(grid);
    vol.transform.position = origin;
    vol.transform.rotation.x = effect.GetRotationPitch();
    vol.transform.rotation.y = effect.GetRotationYaw();
    vol.transform.rotation.z = effect.GetRotationRoll();
    vol.transform.scale = HalfExtentsFromEffect(effect, grid);
    vol.shear_xy = effect.GetShearXY();
    vol.shear_xz = effect.GetShearXZ();
    vol.shear_yz = effect.GetShearYZ();
    return vol;
}

void EffectVolume3D::ApplyToEffect(SpatialEffect3D& effect, const GridContext3D& grid) const
{
    effect.SetReferenceMode(REF_MODE_ROOM_CENTER);
    const Vector3D ref = effect.GetReferencePointGrid(grid);
    const float half_w = std::max(1e-4f, grid.width * 0.5f);
    const float half_h = std::max(1e-4f, grid.height * 0.5f);
    const float half_d = std::max(1e-4f, grid.depth * 0.5f);

    effect.SetOffsetPercents(
        (int)std::lround(ClampOffsetPercent(((transform.position.x - ref.x) / half_w) * 100.0f)),
        (int)std::lround(ClampOffsetPercent(((transform.position.y - ref.y) / half_h) * 100.0f)),
        (int)std::lround(ClampOffsetPercent(((transform.position.z - ref.z) / half_d) * 100.0f)));

    effect.SetRotationDegrees(transform.rotation.y, transform.rotation.x, transform.rotation.z);
    effect.SetShear(shear_xy, shear_xz, shear_yz);

    /* Occupancy from the largest relative half-extent; axis % = residual non-uniformity. */
    const float rx = transform.scale.x / half_w;
    const float ry = transform.scale.y / half_h;
    const float rz = transform.scale.z / half_d;
    const float occ = std::max(0.05f, std::max(rx, std::max(ry, rz)));
    effect.SetOccupancyScalePercent((unsigned int)std::clamp((int)std::lround(occ * 100.0f), 1, 200));
    const float base_x = half_w * occ;
    const float base_y = half_h * occ;
    const float base_z = half_d * occ;
    effect.SetAxisScalePercents(
        ClampAxisScalePercent(transform.scale.x, base_x),
        ClampAxisScalePercent(transform.scale.y, base_y),
        ClampAxisScalePercent(transform.scale.z, base_z));
}

void EffectVolume3D::GetCorners(Vector3D out_corners[8]) const
{
    const float sx = transform.scale.x;
    const float sy = transform.scale.y;
    const float sz = transform.scale.z;
    const float yaw = transform.rotation.y;
    const float pitch = transform.rotation.x;
    const float roll = transform.rotation.z;
    int i = 0;
    for(int ix = -1; ix <= 1; ix += 2)
    {
        for(int iy = -1; iy <= 1; iy += 2)
        {
            for(int iz = -1; iz <= 1; iz += 2)
            {
                float lx = (float)ix * sx;
                float ly = (float)iy * sy;
                float lz = (float)iz * sz;
                lx += shear_xy * ly + shear_xz * lz;
                ly += shear_yz * lz;
                Vector3D local = RotateLocalToWorld(lx, ly, lz, yaw, pitch, roll);
                out_corners[i].x = transform.position.x + local.x;
                out_corners[i].y = transform.position.y + local.y;
                out_corners[i].z = transform.position.z + local.z;
                ++i;
            }
        }
    }
}
