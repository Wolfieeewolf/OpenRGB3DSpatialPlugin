// SPDX-License-Identifier: GPL-2.0-only
// Map emitter LED positions into a per-device effect canvas (full pattern on each emitter).

#ifndef EMITTERLOCALSAMPLING_H
#define EMITTERLOCALSAMPLING_H

#include "LEDPosition3D.h"

#include <cstdint>

struct ControllerTransform;
struct GridContext3D;

namespace EmitterLocalSampling
{

/** Build sample coords so the effect fills the emitter's LED layout (origin at device center). */
bool TryBuildEmitterLocalSample(const ControllerTransform* ctrl,
                                const LEDPosition3D& led,
                                float grid_scale_mm,
                                std::uint64_t render_sequence,
                                float& out_x,
                                float& out_y,
                                float& out_z,
                                GridContext3D& out_grid);

} // namespace EmitterLocalSampling

#endif
