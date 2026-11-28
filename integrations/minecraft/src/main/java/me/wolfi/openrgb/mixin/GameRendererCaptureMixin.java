// SPDX-License-Identifier: GPL-2.0-or-later

package me.wolfi.openrgb.mixin;

import me.wolfi.openrgb.ScreenFrameHook;
import net.minecraft.client.renderer.GameRenderer;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;

/** Room Ambilight picture capture: right after the world is drawn, before the hand, screen
    effects and HUD (so none of those end up on the LEDs). */
@Mixin(GameRenderer.class)
public abstract class GameRendererCaptureMixin
{
    @Inject(
            method = "renderLevel",
            at = @At(value = "INVOKE",
                     target = "Lnet/minecraft/client/renderer/GameRenderer;render3dHud(Lnet/minecraft/client/renderer/state/level/CameraRenderState;Lnet/minecraft/client/renderer/state/level/PlayerRenderState;Lnet/minecraft/client/renderer/state/OptionsRenderState;Z)V"),
            require = 0)
    private void openrgb$captureWorld(CallbackInfo ci)
    {
        ScreenFrameHook.onWorldRendered((GameRenderer)(Object)this);
    }
}
