// SPDX-License-Identifier: GPL-2.0-only
package me.wolfi.openrgb;

import net.jpountz.lz4.LZ4Compressor;
import net.jpountz.lz4.LZ4Factory;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.IOException;
import java.io.RandomAccessFile;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.MappedByteBuffer;
import java.nio.channels.FileChannel;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Publishes room sample frames without blocking the Minecraft client tick on LZ4 or disk.
 * Latest-frame-wins queue: if the worker is busy, older pending frames are dropped.
 * Uses an in-place memory-mapped 4 MiB file (seqlock) instead of truncate-rewrite each frame.
 */
final class RoomSampleFrameShmWriter
{
    static final int FRAME_MAGIC = 0x5253414D; // RSAM
    static final short VERSION = 1;
    static final int HEADER_BYTES = 64;
    static final int SHM_TOTAL_BYTES = 4194304;
    static final int FLAG_LZ4 = 1 << 0;

    private static final int OFF_SEQUENCE = 8;
    private static final Logger LOGGER = LoggerFactory.getLogger("openrgb-sender");

    @FunctionalInterface
    interface PublishListener
    {
        void onPublished(int frameId, long timestampMs, int configId);
    }

    private Path shmPath;
    private RandomAccessFile raf;
    private MappedByteBuffer mapped;
    private int sequence = 0;

    private final Object queueLock = new Object();
    private byte[] pendingRgba = new byte[0];
    private byte[] freeRgba = new byte[0];
    private byte[] spareRgba = new byte[0];
    private int pendingRgbaLen = 0;
    private int pendingFrameId;
    private long pendingTimestampMs;
    private int pendingConfigId;
    private int pendingSizeX;
    private int pendingSizeY;
    private int pendingSizeZ;
    private boolean hasPending = false;
    private final AtomicBoolean running = new AtomicBoolean(false);
    private Thread worker;
    private volatile PublishListener publishListener;

    private final byte[] shmScratch = new byte[SHM_TOTAL_BYTES];
    private final ByteBuffer shmScratchBuf = ByteBuffer.wrap(shmScratch).order(ByteOrder.LITTLE_ENDIAN);
    private byte[] compressScratch = new byte[0];
    private final LZ4Compressor lz4 = LZ4Factory.fastestInstance().fastCompressor();
    private long droppedFrames = 0L;
    private static final byte[] EMPTY = new byte[0];

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
     * Zero-copy publish with a fixed 2-buffer pool (no per-drop allocations).
     * Takes ownership of {@code filled} and returns a same-sized buffer for the next sample.
     */
    byte[] offerSwap(int frameId,
                     long timestampMs,
                     int configId,
                     int sizeX,
                     int sizeY,
                     int sizeZ,
                     byte[] filled)
    {
        if(filled == null || filled.length == 0)
        {
            return filled;
        }
        ensureWorker();
        synchronized(queueLock)
        {
            ensureSpareSize(filled.length);
            final byte[] ret;
            if(hasPending)
            {
                droppedFrames++;
                if((droppedFrames % 120L) == 0L)
                {
                    LOGGER.debug("Room sample publish dropped {} frames under load (latest-wins)", droppedFrames);
                }
                ret = pendingRgba; // recycle superseded pending for next sample
            }
            else if(freeRgba.length == filled.length)
            {
                ret = freeRgba;
                freeRgba = EMPTY;
            }
            else
            {
                ret = spareRgba;
                spareRgba = EMPTY;
            }
            pendingRgba = filled;
            pendingRgbaLen = filled.length;
            pendingFrameId = frameId;
            pendingTimestampMs = timestampMs;
            pendingConfigId = configId;
            pendingSizeX = sizeX;
            pendingSizeY = sizeY;
            pendingSizeZ = sizeZ;
            hasPending = true;
            queueLock.notifyAll();
            return ret;
        }
    }

    private void ensureSpareSize(int len)
    {
        if(spareRgba.length != len)
        {
            spareRgba = new byte[len];
        }
    }

    void shutdown()
    {
        running.set(false);
        synchronized(queueLock)
        {
            queueLock.notifyAll();
        }
        final Thread t = worker;
        if(t != null)
        {
            try
            {
                t.join(500L);
            }
            catch(InterruptedException ignored)
            {
                Thread.currentThread().interrupt();
            }
        }
        synchronized(this)
        {
            mapped = null;
            if(raf != null)
            {
                try
                {
                    raf.close();
                }
                catch(IOException ignored)
                {
                }
                raf = null;
            }
        }
    }

    private void ensureWorker()
    {
        if(running.get())
        {
            return;
        }
        if(!running.compareAndSet(false, true))
        {
            return;
        }
        worker = new Thread(this::workerLoop, "openrgb-room-sample-publish");
        worker.setDaemon(true);
        worker.setPriority(Thread.NORM_PRIORITY - 1);
        worker.start();
        LOGGER.info("Room sample async publisher started (mmap + latest-frame-wins)");
    }

    private void workerLoop()
    {
        while(running.get())
        {
            final int frameId;
            final long timestampMs;
            final int configId;
            final int sizeX;
            final int sizeY;
            final int sizeZ;
            final byte[] workRgba;
            final int rgbaLen;
            synchronized(queueLock)
            {
                while(running.get() && !hasPending)
                {
                    try
                    {
                        queueLock.wait(250L);
                    }
                    catch(InterruptedException e)
                    {
                        Thread.currentThread().interrupt();
                        return;
                    }
                }
                if(!running.get() || !hasPending)
                {
                    continue;
                }
                frameId = pendingFrameId;
                timestampMs = pendingTimestampMs;
                configId = pendingConfigId;
                sizeX = pendingSizeX;
                sizeY = pendingSizeY;
                sizeZ = pendingSizeZ;
                rgbaLen = pendingRgbaLen;
                workRgba = pendingRgba;
                pendingRgba = EMPTY;
                hasPending = false;
            }

            try
            {
                if(publishNow(frameId, timestampMs, configId, sizeX, sizeY, sizeZ, workRgba, rgbaLen))
                {
                    final PublishListener listener = publishListener;
                    if(listener != null)
                    {
                        listener.onPublished(frameId, timestampMs, configId);
                    }
                }
            }
            catch(Throwable t)
            {
                QuietCatch.debug(LOGGER, "room sample async publish failed", t);
            }
            finally
            {
                synchronized(queueLock)
                {
                    freeRgba = workRgba;
                }
            }
        }
    }

    private synchronized boolean publishNow(int frameId,
                                            long timestampMs,
                                            int configId,
                                            int sizeX,
                                            int sizeY,
                                            int sizeZ,
                                            byte[] rgbaRaw,
                                            int rgbaLen) throws IOException
    {
        ensureMapped();
        if(rgbaRaw == null || rgbaLen <= 0)
        {
            return false;
        }

        final int maxCompressed = lz4.maxCompressedLength(rgbaLen);
        if(compressScratch.length < maxCompressed)
        {
            compressScratch = new byte[maxCompressed];
        }
        final int storedSize = lz4.compress(rgbaRaw, 0, rgbaLen, compressScratch, 0, maxCompressed);
        if(HEADER_BYTES + storedSize > SHM_TOTAL_BYTES)
        {
            return false;
        }

        sequence += 2;
        final int evenSeq = sequence;
        final int oddSeq = evenSeq | 1;
        final int payloadBytes = HEADER_BYTES + storedSize;

        // Seqlock: odd while mutating mapped region, even when coherent.
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
        buffer.putInt(rgbaLen);
        buffer.putInt(storedSize);
        buffer.putInt(FLAG_LZ4);
        buffer.position(HEADER_BYTES);
        buffer.put(compressScratch, 0, storedSize);
        buffer.putInt(OFF_SEQUENCE, evenSeq);

        mapped.position(0);
        mapped.put(shmScratch, 0, payloadBytes);
        mapped.putInt(OFF_SEQUENCE, evenSeq);
        return true;
    }
}
