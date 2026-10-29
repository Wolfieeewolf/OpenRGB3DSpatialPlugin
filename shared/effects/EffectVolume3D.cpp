// SPDX-License-Identifier: GPL-2.0-only

#include "EffectVolume3D.h"
#include "Geometry3DUtils.h"

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

/** Half-extents that cover the grid AABB from an origin (Scale 100 = fill grid). */
Vector3D CoverHalfExtentsFromOrigin(const Vector3D& origin, const GridContext3D& grid)
{
    return {
        std::max(1e-4f, std::max(origin.x - grid.min_x, grid.max_x - origin.x)),
        std::max(1e-4f, std::max(origin.y - grid.min_y, grid.max_y - origin.y)),
        std::max(1e-4f, std::max(origin.z - grid.min_z, grid.max_z - origin.z))
    };
}

} // namespace

Vector3D EffectVolume3D::HalfExtentsFromEffect(const SpatialEffect3D& effect,
                                               const GridContext3D& grid)
{
    /* Same half-extents as SpatialEffect3D::GetVolumeHalfExtents (eval + gizmo stay flush). */
    float hx = 0.05f, hy = 0.05f, hz = 0.05f;
    effect.GetVolumeHalfExtents(grid, hx, hy, hz);
    return {std::max(hx, 0.05f), std::max(hy, 0.05f), std::max(hz, 0.05f)};
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
    vol.shape = effect.GetVolumeShape();
    return vol;
}

void EffectVolume3D::ApplyToEffect(SpatialEffect3D& effect, const GridContext3D& grid) const
{
    /* Absolute origin while unlocked — render must not re-derive from Spatial Anchor. */
    effect.SetVolumeWorldOrigin(transform.position.x, transform.position.y, transform.position.z);

    /* Also keep offsets relative to the current anchor for lock / slider UI. */
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
    effect.SetVolumeShape(shape);

    /* Occupancy relative to cover extents from this origin (Scale 100 = fill grid).
       Axis % must use the *clamped* occupancy or GetVolumeHalfExtents will not
       round-trip the gizmo scale (LEDs and wireframe drift apart). */
    const Vector3D cover = CoverHalfExtentsFromOrigin(transform.position, grid);
    const float rx = transform.scale.x / cover.x;
    const float ry = transform.scale.y / cover.y;
    const float rz = transform.scale.z / cover.z;
    const float occ_raw = std::max(0.05f, std::max(rx, std::max(ry, rz)));
    const unsigned int occ_pct =
        (unsigned int)std::clamp((int)std::lround(occ_raw * 100.0f), 1, 200);
    effect.SetOccupancyScalePercent(occ_pct);
    const float occ = (float)occ_pct / 100.0f;
    const float base_x = cover.x * occ;
    const float base_y = cover.y * occ;
    const float base_z = cover.z * occ;
    effect.SetAxisScalePercents(
        ClampAxisScalePercent(transform.scale.x, base_x),
        ClampAxisScalePercent(transform.scale.y, base_y),
        ClampAxisScalePercent(transform.scale.z, base_z));
}

Vector3D EffectVolume3D::LocalToWorld(float lx, float ly, float lz) const
{
    lx += shear_xy * ly + shear_xz * lz;
    ly += shear_yz * lz;
    /* Same Euler matrix as controllers / VolumeWorldDeltaToLocal. */
    float m[9];
    Geometry3D::ComputeRotationMatrix(transform.rotation, m);
    const Vector3D local = Geometry3D::RotateVector({lx, ly, lz}, m);
    return {
        transform.position.x + local.x,
        transform.position.y + local.y,
        transform.position.z + local.z
    };
}

void EffectVolume3D::GetCorners(Vector3D out_corners[8]) const
{
    const float sx = transform.scale.x;
    const float sy = transform.scale.y;
    const float sz = transform.scale.z;
    int i = 0;
    for(int ix = -1; ix <= 1; ix += 2)
    {
        for(int iy = -1; iy <= 1; iy += 2)
        {
            for(int iz = -1; iz <= 1; iz += 2)
            {
                out_corners[i++] = LocalToWorld((float)ix * sx, (float)iy * sy, (float)iz * sz);
            }
        }
    }
}
