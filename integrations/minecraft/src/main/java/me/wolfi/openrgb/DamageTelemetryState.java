package me.wolfi.openrgb;

/**
 * Damage direction for OpenRGB LEDs. Populated by a read-only mixin on
 * {@code ClientPacketListener.handleDamageEvent} — never cancels or changes vanilla damage handling.
 */
public final class DamageTelemetryState
{
    public static volatile float lastDirX = 0.0f;
    public static volatile float lastDirY = 0.0f;
    public static volatile float lastDirZ = 1.0f;
    public static volatile long lastPacketMs = 0L;

    private DamageTelemetryState()
    {
    }

    public static void setIncomingDirection(double x, double y, double z)
    {
        double len = Math.sqrt(x * x + y * y + z * z);
        if(len < 1e-6)
        {
            lastDirX = 0.0f;
            lastDirY = 0.0f;
            lastDirZ = 1.0f;
        }
        else
        {
            lastDirX = (float)(x / len);
            lastDirY = (float)(y / len);
            lastDirZ = (float)(z / len);
        }
        lastPacketMs = System.currentTimeMillis();
    }
}
