package me.wolfi.openrgb.mixin;

import me.wolfi.openrgb.DamageTelemetryState;
import net.minecraft.client.Minecraft;
import net.minecraft.client.multiplayer.ClientPacketListener;
import net.minecraft.world.entity.Entity;
import net.minecraft.network.protocol.game.ClientboundDamageEventPacket;
import net.minecraft.world.phys.Vec3;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;

import java.util.Optional;

@Mixin(ClientPacketListener.class)
public class ClientPlayNetworkHandlerMixin
{
    @Inject(method = "handleDamageEvent", at = @At("HEAD"))
    private void openrgb$captureDamageDirection(ClientboundDamageEventPacket packet, CallbackInfo ci)
    {
        Minecraft client = Minecraft.getInstance();
        if(client.player == null || client.level == null)
        {
            return;
        }
        if(packet.entityId() != client.player.getId())
        {
            return;
        }

        Vec3 eye = client.player.getEyePosition();
        Vec3 fromSource = null;

        Optional<Vec3> optPos = packet.sourcePosition();
        if(optPos.isPresent())
        {
            fromSource = eye.subtract(optPos.get());
        }
        else
        {
            Entity direct = client.level.getEntity(packet.sourceDirectId());
            Entity cause = client.level.getEntity(packet.sourceCauseId());
            Vec3 src = null;
            if(direct != null)
            {
                src = direct.getBoundingBox().getCenter();
            }
            else if(cause != null)
            {
                src = cause.getBoundingBox().getCenter();
            }
            if(src != null)
            {
                fromSource = eye.subtract(src);
            }
        }

        if(fromSource == null)
        {
            Vec3 fallback = client.player.getViewVector(1.0f);
            DamageTelemetryState.setIncomingDirection(fallback.x, fallback.y, fallback.z);
            return;
        }

        DamageTelemetryState.setIncomingDirection(fromSource.x, fromSource.y, fromSource.z);
    }
}
