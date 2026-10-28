// SPDX-License-Identifier: GPL-2.0-only

#include "LEDViewport3D.h"
#include "ControllerDisplayUtils.h"
#include "ControllerLayout3D.h"
#include "Colors.h"
#include "viewport/MeshGeometry.h"
#include "viewport/ViewportMath.h"

#include <cmath>
#include <vector>

using MeshGeometry::PushPosColor;
using MeshGeometry::AppendAxisAlignedBoxFaces;
using MeshGeometry::AppendAxisAlignedBoxEdges;
using MeshGeometry::AppendControllerIndicatorSphere;

namespace
{
void ExpandBoundsForBlockerCell(Vector3D& min_bounds,
                                Vector3D& max_bounds,
                                float x0,
                                float y0,
                                float z0,
                                float x1,
                                float y1,
                                float z1)
{
    if(x0 < min_bounds.x) min_bounds.x = x0;
    if(y0 < min_bounds.y) min_bounds.y = y0;
    if(z0 < min_bounds.z) min_bounds.z = z0;
    if(x1 > max_bounds.x) max_bounds.x = x1;
    if(y1 > max_bounds.y) max_bounds.y = y1;
    if(z1 > max_bounds.z) max_bounds.z = z1;
}

bool TryGetViewportGlobalLedIndex(RGBControllerInterface* controller,
                                  unsigned int zone_idx,
                                  unsigned int led_idx,
                                  unsigned int* global_led_idx)
{
    if(!controller || !global_led_idx)
    {
        return false;
    }
    if(zone_idx >= controller->GetZoneCount())
    {
        return false;
    }
    if(led_idx >= controller->GetZoneLEDsCount(zone_idx))
    {
        return false;
    }

    *global_led_idx = controller->GetZoneStartIndex(zone_idx) + led_idx;
    return (*global_led_idx < controller->GetLEDCount());
}
} // namespace

void LEDViewport3D::DrawControllers()
{
    if(!controller_transforms) return;
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    for(size_t i = 0; i < controller_transforms->size(); i++)
    {
        ControllerTransform* ctrl = (*controller_transforms)[i].get();
        if(!ctrl || ctrl->hidden_by_virtual) continue;

        Vector3D min_bounds, max_bounds;
        CalculateControllerBounds(ctrl, min_bounds, max_bounds);

        /* Same LED-only pivot as UpdateWorldPositions — blockers must not shift draw vs effects. */
        const Vector3D led_center = ControllerLayout3D::GetLedLocalCenter(ctrl);
        const Vector3D center_offset = {
            -led_center.x,
            -led_center.y,
            -led_center.z
        };

        // Match effect world space: T×R×S×(-led_center).
        const ViewportMat4 model = ViewportMath::Multiply(
            ViewportMath::FromTransform3D(ctrl->transform),
            ViewportMath::Translation(center_offset.x, center_offset.y, center_offset.z));

        const bool is_selected = IsControllerSelected((int)i);
        const bool is_primary = ((int)i == selected_controller_idx);

        float face_r = 0.42f;
        float face_g = 0.46f;
        float face_b = 0.52f;
        float face_alpha = 0.10f;
        float edge_r = 0.50f;
        float edge_g = 0.54f;
        float edge_b = 0.60f;
        float edge_width = 1.25f;

        if(is_primary)
        {
            face_r = 0.95f;
            face_g = 0.85f;
            face_b = 0.20f;
            face_alpha = 0.18f;
            edge_r = 1.0f;
            edge_g = 0.95f;
            edge_b = 0.35f;
            edge_width = 2.0f;
        }
        else if(is_selected)
        {
            face_r = 0.90f;
            face_g = 0.70f;
            face_b = 0.15f;
            face_alpha = 0.14f;
            edge_r = 1.0f;
            edge_g = 0.80f;
            edge_b = 0.25f;
            edge_width = 1.75f;
        }

        std::vector<float> faces;
        AppendAxisAlignedBoxFaces(faces,
                                  min_bounds.x, min_bounds.y, min_bounds.z,
                                  max_bounds.x, max_bounds.y, max_bounds.z,
                                  face_r, face_g, face_b);
        controller_faces_batch_.Upload(MeshBatch::Layout::PosColor, faces.data(), faces.size() / 6);

        /* Alpha faces must not write depth or interior LED points disappear when zoomed in. */
        glDepthMask(GL_FALSE);
        drawUnlitBatch(controller_faces_batch_, MeshBatch::Primitive::Triangles, 1.0f, face_alpha, &model);
        glDepthMask(GL_TRUE);

        DrawLEDs(ctrl, model);

        std::vector<float> edges;
        AppendAxisAlignedBoxEdges(edges,
                                  min_bounds.x, min_bounds.y, min_bounds.z,
                                  max_bounds.x, max_bounds.y, max_bounds.z,
                                  edge_r, edge_g, edge_b);
        controller_edges_batch_.Upload(MeshBatch::Layout::PosColor, edges.data(), edges.size() / 6);
        drawUnlitBatch(controller_edges_batch_, MeshBatch::Primitive::Lines, edge_width, 1.0f, &model);

        std::vector<float> indicator;
        AppendControllerIndicatorSphere(indicator,
                                        (min_bounds.x + max_bounds.x) * 0.5f,
                                        max_bounds.y,
                                        (min_bounds.z + max_bounds.z) * 0.5f,
                                        0.15f,
                                        8);
        controller_indicator_batch_.Upload(MeshBatch::Layout::PosColor, indicator.data(), indicator.size() / 6);
        drawUnlitBatch(controller_indicator_batch_, MeshBatch::Primitive::Triangles, 1.0f, 1.0f, &model);
    }
}

void LEDViewport3D::DrawLEDs(ControllerTransform* ctrl, const ViewportMat4& model)
{
    if(!ctrl || ctrl->hidden_by_virtual) return;
    if(!ctrl->controller && !ctrl->virtual_controller) return;

    const size_t draw_count = populateLedDrawBuffers(ctrl);
    if(draw_count == 0)
    {
        return;
    }

    controller_leds_batch_.Upload(MeshBatch::Layout::PosColor,
                                  led_draw_interleaved_.data(),
                                  draw_count);

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    drawUnlitPoints(controller_leds_batch_, ledPreviewPointSizeGl(), 1.0f, model);
}

size_t LEDViewport3D::populateLedDrawBuffers(ControllerTransform* ctrl)
{
    if(!ctrl || ctrl->hidden_by_virtual)
    {
        return 0;
    }
    if(!ctrl->controller && !ctrl->virtual_controller)
    {
        return 0;
    }

    const size_t led_count = ctrl->led_positions.size();
    if(led_count == 0)
    {
        return 0;
    }

    auto resolve_mapped_led_color = [](const LEDPosition3D& led_pos) -> RGBColor
    {
        RGBColor color = led_pos.preview_color;
        if(!led_pos.controller)
        {
            return color;
        }

        unsigned int global_led_idx = 0;
        if(!TryGetViewportGlobalLedIndex(led_pos.controller, led_pos.zone_idx, led_pos.led_idx, &global_led_idx))
        {
            return color;
        }

        const RGBColor live_color = led_pos.controller->GetColor(global_led_idx);
        if(color == 0x00FFFFFF)
        {
            return live_color;
        }

        return color;
    };

    std::vector<ControllerLayout3D::ViewportStripDrawSample> strip_samples;
    strip_samples.reserve(led_count * 3);

    led_draw_interleaved_.clear();
    led_draw_interleaved_.reserve(led_count * 9 * 6);

    ControllerLayout3D::BuildViewportStripDrawSamples(ctrl, grid_scale_mm, strip_samples);
    for(const ControllerLayout3D::ViewportStripDrawSample& sample : strip_samples)
    {
        const RGBColor mapped_color = resolve_mapped_led_color(ctrl->led_positions[sample.logical_index]);
        const float    r            = static_cast<float>(RGBGetRValue(mapped_color)) / 255.0f;
        const float    g            = static_cast<float>(RGBGetGValue(mapped_color)) / 255.0f;
        const float    b            = static_cast<float>(RGBGetBValue(mapped_color)) / 255.0f;

        PushPosColor(led_draw_interleaved_, sample.position.x, sample.position.y, sample.position.z, r, g, b);
    }

    return led_draw_interleaved_.size() / 6;
}

void LEDViewport3D::CalculateControllerBounds(ControllerTransform* ctrl, Vector3D& min_bounds, Vector3D& max_bounds)
{
    bool have_bounds = false;
    if(ctrl && !ctrl->led_positions.empty())
    {
        Vector3D first_pos = ctrl->led_positions[0].local_position;
        min_bounds = first_pos;
        max_bounds = first_pos;
        have_bounds = true;

        for(unsigned int i = 0; i < ctrl->led_positions.size(); i++)
        {
            const Vector3D& pos = ctrl->led_positions[i].local_position;

            if(pos.x < min_bounds.x) min_bounds.x = pos.x;
            if(pos.y < min_bounds.y) min_bounds.y = pos.y;
            if(pos.z < min_bounds.z) min_bounds.z = pos.z;

            if(pos.x > max_bounds.x) max_bounds.x = pos.x;
            if(pos.y > max_bounds.y) max_bounds.y = pos.y;
            if(pos.z > max_bounds.z) max_bounds.z = pos.z;
        }
    }

    if(ctrl && ctrl->virtual_controller)
    {
        for(const CustomControllerLightBlocker& blocker : ctrl->virtual_controller->GetLightBlockers())
        {
            Vector3D local_min{};
            Vector3D local_max{};
            ctrl->virtual_controller->CellLocalBoundsMm(blocker.x, blocker.y, blocker.z, &local_min, &local_max);
            const float x0 = MMToGridUnits(local_min.x, grid_scale_mm);
            const float y0 = MMToGridUnits(local_min.y, grid_scale_mm);
            const float z0 = MMToGridUnits(local_min.z, grid_scale_mm);
            const float x1 = MMToGridUnits(local_max.x, grid_scale_mm);
            const float y1 = MMToGridUnits(local_max.y, grid_scale_mm);
            const float z1 = MMToGridUnits(local_max.z, grid_scale_mm);

            if(!have_bounds)
            {
                min_bounds = {x0, y0, z0};
                max_bounds = {x1, y1, z1};
                have_bounds = true;
            }
            else
            {
                ExpandBoundsForBlockerCell(min_bounds, max_bounds, x0, y0, z0, x1, y1, z1);
            }
        }
    }

    if(!have_bounds)
    {
        min_bounds = {-0.5f, -0.5f, -0.5f};
        max_bounds = {0.5f, 0.5f, 0.5f};
        return;
    }

    float size_x = max_bounds.x - min_bounds.x;
    float size_y = max_bounds.y - min_bounds.y;
    float size_z = max_bounds.z - min_bounds.z;

    float min_dimension = 0.2f;

    if(size_x < 0.001f)
    {
        float center_x = (min_bounds.x + max_bounds.x) * 0.5f;
        min_bounds.x = center_x - min_dimension;
        max_bounds.x = center_x + min_dimension;
    }
    if(size_y < 0.001f)
    {
        float center_y = (min_bounds.y + max_bounds.y) * 0.5f;
        min_bounds.y = center_y - min_dimension;
        max_bounds.y = center_y + min_dimension;
    }
    if(size_z < 0.001f)
    {
        float center_z = (min_bounds.z + max_bounds.z) * 0.5f;
        min_bounds.z = center_z - min_dimension;
        max_bounds.z = center_z + min_dimension;
    }

    float padding = 0.1f;
    min_bounds.x -= padding;
    min_bounds.y -= padding;
    min_bounds.z -= padding;
    max_bounds.x += padding;
    max_bounds.y += padding;
    max_bounds.z += padding;
}

Vector3D LEDViewport3D::GetControllerCenter(ControllerTransform* ctrl)
{
    return ControllerLayout3D::GetControllerCenterWorld(ctrl);
}

Vector3D LEDViewport3D::GetControllerSize(ControllerTransform* ctrl)
{
    Vector3D min_bounds, max_bounds;
    CalculateControllerBounds(ctrl, min_bounds, max_bounds);

    return {
        max_bounds.x - min_bounds.x,
        max_bounds.y - min_bounds.y,
        max_bounds.z - min_bounds.z
    };
}
