// SPDX-License-Identifier: GPL-2.0-only

#include "LEDViewport3D.h"
#include "Gizmo3D.h"
#include "QtCompat.h"
#include "Colors.h"
#include "GridSpaceUtils.h"

#include <cmath>
#include <map>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace
{
bool tryGetViewportGlobalLedIndex(RGBController* controller,
                                  unsigned int zone_idx,
                                  unsigned int led_idx,
                                  unsigned int* global_led_idx)
{
    if(!controller || !global_led_idx)
    {
        return false;
    }
    if(zone_idx >= controller->zones.size())
    {
        return false;
    }
    if(led_idx >= controller->zones[zone_idx].leds_count)
    {
        return false;
    }

    *global_led_idx = controller->zones[zone_idx].start_idx + led_idx;
    return (*global_led_idx < controller->colors.size());
}

void viewportMeshAddLine(GizmoDrawMesh& mesh, float width, const float color[3],
                         float x0, float y0, float z0, float x1, float y1, float z1)
{
    GizmoDrawMesh::LineBatch& batch = mesh.lineBatchForWidth(width);
    batch.positions.insert(batch.positions.end(), {x0, y0, z0, x1, y1, z1});
    batch.colors.insert(batch.colors.end(), {color[0], color[1], color[2], color[0], color[1], color[2]});
}

void viewportMeshAddTriangle(GizmoDrawMesh& mesh, const float color[3],
                             float x0, float y0, float z0,
                             float x1, float y1, float z1,
                             float x2, float y2, float z2)
{
    mesh.triangle_positions.insert(mesh.triangle_positions.end(), {x0, y0, z0, x1, y1, z1, x2, y2, z2});
    mesh.triangle_colors.insert(mesh.triangle_colors.end(),
                                {color[0], color[1], color[2],
                                 color[0], color[1], color[2],
                                 color[0], color[1], color[2]});
}

void appendControllerBounds(GizmoDrawMesh& mesh, const Vector3D& bmin, const Vector3D& bmax,
                            const float color[3], float line_width)
{
    const float x0 = bmin.x;
    const float y0 = bmin.y;
    const float z0 = bmin.z;
    const float x1 = bmax.x;
    const float y1 = bmax.y;
    const float z1 = bmax.z;

    viewportMeshAddLine(mesh, line_width, color, x0, y0, z0, x1, y0, z0);
    viewportMeshAddLine(mesh, line_width, color, x1, y0, z0, x1, y0, z1);
    viewportMeshAddLine(mesh, line_width, color, x1, y0, z1, x0, y0, z1);
    viewportMeshAddLine(mesh, line_width, color, x0, y0, z1, x0, y0, z0);

    viewportMeshAddLine(mesh, line_width, color, x0, y1, z0, x1, y1, z0);
    viewportMeshAddLine(mesh, line_width, color, x1, y1, z0, x1, y1, z1);
    viewportMeshAddLine(mesh, line_width, color, x1, y1, z1, x0, y1, z1);
    viewportMeshAddLine(mesh, line_width, color, x0, y1, z1, x0, y1, z0);

    viewportMeshAddLine(mesh, line_width, color, x0, y0, z0, x0, y1, z0);
    viewportMeshAddLine(mesh, line_width, color, x1, y0, z0, x1, y1, z0);
    viewportMeshAddLine(mesh, line_width, color, x1, y0, z1, x1, y1, z1);
    viewportMeshAddLine(mesh, line_width, color, x0, y0, z1, x0, y1, z1);
}

void appendControllerTopIndicator(GizmoDrawMesh& mesh, float cx, float cy, float cz, float radius)
{
    const int segments = 8;
    const float green[3] = {0.0f, 1.0f, 0.0f};
    const float red[3] = {1.0f, 0.0f, 0.0f};

    const auto append_lat_range = [&](int lat_start, int lat_end, const float color[3]) {
        for(int i = lat_start; i < lat_end; i++)
        {
            const float lat0 = (float)M_PI * (0.0f + (float)i / segments);
            const float lat1 = (float)M_PI * (0.0f + (float)(i + 1) / segments);
            const float y0 = radius * sinf(lat0);
            const float y1 = radius * sinf(lat1);
            const float r0 = radius * cosf(lat0);
            const float r1 = radius * cosf(lat1);

            for(int j = 0; j < segments; j++)
            {
                const float lng0 = 2.0f * (float)M_PI * (float)j / segments;
                const float lng1 = 2.0f * (float)M_PI * (float)(j + 1) / segments;
                const float x00 = cx + cosf(lng0) * r0;
                const float z00 = cz + sinf(lng0) * r0;
                const float x01 = cx + cosf(lng1) * r0;
                const float z01 = cz + sinf(lng1) * r0;
                const float x10 = cx + cosf(lng0) * r1;
                const float z10 = cz + sinf(lng0) * r1;
                const float x11 = cx + cosf(lng1) * r1;
                const float z11 = cz + sinf(lng1) * r1;

                viewportMeshAddTriangle(mesh, color, x00, cy + y0, z00, x01, cy + y0, z01, x11, cy + y1, z11);
                viewportMeshAddTriangle(mesh, color, x00, cy + y0, z00, x11, cy + y1, z11, x10, cy + y1, z10);
            }
        }
    };

    append_lat_range(0, segments / 2, green);
    append_lat_range(segments / 2, segments, red);
}

void appendLocalSphere(GizmoDrawMesh& mesh, float radius, const float color[3])
{
    const int slices = 16;
    const int stacks = 16;

    for(int i = 0; i < stacks; i++)
    {
        const float lat0 = (float)M_PI * (-0.5f + (float)i / stacks);
        const float lat1 = (float)M_PI * (-0.5f + (float)(i + 1) / stacks);
        const float y0 = radius * sinf(lat0);
        const float y1 = radius * sinf(lat1);
        const float r0 = radius * cosf(lat0);
        const float r1 = radius * cosf(lat1);

        for(int j = 0; j < slices; j++)
        {
            const float lng0 = 2.0f * (float)M_PI * (float)j / slices;
            const float lng1 = 2.0f * (float)M_PI * (float)(j + 1) / slices;
            const float x00 = cosf(lng0) * r0;
            const float z00 = sinf(lng0) * r0;
            const float x01 = cosf(lng1) * r0;
            const float z01 = sinf(lng1) * r0;
            const float x10 = cosf(lng0) * r1;
            const float z10 = sinf(lng0) * r1;
            const float x11 = cosf(lng1) * r1;
            const float z11 = sinf(lng1) * r1;

            viewportMeshAddTriangle(mesh, color, x00, y0, z00, x01, y0, z01, x11, y1, z11);
            viewportMeshAddTriangle(mesh, color, x00, y0, z00, x11, y1, z11, x10, y1, z10);
        }
    }
}

void appendSelectionBox(GizmoDrawMesh& mesh, float box_size, const float color[3], float line_width)
{
    appendControllerBounds(mesh,
                           {-box_size, -box_size, -box_size},
                           {box_size, box_size, box_size},
                           color,
                           line_width);
}

void drawColoredMeshBatches(ViewportRenderer& renderer,
                            const GizmoDrawMesh& mesh,
                            const ViewportMat4& model,
                            float triangle_alpha = 1.0f)
{
    for(const GizmoDrawMesh::LineBatch& batch : mesh.line_batches)
    {
        if(!batch.positions.empty())
        {
            renderer.drawColoredLines(batch.positions, batch.colors, model, batch.width);
        }
    }
    if(!mesh.triangle_positions.empty())
    {
        renderer.drawColoredTriangles(mesh.triangle_positions, mesh.triangle_colors, model, triangle_alpha);
    }
}

void appendWorldQuadTriangles(GizmoDrawMesh& mesh,
                              const Vector3D& c0,
                              const Vector3D& c1,
                              const Vector3D& c2,
                              const Vector3D& c3,
                              const float color[3])
{
    viewportMeshAddTriangle(mesh, color, c0.x, c0.y, c0.z, c1.x, c1.y, c1.z, c2.x, c2.y, c2.z);
    viewportMeshAddTriangle(mesh, color, c0.x, c0.y, c0.z, c2.x, c2.y, c2.z, c3.x, c3.y, c3.z);
}

void appendLocalFlatQuadXY(GizmoDrawMesh& mesh,
                           float x0,
                           float y0,
                           float x1,
                           float y1,
                           float z,
                           const float color[3])
{
    const Vector3D c0 = {x0, y0, z};
    const Vector3D c1 = {x1, y0, z};
    const Vector3D c2 = {x1, y1, z};
    const Vector3D c3 = {x0, y1, z};
    appendWorldQuadTriangles(mesh, c0, c1, c2, c3, color);
}

void appendLocalFlatQuadBorderXY(GizmoDrawMesh& mesh,
                                 float x0,
                                 float y0,
                                 float x1,
                                 float y1,
                                 float z,
                                 const float color[3],
                                 float line_width)
{
    viewportMeshAddLine(mesh, line_width, color, x0, y0, z, x1, y0, z);
    viewportMeshAddLine(mesh, line_width, color, x1, y0, z, x1, y1, z);
    viewportMeshAddLine(mesh, line_width, color, x1, y1, z, x0, y1, z);
    viewportMeshAddLine(mesh, line_width, color, x0, y1, z, x0, y0, z);
}

void appendLightBlockerLayers(GizmoDrawMesh& cell_fill_mesh,
                              GizmoDrawMesh& border_mesh,
                              VirtualController3D* layout,
                              float grid_scale_mm,
                              float border_width)
{
    if(!layout)
    {
        return;
    }

    const std::vector<CustomControllerLightBlocker>& blockers = layout->GetLightBlockers();
    if(blockers.empty())
    {
        return;
    }

    const float cell_fill[3] = {0.22f, 0.14f, 0.29f};
    const float border[3] = {0.47f, 0.27f, 0.63f};

    for(const CustomControllerLightBlocker& blocker : blockers)
    {
        Vector3D local_min{};
        Vector3D local_max{};
        layout->CellLocalBoundsMm(blocker.x, blocker.y, blocker.z, &local_min, &local_max);
        const float x0 = MMToGridUnits(local_min.x, grid_scale_mm);
        const float y0 = MMToGridUnits(local_min.y, grid_scale_mm);
        const float x1 = MMToGridUnits(local_max.x, grid_scale_mm);
        const float y1 = MMToGridUnits(local_max.y, grid_scale_mm);
        const float z_plane = MMToGridUnits(local_max.z, grid_scale_mm);

        appendLocalFlatQuadXY(cell_fill_mesh, x0, y0, x1, y1, z_plane, cell_fill);
        appendLocalFlatQuadBorderXY(border_mesh, x0, y0, x1, y1, z_plane, border, border_width);
    }
}

bool transformsMatch(const Transform3D& a, const Transform3D& b)
{
    return a.position.x == b.position.x && a.position.y == b.position.y && a.position.z == b.position.z &&
           a.rotation.x == b.rotation.x && a.rotation.y == b.rotation.y && a.rotation.z == b.rotation.z &&
           a.scale.x == b.scale.x && a.scale.y == b.scale.y && a.scale.z == b.scale.z;
}

void appendCalibrationPattern(GizmoDrawMesh& mesh, const Vector3D corners[4])
{
    Vector3D center;
    center.x = (corners[0].x + corners[2].x) * 0.5f;
    center.y = (corners[0].y + corners[2].y) * 0.5f;
    center.z = (corners[0].z + corners[2].z) * 0.5f;

    Vector3D mid_bottom, mid_top, mid_left, mid_right;
    mid_bottom.x = (corners[0].x + corners[1].x) * 0.5f;
    mid_bottom.y = (corners[0].y + corners[1].y) * 0.5f;
    mid_bottom.z = (corners[0].z + corners[1].z) * 0.5f;

    mid_top.x = (corners[2].x + corners[3].x) * 0.5f;
    mid_top.y = (corners[2].y + corners[3].y) * 0.5f;
    mid_top.z = (corners[2].z + corners[3].z) * 0.5f;

    mid_left.x = (corners[0].x + corners[3].x) * 0.5f;
    mid_left.y = (corners[0].y + corners[3].y) * 0.5f;
    mid_left.z = (corners[0].z + corners[3].z) * 0.5f;

    mid_right.x = (corners[1].x + corners[2].x) * 0.5f;
    mid_right.y = (corners[1].y + corners[2].y) * 0.5f;
    mid_right.z = (corners[1].z + corners[2].z) * 0.5f;

    const float red[3] = {1.0f, 0.0f, 0.0f};
    const float green[3] = {0.0f, 1.0f, 0.0f};
    const float blue[3] = {0.0f, 0.0f, 1.0f};
    const float yellow[3] = {1.0f, 1.0f, 0.0f};

    appendWorldQuadTriangles(mesh, corners[0], mid_bottom, center, mid_left, red);
    appendWorldQuadTriangles(mesh, mid_bottom, corners[1], mid_right, center, green);
    appendWorldQuadTriangles(mesh, center, mid_right, corners[2], mid_top, blue);
    appendWorldQuadTriangles(mesh, mid_left, center, mid_top, corners[3], yellow);
}
} // namespace

void LEDViewport3D::ClearLightBlockerDrawCache()
{
    light_blocker_draw_cache_.clear();
}

bool LEDViewport3D::TryGetLightBlockerDrawMeshes(size_t controller_index,
                                                 ControllerTransform* ctrl,
                                                 float border_width,
                                                 const GizmoDrawMesh*& fill_mesh,
                                                 const GizmoDrawMesh*& border_mesh)
{
    fill_mesh = nullptr;
    border_mesh = nullptr;
    if(!ctrl || !ctrl->virtual_controller)
    {
        return false;
    }

    VirtualController3D* layout = ctrl->virtual_controller;
    const std::vector<CustomControllerLightBlocker>& blockers = layout->GetLightBlockers();
    if(blockers.empty())
    {
        return false;
    }

    if(light_blocker_draw_cache_.size() <= controller_index)
    {
        light_blocker_draw_cache_.resize(controller_index + 1);
    }

    LightBlockerDrawCacheEntry& entry = light_blocker_draw_cache_[controller_index];
    const bool cache_hit = entry.valid && entry.layout == layout && entry.blocker_count == blockers.size() &&
                           std::fabs(entry.grid_scale_mm - grid_scale_mm) < 0.0001f &&
                           std::fabs(entry.border_width - border_width) < 0.0001f &&
                           transformsMatch(entry.transform, ctrl->transform);
    if(!cache_hit)
    {
        entry.fill_mesh = GizmoDrawMesh{};
        entry.border_mesh = GizmoDrawMesh{};
        appendLightBlockerLayers(entry.fill_mesh, entry.border_mesh, layout, grid_scale_mm, border_width);
        entry.layout = layout;
        entry.blocker_count = blockers.size();
        entry.grid_scale_mm = grid_scale_mm;
        entry.border_width = border_width;
        entry.transform = ctrl->transform;
        entry.valid = true;
    }

    fill_mesh = &entry.fill_mesh;
    border_mesh = &entry.border_mesh;
    return true;
}

ViewportMat4 LEDViewport3D::buildObjectLocalMatrix(float px, float py, float pz, const Rotation3D& rot) const
{
    using namespace ViewportMath;
    const ViewportMat4 pos = Translation(px, py, pz);
    const ViewportMat4 rx = RotationX(rot.x);
    const ViewportMat4 ry = RotationY(rot.y);
    const ViewportMat4 rz = RotationZ(rot.z);
    return Multiply(pos, Multiply(rx, Multiply(ry, rz)));
}

ViewportMat4 LEDViewport3D::buildControllerLocalMatrix(const Transform3D& transform, const Vector3D& center_offset) const
{
    using namespace ViewportMath;
    const ViewportMat4 pos = Translation(transform.position.x, transform.position.y, transform.position.z);
    const ViewportMat4 rx = RotationX(transform.rotation.x);
    const ViewportMat4 ry = RotationY(transform.rotation.y);
    const ViewportMat4 rz = RotationZ(transform.rotation.z);
    const ViewportMat4 s = Scale(transform.scale.x, transform.scale.y, transform.scale.z);
    const ViewportMat4 center = Translation(center_offset.x, center_offset.y, center_offset.z);
    return Multiply(pos, Multiply(rx, Multiply(ry, Multiply(rz, Multiply(s, center)))));
}

bool LEDViewport3D::fillLedDrawBuffers(ControllerTransform* ctrl)
{
    return populateLedDrawBuffers(ctrl) > 0;
}

void LEDViewport3D::ensureRoomGridOverlayBuffers()
{
    int nx = 0;
    int ny = 0;
    int nz = 0;
    GetRoomGridOverlayDimensions(&nx, &ny, &nz);
    const size_t count = (size_t)nx * (size_t)ny * (size_t)nz;
    if(count <= 0)
    {
        return;
    }

    const bool use_buffer = (room_grid_color_buffer.size() == count);
    const bool use_callback = (!use_buffer && room_grid_color_callback != nullptr);
    const float default_r = 0.0f;
    const float default_g = 0.0f;
    const float default_b = 0.0f;

    const bool layout_changed = room_grid_overlay_positions.size() != (3u * count) ||
                                room_grid_overlay_last_nx != nx || room_grid_overlay_last_ny != ny ||
                                room_grid_overlay_last_nz != nz || room_grid_overlay_last_step != room_grid_step;
    if(layout_changed)
    {
        room_grid_overlay_last_nx = nx;
        room_grid_overlay_last_ny = ny;
        room_grid_overlay_last_nz = nz;
        room_grid_overlay_last_step = room_grid_step;
        room_grid_overlay_colors_dirty = true;
        room_grid_overlay_positions.resize(3u * count);
        float* pos = room_grid_overlay_positions.data();
        for(int ix = 0; ix < nx; ix++)
        {
            for(int iy = 0; iy < ny; iy++)
            {
                for(int iz = 0; iz < nz; iz++)
                {
                    float x = 0.0f;
                    float y = 0.0f;
                    float z = 0.0f;
                    GetRoomGridOverlaySamplePosition(ix, iy, iz, x, y, z);
                    *pos++ = x;
                    *pos++ = y;
                    *pos++ = z;
                }
            }
        }
    }

    const bool need_color_refill = room_grid_overlay_colors_dirty || room_grid_overlay_colors.size() != (3u * count);
    if(need_color_refill)
    {
        room_grid_overlay_colors_dirty = false;
        room_grid_overlay_colors.resize(3u * count);
        float* col = room_grid_overlay_colors.data();
        for(int ix = 0; ix < nx; ix++)
        {
            for(int iy = 0; iy < ny; iy++)
            {
                for(int iz = 0; iz < nz; iz++)
                {
                    float x = 0.0f;
                    float y = 0.0f;
                    float z = 0.0f;
                    GetRoomGridOverlaySamplePosition(ix, iy, iz, x, y, z);
                    float r = default_r;
                    float g = default_g;
                    float b = default_b;
                    if(use_buffer)
                    {
                        const size_t idx = (size_t)(ix * ny * nz + iy * nz + iz);
                        const RGBColor c = room_grid_color_buffer[idx];
                        r = (float)RGBGetRValue(c) / 255.0f * room_grid_brightness;
                        g = (float)RGBGetGValue(c) / 255.0f * room_grid_brightness;
                        b = (float)RGBGetBValue(c) / 255.0f * room_grid_brightness;
                    }
                    else if(use_callback)
                    {
                        const RGBColor c = room_grid_color_callback(x, y, z);
                        r = (float)RGBGetRValue(c) / 255.0f * room_grid_brightness;
                        g = (float)RGBGetGValue(c) / 255.0f * room_grid_brightness;
                        b = (float)RGBGetBValue(c) / 255.0f * room_grid_brightness;
                    }
                    *col++ = r;
                    *col++ = g;
                    *col++ = b;
                }
            }
        }
    }
}

void LEDViewport3D::drawViewportSceneGpu()
{
    const ViewportMat4 scene_root = roomTurntableMatrix();
    using namespace ViewportMath;

    const GridExtents extents = GetRoomExtents();
    const float max_x = extents.width_units;
    const float max_z = extents.depth_units;
    if(cached_floor_grid_max_x != max_x || cached_floor_grid_max_z != max_z)
    {
        RebuildFloorGridCache(extents);
    }
    if(!cached_floor_grid_vertices.empty())
    {
        viewport_renderer_.setFloorGridMesh(cached_floor_grid_vertices, cached_floor_grid_colors);
        viewport_renderer_.drawFloorGridLines(scene_root);
    }
    viewport_renderer_.drawFloorGridPerimeter(max_x, max_z, scene_root);

    {
        GizmoDrawMesh mesh;
        const float red[3] = {1.0f, 0.0f, 0.0f};
        const float green[3] = {0.0f, 1.0f, 0.0f};
        const float blue[3] = {0.0f, 0.0f, 1.0f};
        viewportMeshAddLine(mesh, 3.0f, red, 0.0f, 0.0f, 0.0f, 3.0f, 0.0f, 0.0f);
        viewportMeshAddLine(mesh, 3.0f, green, 0.0f, 0.0f, 0.0f, 0.0f, 3.0f, 0.0f);
        viewportMeshAddLine(mesh, 3.0f, blue, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 3.0f);
        viewportMeshAddTriangle(mesh, red, 3.0f, 0.0f, 0.0f, 2.7f, 0.15f, 0.0f, 2.7f, -0.15f, 0.0f);
        viewportMeshAddTriangle(mesh, green, 0.0f, 3.0f, 0.0f, 0.15f, 2.7f, 0.0f, -0.15f, 2.7f, 0.0f);
        viewportMeshAddTriangle(mesh, blue, 0.0f, 0.0f, 3.0f, 0.15f, 0.0f, 2.7f, -0.15f, 0.0f, 2.7f);
        drawColoredMeshBatches(viewport_renderer_, mesh, scene_root);
    }

    if(use_manual_room_dimensions)
    {
        GizmoDrawMesh mesh;
        const float cyan[3] = {0.0f, 0.8f, 0.8f};
        const float max_y = extents.height_units;
        viewportMeshAddLine(mesh, 2.0f, cyan, 0.0f, 0.0f, 0.0f, max_x, 0.0f, 0.0f);
        viewportMeshAddLine(mesh, 2.0f, cyan, max_x, 0.0f, 0.0f, max_x, 0.0f, max_z);
        viewportMeshAddLine(mesh, 2.0f, cyan, max_x, 0.0f, max_z, 0.0f, 0.0f, max_z);
        viewportMeshAddLine(mesh, 2.0f, cyan, 0.0f, 0.0f, max_z, 0.0f, 0.0f, 0.0f);
        viewportMeshAddLine(mesh, 2.0f, cyan, 0.0f, max_y, 0.0f, max_x, max_y, 0.0f);
        viewportMeshAddLine(mesh, 2.0f, cyan, max_x, max_y, 0.0f, max_x, max_y, max_z);
        viewportMeshAddLine(mesh, 2.0f, cyan, max_x, max_y, max_z, 0.0f, max_y, max_z);
        viewportMeshAddLine(mesh, 2.0f, cyan, 0.0f, max_y, max_z, 0.0f, max_y, 0.0f);
        viewportMeshAddLine(mesh, 2.0f, cyan, 0.0f, 0.0f, 0.0f, 0.0f, max_y, 0.0f);
        viewportMeshAddLine(mesh, 2.0f, cyan, max_x, 0.0f, 0.0f, max_x, max_y, 0.0f);
        viewportMeshAddLine(mesh, 2.0f, cyan, max_x, 0.0f, max_z, max_x, max_y, max_z);
        viewportMeshAddLine(mesh, 2.0f, cyan, 0.0f, 0.0f, max_z, 0.0f, max_y, max_z);
        drawColoredMeshBatches(viewport_renderer_, mesh, scene_root);
    }

    if(display_planes)
    {
        for(size_t plane_index = 0; plane_index < display_planes->size(); plane_index++)
        {
            DisplayPlane3D* plane_ptr = (*display_planes)[plane_index].get();
            if(!plane_ptr)
            {
                continue;
            }

            const bool wants_cal = PlaneWantsCalibrationPattern(plane_ptr);
            const bool wants_prev = PlaneWantsScreenPreview(plane_ptr);
            if(!plane_ptr->IsVisible() && !wants_cal && !wants_prev)
            {
                continue;
            }

            const float width_units = MMToGridUnits(plane_ptr->GetWidthMM(), grid_scale_mm);
            const float height_units = MMToGridUnits(plane_ptr->GetHeightMM(), grid_scale_mm);
            if(width_units <= 0.0f || height_units <= 0.0f)
            {
                continue;
            }

            const float half_w = width_units * 0.5f;
            const float half_h = height_units * 0.5f;
            const Vector3D local_corners[4] = {
                {-half_w, -half_h, 0.0f},
                {half_w, -half_h, 0.0f},
                {half_w, half_h, 0.0f},
                {-half_w, half_h, 0.0f},
            };

            Vector3D world_corners[4];
            for(int i = 0; i < 4; ++i)
            {
                world_corners[i] = TransformLocalToWorld(local_corners[i], plane_ptr->GetTransform());
            }

            const bool selected = ((int)plane_index == selected_display_plane_idx);
            const float fill_color[3] = {
                selected ? 0.35f : 0.2f,
                selected ? 0.80f : 0.60f,
                1.0f,
            };
            const float border_color[3] = {
                selected ? 0.65f : 0.35f,
                selected ? 0.90f : 0.70f,
                1.0f,
            };
            const float border_width = selected ? 3.0f : 2.0f;

            if(wants_cal)
            {
                GizmoDrawMesh cal_mesh;
                appendCalibrationPattern(cal_mesh, world_corners);
                drawColoredMeshBatches(viewport_renderer_, cal_mesh, scene_root, 0.85f);
            }
            else if(wants_prev)
            {
                const std::string source_id = plane_ptr->GetCaptureSourceId();
                GLuint texture_id = 0;
                if(!source_id.empty())
                {
                    const auto texture_it = display_plane_textures.find(source_id);
                    if(texture_it != display_plane_textures.end())
                    {
                        texture_id = texture_it->second;
                    }
                }

                if(texture_id != 0)
                {
                    const std::vector<float> positions = {
                        world_corners[0].x, world_corners[0].y, world_corners[0].z,
                        world_corners[1].x, world_corners[1].y, world_corners[1].z,
                        world_corners[2].x, world_corners[2].y, world_corners[2].z,
                        world_corners[3].x, world_corners[3].y, world_corners[3].z,
                    };
                    const std::vector<float> uvs = {0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f};
                    viewport_renderer_.drawTexturedQuad(positions, uvs, texture_id, scene_root, 0.85f);
                }
                else
                {
                    GizmoDrawMesh cal_mesh;
                    appendCalibrationPattern(cal_mesh, world_corners);
                    drawColoredMeshBatches(viewport_renderer_, cal_mesh, scene_root, 0.85f);
                }
            }
            else
            {
                GizmoDrawMesh fill_mesh;
                appendWorldQuadTriangles(fill_mesh, world_corners[0], world_corners[1], world_corners[2], world_corners[3], fill_color);
                viewport_renderer_.drawColoredTriangles(fill_mesh.triangle_positions, fill_mesh.triangle_colors, scene_root,
                                                        selected ? 0.30f : 0.18f);
            }

            GizmoDrawMesh border_mesh;
            viewportMeshAddLine(border_mesh, border_width, border_color,
                                world_corners[0].x, world_corners[0].y, world_corners[0].z,
                                world_corners[1].x, world_corners[1].y, world_corners[1].z);
            viewportMeshAddLine(border_mesh, border_width, border_color,
                                world_corners[1].x, world_corners[1].y, world_corners[1].z,
                                world_corners[2].x, world_corners[2].y, world_corners[2].z);
            viewportMeshAddLine(border_mesh, border_width, border_color,
                                world_corners[2].x, world_corners[2].y, world_corners[2].z,
                                world_corners[3].x, world_corners[3].y, world_corners[3].z);
            viewportMeshAddLine(border_mesh, border_width, border_color,
                                world_corners[3].x, world_corners[3].y, world_corners[3].z,
                                world_corners[0].x, world_corners[0].y, world_corners[0].z);
            drawColoredMeshBatches(viewport_renderer_, border_mesh, scene_root);
        }
    }

    if(show_room_grid_overlay)
    {
        ensureRoomGridOverlayBuffers();
        if(!room_grid_overlay_positions.empty())
        {
            viewport_renderer_.drawColoredPoints(room_grid_overlay_positions, room_grid_overlay_colors, scene_root,
                                                   room_grid_point_size);
        }
    }

    if(controller_transforms)
    {
        for(size_t i = 0; i < controller_transforms->size(); i++)
        {
            ControllerTransform* ctrl = (*controller_transforms)[i].get();
            if(!ctrl || ctrl->hidden_by_virtual)
            {
                continue;
            }

            Vector3D min_bounds, max_bounds;
            CalculateControllerBounds(ctrl, min_bounds, max_bounds);
            const Vector3D center_offset = {
                -(min_bounds.x + max_bounds.x) * 0.5f,
                -(min_bounds.y + max_bounds.y) * 0.5f,
                -(min_bounds.z + max_bounds.z) * 0.5f,
            };

            float box_color[3] = {0.3f, 0.3f, 0.3f};
            float line_width = 1.0f;
            if((int)i == selected_controller_idx)
            {
                box_color[0] = 1.0f;
                box_color[1] = 1.0f;
                box_color[2] = 0.0f;
                line_width = 3.0f;
            }
            else if(IsControllerSelected((int)i))
            {
                box_color[0] = 1.0f;
                box_color[1] = 0.8f;
                box_color[2] = 0.0f;
                line_width = 2.0f;
            }

            GizmoDrawMesh mesh;
            appendControllerBounds(mesh, min_bounds, max_bounds, box_color, line_width);
            const float center_x = (min_bounds.x + max_bounds.x) * 0.5f;
            const float top_y = max_bounds.y;
            const float center_z = (min_bounds.z + max_bounds.z) * 0.5f;
            appendControllerTopIndicator(mesh, center_x, top_y, center_z, 0.15f);

            const ViewportMat4 model = Multiply(scene_root, buildControllerLocalMatrix(ctrl->transform, center_offset));
            drawColoredMeshBatches(viewport_renderer_, mesh, model);
            if(fillLedDrawBuffers(ctrl))
            {
                viewport_renderer_.drawColoredPoints(led_draw_positions, led_draw_colors, model,
                                                     ledPreviewPointSizeGl());
            }

            if(ctrl->virtual_controller && !ctrl->virtual_controller->GetLightBlockers().empty())
            {
                const bool is_primary = ((int)i == selected_controller_idx);
                const bool is_selected = IsControllerSelected((int)i);
                const float cell_alpha = is_primary ? 0.50f : (is_selected ? 0.44f : 0.38f);
                const float border_width = is_primary ? 2.5f : (is_selected ? 2.0f : 1.5f);

                const GizmoDrawMesh* blocker_fill_mesh = nullptr;
                const GizmoDrawMesh* blocker_border_mesh = nullptr;
                if(this->TryGetLightBlockerDrawMeshes(i, ctrl, border_width, blocker_fill_mesh, blocker_border_mesh) &&
                   blocker_fill_mesh && blocker_border_mesh)
                {
                    if(!blocker_fill_mesh->triangle_positions.empty())
                    {
                        viewport_renderer_.drawColoredTriangles(blocker_fill_mesh->triangle_positions,
                                                                blocker_fill_mesh->triangle_colors, model, cell_alpha);
                    }
                    drawColoredMeshBatches(viewport_renderer_, *blocker_border_mesh, model);
                }
            }
        }
    }

    if(reference_points)
    {
        for(size_t idx = 0; idx < reference_points->size(); idx++)
        {
            VirtualReferencePoint3D* ref_point = (*reference_points)[idx].get();
            if(!ref_point || !ref_point->IsVisible())
            {
                continue;
            }

            const Vector3D pos = ref_point->GetPosition();
            const Rotation3D rot = ref_point->GetRotation();
            const RGBColor color = ref_point->GetDisplayColor();
            const float rgb[3] = {
                (color & 0xFF) / 255.0f,
                ((color >> 8) & 0xFF) / 255.0f,
                ((color >> 16) & 0xFF) / 255.0f,
            };

            if(ref_point->GetType() == REF_POINT_USER)
            {
                const ViewportMat4 model = Multiply(scene_root, buildObjectLocalMatrix(pos.x, pos.y, pos.z, rot));
                GizmoDrawMesh mesh;
                const int segments = 20;
                for(int i = 0; i < segments; i++)
                {
                    const float a0 = 2.0f * (float)M_PI * (float)i / segments;
                    const float a1 = 2.0f * (float)M_PI * (float)(i + 1) / segments;
                    viewportMeshAddLine(mesh, 2.0f, rgb,
                                        0.4f * cosf(a0), 0.4f * sinf(a0), 0.0f,
                                        0.4f * cosf(a1), 0.4f * sinf(a1), 0.0f);
                }
                for(int i = 0; i < 10; i++)
                {
                    const float t0 = (float)i / 10.0f;
                    const float t1 = (float)(i + 1) / 10.0f;
                    const float ang0 = (float)M_PI + t0 * (float)M_PI;
                    const float ang1 = (float)M_PI + t1 * (float)M_PI;
                    viewportMeshAddLine(mesh, 2.0f, rgb,
                                        0.25f * cosf(ang0), -0.05f + 0.25f * sinf(ang0), 0.0f,
                                        0.25f * cosf(ang1), -0.05f + 0.25f * sinf(ang1), 0.0f);
                }
                viewport_renderer_.drawColoredPoints(
                    {-0.15f, 0.1f, 0.0f, 0.15f, 0.1f, 0.0f},
                    {rgb[0], rgb[1], rgb[2], rgb[0], rgb[1], rgb[2]},
                    model,
                    6.0f);
                drawColoredMeshBatches(viewport_renderer_, mesh, model);
                if((int)idx == selected_ref_point_idx)
                {
                    GizmoDrawMesh box_mesh;
                    const float yellow[3] = {1.0f, 1.0f, 0.0f};
                    appendSelectionBox(box_mesh, 0.6f, yellow, 3.0f);
                    viewport_renderer_.setGpuDepthState(false, false);
                    drawColoredMeshBatches(viewport_renderer_, box_mesh, model);
                    viewport_renderer_.setGpuDepthState(true, true);
                }
                continue;
            }

            const ViewportMat4 model = Multiply(scene_root, buildObjectLocalMatrix(pos.x, pos.y, pos.z, rot));
            GizmoDrawMesh mesh;
            appendLocalSphere(mesh, 0.3f, rgb);
            drawColoredMeshBatches(viewport_renderer_, mesh, model);

            const bool is_selected = ((int)idx == selected_ref_point_idx);
            const float ring_color[3] = {
                is_selected ? 1.0f : rgb[0] * 0.5f,
                is_selected ? 1.0f : rgb[1] * 0.5f,
                is_selected ? 1.0f : rgb[2] * 0.5f,
            };
            GizmoDrawMesh ring_mesh;
            const int segments = 16;
            for(int i = 0; i < segments; i++)
            {
                const float a0 = 2.0f * (float)M_PI * (float)i / segments;
                const float a1 = 2.0f * (float)M_PI * (float)(i + 1) / segments;
                viewportMeshAddLine(ring_mesh, is_selected ? 4.0f : 2.0f, ring_color,
                                    0.3f * cosf(a0), 0.0f, 0.3f * sinf(a0),
                                    0.3f * cosf(a1), 0.0f, 0.3f * sinf(a1));
            }
            drawColoredMeshBatches(viewport_renderer_, ring_mesh, model);

            if(is_selected)
            {
                GizmoDrawMesh box_mesh;
                const float yellow[3] = {1.0f, 1.0f, 0.0f};
                appendSelectionBox(box_mesh, 0.45f, yellow, 3.0f);
                viewport_renderer_.setGpuDepthState(false, false);
                drawColoredMeshBatches(viewport_renderer_, box_mesh, model);
                viewport_renderer_.setGpuDepthState(true, true);
            }
        }
    }
}

void LEDViewport3D::renderGizmoGpu()
{
    if(!gizmo.IsActive())
    {
        return;
    }

    GizmoDrawMesh mesh;
    gizmo.buildDrawMesh(mesh);
    if(mesh.line_batches.empty() && mesh.triangle_positions.empty())
    {
        return;
    }

    float gx = 0.0f;
    float gy = 0.0f;
    float gz = 0.0f;
    gizmo.GetPosition(gx, gy, gz);

    using namespace ViewportMath;
    const ViewportMat4 scene_root = roomTurntableMatrix();
    const ViewportMat4 model = Multiply(scene_root, Translation(gx, gy, gz));

    const auto drawPass = [&](bool depth_occluded_pass) {
        viewport_renderer_.setGpuDepthState(depth_occluded_pass, false);
        for(const GizmoDrawMesh::LineBatch& batch : mesh.line_batches)
        {
            if(!batch.positions.empty())
            {
                viewport_renderer_.drawColoredLines(batch.positions, batch.colors, model, batch.width);
            }
        }
        if(!mesh.triangle_positions.empty())
        {
            viewport_renderer_.drawColoredTriangles(mesh.triangle_positions, mesh.triangle_colors, model);
        }
    };

    drawPass(true);
    drawPass(false);
}

void LEDViewport3D::renderRoomViewportSelectionGpu()
{
    if(!room_viewport_selected_)
    {
        return;
    }

    std::vector<float> positions;
    std::vector<float> colors;
    fillRoomViewportSelectionLineBuffers(positions, colors);

    using namespace ViewportMath;
    const ViewportMat4 model = roomTurntableMatrix();
    viewport_renderer_.setGpuDepthState(true, false);
    viewport_renderer_.drawColoredLines(positions, colors, model, 3.0f);
}
