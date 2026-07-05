package me.wolfi.openrgb.mixin;

import me.wolfi.openrgb.DamageTelemetryState;
import me.wolfi.openrgb.LightningTelemetryState;
import net.minecraft.client.Minecraft;
import net.minecraft.client.multiplayer.ClientPacketListener;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EntityTypes;
import net.minecraft.network.protocol.game.ClientboundDamageEventPacket;
import net.minecraft.network.protocol.game.ClientboundAddEntityPacket;
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

    @Inject(method = "handleAddEntity", at = @At("HEAD"))
    private void openrgb$captureLightningSpawn(ClientboundAddEntityPacket packet, CallbackInfo ci)
    {
        if(packet.getType() == EntityTypes.LIGHTNING_BOLT)
        {
            Minecraft client = Minecraft.getInstance();
            if(client.player != null)
            {
                Vec3 eye = client.player.getEyePosition();
                Vec3 strike = new Vec3(packet.getX(), packet.getY(), packet.getZ());
                Vec3 toStrike = strike.subtract(eye);
                double len = toStrike.length();
                if(len > 1e-5)
                {
                    Vec3 dir = toStrike.scale(1.0 / len);
                    float focus = (float)Math.max(0.0, Math.min(1.0, 1.0 - (len / 96.0)));
                    LightningTelemetryState.markLightning(1.0f, (float)dir.x, (float)dir.y, (float)dir.z, focus);
                    return;
                }
            }
            LightningTelemetryState.markLightning(1.0f);
        }
    }
}
