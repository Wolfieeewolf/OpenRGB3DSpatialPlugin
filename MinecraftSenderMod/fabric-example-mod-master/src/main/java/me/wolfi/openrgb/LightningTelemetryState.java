package me.wolfi.openrgb;

public final class LightningTelemetryState
{
    public static volatile float lastStrength = 1.0f;
    public static volatile long lastPacketMs = 0L;

    private LightningTelemetryState()
    {
    }

    public static void markLightning(float strength)
    {
        lastStrength = Math.max(0.0f, Math.min(2.0f, strength));
        lastPacketMs = System.currentTimeMillis();
    }
}
