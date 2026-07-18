package me.wolfi.openrgb;

import net.minecraft.client.model.geom.builders.UVPair;

/**
 * Unpack Mojang packed UV longs used by {@link net.minecraft.client.resources.model.geometry.BakedQuad}.
 */
final class PackedUv
{
    private PackedUv()
    {
    }

    static float unpackU(long packed)
    {
        return UVPair.unpackU(packed);
    }

    static float unpackV(long packed)
    {
        return UVPair.unpackV(packed);
    }
}
