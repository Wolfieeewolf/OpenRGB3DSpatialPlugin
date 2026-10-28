// SPDX-License-Identifier: GPL-2.0-only

#include "EmitterRelayMirror.h"

#include "ControllerLayout3D.h"
#include "Geometry3DUtils.h"
#include "GridSpaceUtils.h"
#include "LEDPosition3D.h"
#include "SpatialLighting/SpatialLightingEngine.h"
#include "SpatialLighting/SpatialLightingSceneProvider.h"
#include "SpatialLighting/OccluderSpatialIndex.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace EmitterRelayMirror
{

namespace
{

void SplatSampleDominant(EmitterSurface& surface, float u, float v, uint8_t r, uint8_t g, uint8_t b)
{
    u = std::clamp(u, 0.0f, 1.0f);
    v = std::clamp(v, 0.0f, 1.0f);
    const int x = std::clamp(static_cast<int>(u * static_cast<float>(surface.tex_w - 1)), 0, surface.tex_w - 1);
    const int y = std::clamp(static_cast<int>(v * static_cast<float>(surface.tex_h - 1)), 0, surface.tex_h - 1);
    const size_t idx = static_cast<size_t>(y * surface.tex_w + x);
    const float luma = static_cast<float>(r) + static_cast<float>(g) + static_cast<float>(b);
    if(surface.tex_weight[idx] == 0)
    {
        surface.tex_rgb[idx * 3 + 0] = static_cast<float>(r);
        surface.tex_rgb[idx * 3 + 1] = static_cast<float>(g);
        surface.tex_rgb[idx * 3 + 2] = static_cast<float>(b);
        surface.tex_weight[idx] = 1;
        return;
    }
    const float existing_luma = surface.tex_rgb[idx * 3 + 0] + surface.tex_rgb[idx * 3 + 1] + surface.tex_rgb[idx * 3 + 2];
    if(luma > existing_luma)
    {
        surface.tex_rgb[idx * 3 + 0] = static_cast<float>(r);
        surface.tex_rgb[idx * 3 + 1] = static_cast<float>(g);
        surface.tex_rgb[idx * 3 + 2] = static_cast<float>(b);
    }
}

RGBColor SampleBilinearWeighted(const EmitterSurface& surface, float u, float v)
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

    const float w00 = (surface.tex_weight[static_cast<size_t>(y0 * surface.tex_w + x0)] > 0) ? (1.0f - tx) * (1.0f - ty) : 0.0f;
    const float w10 = (surface.tex_weight[static_cast<size_t>(y0 * surface.tex_w + x1)] > 0) ? tx * (1.0f - ty) : 0.0f;
    const float w01 = (surface.tex_weight[static_cast<size_t>(y1 * surface.tex_w + x0)] > 0) ? (1.0f - tx) * ty : 0.0f;
    const float w11 = (surface.tex_weight[static_cast<size_t>(y1 * surface.tex_w + x1)] > 0) ? tx * ty : 0.0f;
    const float w_sum = w00 + w10 + w01 + w11;
    if(w_sum < 1e-5f)
    {
        return 0x00000000;
    }

    float rf = (w00 * c00[0] + w10 * c10[0] + w01 * c01[0] + w11 * c11[0]) / w_sum;
    float gf = (w00 * c00[1] + w10 * c10[1] + w01 * c01[1] + w11 * c11[1]) / w_sum;
    float bf = (w00 * c00[2] + w10 * c10[2] + w01 * c01[2] + w11 * c11[2]) / w_sum;

    return ToRGBColor(static_cast<uint8_t>(std::clamp(rf, 0.0f, 255.0f)),
                      static_cast<uint8_t>(std::clamp(gf, 0.0f, 255.0f)),
                      static_cast<uint8_t>(std::clamp(bf, 0.0f, 255.0f)));
}

struct EmitterReceiverProjection
{
    float u = 0.5f;
    float v = 0.5f;
    float distance_mm = 0.0f;
    bool is_valid = false;
};

EmitterReceiverProjection ProjectReceiverOntoEmitterSurface(const Vector3D& receiver_room,
                                                            const EmitterSurface& surface,
                                                            float grid_scale_mm)
{
    EmitterReceiverProjection result{};
    if(surface.width_grid < 1e-5f || surface.height_grid < 1e-5f)
    {
        return result;
    }

    Transform3D transform{};
    transform.position = surface.plane_center_room;
    transform.rotation = surface.plane_rotation;
    transform.scale = {1.0f, 1.0f, 1.0f};

    const Vector3D local = Geometry3D::TransformDisplayPlaneWorldToLocal(receiver_room, transform);
    const float half_w = surface.width_grid * 0.5f;
    const float half_h = surface.height_grid * 0.5f;
    const float u = (local.x + half_w) / surface.width_grid;
    const float v = (local.y + half_h) / surface.height_grid;

    static constexpr float kFootprintBleed = 0.06f;
    if(u < -kFootprintBleed || u > 1.0f + kFootprintBleed || v < -kFootprintBleed || v > 1.0f + kFootprintBleed)
    {
        return result;
    }

    const float scale_mm = SafeGridScaleMm(grid_scale_mm);
    result.u = std::clamp(u, 0.0f, 1.0f);
    result.v = std::clamp(v, 0.0f, 1.0f);
    result.distance_mm = GridUnitsToMM(std::fabs(local.z), scale_mm);
    result.is_valid = true;
    return result;
}

void ComputeAutoTextureDimensions(size_t led_count, float width_grid, float height_grid, int& out_w, int& out_h)
{
    static constexpr int kMinSide = 32;
    static constexpr int kMaxSide = 160;

    const float led_f = static_cast<float>(std::max<size_t>(1, led_count));
    int short_side = static_cast<int>(std::lround(std::sqrt(led_f) * 3.5f));
    short_side = std::clamp(short_side, kMinSide, kMaxSide);

    if(width_grid >= height_grid)
    {
        out_h = short_side;
        out_w = static_cast<int>(std::lround(static_cast<float>(short_side) *
                                              (width_grid / std::max(height_grid, 0.01f))));
    }
    else
    {
        out_w = short_side;
        out_h = static_cast<int>(std::lround(static_cast<float>(short_side) *
                                              (height_grid / std::max(width_grid, 0.01f))));
    }

    out_w = std::clamp(out_w, kMinSide, kMaxSide);
    out_h = std::clamp(out_h, kMinSide, kMaxSide);
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

    out.plane_center_room = ControllerLayout3D::GetControllerCenterWorld(ctrl);

    const size_t layout_led_count =
        ctrl->led_positions.empty() ? samples.size() : ctrl->led_positions.size();
    ComputeAutoTextureDimensions(layout_led_count, span_x, span_y, out.tex_w, out.tex_h);

    const size_t px = static_cast<size_t>(out.tex_w * out.tex_h);
    out.tex_rgb.assign(px * 3, 0.0f);
    out.tex_weight.assign(px, 0);

    for(const LedColorSample& sample : samples)
    {
        SplatSampleDominant(out, sample.u, sample.v, sample.r, sample.g, sample.b);
    }
}

RGBColor SampleReceiver(const MirrorFrame& frame,
                        float room_x,
                        float room_y,
                        float room_z,
                        const MirrorShadeContext* shade_ctx)
{
    const bool use_points = !frame.point_emitters.empty();
    if(!use_points && frame.surfaces.empty())
    {
        return 0x00000000;
    }

    const Vector3D led_pos = {room_x, room_y, room_z};
    const float scale_mm = SafeGridScaleMm(frame.grid_scale_mm);
    const float effective_range = std::max(frame.light_reach_mm, 40.0f);
    const float feather_percent = std::clamp(frame.glow_feather_percent, 5.0f, 90.0f);

    SpatialLightingSceneProvider* provider = SpatialLightingSceneProvider::instance();
    const bool shade_enabled = shade_ctx && shade_ctx->shade;
    const bool use_occlusion = shade_enabled && shade_ctx->shade->use_occlusion;
    static const std::vector<SpatialLighting::OccluderAabb> kEmptyAabbs;
    static const std::vector<SpatialLighting::OccluderQuad> kEmptyQuads;
    static const std::vector<SpatialLighting::BlockerGridOccluder> kEmptyGrids;
    const SpatialLighting::RoomBlockerField* room_blocker_field =
        (provider && provider->frameRoomBlockerField().IsValid()) ? &provider->frameRoomBlockerField() : nullptr;
    const std::vector<SpatialLighting::OccluderAabb>& aabbs =
        (shade_ctx && shade_ctx->occluder_aabbs) ? *shade_ctx->occluder_aabbs : kEmptyAabbs;
    const std::vector<SpatialLighting::OccluderQuad>& quads =
        (shade_ctx && shade_ctx->occluders) ? *shade_ctx->occluders : kEmptyQuads;
    const std::vector<SpatialLighting::BlockerGridOccluder>& blocker_grids =
        provider ? provider->frameBlockerGrids() : kEmptyGrids;
    const SpatialLighting::OccluderSpatialIndex* aabb_index =
        provider ? &provider->frameOccluderSpatialIndex() : nullptr;
    const bool has_occluders =
        use_occlusion &&
        (!aabbs.empty() || !quads.empty() || !blocker_grids.empty() ||
         (room_blocker_field && room_blocker_field->IsValid()));

    const auto emitter_visible = [&](float src_x, float src_y, float src_z) -> bool {
        if(!has_occluders)
        {
            return true;
        }
        return !SpatialLighting::SegmentIsOccluded(src_x,
                                                   src_y,
                                                   src_z,
                                                   room_x,
                                                   room_y,
                                                   room_z,
                                                   aabbs,
                                                   quads,
                                                   blocker_grids,
                                                   room_blocker_field,
                                                   aabb_index,
                                                   -1);
    };

    float total_r = 0.0f;
    float total_g = 0.0f;
    float total_b = 0.0f;
    float total_w = 0.0f;

    if(use_points)
    {
        for(const PointEmitter& src : frame.point_emitters)
        {
            const float dx = room_x - src.room_position.x;
            const float dy = room_y - src.room_position.y;
            const float dz = room_z - src.room_position.z;
            const float dist_u = std::sqrt(dx * dx + dy * dy + dz * dz);
            const float dist_mm = GridUnitsToMM(dist_u, scale_mm);
            const float weight = Geometry3D::ComputeFalloff(dist_mm, effective_range, feather_percent);
            if(weight < 0.001f)
            {
                continue;
            }
            if(!emitter_visible(src.room_position.x, src.room_position.y, src.room_position.z))
            {
                continue;
            }
            total_r += static_cast<float>(src.r) * weight;
            total_g += static_cast<float>(src.g) * weight;
            total_b += static_cast<float>(src.b) * weight;
            total_w += weight;
        }
    }
    else
    {
        for(const EmitterSurface& surface : frame.surfaces)
        {
            const EmitterReceiverProjection proj = ProjectReceiverOntoEmitterSurface(led_pos, surface, scale_mm);
            if(!proj.is_valid)
            {
                continue;
            }

            RGBColor sampled = SampleBilinearWeighted(surface, proj.u, proj.v);
            const float rf = static_cast<float>(sampled & 0xFF);
            const float gf = static_cast<float>((sampled >> 8) & 0xFF);
            const float bf = static_cast<float>((sampled >> 16) & 0xFF);
            if(rf + gf + bf < 0.5f)
            {
                continue;
            }

            const float weight = Geometry3D::ComputeFalloff(proj.distance_mm, effective_range, feather_percent);
            if(weight < 0.001f)
            {
                continue;
            }

            total_r += rf * weight;
            total_g += gf * weight;
            total_b += bf * weight;
            total_w += weight;
        }
    }

    if(total_w < 1e-5f)
    {
        return 0x00000000;
    }

    const float bright = std::max(0.12f, frame.brightness);
    const float fill_mul = 1.0f + frame.room_fill_strength * 0.42f;
    total_r = total_r / total_w * bright * fill_mul;
    total_g = total_g / total_w * bright * fill_mul;
    total_b = total_b / total_w * bright * fill_mul;

    if(use_occlusion && shade_ctx->shade->use_ambient_occlusion && shade_ctx->shade->ao_strength > 0.01f &&
       has_occluders)
    {
        const float shade_factor = SpatialLighting::ComputeRoomAmbientShadeFactor(room_x,
                                                                                room_y,
                                                                                room_z,
                                                                                frame.room_center.x,
                                                                                frame.room_center.y,
                                                                                frame.room_center.z,
                                                                                aabbs,
                                                                                quads,
                                                                                blocker_grids,
                                                                                shade_ctx->shade->ao_strength,
                                                                                shade_ctx->shade->ao_probe_span,
                                                                                aabb_index,
                                                                                room_blocker_field);
        total_r *= shade_factor;
        total_g *= shade_factor;
        total_b *= shade_factor;
    }

    return ToRGBColor(static_cast<uint8_t>(std::clamp(total_r, 0.0f, 255.0f)),
                      static_cast<uint8_t>(std::clamp(total_g, 0.0f, 255.0f)),
                      static_cast<uint8_t>(std::clamp(total_b, 0.0f, 255.0f)));
}

} // namespace EmitterRelayMirror
