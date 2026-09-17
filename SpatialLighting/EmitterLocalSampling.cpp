// SPDX-License-Identifier: GPL-2.0-only

#include "EmitterLocalSampling.h"

#include "ControllerLayout3D.h"
#include "SpatialEffect3D.h"

#include <algorithm>
#include <limits>

namespace EmitterLocalSampling
{

bool TryBuildCombinedEmitterCanvas(
    const std::vector<std::unique_ptr<ControllerTransform>>& transforms,
    const std::unordered_set<int>& emitter_controller_indices,
    float grid_scale_mm,
    std::uint64_t render_sequence,
    CombinedEmitterCanvas& out)
{
    out = CombinedEmitterCanvas{};
    if(emitter_controller_indices.empty())
    {
        return false;
    }

    float min_x = std::numeric_limits<float>::max();
    float min_y = std::numeric_limits<float>::max();
    float min_z = std::numeric_limits<float>::max();
    float max_x = std::numeric_limits<float>::lowest();
    float max_y = std::numeric_limits<float>::lowest();
    float max_z = std::numeric_limits<float>::lowest();
    double sum_x = 0.0;
    double sum_y = 0.0;
    double sum_z = 0.0;
    std::uint64_t led_count = 0;

    for(int ctrl_idx : emitter_controller_indices)
    {
        if(ctrl_idx < 0 || ctrl_idx >= static_cast<int>(transforms.size()))
        {
            continue;
        }
        ControllerTransform* transform = transforms[static_cast<size_t>(ctrl_idx)].get();
        if(!transform || transform->hidden_by_virtual)
        {
            continue;
        }
        ControllerLayout3D::UpdateWorldPositions(transform);
        for(const LEDPosition3D& led : transform->led_positions)
        {
            const Vector3D& room = led.room_position;
            min_x = std::min(min_x, room.x);
            min_y = std::min(min_y, room.y);
            min_z = std::min(min_z, room.z);
            max_x = std::max(max_x, room.x);
            max_y = std::max(max_y, room.y);
            max_z = std::max(max_z, room.z);
            sum_x += room.x;
            sum_y += room.y;
            sum_z += room.z;
            led_count++;
        }
    }

    if(led_count == 0)
    {
        return false;
    }

    const float min_dimension = 0.2f;
    if(max_x - min_x < 0.001f)
    {
        const float center_x = (min_x + max_x) * 0.5f;
        min_x = center_x - min_dimension;
        max_x = center_x + min_dimension;
    }
    if(max_y - min_y < 0.001f)
    {
        const float center_y = (min_y + max_y) * 0.5f;
        min_y = center_y - min_dimension;
        max_y = center_y + min_dimension;
    }
    if(max_z - min_z < 0.001f)
    {
        const float center_z = (min_z + max_z) * 0.5f;
        min_z = center_z - min_dimension;
        max_z = center_z + min_dimension;
    }

    out.grid = std::make_unique<GridContext3D>(
        min_x, max_x, min_y, max_y, min_z, max_z, grid_scale_mm);
    out.grid->render_sequence = render_sequence;
    const double inv = 1.0 / static_cast<double>(led_count);
    out.grid->SetLedCentroid(static_cast<float>(sum_x * inv),
                             static_cast<float>(sum_y * inv),
                             static_cast<float>(sum_z * inv));
    out.valid = true;
    return true;
}

} // namespace EmitterLocalSampling
