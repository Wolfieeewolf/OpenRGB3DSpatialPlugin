// SPDX-License-Identifier: GPL-2.0-only
//
// Coordinate-space contract for room effects and game telemetry.
//
// There is no single "game industry" world axis layout. Engines disagree on
// handedness, which axis is up, and which is forward. See the table below.
// This plugin therefore uses THREE explicit spaces and converts between them.
//
// ---- Game engine world-space survey (native axes, not view/camera) ----
// | Engine / API        | Handed | Up | Forward (fixed axis) | Notes              |
// |---------------------|--------|----|----------------------|--------------------|
// | Unity               | left   | +Y | +Z                   | Largest mod ecosystem |
// | Unreal Engine       | left   | +Z | +X                   | Z-up world         |
// | OpenGL / Three.js     | right  | +Y | -Z (camera view)     | View space; world varies |
// | Direct3D / Vulkan view| left   | +Y | +Z                   | View space         |
// | Blender / 3ds Max   | right  | +Z | -Y (Max front view)  | DCC tools          |
// | glTF (asset)          | right  | +Y | +Z                   | Asset interchange  |
// | Quake / Source-ish    | right  | +Z | +X or +Y varies      |                    |
// | Minecraft             | right  | +Y | (none — look vector) | +X east, +Z south  |
//
// References: https://30fps.net/xyz/ , OpenGL / Unity / Unreal docs.
//
// ---- Plugin spaces (do not merge these) ----
//
// 1) RoomGrid — physical room layout (architectural, right-handed)
//    Origin: front-left floor (grid min bounds).
//    +X: right along room width.  +Y: up (floor → ceiling).
//    +Z: depth toward the back wall (Z=0 is the front wall).
//    This is NOT an engine convention; it matches real rooms and GridSpaceUtils.
//    Do not change this to match Unity/Unreal world axes.
//
// 2) PlayerLocal — derived each frame from telemetry (right-handed)
//    +X: player right, +Y: up (from telemetry up vector), +Z: view forward.
//    Built by SpatialBasisUtils (right = forward × up).
//    Matches the usual FPS/VR pose basis when the game world is right-handed Y-up.
//    Left-handed sources (Unity, Unreal) must tag their convention at ingest; see
//    GameTelemetryBridge::GameWorldConvention.
//
// 3) GameWorld — per-title native world coordinates
//    Position (x,y,z) and probe samples are always in the game's native space.
//    Forward/up unit vectors are in the same native space.
//    Minecraft: +X east, +Y up, +Z south (right-handed Y-up; forward = look).
//
// Pipeline: RoomGrid offset → PlayerLocal offset → GameWorld sample point
//           (via orthonormal basis from telemetry forward/up).

#ifndef SPATIALCOORDINATESPACES_H
#define SPATIALCOORDINATESPACES_H

#include "SpatialBasisUtils.h"

namespace SpatialCoordinateSpaces
{

/** Native world axis layout reported by a game mod (optional JSON: world_convention). */
enum class GameWorldConvention : int
{
    /** Right-handed, +Y up. Minecraft, glTF-style, many PC titles. Default. */
    RightHandedYUp = 0,
    /** Left-handed, +Y up, +Z forward. Unity, DirectX-style. */
    LeftHandedYUpUnity = 1,
    /** Left-handed, +Z up, +X forward. Unreal Engine world. */
    LeftHandedZUpUnreal = 2,
};

struct RoomGridDelta
{
    float right_blocks = 0.0f;
    float up_blocks = 0.0f;
    float forward_blocks = 0.0f;
};

/** Room grid position minus calibrated origin, converted to player-local block offsets. */
static inline RoomGridDelta RoomGridToPlayerLocalBlocks(float room_x,
                                                        float room_y,
                                                        float room_z,
                                                        float origin_x,
                                                        float origin_y,
                                                        float origin_z,
                                                        float blocks_per_grid_unit)
{
    RoomGridDelta out{};
    out.right_blocks = (room_x - origin_x) * blocks_per_grid_unit;
    out.up_blocks = (room_y - origin_y) * blocks_per_grid_unit;
    // Room +Z is toward the back wall; player-local +Z is view forward.
    out.forward_blocks = (origin_z - room_z) * blocks_per_grid_unit;
    return out;
}

/** Apply player-local block offsets in the orthonormal player basis (right, up, forward). */
static inline void PlayerLocalBlocksToGameWorld(const SpatialBasisUtils::BasisVectors& basis,
                                                float anchor_x,
                                                float anchor_y,
                                                float anchor_z,
                                                const RoomGridDelta& local,
                                                float& out_world_x,
                                                float& out_world_y,
                                                float& out_world_z)
{
    out_world_x = anchor_x + local.right_blocks * basis.right_x + local.up_blocks * basis.up_x +
                  local.forward_blocks * basis.forward_x;
    out_world_y = anchor_y + local.right_blocks * basis.right_y + local.up_blocks * basis.up_y +
                  local.forward_blocks * basis.forward_y;
    out_world_z = anchor_z + local.right_blocks * basis.right_z + local.up_blocks * basis.up_z +
                  local.forward_blocks * basis.forward_z;
}

/** Build a right-handed player basis from game-native forward/up unit vectors. */
static inline SpatialBasisUtils::BasisVectors BuildPlayerBasisFromGamePose(float forward_x,
                                                                           float forward_y,
                                                                           float forward_z,
                                                                           float up_x,
                                                                           float up_y,
                                                                           float up_z,
                                                                           GameWorldConvention convention)
{
    float fx = forward_x;
    float fy = forward_y;
    float fz = forward_z;
    float ux = up_x;
    float uy = up_y;
    float uz = up_z;

    if(convention == GameWorldConvention::LeftHandedZUpUnreal)
    {
        // Unreal world (+X forward, +Y right, +Z up) → working RH Y-up vectors.
        const float tx = fx;
        const float ty = fy;
        const float tz = fz;
        fx = tx;
        fy = tz;
        fz = ty;
        const float tux = ux;
        const float tuy = uy;
        const float tuz = uz;
        ux = tux;
        uy = tuz;
        uz = tuy;
    }

    SpatialBasisUtils::BasisVectors basis =
        SpatialBasisUtils::BuildOrthonormalBasis(fx, fy, fz, ux, uy, uz);

    if(convention == GameWorldConvention::LeftHandedYUpUnity)
    {
        basis.right_x = -basis.right_x;
        basis.right_y = -basis.right_y;
        basis.right_z = -basis.right_z;
    }

    return basis;
}

}

#endif
