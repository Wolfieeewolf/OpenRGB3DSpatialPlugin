// SPDX-License-Identifier: GPL-2.0-only

#include "LEDViewport3D.h"
#include "ControllerLayout3D.h"
#include "VirtualReferencePoint3D.h"
#include "viewport/MeshGeometry.h"
#include "viewport/ViewportMath.h"

#include <cmath>
#include <vector>

using MeshGeometry::PushPosColor;
using MeshGeometry::PushTri;
using MeshGeometry::AppendAxisAlignedBoxFaces;
using MeshGeometry::AppendAxisAlignedBoxEdges;
using MeshGeometry::AppendFlatQuadXY;
using MeshGeometry::AppendFlatQuadBorderXY;
using MeshGeometry::AppendCircleLineLoopXY;
using MeshGeometry::AppendCircleLineLoopXZ;
using MeshGeometry::AppendUVSphere;

namespace
{
ViewportMat4 ModelFromPosRot(const Vector3D& pos, const Rotation3D& rot)
{
    Transform3D xf{};
    xf.position = pos;
    xf.rotation = rot;
    xf.scale = {1.0f, 1.0f, 1.0f};
    return ViewportMath::FromTransform3D(xf);
}
} // namespace

void LEDViewport3D::DrawLightBlockerLayers()
{
    if(!controller_transforms)
    {
        return;
    }
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);

    constexpr float kCellFillR = 0.22f;
    constexpr float kCellFillG = 0.14f;
    constexpr float kCellFillB = 0.29f;
    constexpr float kCellFillA = 0.38f;
    constexpr float kBorderR = 0.47f;
    constexpr float kBorderG = 0.27f;
    constexpr float kBorderB = 0.63f;
    constexpr float kBorderA = 0.85f;

    for(size_t i = 0; i < controller_transforms->size(); i++)
    {
        ControllerTransform* ctrl = (*controller_transforms)[i].get();
        if(!ctrl || ctrl->hidden_by_virtual || !ctrl->virtual_controller)
        {
            continue;
        }

        VirtualController3D* layout = ctrl->virtual_controller;
        const std::vector<CustomControllerLightBlocker>& blockers = layout->GetLightBlockers();
        if(blockers.empty())
        {
            continue;
        }

        Vector3D min_bounds{};
        Vector3D max_bounds{};
        CalculateControllerBounds(ctrl, min_bounds, max_bounds);
        const Vector3D led_center = ControllerLayout3D::GetLedLocalCenter(ctrl);
        const Vector3D center_offset = {
            -led_center.x,
            -led_center.y,
            -led_center.z,
        };

        const bool is_primary = ((int)i == selected_controller_idx);
        const bool is_selected = IsControllerSelected((int)i);
        const float cell_alpha = is_primary ? 0.50f : (is_selected ? 0.44f : kCellFillA);
        const float border_width = is_primary ? 2.5f : (is_selected ? 2.0f : 1.5f);

        const ViewportMat4 model = ViewportMath::Multiply(
            ViewportMath::FromTransform3D(ctrl->transform),
            ViewportMath::Translation(center_offset.x, center_offset.y, center_offset.z));

        std::vector<float> fills;
        std::vector<float> borders;
        fills.reserve(blockers.size() * 6 * 6);
        borders.reserve(blockers.size() * 8 * 6);

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

            AppendFlatQuadXY(fills, x0, y0, x1, y1, z_plane, kCellFillR, kCellFillG, kCellFillB);
            AppendFlatQuadBorderXY(borders, x0, y0, x1, y1, z_plane, kBorderR, kBorderG, kBorderB);
        }

        if(!fills.empty())
        {
            controller_faces_batch_.Upload(MeshBatch::Layout::PosColor, fills.data(), fills.size() / 6);
            drawUnlitBatch(controller_faces_batch_, MeshBatch::Primitive::Triangles, 1.0f, cell_alpha, &model);
        }
        if(!borders.empty())
        {
            controller_edges_batch_.Upload(MeshBatch::Layout::PosColor, borders.data(), borders.size() / 6);
            drawUnlitBatch(controller_edges_batch_, MeshBatch::Primitive::Lines, border_width, kBorderA, &model);
        }
    }

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

void LEDViewport3D::DrawUserFigure()
{
    if(!reference_points) return;

    for(size_t idx = 0; idx < reference_points->size(); idx++)
    {
        VirtualReferencePoint3D* ref_point = (*reference_points)[idx].get();
        if(ref_point->GetType() != REF_POINT_USER || !ref_point->IsVisible())
        {
            continue;
        }

        const bool is_selected = ((int)idx == selected_ref_point_idx);
        const Vector3D pos = ref_point->GetPosition();
        const Rotation3D rot = ref_point->GetRotation();
        const RGBColor color = ref_point->GetDisplayColor();

        const float r = (color & 0xFF) / 255.0f;
        const float g = ((color >> 8) & 0xFF) / 255.0f;
        const float b = ((color >> 16) & 0xFF) / 255.0f;
        const ViewportMat4 model = ModelFromPosRot(pos, rot);

        const float head_radius = 0.4f;
        const int segments = 20;

        std::vector<float> lines;
        AppendCircleLineLoopXY(lines, head_radius, segments, r, g, b);
        for(int i = 0; i < 10; ++i)
        {
            const float t0 = (float)i / 10.0f;
            const float t1 = (float)(i + 1) / 10.0f;
            const float a0 = (float)M_PI + t0 * (float)M_PI;
            const float a1 = (float)M_PI + t1 * (float)M_PI;
            PushPosColor(lines, 0.25f * std::cos(a0), -0.05f + 0.25f * std::sin(a0), 0.0f, r, g, b);
            PushPosColor(lines, 0.25f * std::cos(a1), -0.05f + 0.25f * std::sin(a1), 0.0f, r, g, b);
        }
        controller_edges_batch_.Upload(MeshBatch::Layout::PosColor, lines.data(), lines.size() / 6);
        drawUnlitBatch(controller_edges_batch_, MeshBatch::Primitive::Lines, 2.0f, 1.0f, &model);

        const float eyes[] = {
            -0.15f, 0.1f, 0.0f, r, g, b,
            0.15f, 0.1f, 0.0f, r, g, b,
        };
        controller_leds_batch_.Upload(MeshBatch::Layout::PosColor, eyes, 2);
        drawUnlitPoints(controller_leds_batch_, 6.0f, 1.0f, model);

        if(is_selected)
        {
            glDisable(GL_DEPTH_TEST);
            const float box_size = head_radius * 1.5f;
            std::vector<float> sel;
            AppendAxisAlignedBoxEdges(sel, -box_size, -box_size, -box_size, box_size, box_size, box_size, 1.0f, 1.0f, 0.0f);
            controller_edges_batch_.Upload(MeshBatch::Layout::PosColor, sel.data(), sel.size() / 6);
            drawUnlitBatch(controller_edges_batch_, MeshBatch::Primitive::Lines, 3.0f, 1.0f, &model);
            glEnable(GL_DEPTH_TEST);
        }

        break;
    }
}

void LEDViewport3D::DrawReferencePoints()
{
    if(!reference_points) return;

    const float sphere_radius = 0.3f;
    const int segments = 16;
    const int rings = 12;

    for(size_t idx = 0; idx < reference_points->size(); idx++)
    {
        VirtualReferencePoint3D* ref_point = (*reference_points)[idx].get();
        if(!ref_point->IsVisible() || ref_point->GetType() == REF_POINT_USER)
        {
            continue;
        }

        const bool is_selected = ((int)idx == selected_ref_point_idx);
        const Vector3D pos = ref_point->GetPosition();
        const Rotation3D rot = ref_point->GetRotation();
        const RGBColor color = ref_point->GetDisplayColor();

        const float r = (color & 0xFF) / 255.0f;
        const float g = ((color >> 8) & 0xFF) / 255.0f;
        const float b = ((color >> 16) & 0xFF) / 255.0f;
        const ViewportMat4 model = ModelFromPosRot(pos, rot);

        std::vector<float> sphere;
        AppendUVSphere(sphere, sphere_radius, segments, rings, r, g, b);
        controller_faces_batch_.Upload(MeshBatch::Layout::PosColor, sphere.data(), sphere.size() / 6);
        drawUnlitBatch(controller_faces_batch_, MeshBatch::Primitive::Triangles, 1.0f, 1.0f, &model);

        std::vector<float> ring;
        if(is_selected)
        {
            AppendCircleLineLoopXZ(ring, sphere_radius, segments, 1.0f, 1.0f, 0.0f);
        }
        else
        {
            AppendCircleLineLoopXZ(ring, sphere_radius, segments, r * 0.5f, g * 0.5f, b * 0.5f);
        }
        controller_edges_batch_.Upload(MeshBatch::Layout::PosColor, ring.data(), ring.size() / 6);
        drawUnlitBatch(controller_edges_batch_, MeshBatch::Primitive::Lines, is_selected ? 4.0f : 2.0f, 1.0f, &model);

        if(is_selected)
        {
            glDisable(GL_DEPTH_TEST);
            const float box_size = sphere_radius * 1.5f;
            std::vector<float> sel;
            AppendAxisAlignedBoxEdges(sel, -box_size, -box_size, -box_size, box_size, box_size, box_size, 1.0f, 1.0f, 0.0f);
            controller_edges_batch_.Upload(MeshBatch::Layout::PosColor, sel.data(), sel.size() / 6);
            drawUnlitBatch(controller_edges_batch_, MeshBatch::Primitive::Lines, 3.0f, 1.0f, &model);
            glEnable(GL_DEPTH_TEST);
        }
    }
}

