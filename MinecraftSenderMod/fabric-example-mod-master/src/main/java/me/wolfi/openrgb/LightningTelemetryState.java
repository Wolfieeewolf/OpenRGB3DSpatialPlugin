package me.wolfi.openrgb;

public final class LightningTelemetryState
{
    public static volatile float lastStrength = 1.0f;
    public static volatile long lastPacketMs = 0L;
    public static volatile float lastDirX = 0.0f;
    public static volatile float lastDirY = 1.0f;
    public static volatile float lastDirZ = 0.0f;
    public static volatile float lastDirFocus = 0.0f;

    private LightningTelemetryState()
    {
    }

    public static void markLightning(float strength)
    {
        lastStrength = Math.max(0.0f, Math.min(2.0f, strength));
        lastPacketMs = System.currentTimeMillis();
    }

    public static void markLightning(float strength, float dirX, float dirY, float dirZ, float dirFocus)
    {
        markLightning(strength);
        lastDirX = dirX;
        lastDirY = dirY;
        lastDirZ = dirZ;
        lastDirFocus = Math.max(0.0f, Math.min(1.0f, dirFocus));
    }
}
