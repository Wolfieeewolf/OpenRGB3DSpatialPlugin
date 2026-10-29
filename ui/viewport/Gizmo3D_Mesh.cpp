// SPDX-License-Identifier: GPL-2.0-only

#include "Gizmo3D.h"

#include <algorithm>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace
{
void PushPosColor(std::vector<float>& out, float x, float y, float z, float r, float g, float b)
{
    out.push_back(x);
    out.push_back(y);
    out.push_back(z);
    out.push_back(r);
    out.push_back(g);
    out.push_back(b);
}

void meshAddLine(GizmoDrawMesh& mesh, float width, const float color[3],
                 float x0, float y0, float z0, float x1, float y1, float z1)
{
    GizmoDrawMesh::LineBatch& batch = mesh.lineBatchForWidth(width);
    PushPosColor(batch.interleaved, x0, y0, z0, color[0], color[1], color[2]);
    PushPosColor(batch.interleaved, x1, y1, z1, color[0], color[1], color[2]);
}

void meshAddTriangle(GizmoDrawMesh& mesh, const float color[3],
                     float x0, float y0, float z0,
                     float x1, float y1, float z1,
                     float x2, float y2, float z2)
{
    PushPosColor(mesh.triangle_interleaved, x0, y0, z0, color[0], color[1], color[2]);
    PushPosColor(mesh.triangle_interleaved, x1, y1, z1, color[0], color[1], color[2]);
    PushPosColor(mesh.triangle_interleaved, x2, y2, z2, color[0], color[1], color[2]);
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

/** Offset a point in the plane perpendicular to axis 0/1/2. */
void offsetPerp(int axis, float along, float u, float v,
                float& x, float& y, float& z)
{
    if(axis == 0)
    {
        x = along;
        y = u;
        z = v;
    }
    else if(axis == 1)
    {
        x = u;
        y = along;
        z = v;
    }
    else
    {
        x = u;
        y = v;
        z = along;
    }
}

/** Solid cylinder along +X/+Y/+Z (Qt3D-style arrow shaft). */
void meshAddAxisCylinder(GizmoDrawMesh& mesh, const float color[3],
                         int axis, float from_along, float to_along, float radius,
                         int sides = 10)
{
    for(int i = 0; i < sides; i++)
    {
        const float a0 = 2.0f * (float)M_PI * (float)i / (float)sides;
        const float a1 = 2.0f * (float)M_PI * (float)(i + 1) / (float)sides;
        const float u0 = cosf(a0) * radius;
        const float v0 = sinf(a0) * radius;
        const float u1 = cosf(a1) * radius;
        const float v1 = sinf(a1) * radius;

        float x00, y00, z00, x01, y01, z01, x10, y10, z10, x11, y11, z11;
        offsetPerp(axis, from_along, u0, v0, x00, y00, z00);
        offsetPerp(axis, from_along, u1, v1, x01, y01, z01);
        offsetPerp(axis, to_along, u0, v0, x10, y10, z10);
        offsetPerp(axis, to_along, u1, v1, x11, y11, z11);

        meshAddQuad(mesh, color, x00, y00, z00, x01, y01, z01, x11, y11, z11, x10, y10, z10);
    }
}

/** Cone tip pointing along +axis (Qt3D arrow head). */
void meshAddAxisCone(GizmoDrawMesh& mesh, const float color[3],
                     int axis, float tip_along, float length, float radius,
                     int sides = 12)
{
    const float base_along = tip_along - length;
    float tip_x = 0.0f;
    float tip_y = 0.0f;
    float tip_z = 0.0f;
    offsetPerp(axis, tip_along, 0.0f, 0.0f, tip_x, tip_y, tip_z);

    for(int i = 0; i < sides; i++)
    {
        const float a0 = 2.0f * (float)M_PI * (float)i / (float)sides;
        const float a1 = 2.0f * (float)M_PI * (float)(i + 1) / (float)sides;
        float p0x, p0y, p0z, p1x, p1y, p1z;
        offsetPerp(axis, base_along, cosf(a0) * radius, sinf(a0) * radius, p0x, p0y, p0z);
        offsetPerp(axis, base_along, cosf(a1) * radius, sinf(a1) * radius, p1x, p1y, p1z);
        meshAddTriangle(mesh, color, tip_x, tip_y, tip_z, p0x, p0y, p0z, p1x, p1y, p1z);
    }

    /* Flat base cap. */
    float bx, by, bz;
    offsetPerp(axis, base_along, 0.0f, 0.0f, bx, by, bz);
    for(int i = 0; i < sides; i++)
    {
        const float a0 = 2.0f * (float)M_PI * (float)i / (float)sides;
        const float a1 = 2.0f * (float)M_PI * (float)(i + 1) / (float)sides;
        float p0x, p0y, p0z, p1x, p1y, p1z;
        offsetPerp(axis, base_along, cosf(a0) * radius, sinf(a0) * radius, p0x, p0y, p0z);
        offsetPerp(axis, base_along, cosf(a1) * radius, sinf(a1) * radius, p1x, p1y, p1z);
        meshAddTriangle(mesh, color, bx, by, bz, p1x, p1y, p1z, p0x, p0y, p0z);
    }
}

/**
 * Thin torus ring in the plane of axis (0=YZ, 1=XZ, 2=XY) — Qt3D rotation handle.
 * No bead spheres; the tube itself is the grabber.
 */
void meshAddTorusRing(GizmoDrawMesh& mesh, const float color[3],
                      int axis, float major_r, float minor_r,
                      int rings = 32, int slices = 6)
{
    for(int i = 0; i < rings; i++)
    {
        const float a0 = 2.0f * (float)M_PI * (float)i / (float)rings;
        const float a1 = 2.0f * (float)M_PI * (float)(i + 1) / (float)rings;
        const float c0 = cosf(a0);
        const float s0 = sinf(a0);
        const float c1 = cosf(a1);
        const float s1 = sinf(a1);

        for(int j = 0; j < slices; j++)
        {
            const float b0 = 2.0f * (float)M_PI * (float)j / (float)slices;
            const float b1 = 2.0f * (float)M_PI * (float)(j + 1) / (float)slices;
            const float cb0 = cosf(b0);
            const float sb0 = sinf(b0);
            const float cb1 = cosf(b1);
            const float sb1 = sinf(b1);

            auto torusPoint = [&](float c, float s, float cb, float sb,
                                  float& x, float& y, float& z) {
                const float R = major_r + minor_r * cb;
                if(axis == 0)
                {
                    x = minor_r * sb;
                    y = R * c;
                    z = R * s;
                }
                else if(axis == 1)
                {
                    x = R * c;
                    y = minor_r * sb;
                    z = R * s;
                }
                else
                {
                    x = R * c;
                    y = R * s;
                    z = minor_r * sb;
                }
            };

            float x00, y00, z00, x01, y01, z01, x10, y10, z10, x11, y11, z11;
            torusPoint(c0, s0, cb0, sb0, x00, y00, z00);
            torusPoint(c0, s0, cb1, sb1, x01, y01, z01);
            torusPoint(c1, s1, cb0, sb0, x10, y10, z10);
            torusPoint(c1, s1, cb1, sb1, x11, y11, z11);
            meshAddQuad(mesh, color, x00, y00, z00, x01, y01, z01, x11, y11, z11, x10, y10, z10);
        }
    }
}

void shadeAxisColor(float out[3], const float base[3], const float highlight[3],
                    bool active, bool any_focus)
{
    if(active)
    {
        out[0] = highlight[0];
        out[1] = highlight[1];
        out[2] = highlight[2];
        return;
    }
    /* Qt3D / Gizmo3D keep inactive axes readable — light dim only. */
    const float dim = any_focus ? 0.55f : 1.0f;
    out[0] = base[0] * dim;
    out[1] = base[1] * dim;
    out[2] = base[2] * dim;
}

} // namespace

void GizmoDrawMesh::clear()
{
    line_batches.clear();
    triangle_interleaved.clear();
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
        case GIZMO_MODE_SCALE:
            appendScaleGizmoMesh(mesh);
            break;
        case GIZMO_MODE_SKEW:
            appendSkewGizmoMesh(mesh);
            break;
        case GIZMO_MODE_FREEROAM:
            appendFreeroamGizmoMesh(mesh);
            break;
        default:
            appendMoveGizmoMesh(mesh);
            break;
    }
}

void Gizmo3D::appendMoveGizmoMesh(GizmoDrawMesh& mesh) const
{
    /* Qt3D ArrowTranslationHandle proportions: thin cylinder + cone tip. */
    const GizmoAxis hl = dragging ? selected_axis : hover_axis;
    const bool any_focus = (hl != GIZMO_AXIS_NONE);
    const float shaft0 = gizmo_size * 0.12f;
    const float cone_len = gizmo_size * 0.22f;
    const float shaft1 = gizmo_size - cone_len;
    const float shaft_r = std::min(gizmo_size * 0.018f, 0.22f);
    const float cone_r = std::min(gizmo_size * 0.055f, 0.65f);
    float shaded[3];

    auto drawArrow = [&](GizmoAxis axis_id, int axis, const float base[3]) {
        shadeAxisColor(shaded, base, color_highlight, hl == axis_id, any_focus);
        meshAddAxisCylinder(mesh, shaded, axis, shaft0, shaft1, shaft_r);
        meshAddAxisCone(mesh, shaded, axis, gizmo_size, cone_len, cone_r);
    };

    drawArrow(GIZMO_AXIS_X, 0, color_x_axis);
    drawArrow(GIZMO_AXIS_Y, 1, color_y_axis);
    drawArrow(GIZMO_AXIS_Z, 2, color_z_axis);

    shadeAxisColor(shaded, color_center, color_highlight, hl == GIZMO_AXIS_CENTER, any_focus);
    meshAddCube(mesh, shaded, 0.0f, 0.0f, 0.0f, std::min(center_sphere_radius * 0.75f, 0.95f));
}

void Gizmo3D::appendRotateGizmoMesh(GizmoDrawMesh& mesh) const
{
    /* Qt3D RotationHandle: thin torus only — no bead spheres (virtualGizmo look). */
    const GizmoAxis hl = dragging ? selected_axis : hover_axis;
    const bool any_focus = (hl != GIZMO_AXIS_NONE);
    const float major = gizmo_size;
    const float minor = std::min(gizmo_size * 0.018f, 0.22f);
    float shaded[3];

    auto drawRing = [&](int axis, GizmoAxis gizmo_axis, const float base[3]) {
        shadeAxisColor(shaded, base, color_highlight, hl == gizmo_axis, any_focus);
        meshAddTorusRing(mesh, shaded, axis, major, minor);
    };

    drawRing(0, GIZMO_AXIS_X, color_x_axis);
    drawRing(1, GIZMO_AXIS_Y, color_y_axis);
    drawRing(2, GIZMO_AXIS_Z, color_z_axis);

    if(dragging &&
       (selected_axis == GIZMO_AXIS_X || selected_axis == GIZMO_AXIS_Y || selected_axis == GIZMO_AXIS_Z))
    {
        const float arc_radius = gizmo_size * 1.06f;
        const float start = rot_drag_start_angle;
        const float sweep = rot_drag_accum_degrees * ((float)M_PI / 180.0f);
        const float abs_sweep = fabsf(sweep);
        int segments = (int)lroundf(abs_sweep / ((float)M_PI / 24.0f));
        if(segments < 6)
        {
            segments = 6;
        }
        if(segments > 48)
        {
            segments = 48;
        }
        if(abs_sweep < 1e-4f)
        {
            segments = 0;
        }
        const float white[3] = {1.0f, 1.0f, 1.0f};
        const int axis = (selected_axis == GIZMO_AXIS_X) ? 0
                        : ((selected_axis == GIZMO_AXIS_Y) ? 1 : 2);
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
            if(axis == 0)
            {
                y0 = cosf(a0) * arc_radius;
                z0 = sinf(a0) * arc_radius;
                y1 = cosf(a1) * arc_radius;
                z1 = sinf(a1) * arc_radius;
            }
            else if(axis == 1)
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
            meshAddLine(mesh, 3.0f, white, x0, y0, z0, x1, y1, z1);
        }
    }

    shadeAxisColor(shaded, color_center, color_highlight, hl == GIZMO_AXIS_CENTER, any_focus);
    meshAddCube(mesh, shaded, 0.0f, 0.0f, 0.0f, std::min(center_sphere_radius * 0.75f, 0.95f));
}

void Gizmo3D::appendScaleGizmoMesh(GizmoDrawMesh& mesh) const
{
    /* Gizmo3D scale: thin shaft + square tip (no cone). */
    const GizmoAxis hl = dragging ? selected_axis : hover_axis;
    const bool any_focus = (hl != GIZMO_AXIS_NONE);
    const float shaft0 = gizmo_size * 0.12f;
    const float tip = std::min(gizmo_size * 0.08f, 0.95f);
    const float shaft_r = std::min(gizmo_size * 0.018f, 0.22f);
    float shaded[3];

    auto drawArm = [&](GizmoAxis axis_id, int axis, const float base[3]) {
        shadeAxisColor(shaded, base, color_highlight, hl == axis_id, any_focus);
        meshAddAxisCylinder(mesh, shaded, axis, shaft0, gizmo_size - tip * 0.15f, shaft_r);
        float tx = 0.0f;
        float ty = 0.0f;
        float tz = 0.0f;
        offsetPerp(axis, gizmo_size, 0.0f, 0.0f, tx, ty, tz);
        meshAddCube(mesh, shaded, tx, ty, tz, tip * 0.55f);
    };

    drawArm(GIZMO_AXIS_X, 0, color_x_axis);
    drawArm(GIZMO_AXIS_Y, 1, color_y_axis);
    drawArm(GIZMO_AXIS_Z, 2, color_z_axis);

    shadeAxisColor(shaded, color_center, color_highlight, hl == GIZMO_AXIS_CENTER, any_focus);
    meshAddCube(mesh, shaded, 0.0f, 0.0f, 0.0f, std::min(center_sphere_radius * 0.75f, 0.95f));
}

void Gizmo3D::appendSkewGizmoMesh(GizmoDrawMesh& mesh) const
{
    const GizmoAxis hl = dragging ? selected_axis : hover_axis;
    const bool any_focus = (hl != GIZMO_AXIS_NONE);
    const float shaft0 = gizmo_size * 0.12f;
    const float tip = gizmo_size * 0.14f;
    const float tip_w = gizmo_size * 0.05f;
    const float shaft_r = std::min(gizmo_size * 0.018f, 0.22f);
    float shaded[3];

    shadeAxisColor(shaded, color_x_axis, color_highlight, hl == GIZMO_AXIS_X, any_focus);
    meshAddAxisCylinder(mesh, shaded, 0, shaft0, gizmo_size - tip * 0.5f, shaft_r);
    meshAddTriangle(mesh, shaded, gizmo_size, 0.0f, 0.0f,
                    gizmo_size - tip, tip_w, tip_w * 0.5f,
                    gizmo_size - tip, -tip_w, -tip_w * 0.5f);

    shadeAxisColor(shaded, color_y_axis, color_highlight, hl == GIZMO_AXIS_Y, any_focus);
    meshAddAxisCylinder(mesh, shaded, 1, shaft0, gizmo_size - tip * 0.5f, shaft_r);
    meshAddTriangle(mesh, shaded, 0.0f, gizmo_size, 0.0f,
                    tip_w, gizmo_size - tip, tip_w * 0.5f,
                    -tip_w, gizmo_size - tip, -tip_w * 0.5f);

    shadeAxisColor(shaded, color_z_axis, color_highlight, hl == GIZMO_AXIS_Z, any_focus);
    meshAddAxisCylinder(mesh, shaded, 2, shaft0, gizmo_size - tip * 0.5f, shaft_r);
    meshAddTriangle(mesh, shaded, 0.0f, 0.0f, gizmo_size,
                    tip_w, tip_w * 0.5f, gizmo_size - tip,
                    -tip_w, -tip_w * 0.5f, gizmo_size - tip);

    shadeAxisColor(shaded, color_center, color_highlight, hl == GIZMO_AXIS_CENTER, any_focus);
    meshAddCube(mesh, shaded, 0.0f, 0.0f, 0.0f, std::min(center_sphere_radius * 0.75f, 0.95f));
}

void Gizmo3D::appendFreeroamGizmoMesh(GizmoDrawMesh& mesh) const
{
    const GizmoAxis hl = dragging ? selected_axis : hover_axis;
    const bool any_focus = (hl != GIZMO_AXIS_NONE);
    const float purple[3] = {0.55f, 0.35f, 0.95f};
    float stick[3];
    float center[3];
    shadeAxisColor(stick, purple, color_highlight, hl == GIZMO_AXIS_CENTER, any_focus);
    shadeAxisColor(center, color_center, color_highlight, hl == GIZMO_AXIS_CENTER, any_focus);

    const float shaft0 = gizmo_size * 0.12f;
    const float shaft_r = std::min(gizmo_size * 0.018f, 0.22f);
    const float tip = std::min(gizmo_size * 0.09f, 1.0f);
    meshAddAxisCylinder(mesh, stick, 1, shaft0, gizmo_size - tip * 0.2f, shaft_r);
    meshAddCube(mesh, stick, 0.0f, gizmo_size, 0.0f, tip);
    meshAddCube(mesh, center, 0.0f, 0.0f, 0.0f, std::min(center_sphere_radius * 0.75f, 0.95f));
}
