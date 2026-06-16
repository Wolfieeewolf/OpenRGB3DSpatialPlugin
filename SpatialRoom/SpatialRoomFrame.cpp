// SPDX-License-Identifier: GPL-2.0-only

#include "SpatialRoomFrame.h"

namespace SpatialRoom
{
namespace
{
SpatialRoomFrameContext g_frame{};
int g_overlay_pass_depth = 0;
} // namespace

const SpatialRoomFrameContext& CurrentFrameContext()
{
    return g_frame;
}

void BeginEffectRenderFrame(std::uint64_t render_sequence, SpatialRoomDepthPreset preset)
{
    g_frame.render_sequence = render_sequence;
    g_frame.depth_preset = preset;
    g_frame.room_grid_overlay_pass = false;
    g_overlay_pass_depth = 0;
}

void EndEffectRenderFrame()
{
    g_overlay_pass_depth = 0;
    g_frame.room_grid_overlay_pass = false;
}

void BeginRoomGridOverlayPass()
{
    ++g_overlay_pass_depth;
    g_frame.room_grid_overlay_pass = true;
}

void EndRoomGridOverlayPass()
{
    if(g_overlay_pass_depth > 0)
    {
        --g_overlay_pass_depth;
    }
    if(g_overlay_pass_depth <= 0)
    {
        g_overlay_pass_depth = 0;
        g_frame.room_grid_overlay_pass = false;
    }
}

bool IsRoomGridOverlayPass()
{
    return g_frame.room_grid_overlay_pass;
}

bool ShouldUseOverlayFastPreview()
{
    return g_frame.room_grid_overlay_pass;
}

} // namespace SpatialRoom
