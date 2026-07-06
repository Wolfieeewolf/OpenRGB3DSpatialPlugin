package me.wolfi.openrgb;

/**
 * Thread-safe store for GPU-read cubemap faces (16×16 RGBA per face).
 * Written on the render thread, merged into the semantic cubemap on the client tick thread.
 */
final class GpuCubemapCapture
{
    static final int FACE_SIZE = 16;
    static final int FACE_COUNT = 6;

    private static final byte[] GPU_FACES = new byte[FACE_COUNT * FACE_SIZE * FACE_SIZE * 4];
    private static final boolean[] FACE_VALID = new boolean[FACE_COUNT];

    private GpuCubemapCapture()
    {
    }

    static void writeFace(int faceIndex, byte[] rgba)
    {
        if(faceIndex < 0 || faceIndex >= FACE_COUNT || rgba == null)
        {
            return;
        }
        final int need = FACE_SIZE * FACE_SIZE * 4;
        if(rgba.length < need)
        {
            return;
        }
        synchronized(GPU_FACES)
        {
            System.arraycopy(rgba, 0, GPU_FACES, faceIndex * need, need);
            FACE_VALID[faceIndex] = true;
        }
    }

    static void applyToCubemap(byte[] cubemapBuffer, int faceW, int faceCount)
    {
        if(cubemapBuffer == null || faceW != FACE_SIZE || faceCount != FACE_COUNT)
        {
            return;
        }
        synchronized(GPU_FACES)
        {
            for(int face = 0; face < FACE_COUNT; face++)
            {
                if(!FACE_VALID[face])
                {
                    continue;
                }
                final int srcOff = face * FACE_SIZE * FACE_SIZE * 4;
                final int dstOff = face * faceW * faceW * 4;
                System.arraycopy(GPU_FACES, srcOff, cubemapBuffer, dstOff, FACE_SIZE * FACE_SIZE * 4);
            }
        }
    }

    static int countValidFaces()
    {
        synchronized(GPU_FACES)
        {
            int n = 0;
            for(boolean valid : FACE_VALID)
            {
                if(valid)
                {
                    n++;
                }
            }
            return n;
        }
    }

    /**
     * Maps a world-space unit direction to cubemap face index (matches GpuPanoramaMapping.cpp).
     */
    static int directionToFace(float dx, float dy, float dz)
    {
        final float ax = Math.abs(dx);
        final float ay = Math.abs(dy);
        final float az = Math.abs(dz);
        if(ax >= ay && ax >= az)
        {
            return dx >= 0.0f ? 0 : 1;
        }
        if(ay >= ax && ay >= az)
        {
            return dy >= 0.0f ? 2 : 3;
        }
        return dz >= 0.0f ? 4 : 5;
    }

    /** Minecraft camera yaw/pitch to look along the centre of a world-aligned cubemap face. */
    static void faceToYawPitch(int face, float[] outYawPitch)
    {
        switch(face)
        {
            case 0 -> { outYawPitch[0] = -90.0f; outYawPitch[1] = 0.0f; }      // +X east
            case 1 -> { outYawPitch[0] = 90.0f;  outYawPitch[1] = 0.0f; }      // -X west
            case 2 -> { outYawPitch[0] = 0.0f;    outYawPitch[1] = -90.0f; }    // +Y sky
            case 3 -> { outYawPitch[0] = 0.0f;    outYawPitch[1] = 90.0f; }     // -Y ground
            case 4 -> { outYawPitch[0] = 0.0f;    outYawPitch[1] = 0.0f; }      // +Z south
            default -> { outYawPitch[0] = 180.0f; outYawPitch[1] = 0.0f; }     // -Z north
        }
    }
}
