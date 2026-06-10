// SPDX-License-Identifier: GPL-2.0-only

#include "EmitterLocalSampling.h"

#include "ControllerLayout3D.h"
#include "SpatialEffect3D.h"

#include <algorithm>
#include <cmath>

namespace EmitterLocalSampling
{

bool TryBuildEmitterLocalSample(const ControllerTransform* ctrl,
                                const LEDPosition3D& led,
                                float grid_scale_mm,
                                const std::uint64_t render_sequence,
                                float& out_x,
                                float& out_y,
                                float& out_z,
                                GridContext3D& out_grid)
{
    if(!ctrl || ctrl->led_positions.empty())
    {
        return false;
    }

    Vector3D local_min = ctrl->led_positions[0].local_position;
    Vector3D local_max = local_min;
    for(const LEDPosition3D& led_pos : ctrl->led_positions)
    {
        const Vector3D& p = led_pos.local_position;
        local_min.x = std::min(local_min.x, p.x);
        local_min.y = std::min(local_min.y, p.y);
        local_min.z = std::min(local_min.z, p.z);
        local_max.x = std::max(local_max.x, p.x);
        local_max.y = std::max(local_max.y, p.y);
        local_max.z = std::max(local_max.z, p.z);
    }

    const float cx = (local_min.x + local_max.x) * 0.5f;
    const float cy = (local_min.y + local_max.y) * 0.5f;
    const float cz = (local_min.z + local_max.z) * 0.5f;

    Vector3D min_bounds = local_min;
    Vector3D max_bounds = local_max;
    const float min_dimension = 0.2f;
    if(max_bounds.x - min_bounds.x < 0.001f)
    {
        const float center_x = (min_bounds.x + max_bounds.x) * 0.5f;
        min_bounds.x = center_x - min_dimension;
        max_bounds.x = center_x + min_dimension;
    }
    if(max_bounds.y - min_bounds.y < 0.001f)
    {
        const float center_y = (min_bounds.y + max_bounds.y) * 0.5f;
        min_bounds.y = center_y - min_dimension;
        max_bounds.y = center_y + min_dimension;
    }
    if(max_bounds.z - min_bounds.z < 0.001f)
    {
        const float center_z = (min_bounds.z + max_bounds.z) * 0.5f;
        min_bounds.z = center_z - min_dimension;
        max_bounds.z = center_z + min_dimension;
    }

    const float span_x = std::max(max_bounds.x - min_bounds.x, 0.01f);
    const float span_y = std::max(max_bounds.y - min_bounds.y, 0.01f);
    const float span_z = std::max(max_bounds.z - min_bounds.z, 0.01f);

    const float half_w = span_x * 0.5f;
    const float half_h = span_y * 0.5f;
    const float half_d = span_z * 0.5f;

    out_grid = GridContext3D(-half_w, half_w, -half_h, half_h, -half_d, half_d, grid_scale_mm);
    out_grid.render_sequence = render_sequence;
    out_grid.use_grid_center_as_reference = true;

    out_x = led.local_position.x - cx;
    out_y = led.local_position.y - cy;
    out_z = led.local_position.z - cz;
    return true;
}

} // namespace EmitterLocalSampling
