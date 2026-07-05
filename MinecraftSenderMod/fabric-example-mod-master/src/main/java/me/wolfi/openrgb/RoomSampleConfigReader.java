package me.wolfi.openrgb;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.file.Files;
import java.nio.file.Path;

/** Reads plugin-published room grid config (Game/RoomSampleFrameProtocol.h). */
final class RoomSampleConfigReader
{
    static final int CONFIG_MAGIC = 0x52434647; // RCFG
    static final short VERSION = 1;
    static final int HEADER_BYTES = 128;
    static final int FLAG_ENABLED = 1 << 1;

    static final class Config
    {
        int configId;
        int flags;
        int sizeX;
        int sizeY;
        int sizeZ;
        float roomMinX;
        float roomMinY;
        float roomMinZ;
        float roomMaxX;
        float roomMaxY;
        float roomMaxZ;
        float effectOriginX;
        float effectOriginY;
        float effectOriginZ;
        float roomToWorldScale;
        float headingOffsetDeg;
        float posOffsetForwardBlocks;
        float posOffsetRightBlocks;
        float posOffsetUpBlocks;

        int targetCells;

        boolean isEnabled()
        {
            return (flags & FLAG_ENABLED) != 0 && sizeX > 0 && sizeY > 0 && sizeZ > 0;
        }
    }

    private Path configPath;

    synchronized void ensurePath() throws IOException
    {
        if(configPath == null)
        {
            configPath = OpenRGBShmPaths.resolveFile("openrgb_mc_room_config.shm");
        }
    }

    synchronized Config readLatest() throws IOException
    {
        ensurePath();
        if(!Files.isRegularFile(configPath))
        {
            return null;
        }

        final byte[] bytes = Files.readAllBytes(configPath);
        if(bytes.length < HEADER_BYTES)
        {
            return null;
        }

        ByteBuffer buffer = ByteBuffer.wrap(bytes).order(ByteOrder.LITTLE_ENDIAN);
        final int magic = buffer.getInt();
        final short version = buffer.getShort();
        final short headerBytes = buffer.getShort();
        final int sequence = buffer.getInt();
        if(magic != CONFIG_MAGIC || version != VERSION || headerBytes != HEADER_BYTES || (sequence & 1) != 0)
        {
            return null;
        }

        final Config cfg = new Config();
        cfg.configId = buffer.getInt();
        cfg.flags = buffer.getInt();
        cfg.sizeX = buffer.getInt();
        cfg.sizeY = buffer.getInt();
        cfg.sizeZ = buffer.getInt();
        cfg.roomMinX = buffer.getFloat();
        cfg.roomMinY = buffer.getFloat();
        cfg.roomMinZ = buffer.getFloat();
        cfg.roomMaxX = buffer.getFloat();
        cfg.roomMaxY = buffer.getFloat();
        cfg.roomMaxZ = buffer.getFloat();
        cfg.effectOriginX = buffer.getFloat();
        cfg.effectOriginY = buffer.getFloat();
        cfg.effectOriginZ = buffer.getFloat();
        cfg.roomToWorldScale = buffer.getFloat();
        cfg.headingOffsetDeg = buffer.getFloat();
        cfg.posOffsetForwardBlocks = buffer.getFloat();
        cfg.posOffsetRightBlocks = buffer.getFloat();
        cfg.posOffsetUpBlocks = buffer.getFloat();
        if(buffer.remaining() >= 4)
        {
            cfg.targetCells = buffer.getInt();
        }
        else
        {
            cfg.targetCells = 800 * 600;
        }
        return cfg;
    }
}
