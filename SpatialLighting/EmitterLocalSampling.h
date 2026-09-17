// SPDX-License-Identifier: GPL-2.0-only

#ifndef EMITTERLOCALSAMPLING_H
#define EMITTERLOCALSAMPLING_H

#include "LEDPosition3D.h"

#include <cstdint>
#include <memory>
#include <unordered_set>
#include <vector>

struct ControllerTransform;
struct GridContext3D;

namespace EmitterLocalSampling
{

struct CombinedEmitterCanvas
{
    std::unique_ptr<GridContext3D> grid;
    bool valid = false;
};

bool TryBuildCombinedEmitterCanvas(
    const std::vector<std::unique_ptr<ControllerTransform>>& transforms,
    const std::unordered_set<int>& emitter_controller_indices,
    float grid_scale_mm,
    std::uint64_t render_sequence,
    CombinedEmitterCanvas& out);

} // namespace EmitterLocalSampling

#endif
