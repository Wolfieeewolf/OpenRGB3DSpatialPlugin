package me.wolfi.openrgb;

import net.jpountz.lz4.LZ4Compressor;
import net.jpountz.lz4.LZ4Factory;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardOpenOption;

final class GpuPanoramaFrameShmWriter
{
    static final int FRAME_MAGIC = 0x50475055; // PGPU
    static final short VERSION = 1;
    static final int HEADER_BYTES = 64;
    static final int SHM_TOTAL_BYTES = 8388608;
    static final int FLAG_LZ4 = 1 << 0;

    private static final int OFF_SEQUENCE = 8;

    private Path shmPath;
    private int sequence = 0;

    synchronized void ensurePath() throws IOException
    {
        if(shmPath == null)
        {
            shmPath = OpenRGBShmPaths.resolveFile("openrgb_mc_gpu_panorama.shm");
        }
    }

    synchronized boolean writeFrame(int frameId,
                                    long timestampMs,
                                    float anchorX,
                                    float anchorY,
                                    float anchorZ,
                                    int faceW,
                                    int faceH,
                                    int faceCount,
                                    byte[] rgbaRaw) throws IOException
    {
        ensurePath();
        if(rgbaRaw == null || rgbaRaw.length == 0 || faceW <= 0 || faceH <= 0 || faceCount <= 0)
        {
            return false;
        }

        final LZ4Factory lz4 = LZ4Factory.fastestInstance();
        final LZ4Compressor compressor = lz4.fastCompressor();
        final int maxCompressed = compressor.maxCompressedLength(rgbaRaw.length);
        final byte[] compressed = new byte[maxCompressed];
        final int storedSize = compressor.compress(rgbaRaw, 0, rgbaRaw.length, compressed, 0, maxCompressed);

        if(HEADER_BYTES + storedSize > SHM_TOTAL_BYTES)
        {
            return false;
        }

        final byte[] shm = new byte[SHM_TOTAL_BYTES];
        final ByteBuffer buffer = ByteBuffer.wrap(shm).order(ByteOrder.LITTLE_ENDIAN);

        sequence += 2;
        buffer.putInt(FRAME_MAGIC);
        buffer.putShort(VERSION);
        buffer.putShort((short)HEADER_BYTES);
        buffer.putInt(sequence | 1);
        buffer.putInt(frameId);
        buffer.putLong(timestampMs);
        buffer.putFloat(anchorX);
        buffer.putFloat(anchorY);
        buffer.putFloat(anchorZ);
        buffer.putShort((short)faceW);
        buffer.putShort((short)faceH);
        buffer.put((byte)faceCount);
        buffer.put((byte)0);
        buffer.putShort((short)0);
        buffer.putInt(rgbaRaw.length);
        buffer.putInt(storedSize);
        buffer.putInt(FLAG_LZ4);

        buffer.position(HEADER_BYTES);
        buffer.put(compressed, 0, storedSize);

        buffer.putInt(OFF_SEQUENCE, sequence);

        Files.write(shmPath,
                shm,
                StandardOpenOption.CREATE,
                StandardOpenOption.TRUNCATE_EXISTING,
                StandardOpenOption.WRITE);
        return true;
    }
}
