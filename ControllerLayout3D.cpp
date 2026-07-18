// SPDX-License-Identifier: GPL-2.0-only

#include "ControllerLayout3D.h"
#include "Geometry3DUtils.h"
#include "GridSpaceUtils.h"
#include "SpatialLightingSceneProvider.h"
#include "VirtualController3D.h"
#include <algorithm>
#include <cmath>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace
{
constexpr unsigned int MATRIX_MAP_NA = 0xFFFFFFFFu;
constexpr unsigned int DEVICE_VIEW_MAX_COLS = 20;
constexpr float ZONE_STACK_PAD = 1.0f;

static LEDPosition3D MakeLedPosition(RGBControllerInterface* controller, unsigned int zone_idx, unsigned int led_idx, float x, float y, float z)
{
    LEDPosition3D led_pos;
    led_pos.controller = controller;
    led_pos.zone_idx = zone_idx;
    led_pos.led_idx = led_idx;
    led_pos.local_position.x = x;
    led_pos.local_position.y = y;
    led_pos.local_position.z = z;
    led_pos.world_position = led_pos.local_position;
    led_pos.room_position = led_pos.local_position;
    led_pos.preview_color = 0x00FFFFFF;
    return led_pos;
}

/** Skip OpenRGB KEY_EN_UNUSED ("") keyboard matrix filler slots. */
static bool ZoneLedIsAssignable(RGBControllerInterface* controller, unsigned int zone_idx, unsigned int led_idx)
{
    if(!controller || zone_idx >= controller->GetZoneCount())
    {
        return false;
    }
    const unsigned int global_led_idx = controller->GetZoneStartIndex(zone_idx) + led_idx;
    if(global_led_idx >= controller->GetLEDCount())
    {
        return false;
    }
    return !controller->GetLEDName(global_led_idx).empty();
}

static unsigned int LinearWrapColumns(unsigned int led_count, int grid_x)
{
    unsigned int cols = (grid_x > 0) ? (unsigned int)grid_x : DEVICE_VIEW_MAX_COLS;
    if(led_count > 0)
    {
        cols = std::min(led_count, cols);
    }
    cols = std::max(1u, cols);
    cols = std::min(cols, DEVICE_VIEW_MAX_COLS);
    return cols;
}

static bool ResolveZoneLedIndex(const zone* current_zone, unsigned int map_val, unsigned int* out_led_idx)
{
    if(map_val == MATRIX_MAP_NA)
    {
        return false;
    }

    if(map_val < current_zone->leds_count)
    {
        *out_led_idx = map_val;
        return true;
    }

    if(map_val >= current_zone->start_idx && (map_val - current_zone->start_idx) < current_zone->leds_count)
    {
        *out_led_idx = map_val - current_zone->start_idx;
        return true;
    }

    return false;
}

static void AppendLinearWrappedZone(
    RGBControllerInterface* controller,
    unsigned int zone_idx,
    unsigned int led_count,
    unsigned int segment_start_led,
    float base_y,
    unsigned int wrap_cols,
    std::vector<LEDPosition3D>& zone_positions,
    float& zone_max_y)
{
    unsigned int placed = 0;
    for(unsigned int led_idx = 0; led_idx < led_count; led_idx++)
    {
        const unsigned int zone_led_idx = segment_start_led + led_idx;
        if(!ZoneLedIsAssignable(controller, zone_idx, zone_led_idx))
        {
            continue;
        }
        float x = (float)(placed % wrap_cols);
        float y = base_y + (float)(placed / wrap_cols);
        zone_positions.push_back(MakeLedPosition(controller, zone_idx, zone_led_idx, x, y, 0.0f));
        if(y > zone_max_y)
        {
            zone_max_y = y;
        }
        placed++;
    }
}

static void AppendHeuristicBucketZone(
    RGBControllerInterface* controller,
    unsigned int zone_idx,
    const zone* current_zone,
    int grid_x,
    int grid_y,
    unsigned int& fallback_global_idx,
    float zone_stack_y,
    std::vector<LEDPosition3D>& zone_positions,
    float& zone_max_y)
{
    const int safe_grid_x = (grid_x > 0) ? grid_x : 10;
    const int safe_grid_y = (grid_y > 0) ? grid_y : 10;
    const int grid_xy = safe_grid_x * safe_grid_y;

    for(unsigned int led_idx = 0; led_idx < current_zone->leds_count; led_idx++)
    {
        if(!ZoneLedIsAssignable(controller, zone_idx, led_idx))
        {
            continue;
        }
        const unsigned int mapping_idx = fallback_global_idx;
        const int x_pos = mapping_idx % safe_grid_x;
        const int y_pos = (mapping_idx / safe_grid_x) % safe_grid_y;
        const int z_pos = mapping_idx / grid_xy;
        const float y = zone_stack_y + (float)y_pos;
        zone_positions.push_back(MakeLedPosition(controller, zone_idx, led_idx, (float)x_pos, y, (float)z_pos));
        if(y > zone_max_y)
        {
            zone_max_y = y;
        }
        fallback_global_idx++;
    }
}
} // namespace

std::vector<LEDPosition3D> ControllerLayout3D::GenerateCustomGridLayout(RGBControllerInterface* controller, int grid_x, int grid_y, bool center_layout)
{
    std::vector<LEDPosition3D> positions;
    if(!controller) return positions;

    unsigned int fallback_global_idx = 0;
    float zone_stack_y = 0.0f;

    for(unsigned int zone_idx = 0; zone_idx < controller->GetZoneCount(); zone_idx++)
    {
        zone current_zone_data = controller->GetZone(zone_idx);
        const zone* current_zone = &current_zone_data;
        std::vector<LEDPosition3D> zone_positions;
        float zone_max_y = zone_stack_y;
        bool zone_layout_applied = false;

        if(current_zone->type == ZONE_TYPE_MATRIX && current_zone->matrix_map.map.size() > 0)
        {
            const matrix_map_type* map = &current_zone->matrix_map;
            std::vector<bool> placed(current_zone->leds_count, false);
            std::vector<float> coord_x(current_zone->leds_count, 0.0f);
            std::vector<float> coord_y(current_zone->leds_count, 0.0f);

            for(unsigned int led_x = 0; led_x < map->width; led_x++)
            {
                for(unsigned int led_y = 0; led_y < map->height; led_y++)
                {
                    const unsigned int map_idx = led_y * map->width + led_x;
                    unsigned int zone_led_idx = 0;
                    if(!ResolveZoneLedIndex(current_zone, map->map[map_idx], &zone_led_idx))
                    {
                        continue;
                    }

                    coord_x[zone_led_idx] = (float)led_x;
                    coord_y[zone_led_idx] = zone_stack_y + (float)led_y;
                    placed[zone_led_idx] = true;
                }
            }

            const unsigned int wrap_cols = LinearWrapColumns(current_zone->leds_count, grid_x);
            const float orphan_y = zone_stack_y + (float)map->height;
            unsigned int orphan_placed = 0;
            for(unsigned int led_idx = 0; led_idx < current_zone->leds_count; led_idx++)
            {
                if(!ZoneLedIsAssignable(controller, zone_idx, led_idx))
                {
                    continue;
                }

                float x;
                float y;
                if(placed[led_idx])
                {
                    x = coord_x[led_idx];
                    y = coord_y[led_idx];
                }
                else
                {
                    x = (float)(orphan_placed % wrap_cols);
                    y = orphan_y + (float)(orphan_placed / wrap_cols);
                    orphan_placed++;
                }

                zone_positions.push_back(MakeLedPosition(controller, zone_idx, led_idx, x, y, 0.0f));
                if(y > zone_max_y)
                {
                    zone_max_y = y;
                }
            }

            zone_layout_applied = true;
        }
        else if(current_zone->segments.size() > 0)
        {
            float segment_base_y = 0.0f;
            const unsigned int wrap_cols = LinearWrapColumns(current_zone->leds_count, grid_x);

            for(std::size_t segment_idx = 0; segment_idx < current_zone->segments.size(); segment_idx++)
            {
                const unsigned int segment_led_count = current_zone->segments[segment_idx].leds_count;
                const unsigned int segment_start_led = current_zone->segments[segment_idx].start_idx;
                AppendLinearWrappedZone(
                    controller,
                    zone_idx,
                    segment_led_count,
                    segment_start_led,
                    zone_stack_y + segment_base_y,
                    wrap_cols,
                    zone_positions,
                    zone_max_y);
                segment_base_y += (float)((segment_led_count / wrap_cols) + ((segment_led_count % wrap_cols) > 0));
            }

            zone_layout_applied = true;
        }
        else if(current_zone->type == ZONE_TYPE_SINGLE && current_zone->leds_count <= 1)
        {
            for(unsigned int led_idx = 0; led_idx < current_zone->leds_count; led_idx++)
            {
                if(!ZoneLedIsAssignable(controller, zone_idx, led_idx))
                {
                    continue;
                }
                zone_positions.push_back(MakeLedPosition(controller, zone_idx, led_idx, 0.0f, zone_stack_y, 0.0f));
                zone_max_y = zone_stack_y;
            }

            zone_layout_applied = true;
        }
        else if(current_zone->type == ZONE_TYPE_LINEAR || current_zone->leds_count > 0)
        {
            const unsigned int wrap_cols = LinearWrapColumns(current_zone->leds_count, grid_x);

            if(current_zone->type == ZONE_TYPE_LINEAR && current_zone->leds_count <= wrap_cols)
            {
                unsigned int placed = 0;
                for(unsigned int led_idx = 0; led_idx < current_zone->leds_count; led_idx++)
                {
                    if(!ZoneLedIsAssignable(controller, zone_idx, led_idx))
                    {
                        continue;
                    }
                    const float y = zone_stack_y;
                    zone_positions.push_back(MakeLedPosition(controller, zone_idx, led_idx, (float)placed, y, 0.0f));
                    zone_max_y = y;
                    placed++;
                }
            }
            else
            {
                AppendLinearWrappedZone(
                    controller,
                    zone_idx,
                    current_zone->leds_count,
                    0,
                    zone_stack_y,
                    wrap_cols,
                    zone_positions,
                    zone_max_y);
            }

            zone_layout_applied = true;
        }

        if(!zone_layout_applied)
        {
            AppendHeuristicBucketZone(
                controller,
                zone_idx,
                current_zone,
                grid_x,
                grid_y,
                fallback_global_idx,
                zone_stack_y,
                zone_positions,
                zone_max_y);
        }

        positions.insert(positions.end(), zone_positions.begin(), zone_positions.end());

        if(zone_max_y >= zone_stack_y)
        {
            zone_stack_y = zone_max_y + 1.0f + ZONE_STACK_PAD;
        }
    }

    if(center_layout && !positions.empty())
    {
        float min_x = positions[0].local_position.x;
        float max_x = positions[0].local_position.x;
        float min_y = positions[0].local_position.y;
        float max_y = positions[0].local_position.y;
        float min_z = positions[0].local_position.z;
        float max_z = positions[0].local_position.z;

        for(unsigned int i = 1; i < positions.size(); i++)
        {
            if(positions[i].local_position.x < min_x) min_x = positions[i].local_position.x;
            if(positions[i].local_position.x > max_x) max_x = positions[i].local_position.x;
            if(positions[i].local_position.y < min_y) min_y = positions[i].local_position.y;
            if(positions[i].local_position.y > max_y) max_y = positions[i].local_position.y;
            if(positions[i].local_position.z < min_z) min_z = positions[i].local_position.z;
            if(positions[i].local_position.z > max_z) max_z = positions[i].local_position.z;
        }

        const float center_x = (min_x + max_x) / 2.0f;
        const float center_y = (min_y + max_y) / 2.0f;
        const float center_z = (min_z + max_z) / 2.0f;

        for(unsigned int i = 0; i < positions.size(); i++)
        {
            positions[i].local_position.x -= center_x;
            positions[i].local_position.y -= center_y;
            positions[i].local_position.z -= center_z;
            positions[i].world_position = positions[i].local_position;
            positions[i].room_position = positions[i].local_position;
        }
    }

    return positions;
}

std::vector<LEDPosition3D> ControllerLayout3D::GenerateCustomGridLayoutWithSpacing(RGBControllerInterface* controller, int grid_x, int grid_y, float spacing_mm_x, float spacing_mm_y, float spacing_mm_z, float grid_scale_mm, bool center_layout)
{
    std::vector<LEDPosition3D> positions = GenerateCustomGridLayout(controller, grid_x, grid_y, center_layout);

    float scale_x = (spacing_mm_x > 0.001f) ? MMToGridUnits(spacing_mm_x, grid_scale_mm) : 1.0f;
    float scale_y = (spacing_mm_y > 0.001f) ? MMToGridUnits(spacing_mm_y, grid_scale_mm) : 1.0f;
    float scale_z = (spacing_mm_z > 0.001f) ? MMToGridUnits(spacing_mm_z, grid_scale_mm) : 1.0f;

    for(unsigned int i = 0; i < positions.size(); i++)
    {
        positions[i].local_position.x *= scale_x;
        positions[i].local_position.y *= scale_y;
        positions[i].local_position.z *= scale_z;
        positions[i].world_position = positions[i].local_position;
        positions[i].room_position = positions[i].local_position;
        positions[i].preview_color = 0x00FFFFFF;
    }

    return positions;
}

Vector3D ControllerLayout3D::CalculateWorldPosition(Vector3D local_pos, Transform3D transform)
{
    float rotation_matrix[9];
    Geometry3D::ComputeRotationMatrix(transform.rotation, rotation_matrix);
    Vector3D rotated = Geometry3D::RotateVector(local_pos, rotation_matrix);

    Vector3D world_pos;
    world_pos.x = rotated.x + transform.position.x;
    world_pos.y = rotated.y + transform.position.y;
    world_pos.z = rotated.z + transform.position.z;

    return world_pos;
}

void ControllerLayout3D::CalculateControllerLocalBounds(const ControllerTransform* ctrl,
                                                        Vector3D& min_bounds,
                                                        Vector3D& max_bounds)
{
    if(!ctrl || ctrl->led_positions.empty())
    {
        min_bounds = {-0.5f, -0.5f, -0.5f};
        max_bounds = {0.5f, 0.5f, 0.5f};
        return;
    }

    Vector3D first_pos = ctrl->led_positions[0].local_position;
    min_bounds = first_pos;
    max_bounds = first_pos;

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

    const float min_dimension = 0.2f;
    const float size_x = max_bounds.x - min_bounds.x;
    const float size_y = max_bounds.y - min_bounds.y;
    const float size_z = max_bounds.z - min_bounds.z;

    if(size_x < 0.001f)
    {
        const float center_x = (min_bounds.x + max_bounds.x) * 0.5f;
        min_bounds.x = center_x - min_dimension;
        max_bounds.x = center_x + min_dimension;
    }
    if(size_y < 0.001f)
    {
        const float center_y = (min_bounds.y + max_bounds.y) * 0.5f;
        min_bounds.y = center_y - min_dimension;
        max_bounds.y = center_y + min_dimension;
    }
    if(size_z < 0.001f)
    {
        const float center_z = (min_bounds.z + max_bounds.z) * 0.5f;
        min_bounds.z = center_z - min_dimension;
        max_bounds.z = center_z + min_dimension;
    }

    const float padding = 0.1f;
    min_bounds.x -= padding;
    min_bounds.y -= padding;
    min_bounds.z -= padding;
    max_bounds.x += padding;
    max_bounds.y += padding;
    max_bounds.z += padding;
}

static float VectorLength(const Vector3D& v)
{
    return std::sqrt((v.x * v.x) + (v.y * v.y) + (v.z * v.z));
}

static int FindNeighborMappingIndex(const std::vector<GridLEDMapping>& mappings, size_t index, int led_delta)
{
    if(index >= mappings.size())
    {
        return -1;
    }

    const GridLEDMapping& current = mappings[index];
    if(!current.controller)
    {
        return -1;
    }

    if(led_delta < 0 && current.led_idx < static_cast<unsigned int>(-led_delta))
    {
        return -1;
    }

    const unsigned int target_led = static_cast<unsigned int>(static_cast<int>(current.led_idx) + led_delta);
    for(size_t j = 0; j < mappings.size(); ++j)
    {
        if(j == index)
        {
            continue;
        }

        const GridLEDMapping& other = mappings[j];
        if(other.controller == current.controller && other.zone_idx == current.zone_idx && other.led_idx == target_led)
        {
            return static_cast<int>(j);
        }
    }

    return -1;
}

static Vector3D LerpVector3D(const Vector3D& a, const Vector3D& b, float t)
{
    return {
        a.x + ((b.x - a.x) * t),
        a.y + ((b.y - a.y) * t),
        a.z + ((b.z - a.z) * t),
    };
}

static float ClampFloat(float value, float min_value, float max_value)
{
    return std::max(min_value, std::min(max_value, value));
}

static int FindMappingIndexForLedIdx(const std::vector<GridLEDMapping>& mappings, unsigned int led_idx)
{
    for(size_t i = 0; i < mappings.size(); ++i)
    {
        if(mappings[i].led_idx == led_idx)
        {
            return static_cast<int>(i);
        }
    }
    return -1;
}

static bool BuildOrderedStripPolyline(const ControllerTransform* ctrl_transform,
                                      std::vector<Vector3D>& out_points,
                                      std::vector<size_t>& out_logical_indices)
{
    out_points.clear();
    out_logical_indices.clear();
    if(!ctrl_transform || ctrl_transform->led_positions.empty())
    {
        return false;
    }

    VirtualController3D* layout = ctrl_transform->virtual_controller;
    if(layout && !layout->GetMappings().empty())
    {
        const std::vector<GridLEDMapping>& mappings = layout->GetMappings();
        const int                          start    = FindMappingIndexForLedIdx(mappings, 0);
        if(start >= 0)
        {
            int current = start;
            while(current >= 0)
            {
                const size_t logical_index = static_cast<size_t>(current);
                if(logical_index >= ctrl_transform->led_positions.size())
                {
                    break;
                }

                out_logical_indices.push_back(logical_index);
                out_points.push_back(ctrl_transform->led_positions[logical_index].local_position);
                current = FindNeighborMappingIndex(mappings, logical_index, 1);
            }

            if(out_points.size() >= 2)
            {
                return true;
            }
        }
    }

    for(size_t i = 0; i < ctrl_transform->led_positions.size(); ++i)
    {
        out_logical_indices.push_back(i);
        out_points.push_back(ctrl_transform->led_positions[i].local_position);
    }

    return out_points.size() >= 2;
}

static void BuildStripArcLengths(const std::vector<Vector3D>& points, std::vector<float>& out_arc_lengths)
{
    out_arc_lengths.clear();
    out_arc_lengths.reserve(points.size());
    if(points.empty())
    {
        return;
    }

    out_arc_lengths.push_back(0.0f);
    for(size_t i = 1; i < points.size(); ++i)
    {
        const Vector3D delta = {
            points[i].x - points[i - 1].x,
            points[i].y - points[i - 1].y,
            points[i].z - points[i - 1].z,
        };
        out_arc_lengths.push_back(out_arc_lengths.back() + VectorLength(delta));
    }
}

static Vector3D SampleStripPolyline(const std::vector<Vector3D>& points,
                                    const std::vector<float>& arc_lengths,
                                    float arc_s)
{
    if(points.empty())
    {
        return {0.0f, 0.0f, 0.0f};
    }
    if(points.size() == 1 || arc_s <= arc_lengths.front())
    {
        return points.front();
    }
    if(arc_s >= arc_lengths.back())
    {
        return points.back();
    }

    for(size_t i = 1; i < arc_lengths.size(); ++i)
    {
        if(arc_s <= arc_lengths[i])
        {
            const float segment_length = arc_lengths[i] - arc_lengths[i - 1];
            const float t              = (segment_length > 0.0001f) ? ((arc_s - arc_lengths[i - 1]) / segment_length) : 0.0f;
            return LerpVector3D(points[i - 1], points[i], t);
        }
    }

    return points.back();
}

static float SectionViewportStep(float pitch_in, float pitch_out, int leds_per_cluster)
{
    if(leds_per_cluster <= 1)
    {
        return 0.0f;
    }

    if(pitch_in > 0.0001f && pitch_out > 0.0001f)
    {
        return ((pitch_in + pitch_out) * 0.5f) / static_cast<float>(leds_per_cluster);
    }
    if(pitch_out > 0.0001f)
    {
        return pitch_out / static_cast<float>(leds_per_cluster);
    }
    if(pitch_in > 0.0001f)
    {
        return pitch_in / static_cast<float>(leds_per_cluster);
    }

    return 0.0f;
}

void ControllerLayout3D::BuildViewportStripDrawSamples(const ControllerTransform* ctrl_transform,
                                                       float /*grid_scale_mm*/,
                                                       std::vector<ViewportStripDrawSample>& out_samples)
{
    out_samples.clear();
    if(!ctrl_transform || ctrl_transform->led_positions.empty())
    {
        return;
    }

    const size_t led_count = ctrl_transform->led_positions.size();
    VirtualController3D* layout = ctrl_transform->virtual_controller;
    const int            leds_per_cluster =
        layout ? std::max(1, layout->GetLedsPerCluster()) : 1;

    if(leds_per_cluster <= 1)
    {
        out_samples.reserve(led_count);
        for(size_t i = 0; i < led_count; ++i)
        {
            out_samples.push_back({ctrl_transform->led_positions[i].local_position, i});
        }
        return;
    }

    std::vector<Vector3D> strip_points;
    std::vector<size_t>   strip_logical_indices;
    if(!BuildOrderedStripPolyline(ctrl_transform, strip_points, strip_logical_indices))
    {
        for(size_t i = 0; i < led_count; ++i)
        {
            out_samples.push_back({ctrl_transform->led_positions[i].local_position, i});
        }
        return;
    }

    std::vector<float> arc_lengths;
    BuildStripArcLengths(strip_points, arc_lengths);
    const float total_length = arc_lengths.back();
    const size_t strip_count = strip_points.size();

    struct PendingSample
    {
        float  arc_s;
        size_t logical_index;
        bool   is_center;
    };

    std::vector<PendingSample> pending;
    pending.reserve(strip_count * static_cast<size_t>(leds_per_cluster));

    for(size_t strip_i = 0; strip_i < strip_count; ++strip_i)
    {
        const float pitch_in  = (strip_i > 0) ? (arc_lengths[strip_i] - arc_lengths[strip_i - 1]) : 0.0f;
        const float pitch_out = (strip_i + 1 < strip_count) ? (arc_lengths[strip_i + 1] - arc_lengths[strip_i]) : 0.0f;
        const float step      = SectionViewportStep(pitch_in, pitch_out, leds_per_cluster);
        const float center_s  = arc_lengths[strip_i];
        const size_t logical_index = strip_logical_indices[strip_i];

        if(step <= 0.0001f)
        {
            pending.push_back({center_s, logical_index, true});
            continue;
        }

        if(strip_i == 0)
        {
            for(int k = 0; k < leds_per_cluster; ++k)
            {
                pending.push_back({ClampFloat(center_s + (static_cast<float>(k) * step), 0.0f, total_length),
                                   logical_index,
                                   k == 0});
            }
            continue;
        }

        if(strip_i + 1 == strip_count)
        {
            for(int k = 0; k < leds_per_cluster; ++k)
            {
                const float back_offset = static_cast<float>(leds_per_cluster - 1 - k) * step;
                pending.push_back({ClampFloat(center_s - back_offset, 0.0f, total_length),
                                   logical_index,
                                   k == (leds_per_cluster - 1)});
            }
            continue;
        }

        pending.push_back({ClampFloat(center_s - step, 0.0f, total_length), logical_index, false});
        pending.push_back({center_s, logical_index, true});
        pending.push_back({ClampFloat(center_s + step, 0.0f, total_length), logical_index, false});
    }

    std::sort(pending.begin(), pending.end(), [](const PendingSample& a, const PendingSample& b) {
        return a.arc_s < b.arc_s;
    });

    constexpr float kArcDedupeEpsilon = 0.0015f;
    std::vector<PendingSample> merged;
    merged.reserve(pending.size());
    for(const PendingSample& sample : pending)
    {
        if(merged.empty() || (sample.arc_s - merged.back().arc_s) > kArcDedupeEpsilon)
        {
            merged.push_back(sample);
            continue;
        }

        if(sample.is_center && !merged.back().is_center)
        {
            merged.back() = sample;
        }
    }

    out_samples.reserve(merged.size());
    for(const PendingSample& sample : merged)
    {
        out_samples.push_back(
            {SampleStripPolyline(strip_points, arc_lengths, sample.arc_s), sample.logical_index});
    }
}

static Vector3D TransformLocalToWorldWithScale(const Vector3D& local_pos, const Transform3D& transform)
{
    Vector3D scaled = {
        local_pos.x * transform.scale.x,
        local_pos.y * transform.scale.y,
        local_pos.z * transform.scale.z
    };

    float rotation_matrix[9];
    Geometry3D::ComputeRotationMatrix(transform.rotation, rotation_matrix);
    Vector3D rotated = Geometry3D::RotateVector(scaled, rotation_matrix);

    return {
        rotated.x + transform.position.x,
        rotated.y + transform.position.y,
        rotated.z + transform.position.z
    };
}

Vector3D ControllerLayout3D::GetControllerCenterWorld(const ControllerTransform* ctrl_transform)
{
    if(!ctrl_transform)
    {
        return {0.0f, 0.0f, 0.0f};
    }

    Vector3D min_bounds;
    Vector3D max_bounds;
    CalculateControllerLocalBounds(ctrl_transform, min_bounds, max_bounds);

    const Vector3D local_center = {
        (min_bounds.x + max_bounds.x) * 0.5f,
        (min_bounds.y + max_bounds.y) * 0.5f,
        (min_bounds.z + max_bounds.z) * 0.5f
    };

    return TransformLocalToWorldWithScale(local_center, ctrl_transform->transform);
}

void ControllerLayout3D::UpdateWorldPositions(ControllerTransform* ctrl_transform)
{
    if(!ctrl_transform)
    {
        return;
    }

    Vector3D local_min = {0.0f, 0.0f, 0.0f};
    Vector3D local_max = {0.0f, 0.0f, 0.0f};
    bool have_bounds = false;
    for(unsigned int i = 0; i < ctrl_transform->led_positions.size(); i++)
    {
        const Vector3D& p = ctrl_transform->led_positions[i].local_position;
        if(!have_bounds)
        {
            local_min = p;
            local_max = p;
            have_bounds = true;
        }
        else
        {
            if(p.x < local_min.x) local_min.x = p.x;
            if(p.y < local_min.y) local_min.y = p.y;
            if(p.z < local_min.z) local_min.z = p.z;
            if(p.x > local_max.x) local_max.x = p.x;
            if(p.y > local_max.y) local_max.y = p.y;
            if(p.z > local_max.z) local_max.z = p.z;
        }
    }
    Vector3D local_center = {0.0f, 0.0f, 0.0f};
    if(have_bounds)
    {
        local_center.x = (local_min.x + local_max.x) * 0.5f;
        local_center.y = (local_min.y + local_max.y) * 0.5f;
        local_center.z = (local_min.z + local_max.z) * 0.5f;
    }

    float rotation_matrix[9];
    Geometry3D::ComputeRotationMatrix(ctrl_transform->transform.rotation, rotation_matrix);

    for(unsigned int i = 0; i < ctrl_transform->led_positions.size(); i++)
    {
        LEDPosition3D* led_pos = &ctrl_transform->led_positions[i];
        Vector3D local = {
            led_pos->local_position.x - local_center.x,
            led_pos->local_position.y - local_center.y,
            led_pos->local_position.z - local_center.z
        };
        Vector3D scaled_local = {
            local.x * ctrl_transform->transform.scale.x,
            local.y * ctrl_transform->transform.scale.y,
            local.z * ctrl_transform->transform.scale.z
        };

        Vector3D rotated = Geometry3D::RotateVector(scaled_local, rotation_matrix);

        led_pos->world_position.x = rotated.x + ctrl_transform->transform.position.x;
        led_pos->world_position.y = rotated.y + ctrl_transform->transform.position.y;
        led_pos->world_position.z = rotated.z + ctrl_transform->transform.position.z;
        // Room-aligned positions are not split yet; both paths share world coords until
        // per-controller room basis is implemented (see PluginSpatialMeasurement.md §4).
        led_pos->room_position = led_pos->world_position;
    }

    ctrl_transform->world_positions_dirty = false;
}

void ControllerLayout3D::MarkWorldPositionsDirty(ControllerTransform* ctrl_transform)
{
    if(!ctrl_transform)
    {
        return;
    }

    ctrl_transform->world_positions_dirty = true;
    SpatialLightingSceneProvider::instance()->InvalidateFrameOccluders();
}
