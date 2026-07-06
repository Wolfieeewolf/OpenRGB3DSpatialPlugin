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
        final float gridUnitsPerBlock = 1.0f / scale;

        final float effOx = cfg.effectOriginX - cfg.posOffsetRightBlocks * gridUnitsPerBlock;
        final float effOy = cfg.effectOriginY - cfg.posOffsetUpBlocks * gridUnitsPerBlock;
        final float effOz = cfg.effectOriginZ + cfg.posOffsetForwardBlocks * gridUnitsPerBlock;

        final float rightBlocks = (roomX - effOx) * scale;
        final float upBlocks = (roomY - effOy) * scale;
        final float forwardBlocks = (effOz - roomZ) * scale;

        final Basis basis = buildHorizontalBasis(player, cfg.headingOffsetDeg);
        final double ax = player.getX();
        final double ay = player.getEyeY();
        final double az = player.getZ();

        final double wx = ax + rightBlocks * basis.rightX + upBlocks * basis.upX + forwardBlocks * basis.forwardX;
        final double wy = ay + rightBlocks * basis.rightY + upBlocks * basis.upY + forwardBlocks * basis.forwardY;
        final double wz = az + rightBlocks * basis.rightZ + upBlocks * basis.upZ + forwardBlocks * basis.forwardZ;
        return new Vec3(wx, wy, wz);
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

    private static Basis buildHorizontalBasis(LocalPlayer player, float headingOffsetDeg)
    {
        final Vec3 look = player.getViewVector(1.0f);
        double lx = look.x;
        double ly = look.y;
        double lz = look.z;
        double ll = Math.sqrt(lx * lx + ly * ly + lz * lz);
        if(ll <= 1e-5)
        {
            lx = 0.0;
            ly = 0.0;
            lz = 1.0;
        }
        else
        {
            lx /= ll;
            ly /= ll;
            lz /= ll;
        }

        final double ux = 0.0;
        final double uy = 1.0;
        final double uz = 0.0;
        final double horiz = lx * ux + ly * uy + lz * uz;
        double fx = lx - horiz * ux;
        double fy = ly - horiz * uy;
        double fz = lz - horiz * uz;
        double fl = Math.sqrt(fx * fx + fy * fy + fz * fz);
        if(fl <= 1e-5)
        {
            fx = 0.0;
            fy = 0.0;
            fz = 1.0;
        }
        else
        {
            fx /= fl;
            fy /= fl;
            fz /= fl;
        }

        double rx = fy * uz - fz * uy;
        double ry = fz * ux - fx * uz;
        double rz = fx * uy - fy * ux;
        double rl = Math.sqrt(rx * rx + ry * ry + rz * rz);
        if(rl <= 1e-5)
        {
            rx = 1.0;
            ry = 0.0;
            rz = 0.0;
        }
        else
        {
            rx /= rl;
            ry /= rl;
            rz /= rl;
        }

        final double yaw = headingOffsetDeg * 0.01745329251;
        if(Math.abs(yaw) > 1e-5)
        {
            final double c = Math.cos(yaw);
            final double s = Math.sin(yaw);
            final double fx2 = fx * c + rx * s;
            final double fy2 = fy * c + ry * s;
            final double fz2 = fz * c + rz * s;
            final double rx2 = rx * c - fx * s;
            final double ry2 = ry * c - fy * s;
            final double rz2 = rz * c - fz * s;
            fx = fx2;
            fy = fy2;
            fz = fz2;
            rx = rx2;
            ry = ry2;
            rz = rz2;
        }

        return new Basis(fx, fy, fz, ux, uy, uz, rx, ry, rz);
    }
}
