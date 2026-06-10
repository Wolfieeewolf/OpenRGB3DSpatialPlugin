// SPDX-License-Identifier: GPL-2.0-only

#include "EmitterRelayMirror.h"

#include "ControllerLayout3D.h"
#include "DisplayPlane3D.h"
#include "Geometry3DUtils.h"
#include "GridSpaceUtils.h"
#include "LEDPosition3D.h"
#include "SpatialLighting/SpatialLightingEngine.h"

#include <algorithm>
#include <cmath>

namespace EmitterRelayMirror
{

namespace
{

void FinalizeSurface(EmitterSurface& surface)
{
    const size_t px = static_cast<size_t>(surface.tex_w * surface.tex_h);
    for(size_t i = 0; i < px; ++i)
    {
        const uint8_t w = surface.tex_weight[i];
        if(w == 0)
        {
            continue;
        }
        const float inv = 1.0f / static_cast<float>(w);
        surface.tex_rgb[i * 3 + 0] *= inv;
        surface.tex_rgb[i * 3 + 1] *= inv;
        surface.tex_rgb[i * 3 + 2] *= inv;
    }
}

void SplatSample(EmitterSurface& surface, float u, float v, uint8_t r, uint8_t g, uint8_t b)
{
    u = std::clamp(u, 0.0f, 1.0f);
    v = std::clamp(v, 0.0f, 1.0f);
    const int x = std::clamp(static_cast<int>(u * static_cast<float>(surface.tex_w - 1)), 0, surface.tex_w - 1);
    const int y = std::clamp(static_cast<int>(v * static_cast<float>(surface.tex_h - 1)), 0, surface.tex_h - 1);
    const size_t idx = static_cast<size_t>(y * surface.tex_w + x);
    surface.tex_rgb[idx * 3 + 0] += static_cast<float>(r);
    surface.tex_rgb[idx * 3 + 1] += static_cast<float>(g);
    surface.tex_rgb[idx * 3 + 2] += static_cast<float>(b);
    if(surface.tex_weight[idx] < 255)
    {
        ++surface.tex_weight[idx];
    }
}

RGBColor SampleBilinear(const EmitterSurface& surface, float u, float v)
{
    u = std::clamp(u, 0.0f, 1.0f);
    v = std::clamp(v, 0.0f, 1.0f);
    const float fx = u * static_cast<float>(surface.tex_w - 1);
    const float fy = v * static_cast<float>(surface.tex_h - 1);
    const int x0 = static_cast<int>(std::floor(fx));
    const int y0 = static_cast<int>(std::floor(fy));
    const int x1 = std::min(x0 + 1, surface.tex_w - 1);
    const int y1 = std::min(y0 + 1, surface.tex_h - 1);
    const float tx = fx - static_cast<float>(x0);
    const float ty = fy - static_cast<float>(y0);

    auto fetch = [&](int x, int y) -> std::array<float, 3> {
        const size_t idx = static_cast<size_t>(y * surface.tex_w + x);
        if(surface.tex_weight[idx] == 0)
        {
            return {0.0f, 0.0f, 0.0f};
        }
        return {surface.tex_rgb[idx * 3 + 0], surface.tex_rgb[idx * 3 + 1], surface.tex_rgb[idx * 3 + 2]};
    };

    const auto c00 = fetch(x0, y0);
    const auto c10 = fetch(x1, y0);
    const auto c01 = fetch(x0, y1);
    const auto c11 = fetch(x1, y1);

    float rf = (1.0f - tx) * (1.0f - ty) * c00[0] + tx * (1.0f - ty) * c10[0] + (1.0f - tx) * ty * c01[0] +
               tx * ty * c11[0];
    float gf = (1.0f - tx) * (1.0f - ty) * c00[1] + tx * (1.0f - ty) * c10[1] + (1.0f - tx) * ty * c01[1] +
               tx * ty * c11[1];
    float bf = (1.0f - tx) * (1.0f - ty) * c00[2] + tx * (1.0f - ty) * c10[2] + (1.0f - tx) * ty * c01[2] +
               tx * ty * c11[2];

    return ToRGBColor(static_cast<uint8_t>(std::clamp(rf, 0.0f, 255.0f)),
                      static_cast<uint8_t>(std::clamp(gf, 0.0f, 255.0f)),
                      static_cast<uint8_t>(std::clamp(bf, 0.0f, 255.0f)));
}

Vector3D SamplePointOnEmitterSurface(const EmitterSurface& surface, float u, float v)
{
    const float half_w = surface.width_grid * 0.5f;
    const float half_h = surface.height_grid * 0.5f;
    const Vector3D local = {
        -half_w + u * surface.width_grid,
        -half_h + v * surface.height_grid,
        0.0f,
    };
    Transform3D transform{};
    transform.position = surface.plane_center_room;
    transform.rotation = surface.plane_rotation;
    transform.scale = {1.0f, 1.0f, 1.0f};
    return Geometry3D::TransformDisplayPlaneLocalToWorld(local, transform);
}

DisplayPlane3D MakeVirtualPlane(const EmitterSurface& surface, float grid_scale_mm)
{
    DisplayPlane3D plane("emitter");
    Transform3D& t = plane.GetTransform();
    t.position = surface.plane_center_room;
    t.rotation = surface.plane_rotation;
    t.scale = {1.0f, 1.0f, 1.0f};
    const float scale_mm = (grid_scale_mm > 0.001f) ? grid_scale_mm : 10.0f;
    plane.SetWidthMM(GridUnitsToMM(surface.width_grid, scale_mm));
    plane.SetHeightMM(GridUnitsToMM(surface.height_grid, scale_mm));
    return plane;
}

} // namespace

void BuildSurfaceFromSamples(int controller_index,
                             const ControllerTransform* ctrl,
                             const std::vector<LedColorSample>& samples,
                             EmitterSurface& out)
{
    out = EmitterSurface{};
    out.controller_index = controller_index;
    if(!ctrl || samples.empty())
    {
        return;
    }

    Vector3D min_bounds{};
    Vector3D max_bounds{};
    ControllerLayout3D::CalculateControllerLocalBounds(ctrl, min_bounds, max_bounds);
    const float span_x = std::max(max_bounds.x - min_bounds.x, 0.01f);
    const float span_y = std::max(max_bounds.y - min_bounds.y, 0.01f);
    out.width_grid = span_x;
    out.height_grid = span_y;
    out.plane_rotation = ctrl->transform.rotation;

    float cx = 0.0f;
    float cy = 0.0f;
    float cz = 0.0f;
    int room_count = 0;
    for(const LEDPosition3D& led : ctrl->led_positions)
    {
        cx += led.room_position.x;
        cy += led.room_position.y;
        cz += led.room_position.z;
        ++room_count;
    }
    if(room_count > 0)
    {
        const float inv = 1.0f / static_cast<float>(room_count);
        out.plane_center_room = {cx * inv, cy * inv, cz * inv};
    }
    else
    {
        const Vector3D center_local = {
            (min_bounds.x + max_bounds.x) * 0.5f,
            (min_bounds.y + max_bounds.y) * 0.5f,
            (min_bounds.z + max_bounds.z) * 0.5f,
        };
        out.plane_center_room = ControllerLayout3D::CalculateWorldPosition(center_local, ctrl->transform);
    }

    const size_t px = static_cast<size_t>(out.tex_w * out.tex_h);
    out.tex_rgb.assign(px * 3, 0.0f);
    out.tex_weight.assign(px, 0);

    for(const LedColorSample& sample : samples)
    {
        SplatSample(out, sample.u, sample.v, sample.r, sample.g, sample.b);
    }
    FinalizeSurface(out);
}

RGBColor SampleReceiver(const MirrorFrame& frame,
                        float room_x,
                        float room_y,
                        float room_z,
                        const MirrorShadeContext* shade_ctx)
{
    if(frame.surfaces.empty())
    {
        return 0x00000000;
    }

    const Vector3D led_pos = {room_x, room_y, room_z};
    const Vector3D& ref = frame.reference_room;
    const float scale_mm = (frame.grid_scale_mm > 0.001f) ? frame.grid_scale_mm : 10.0f;

    const bool shade_enabled = shade_ctx && shade_ctx->shade && shade_ctx->occluder_aabbs && shade_ctx->occluders;
    const bool has_occluders =
        shade_enabled && (!shade_ctx->occluder_aabbs->empty() || !shade_ctx->occluders->empty());
    const bool use_line_occlusion =
        shade_enabled && !shade_ctx->overlay_preview && shade_ctx->shade->use_occlusion && has_occluders;

    float total_r = 0.0f;
    float total_g = 0.0f;
    float total_b = 0.0f;
    float total_w = 0.0f;

    for(const EmitterSurface& surface : frame.surfaces)
    {
        DisplayPlane3D plane = MakeVirtualPlane(surface, frame.grid_scale_mm);

        Geometry3D::PlaneProjection proj =
            Geometry3D::SpatialMapToScreen(led_pos, plane, 0.0f, &ref, scale_mm);
        if(!proj.is_valid)
        {
            continue;
        }

        const float u = std::clamp(proj.u, 0.0f, 1.0f);
        const float v = std::clamp(proj.v, 0.0f, 1.0f);

        if(use_line_occlusion)
        {
            const Vector3D emitter_point = SamplePointOnEmitterSurface(surface, u, v);
            if(SpatialLighting::LedSegmentOccluded(room_x,
                                                     room_y,
                                                     room_z,
                                                     emitter_point.x,
                                                     emitter_point.y,
                                                     emitter_point.z,
                                                     *shade_ctx->occluder_aabbs,
                                                     *shade_ctx->occluders,
                                                     surface.controller_index))
            {
                continue;
            }
        }

        RGBColor sampled = SampleBilinear(surface, u, v);
        const float rf = static_cast<float>(sampled & 0xFF);
        const float gf = static_cast<float>((sampled >> 8) & 0xFF);
        const float bf = static_cast<float>((sampled >> 16) & 0xFF);
        if(rf + gf + bf < 1.5f)
        {
            continue;
        }

        float effective_range = std::max(frame.reference_max_distance_mm * std::clamp(frame.coverage, 0.05f, 3.0f),
                                         10.0f);
        const float weight = Geometry3D::ComputeFalloff(proj.distance, effective_range, frame.edge_softness);
        if(weight < 0.01f)
        {
            continue;
        }

        total_r += rf * weight;
        total_g += gf * weight;
        total_b += bf * weight;
        total_w += weight;
    }

    if(total_w < 1e-5f)
    {
        return 0x00000000;
    }

    const float bright = std::max(0.05f, frame.brightness);
    total_r = total_r / total_w * bright;
    total_g = total_g / total_w * bright;
    total_b = total_b / total_w * bright;

    float ao_factor = 1.0f;
    if(shade_enabled && !shade_ctx->overlay_preview && shade_ctx->shade->use_occlusion &&
       shade_ctx->shade->use_ambient_occlusion && has_occluders && shade_ctx->shade->ao_strength > 0.001f)
    {
        const float openness = SpatialLighting::ComputeLedAmbientOcclusion(room_x,
                                                                           room_y,
                                                                           room_z,
                                                                           *shade_ctx->occluder_aabbs,
                                                                           *shade_ctx->occluders,
                                                                           shade_ctx->shade->ao_probe_span);
        ao_factor = 1.0f - shade_ctx->shade->ao_strength * (1.0f - openness);
    }

    total_r *= ao_factor;
    total_g *= ao_factor;
    total_b *= ao_factor;

    return ToRGBColor(static_cast<uint8_t>(std::clamp(total_r, 0.0f, 255.0f)),
                      static_cast<uint8_t>(std::clamp(total_g, 0.0f, 255.0f)),
                      static_cast<uint8_t>(std::clamp(total_b, 0.0f, 255.0f)));
}

} // namespace EmitterRelayMirror
