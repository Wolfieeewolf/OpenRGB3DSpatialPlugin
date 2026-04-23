package me.wolfi.openrgb.mixin;

import me.wolfi.openrgb.DamageTelemetryState;
import me.wolfi.openrgb.LightningTelemetryState;
import net.minecraft.client.MinecraftClient;
import net.minecraft.client.network.ClientPlayNetworkHandler;
import net.minecraft.entity.Entity;
import net.minecraft.entity.EntityType;
import net.minecraft.network.packet.s2c.play.EntityDamageS2CPacket;
import net.minecraft.network.packet.s2c.play.EntitySpawnS2CPacket;
import net.minecraft.util.math.Vec3d;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;

import java.util.Optional;

@Mixin(ClientPlayNetworkHandler.class)
public class ClientPlayNetworkHandlerMixin
{
    @Inject(method = "onEntityDamage", at = @At("HEAD"))
    private void openrgb$captureDamageDirection(EntityDamageS2CPacket packet, CallbackInfo ci)
    {
        MinecraftClient client = MinecraftClient.getInstance();
        if(client.player == null || client.world == null)
        {
            return;
        }
        if(packet.entityId() != client.player.getId())
        {
            return;
        }

        Vec3d eye = client.player.getEyePos();
        Vec3d fromSource = null;

        Optional<Vec3d> optPos = packet.sourcePosition();
        if(optPos.isPresent())
        {
            fromSource = eye.subtract(optPos.get());
        }
        else
        {
            Entity direct = client.world.getEntityById(packet.sourceDirectId());
            Entity cause = client.world.getEntityById(packet.sourceCauseId());
            Vec3d src = null;
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
            Vec3d fallback = client.player.getRotationVec(1.0f);
            DamageTelemetryState.setIncomingDirection(fallback.x, fallback.y, fallback.z);
            return;
        }

        DamageTelemetryState.setIncomingDirection(fromSource.x, fromSource.y, fromSource.z);
    }

    @Inject(method = "onEntitySpawn", at = @At("HEAD"))
    private void openrgb$captureLightningSpawn(EntitySpawnS2CPacket packet, CallbackInfo ci)
    {
        if(packet.getEntityType() == EntityType.LIGHTNING_BOLT)
        {
            MinecraftClient client = MinecraftClient.getInstance();
            if(client.player != null)
            {
                Vec3d eye = client.player.getEyePos();
                Vec3d strike = new Vec3d(packet.getX(), packet.getY(), packet.getZ());
                Vec3d toStrike = strike.subtract(eye);
                double len = toStrike.length();
                if(len > 1e-5)
                {
                    Vec3d dir = toStrike.multiply(1.0 / len);
                    float focus = (float)Math.max(0.0, Math.min(1.0, 1.0 - (len / 96.0)));
                    LightningTelemetryState.markLightning(1.0f, (float)dir.x, (float)dir.y, (float)dir.z, focus);
                    return;
                }
            }
            LightningTelemetryState.markLightning(1.0f);
        }
    }
}
