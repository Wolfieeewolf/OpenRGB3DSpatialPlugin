package me.wolfi.openrgb;

/**
 * Populated from {@link net.minecraft.client.network.ClientPlayNetworkHandler#onEntityDamage}
 * via mixin — uses the same {@link net.minecraft.network.packet.s2c.play.EntityDamageS2CPacket}
 * data the client applies for damage.
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
