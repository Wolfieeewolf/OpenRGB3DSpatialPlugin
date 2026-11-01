// SPDX-License-Identifier: GPL-2.0-only
package me.wolfi.openrgb;

import java.io.IOException;
import java.io.RandomAccessFile;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.MappedByteBuffer;
import java.nio.channels.FileChannel;
import java.nio.file.Files;
import java.nio.file.Path;

/**
 * Publishes sparse LED-texel cubemap frames in-place (seqlock).
 * Payload is O(LEDs) — never face²×6 — so face size 512 stays viable.
 */
final class RoomSampleFrameShmWriter
{
    static final int FRAME_MAGIC = 0x5253414D; // RSAM
    static final short VERSION = 1;
    static final int HEADER_BYTES = 64;
    static final int SHM_TOTAL_BYTES = 8 * 1024 * 1024;
    static final int FLAG_SPARSE_LED_TEXELS = 1 << 7;

    private static final int OFF_SEQUENCE = 8;

    @FunctionalInterface
    interface PublishListener
    {
        void onPublished(int frameId, long timestampMs, int configId);
    }

    private Path shmPath;
    private RandomAccessFile raf;
    private MappedByteBuffer mapped;
    private int sequence = 0;
    private volatile PublishListener publishListener;

    private final byte[] shmScratch = new byte[SHM_TOTAL_BYTES];
    private final ByteBuffer shmScratchBuf = ByteBuffer.wrap(shmScratch).order(ByteOrder.LITTLE_ENDIAN);

    void setPublishListener(PublishListener listener)
    {
        this.publishListener = listener;
    }

    synchronized void ensureMapped() throws IOException
    {
        if(mapped != null)
        {
            return;
        }
        if(shmPath == null)
        {
            shmPath = OpenRGBShmPaths.resolveFile("openrgb_mc_room_sample.shm");
        }
        if(!Files.isRegularFile(shmPath) || Files.size(shmPath) != SHM_TOTAL_BYTES)
        {
            Files.createDirectories(shmPath.getParent());
            try(RandomAccessFile create = new RandomAccessFile(shmPath.toFile(), "rw"))
            {
                create.setLength(SHM_TOTAL_BYTES);
            }
        }
        raf = new RandomAccessFile(shmPath.toFile(), "rw");
        mapped = raf.getChannel().map(FileChannel.MapMode.READ_WRITE, 0, SHM_TOTAL_BYTES);
        mapped.order(ByteOrder.LITTLE_ENDIAN);
    }

    /**
     * {@code ledRgba} is compact (important.length × 4).
     */
    boolean publishSparseLedTexels(int frameId,
                                   long timestampMs,
                                   int configId,
                                   int sizeX,
                                   int sizeY,
                                   int sizeZ,
                                   int[] important,
                                   byte[] ledRgba) throws IOException
    {
        if(important == null || ledRgba == null || important.length == 0)
        {
            return false;
        }
        final int count = important.length;
        if(ledRgba.length < count * 4)
        {
            return false;
        }
        final int storedSize = 4 + count * 8;
        if(HEADER_BYTES + storedSize > SHM_TOTAL_BYTES)
        {
            return false;
        }
        final int denseBytes = sizeX * sizeY * sizeZ * 4;
        if(denseBytes <= 0)
        {
            return false;
        }

        synchronized(this)
        {
            ensureMapped();
            sequence += 2;
            final int evenSeq = sequence;
            final int oddSeq = evenSeq | 1;

            mapped.putInt(OFF_SEQUENCE, oddSeq);

            final ByteBuffer buffer = shmScratchBuf;
            buffer.clear();
            buffer.putInt(FRAME_MAGIC);
            buffer.putShort(VERSION);
            buffer.putShort((short)HEADER_BYTES);
            buffer.putInt(oddSeq);
            buffer.putInt(frameId);
            buffer.putLong(timestampMs);
            buffer.putInt(configId);
            buffer.putInt(sizeX);
            buffer.putInt(sizeY);
            buffer.putInt(sizeZ);
            buffer.putInt(denseBytes);
            buffer.putInt(storedSize);
            buffer.putInt(FLAG_SPARSE_LED_TEXELS);
            buffer.position(HEADER_BYTES);
            buffer.putInt(count);
            for(int i = 0; i < count; i++)
            {
                buffer.putInt(important[i]);
                final int o = i * 4;
                buffer.put(ledRgba[o]);
                buffer.put(ledRgba[o + 1]);
                buffer.put(ledRgba[o + 2]);
                buffer.put(ledRgba[o + 3]);
            }
            // Keep header sequence odd for the whole memcpy so readers never see even+torn payload.
            buffer.putInt(OFF_SEQUENCE, oddSeq);

            mapped.position(0);
            mapped.put(shmScratch, 0, HEADER_BYTES + storedSize);
            mapped.putInt(OFF_SEQUENCE, evenSeq);
        }

        final PublishListener listener = publishListener;
        if(listener != null)
        {
            listener.onPublished(frameId, timestampMs, configId);
        }
        return true;
    }
}
