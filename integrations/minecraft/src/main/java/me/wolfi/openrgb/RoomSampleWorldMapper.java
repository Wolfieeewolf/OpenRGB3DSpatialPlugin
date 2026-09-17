package me.wolfi.openrgb;

import net.minecraft.client.player.LocalPlayer;
import net.minecraft.world.phys.Vec3;

final class RoomSampleWorldMapper
{
    private RoomSampleWorldMapper()
    {
    }

    static float roomCellCenterX(RoomSampleConfigReader.Config cfg, int ix)
    {
        final float span = Math.max(1e-6f, cfg.roomMaxX - cfg.roomMinX);
        return cfg.roomMinX + (ix + 0.5f) * span / (float)cfg.sizeX;
    }

    static float roomCellCenterY(RoomSampleConfigReader.Config cfg, int iy)
    {
        final float span = Math.max(1e-6f, cfg.roomMaxY - cfg.roomMinY);
        return cfg.roomMinY + (iy + 0.5f) * span / (float)cfg.sizeY;
    }

    static float roomCellCenterZ(RoomSampleConfigReader.Config cfg, int iz)
    {
        final float span = Math.max(1e-6f, cfg.roomMaxZ - cfg.roomMinZ);
        return cfg.roomMinZ + (iz + 0.5f) * span / (float)cfg.sizeZ;
    }

    /**
     * Maps a room-grid sample to a world target. Anchor is the player eye position so room offsets
     * align with plugin reference points (eye height), not feet.
     */
    static Vec3 mapRoomToWorldTarget(RoomSampleConfigReader.Config cfg, LocalPlayer player, float roomX, float roomY, float roomZ)
    {
        final float scale = Math.max(0.005f, Math.min(0.80f, cfg.roomToWorldScale));
        final float rightBlocks = (roomX - cfg.effectOriginX) * scale;
        final float upBlocks = (roomY - cfg.effectOriginY) * scale;
        final float forwardBlocks = (cfg.effectOriginZ - roomZ) * scale;

        final Basis basis = buildHorizontalBasis(player);
        final double ax = player.getX();
        final double ay = player.getEyeY();
        final double az = player.getZ();

        final double wx = ax + rightBlocks * basis.rightX + upBlocks * basis.upX + forwardBlocks * basis.forwardX;
        final double wy = ay + rightBlocks * basis.rightY + upBlocks * basis.upY + forwardBlocks * basis.forwardY;
        final double wz = az + rightBlocks * basis.rightZ + upBlocks * basis.upZ + forwardBlocks * basis.forwardZ;
        return new Vec3(wx, wy, wz);
    }

    /**
     * Eye + player-local direction (right, up, forward) scaled by range — used for cubemap rays.
     * Local axes match OpenGL-style player frame: +X right, +Y up, +Z look-forward (horizontal).
     * Minecraft world: +X east, +Y up, +Z south; yaw 0 faces south (+Z).
     */
    static Vec3 mapLocalDirToWorldTarget(RoomSampleConfigReader.Config cfg,
                                         LocalPlayer player,
                                         float localRight,
                                         float localUp,
                                         float localForward,
                                         double range)
    {
        final Basis basis = buildHorizontalBasis(player);
        final Vec3 eye = player.getEyePosition();
        final double wx = eye.x + range * (localRight * basis.rightX + localUp * basis.upX + localForward * basis.forwardX);
        final double wy = eye.y + range * (localRight * basis.rightY + localUp * basis.upY + localForward * basis.forwardY);
        final double wz = eye.z + range * (localRight * basis.rightZ + localUp * basis.upZ + localForward * basis.forwardZ);
        return new Vec3(wx, wy, wz);
    }

    /**
     * Distance in blocks from the effect origin to the published room AABB along a local direction.
     * Cubemap rays stop here so we never sample beyond the physical room grid.
     */
    static double roomRayRangeBlocks(RoomSampleConfigReader.Config cfg,
                                     float localRight,
                                     float localUp,
                                     float localForward)
    {
        final float scale = Math.max(0.005f, Math.min(0.80f, cfg.roomToWorldScale));
        final float ox = cfg.effectOriginX;
        final float oy = cfg.effectOriginY;
        final float oz = cfg.effectOriginZ;

        float minR = (cfg.roomMinX - ox) * scale;
        float maxR = (cfg.roomMaxX - ox) * scale;
        float minU = (cfg.roomMinY - oy) * scale;
        float maxU = (cfg.roomMaxY - oy) * scale;
        // forwardBlocks = (oz - roomZ) * scale  → roomZ min/max map to forward max/min
        float maxF = (oz - cfg.roomMinZ) * scale;
        float minF = (oz - cfg.roomMaxZ) * scale;
        if(minR > maxR)
        {
            final float t = minR;
            minR = maxR;
            maxR = t;
        }
        if(minU > maxU)
        {
            final float t = minU;
            minU = maxU;
            maxU = t;
        }
        if(minF > maxF)
        {
            final float t = minF;
            minF = maxF;
            maxF = t;
        }

        double tExit = Double.POSITIVE_INFINITY;
        tExit = Math.min(tExit, slabExit(localRight, minR, maxR));
        tExit = Math.min(tExit, slabExit(localUp, minU, maxU));
        tExit = Math.min(tExit, slabExit(localForward, minF, maxF));

        if(!Double.isFinite(tExit) || tExit < 0.35)
        {
            final double hx = Math.max(Math.abs(minR), Math.abs(maxR));
            final double hy = Math.max(Math.abs(minU), Math.abs(maxU));
            final double hz = Math.max(Math.abs(minF), Math.abs(maxF));
            tExit = Math.sqrt(hx * hx + hy * hy + hz * hz);
        }
        return Math.max(0.5, tExit + 0.35);
    }

    /** Half-diagonal of the room in blocks — upper bound for entity probe radius. */
    static double roomHalfDiagonalBlocks(RoomSampleConfigReader.Config cfg)
    {
        final float scale = Math.max(0.005f, Math.min(0.80f, cfg.roomToWorldScale));
        final double hx = 0.5 * Math.max(1e-3f, cfg.roomMaxX - cfg.roomMinX) * scale;
        final double hy = 0.5 * Math.max(1e-3f, cfg.roomMaxY - cfg.roomMinY) * scale;
        final double hz = 0.5 * Math.max(1e-3f, cfg.roomMaxZ - cfg.roomMinZ) * scale;
        return Math.sqrt(hx * hx + hy * hy + hz * hz);
    }

    private static double slabExit(float dir, float min, float max)
    {
        if(Math.abs(dir) < 1e-6f)
        {
            return Double.POSITIVE_INFINITY;
        }
        final float bound = dir > 0.0f ? max : min;
        final double t = bound / (double)dir;
        return t > 1e-5 ? t : Double.POSITIVE_INFINITY;
    }

    private static final class Basis
    {
        final double forwardX;
        final double forwardY;
        final double forwardZ;
        final double upX;
        final double upY;
        final double upZ;
        final double rightX;
        final double rightY;
        final double rightZ;

        Basis(double fx, double fy, double fz, double ux, double uy, double uz, double rx, double ry, double rz)
        {
            forwardX = fx;
            forwardY = fy;
            forwardZ = fz;
            upX = ux;
            upY = uy;
            upZ = uz;
            rightX = rx;
            rightY = ry;
            rightZ = rz;
        }
    }

    /**
     * Yaw-locked player basis from Minecraft rotation (not view-vector pitch).
     * Yaw 0 = south (+Z), +90 = west (−X) — https://minecraft.wiki/w/Rotation
     */
    private static Basis buildHorizontalBasis(LocalPlayer player)
    {
        final double yawRad = Math.toRadians(player.getYRot());
        double fx = -Math.sin(yawRad);
        double fy = 0.0;
        double fz = Math.cos(yawRad);

        final double ux = 0.0;
        final double uy = 1.0;
        final double uz = 0.0;

        double rx = fy * uz - fz * uy;
        double ry = fz * ux - fx * uz;
        double rz = fx * uy - fy * ux;
        double rl = Math.sqrt(rx * rx + ry * ry + rz * rz);
        if(rl <= 1e-5)
        {
            rx = 1.0;
            ry = 0.0;
            rz = 0.0;
            rl = 1.0;
        }
        rx /= rl;
        ry /= rl;
        rz /= rl;

        return new Basis(fx, fy, fz, ux, uy, uz, rx, ry, rz);
    }
}
