package me.wolfi.openrgb.mixin;

import me.wolfi.openrgb.OpenRGBSenderMod;
import net.minecraft.client.Minecraft;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;

@Mixin(Minecraft.class)
public class MinecraftClientMixin
{
    @Inject(method = "tick", at = @At("TAIL"))
    private void openrgb$onTick(CallbackInfo ci)
    {
        OpenRGBSenderMod.onClientTick((Minecraft)(Object)this);
    }
}
