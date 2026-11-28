// SPDX-License-Identifier: GPL-2.0-or-later

package me.wolfi.openrgb;

import net.minecraft.client.renderer.GameRenderer;

/** Public entry for the render mixin (the capture itself stays package-private). */
public final class ScreenFrameHook
{
    private ScreenFrameHook()
    {
    }

    public static void onWorldRendered(GameRenderer renderer)
    {
        LightmapReader.update(renderer);
        ScreenFrameCapture.onWorldRendered(renderer);
    }
}
