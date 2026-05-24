// SPDX-License-Identifier: GPL-2.0-only

#include "ControllerLayout3D.h"
#include "Geometry3DUtils.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace
{
constexpr unsigned int MATRIX_MAP_NA = 0xFFFFFFFFu;
constexpr unsigned int DEVICE_VIEW_MAX_COLS = 20;
constexpr float ZONE_STACK_PAD = 1.0f;

static LEDPosition3D MakeLedPosition(RGBController* controller, unsigned int zone_idx, unsigned int led_idx, float x, float y, float z)
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
    RGBController* controller,
    unsigned int zone_idx,
    unsigned int led_count,
    unsigned int segment_start_led,
    float base_y,
    unsigned int wrap_cols,
    std::vector<LEDPosition3D>& zone_positions,
    float& zone_max_y)
{
    for(unsigned int led_idx = 0; led_idx < led_count; led_idx++)
    {
        float x = (float)(led_idx % wrap_cols);
        float y = base_y + (float)(led_idx / wrap_cols);
        zone_positions.push_back(MakeLedPosition(controller, zone_idx, segment_start_led + led_idx, x, y, 0.0f));
        if(y > zone_max_y)
        {
            zone_max_y = y;
        }
    }
}

static void AppendHeuristicBucketZone(
    RGBController* controller,
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

std::vector<LEDPosition3D> ControllerLayout3D::GenerateCustomGridLayout(RGBController* controller, int grid_x, int grid_y, bool center_layout)
{
    std::vector<LEDPosition3D> positions;
    if(!controller) return positions;

    unsigned int fallback_global_idx = 0;
    float zone_stack_y = 0.0f;

    for(unsigned int zone_idx = 0; zone_idx < controller->zones.size(); zone_idx++)
    {
        zone* current_zone = &controller->zones[zone_idx];
        std::vector<LEDPosition3D> zone_positions;
        float zone_max_y = zone_stack_y;
        bool zone_layout_applied = false;

        if(current_zone->type == ZONE_TYPE_MATRIX && current_zone->matrix_map != nullptr)
        {
            matrix_map_type* map = current_zone->matrix_map;
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
            for(unsigned int led_idx = 0; led_idx < current_zone->leds_count; led_idx++)
            {
                float x;
                float y;
                if(placed[led_idx])
                {
                    x = coord_x[led_idx];
                    y = coord_y[led_idx];
                }
                else
                {
                    x = (float)(led_idx % wrap_cols);
                    y = orphan_y + (float)(led_idx / wrap_cols);
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
                for(unsigned int led_idx = 0; led_idx < current_zone->leds_count; led_idx++)
                {
                    const float y = zone_stack_y;
                    zone_positions.push_back(MakeLedPosition(controller, zone_idx, led_idx, (float)led_idx, y, 0.0f));
                    zone_max_y = y;
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

std::vector<LEDPosition3D> ControllerLayout3D::GenerateCustomGridLayoutWithSpacing(RGBController* controller, int grid_x, int grid_y, float spacing_mm_x, float spacing_mm_y, float spacing_mm_z, float grid_scale_mm, bool center_layout)
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

static void CalculateControllerLocalBounds(const ControllerTransform* ctrl, Vector3D& min_bounds, Vector3D& max_bounds)
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
}

ControllerLayout3D::SpatialHash::SpatialHash(float cell_size)
    : cell_size(cell_size)
{
}

void ControllerLayout3D::SpatialHash::Clear()
{
    grid.clear();
}

int64_t ControllerLayout3D::SpatialHash::HashCell(int x, int y, int z) const
{
    int64_t hash = (int64_t)x;
    hash = hash * 73856093;
    hash ^= (int64_t)y * 19349663;
    hash ^= (int64_t)z * 83492791;
    return hash;
}

void ControllerLayout3D::SpatialHash::GetCellCoords(float x, float y, float z, int& cx, int& cy, int& cz) const
{
    cx = (int)floorf(x / cell_size);
    cy = (int)floorf(y / cell_size);
    cz = (int)floorf(z / cell_size);
}

void ControllerLayout3D::SpatialHash::Insert(LEDPosition3D* led_pos)
{
    if(!led_pos) return;

    int cx, cy, cz;
    GetCellCoords(led_pos->world_position.x,
                  led_pos->world_position.y,
                  led_pos->world_position.z,
                  cx, cy, cz);

    int64_t hash = HashCell(cx, cy, cz);
    grid[hash].leds.push_back(led_pos);
}

void ControllerLayout3D::SpatialHash::Build(const std::vector<std::unique_ptr<ControllerTransform>>& transforms)
{
    Clear();

    for(unsigned int i = 0; i < transforms.size(); i++)
    {
        ControllerTransform* transform = transforms[i].get();
        if(!transform) continue;

        for(unsigned int j = 0; j < transform->led_positions.size(); j++)
        {
            Insert(&transform->led_positions[j]);
        }
    }
}

float ControllerLayout3D::SpatialHash::DistanceSquared(float x1, float y1, float z1, float x2, float y2, float z2) const
{
    float dx = x2 - x1;
    float dy = y2 - y1;
    float dz = z2 - z1;
    return dx*dx + dy*dy + dz*dz;
}

std::vector<LEDPosition3D*> ControllerLayout3D::SpatialHash::QueryRadius(float x, float y, float z, float radius)
{
    std::vector<LEDPosition3D*> results;

    int min_cx = (int)floorf((x - radius) / cell_size);
    int max_cx = (int)floorf((x + radius) / cell_size);
    int min_cy = (int)floorf((y - radius) / cell_size);
    int max_cy = (int)floorf((y + radius) / cell_size);
    int min_cz = (int)floorf((z - radius) / cell_size);
    int max_cz = (int)floorf((z + radius) / cell_size);

    float radius_sq = radius * radius;

    for(int cx = min_cx; cx <= max_cx; cx++)
    {
        for(int cy = min_cy; cy <= max_cy; cy++)
        {
            for(int cz = min_cz; cz <= max_cz; cz++)
            {
                int64_t hash = HashCell(cx, cy, cz);
                std::unordered_map<int64_t, SpatialCell>::iterator it = grid.find(hash);
                if(it == grid.end()) continue;

                for(unsigned int i = 0; i < it->second.leds.size(); i++)
                {
                    LEDPosition3D* led = it->second.leds[i];
                    float dist_sq = DistanceSquared(x, y, z,
                                                    led->world_position.x,
                                                    led->world_position.y,
                                                    led->world_position.z);
                    if(dist_sq <= radius_sq)
                    {
                        results.push_back(led);
                    }
                }
            }
        }
    }

    return results;
}

LEDPosition3D* ControllerLayout3D::SpatialHash::FindNearest(float x, float y, float z)
{
    LEDPosition3D* nearest = nullptr;
    float min_dist_sq = std::numeric_limits<float>::max();

    int cx, cy, cz;
    GetCellCoords(x, y, z, cx, cy, cz);

    int search_radius = 0;
    const int max_search_radius = 10;

    while(!nearest && search_radius <= max_search_radius)
    {
        for(int dx = -search_radius; dx <= search_radius; dx++)
        {
            for(int dy = -search_radius; dy <= search_radius; dy++)
            {
                for(int dz = -search_radius; dz <= search_radius; dz++)
                {
                    if(search_radius > 0 &&
                       abs(dx) != search_radius &&
                       abs(dy) != search_radius &&
                       abs(dz) != search_radius)
                    {
                        continue;
                    }

                    int64_t hash = HashCell(cx + dx, cy + dy, cz + dz);
                    std::unordered_map<int64_t, SpatialCell>::iterator it = grid.find(hash);
                    if(it == grid.end()) continue;

                    for(unsigned int i = 0; i < it->second.leds.size(); i++)
                    {
                        LEDPosition3D* led = it->second.leds[i];
                        float dist_sq = DistanceSquared(x, y, z,
                                                        led->world_position.x,
                                                        led->world_position.y,
                                                        led->world_position.z);
                        if(dist_sq < min_dist_sq)
                        {
                            min_dist_sq = dist_sq;
                            nearest = led;
                        }
                    }
                }
            }
        }

        search_radius++;
    }

    return nearest;
}
