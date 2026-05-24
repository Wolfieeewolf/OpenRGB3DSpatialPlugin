package me.wolfi.openrgb.mixin;

import me.wolfi.openrgb.OpenRGBSenderMod;
import net.minecraft.client.MinecraftClient;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;

/**
 * Runs telemetry on the client thread. The previous background thread called {@link MinecraftClient#getInstance()}
 * off-thread, which is unsafe and produced rare/stale packets and frozen health.
 */
@Mixin(MinecraftClient.class)
public class MinecraftClientMixin
{
    @Inject(method = "tick", at = @At("TAIL"))
    private void openrgb$sendTelemetryAfterTick(CallbackInfo ci)
    {
        OpenRGBSenderMod.onClientTick((MinecraftClient)(Object)this);
    }
}
