package me.wolfi.openrgb;

/**
 * Cubemap face UV ↔ player-local direction. Must match Game/RoomSampleFrameProtocol.h
 * (DirectionToCubemapUv / CubemapUvToDirection). Face order: +X,-X,+Y,-Y,+Z,-Z.
 */
final class RoomSampleCubemap
{
    static final int FACE_COUNT = 6;
    static final int FACE_SIZE_MAX = 512;

    private RoomSampleCubemap()
    {
    }

    static void uvToDirection(int face, float u, float v, float[] outXyz)
    {
        final float s = u * 2.0f - 1.0f;
        final float t = v * 2.0f - 1.0f;
        float dx;
        float dy;
        float dz;
        switch(face)
        {
            case 0: // +X
                dx = 1.0f;
                dy = -t;
                dz = -s;
                break;
            case 1: // -X
                dx = -1.0f;
                dy = -t;
                dz = s;
                break;
            case 2: // +Y
                dx = s;
                dy = 1.0f;
                dz = t;
                break;
            case 3: // -Y
                dx = s;
                dy = -1.0f;
                dz = -t;
                break;
            case 4: // +Z forward
                dx = s;
                dy = -t;
                dz = 1.0f;
                break;
            case 5: // -Z back
                dx = -s;
                dy = -t;
                dz = -1.0f;
                break;
            default:
                dx = 0.0f;
                dy = 0.0f;
                dz = 1.0f;
                break;
        }
        final float len = (float)Math.sqrt(dx * dx + dy * dy + dz * dz);
        if(len > 1e-6f)
        {
            dx /= len;
            dy /= len;
            dz /= len;
        }
        outXyz[0] = dx;
        outXyz[1] = dy;
        outXyz[2] = dz;
    }
}
