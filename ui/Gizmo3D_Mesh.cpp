// SPDX-License-Identifier: GPL-2.0-only

#include "Gizmo3D.h"

#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace
{
void meshAddLine(GizmoDrawMesh& mesh, float width, const float color[3],
                 float x0, float y0, float z0, float x1, float y1, float z1)
{
    GizmoDrawMesh::LineBatch& batch = mesh.lineBatchForWidth(width);
    batch.positions.insert(batch.positions.end(), {x0, y0, z0, x1, y1, z1});
    batch.colors.insert(batch.colors.end(), {color[0], color[1], color[2], color[0], color[1], color[2]});
}

void meshAddTriangle(GizmoDrawMesh& mesh, const float color[3],
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

void meshAddQuad(GizmoDrawMesh& mesh, const float color[3],
                 float x0, float y0, float z0,
                 float x1, float y1, float z1,
                 float x2, float y2, float z2,
                 float x3, float y3, float z3)
{
    meshAddTriangle(mesh, color, x0, y0, z0, x1, y1, z1, x2, y2, z2);
    meshAddTriangle(mesh, color, x0, y0, z0, x2, y2, z2, x3, y3, z3);
}

void meshAddCube(GizmoDrawMesh& mesh, const float color[3], float cx, float cy, float cz, float size)
{
    const float x0 = cx - size;
    const float x1 = cx + size;
    const float y0 = cy - size;
    const float y1 = cy + size;
    const float z0 = cz - size;
    const float z1 = cz + size;

    meshAddQuad(mesh, color, x0, y0, z0, x1, y0, z0, x1, y1, z0, x0, y1, z0);
    meshAddQuad(mesh, color, x0, y0, z1, x1, y0, z1, x1, y1, z1, x0, y1, z1);
    meshAddQuad(mesh, color, x0, y0, z0, x0, y0, z1, x0, y1, z1, x0, y1, z0);
    meshAddQuad(mesh, color, x1, y0, z0, x1, y0, z1, x1, y1, z1, x1, y1, z0);
    meshAddQuad(mesh, color, x0, y0, z0, x1, y0, z0, x1, y0, z1, x0, y0, z1);
    meshAddQuad(mesh, color, x0, y1, z0, x1, y1, z0, x1, y1, z1, x0, y1, z1);
}

void meshAddSphere(GizmoDrawMesh& mesh, const float color[3], float cx, float cy, float cz, float radius)
{
    const int slices = 12;
    const int stacks = 8;
    for(int i = 0; i < stacks; i++)
    {
        const float lat0 = (float)M_PI * (-0.5f + (float)i / (float)stacks);
        const float lat1 = (float)M_PI * (-0.5f + (float)(i + 1) / (float)stacks);
        const float y0 = cy + radius * sinf(lat0);
        const float y1 = cy + radius * sinf(lat1);
        const float r0 = radius * cosf(lat0);
        const float r1 = radius * cosf(lat1);

        for(int j = 0; j < slices; j++)
        {
            const float lng0 = 2.0f * (float)M_PI * (float)j / (float)slices;
            const float lng1 = 2.0f * (float)M_PI * (float)(j + 1) / (float)slices;
            const float x00 = cx + cosf(lng0) * r0;
            const float z00 = cz + sinf(lng0) * r0;
            const float x01 = cx + cosf(lng1) * r0;
            const float z01 = cz + sinf(lng1) * r0;
            const float x10 = cx + cosf(lng0) * r1;
            const float z10 = cz + sinf(lng0) * r1;
            const float x11 = cx + cosf(lng1) * r1;
            const float z11 = cz + sinf(lng1) * r1;

            meshAddTriangle(mesh, color, x00, y0, z00, x01, y0, z01, x11, y1, z11);
            meshAddTriangle(mesh, color, x00, y0, z00, x11, y1, z11, x10, y1, z10);
        }
    }
}

} // namespace

void GizmoDrawMesh::clear()
{
    line_batches.clear();
    triangle_positions.clear();
    triangle_colors.clear();
}

GizmoDrawMesh::LineBatch& GizmoDrawMesh::lineBatchForWidth(float width)
{
    for(LineBatch& batch : line_batches)
    {
        if(batch.width == width)
        {
            return batch;
        }
    }
    LineBatch batch;
    batch.width = width;
    line_batches.push_back(std::move(batch));
    return line_batches.back();
}

void Gizmo3D::buildDrawMesh(GizmoDrawMesh& mesh) const
{
    mesh.clear();
    if(!active)
    {
        return;
    }

    switch(mode)
    {
        case GIZMO_MODE_MOVE:
            appendMoveGizmoMesh(mesh);
            break;
        case GIZMO_MODE_ROTATE:
            appendRotateGizmoMesh(mesh);
            break;
        case GIZMO_MODE_FREEROAM:
            appendFreeroamGizmoMesh(mesh);
            break;
    }
}

void Gizmo3D::appendMoveGizmoMesh(GizmoDrawMesh& mesh) const
{
    const GizmoAxis hl = dragging ? selected_axis : hover_axis;

    auto axisColor = [&](GizmoAxis axis, const float base[3]) -> const float* {
        return (hl == axis) ? color_highlight : base;
    };

    const float* color = axisColor(GIZMO_AXIS_X, color_x_axis);
    meshAddLine(mesh, 4.0f, color, 0.0f, 0.0f, 0.0f, gizmo_size, 0.0f, 0.0f);
    meshAddTriangle(mesh, color, gizmo_size, 0.0f, 0.0f, gizmo_size - 0.3f, 0.15f, 0.0f, gizmo_size - 0.3f, -0.15f, 0.0f);
    meshAddTriangle(mesh, color, gizmo_size, 0.0f, 0.0f, gizmo_size - 0.3f, 0.0f, 0.15f, gizmo_size - 0.3f, 0.0f, -0.15f);

    color = axisColor(GIZMO_AXIS_Y, color_y_axis);
    meshAddLine(mesh, 4.0f, color, 0.0f, 0.0f, 0.0f, 0.0f, gizmo_size, 0.0f);
    meshAddTriangle(mesh, color, 0.0f, gizmo_size, 0.0f, 0.15f, gizmo_size - 0.3f, 0.0f, -0.15f, gizmo_size - 0.3f, 0.0f);
    meshAddTriangle(mesh, color, 0.0f, gizmo_size, 0.0f, 0.0f, gizmo_size - 0.3f, 0.15f, 0.0f, gizmo_size - 0.3f, -0.15f);

    color = axisColor(GIZMO_AXIS_Z, color_z_axis);
    meshAddLine(mesh, 4.0f, color, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, gizmo_size);
    meshAddTriangle(mesh, color, 0.0f, 0.0f, gizmo_size, 0.15f, 0.0f, gizmo_size - 0.3f, -0.15f, 0.0f, gizmo_size - 0.3f);
    meshAddTriangle(mesh, color, 0.0f, 0.0f, gizmo_size, 0.0f, 0.15f, gizmo_size - 0.3f, 0.0f, -0.15f, gizmo_size - 0.3f);

    const float orange[3] = {1.0f, 0.5f, 0.0f};
    color = axisColor(GIZMO_AXIS_CENTER, orange);
    meshAddCube(mesh, color, 0.0f, 0.0f, 0.0f, center_sphere_radius);
}

void Gizmo3D::appendRotateGizmoMesh(GizmoDrawMesh& mesh) const
{
    const float handle_radius = 0.15f;
    const GizmoAxis hl = dragging ? selected_axis : hover_axis;
    const int ring_segments = 33;

    auto drawRingX = [&]() {
        const float* color = (hl == GIZMO_AXIS_X) ? color_highlight : color_x_axis;
        const float width = (hl == GIZMO_AXIS_X) ? 6.0f : 3.0f;
        for(int i = 0; i < ring_segments - 1; i++)
        {
            const float a0 = (i / (float)(ring_segments - 1)) * 2.0f * (float)M_PI;
            const float a1 = ((i + 1) / (float)(ring_segments - 1)) * 2.0f * (float)M_PI;
            meshAddLine(mesh, width, color,
                        0.0f, cosf(a0) * gizmo_size, sinf(a0) * gizmo_size,
                        0.0f, cosf(a1) * gizmo_size, sinf(a1) * gizmo_size);
        }
        for(int i = 0; i < 4; i++)
        {
            const float angle = (i / 4.0f) * 2.0f * (float)M_PI;
            meshAddSphere(mesh, color, 0.0f, cosf(angle) * gizmo_size, sinf(angle) * gizmo_size, handle_radius);
        }
    };

    auto drawRingY = [&]() {
        const float* color = (hl == GIZMO_AXIS_Y) ? color_highlight : color_y_axis;
        const float width = (hl == GIZMO_AXIS_Y) ? 6.0f : 3.0f;
        for(int i = 0; i < ring_segments - 1; i++)
        {
            const float a0 = (i / (float)(ring_segments - 1)) * 2.0f * (float)M_PI;
            const float a1 = ((i + 1) / (float)(ring_segments - 1)) * 2.0f * (float)M_PI;
            meshAddLine(mesh, width, color,
                        cosf(a0) * gizmo_size, 0.0f, sinf(a0) * gizmo_size,
                        cosf(a1) * gizmo_size, 0.0f, sinf(a1) * gizmo_size);
        }
        for(int i = 0; i < 4; i++)
        {
            const float angle = (i / 4.0f) * 2.0f * (float)M_PI;
            meshAddSphere(mesh, color, cosf(angle) * gizmo_size, 0.0f, sinf(angle) * gizmo_size, handle_radius);
        }
    };

    auto drawRingZ = [&]() {
        const float* color = (hl == GIZMO_AXIS_Z) ? color_highlight : color_z_axis;
        const float width = (hl == GIZMO_AXIS_Z) ? 6.0f : 3.0f;
        for(int i = 0; i < ring_segments - 1; i++)
        {
            const float a0 = (i / (float)(ring_segments - 1)) * 2.0f * (float)M_PI;
            const float a1 = ((i + 1) / (float)(ring_segments - 1)) * 2.0f * (float)M_PI;
            meshAddLine(mesh, width, color,
                        cosf(a0) * gizmo_size, sinf(a0) * gizmo_size, 0.0f,
                        cosf(a1) * gizmo_size, sinf(a1) * gizmo_size, 0.0f);
        }
        for(int i = 0; i < 4; i++)
        {
            const float angle = (i / 4.0f) * 2.0f * (float)M_PI;
            meshAddSphere(mesh, color, cosf(angle) * gizmo_size, sinf(angle) * gizmo_size, 0.0f, handle_radius);
        }
    };

    drawRingX();
    drawRingY();
    drawRingZ();

    if(dragging && mode == GIZMO_MODE_ROTATE &&
       (selected_axis == GIZMO_AXIS_X || selected_axis == GIZMO_AXIS_Y || selected_axis == GIZMO_AXIS_Z))
    {
        const float arc_radius = gizmo_size * 1.12f;
        const float start = rot_drag_start_angle;
        const float sweep = rot_drag_accum_degrees * ((float)M_PI / 180.0f);
        const int segments = 40;
        const float white[3] = {1.0f, 1.0f, 1.0f};
        for(int i = 0; i < segments; i++)
        {
            const float t0 = (float)i / (float)segments;
            const float t1 = (float)(i + 1) / (float)segments;
            const float a0 = start + sweep * t0;
            const float a1 = start + sweep * t1;
            float x0 = 0.0f;
            float y0 = 0.0f;
            float z0 = 0.0f;
            float x1 = 0.0f;
            float y1 = 0.0f;
            float z1 = 0.0f;
            if(selected_axis == GIZMO_AXIS_X)
            {
                y0 = cosf(a0) * arc_radius;
                z0 = sinf(a0) * arc_radius;
                y1 = cosf(a1) * arc_radius;
                z1 = sinf(a1) * arc_radius;
            }
            else if(selected_axis == GIZMO_AXIS_Y)
            {
                x0 = cosf(a0) * arc_radius;
                z0 = sinf(a0) * arc_radius;
                x1 = cosf(a1) * arc_radius;
                z1 = sinf(a1) * arc_radius;
            }
            else
            {
                x0 = cosf(a0) * arc_radius;
                y0 = sinf(a0) * arc_radius;
                x1 = cosf(a1) * arc_radius;
                y1 = sinf(a1) * arc_radius;
            }
            meshAddLine(mesh, 5.0f, white, x0, y0, z0, x1, y1, z1);
        }
    }

    const float orange[3] = {1.0f, 0.5f, 0.0f};
    const float* color = (hl == GIZMO_AXIS_CENTER) ? color_highlight : orange;
    meshAddCube(mesh, color, 0.0f, 0.0f, 0.0f, center_sphere_radius);
}

void Gizmo3D::appendFreeroamGizmoMesh(GizmoDrawMesh& mesh) const
{
    const GizmoAxis hl = dragging ? selected_axis : hover_axis;
    const float purple[3] = {0.5f, 0.0f, 1.0f};
    const float* stick_color = (hl == GIZMO_AXIS_CENTER) ? color_highlight : purple;
    meshAddLine(mesh, 5.0f, stick_color, 0.0f, 0.0f, 0.0f, 0.0f, gizmo_size, 0.0f);

    const float cube_size = 0.3f;
    const float stick_height = gizmo_size;
    const float orange[3] = {1.0f, 0.5f, 0.0f};
    const float* center_color = (hl == GIZMO_AXIS_CENTER) ? color_highlight : orange;

    meshAddQuad(mesh, stick_color,
                -cube_size, stick_height - cube_size, -cube_size,
                +cube_size, stick_height - cube_size, -cube_size,
                +cube_size, stick_height + cube_size, -cube_size,
                -cube_size, stick_height + cube_size, -cube_size);
    meshAddQuad(mesh, stick_color,
                -cube_size, stick_height - cube_size, +cube_size,
                +cube_size, stick_height - cube_size, +cube_size,
                +cube_size, stick_height + cube_size, +cube_size,
                -cube_size, stick_height + cube_size, +cube_size);
    meshAddQuad(mesh, stick_color,
                -cube_size, stick_height - cube_size, -cube_size,
                -cube_size, stick_height + cube_size, -cube_size,
                -cube_size, stick_height + cube_size, +cube_size,
                -cube_size, stick_height - cube_size, +cube_size);
    meshAddQuad(mesh, stick_color,
                +cube_size, stick_height - cube_size, -cube_size,
                +cube_size, stick_height + cube_size, -cube_size,
                +cube_size, stick_height + cube_size, +cube_size,
                +cube_size, stick_height - cube_size, +cube_size);
    meshAddQuad(mesh, stick_color,
                -cube_size, stick_height - cube_size, -cube_size,
                +cube_size, stick_height - cube_size, -cube_size,
                +cube_size, stick_height - cube_size, +cube_size,
                -cube_size, stick_height - cube_size, +cube_size);
    meshAddQuad(mesh, stick_color,
                -cube_size, stick_height + cube_size, -cube_size,
                +cube_size, stick_height + cube_size, -cube_size,
                +cube_size, stick_height + cube_size, +cube_size,
                -cube_size, stick_height + cube_size, +cube_size);

    meshAddCube(mesh, center_color, 0.0f, 0.0f, 0.0f, center_sphere_radius);
}
