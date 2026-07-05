package me.wolfi.openrgb;

import net.minecraft.world.level.material.MapColor;
import net.minecraft.client.Minecraft;
import net.minecraft.client.player.LocalPlayer;
import net.minecraft.world.phys.BlockHitResult;
import net.minecraft.world.phys.HitResult;
import net.minecraft.core.BlockPos;
import net.minecraft.world.phys.Vec3;
import net.minecraft.world.level.LightLayer;
import net.minecraft.world.level.ClipContext;
import net.minecraft.world.level.Level;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/**
 * Fallback ambient cubemap built from CPU block raycasts when room samples are disabled.
 * When room samples are enabled, {@link OpenRGBSenderMod} builds a probe-based cubemap instead.
 */
public final class GpuPanoramaCapturer
{
    private static final Logger LOGGER = LoggerFactory.getLogger("openrgb-sender");

    private static final int FACE_COUNT  = 6;
    private static final int FACE_SIZE   = 16;
    private static final int MAX_RANGE   = 32;
    private static final int TICKS_PER_UPDATE = 3;

    private static GpuPanoramaCapturer instance;

    private final GpuPanoramaFrameShmWriter writer = new GpuPanoramaFrameShmWriter();
    private final byte[] faceBuffer = new byte[FACE_COUNT * FACE_SIZE * FACE_SIZE * 4];
    private int frameId = 0;
    private boolean loggedActive = false;
    private int tickCountdown = TICKS_PER_UPDATE;

    public static void onClientTick(Minecraft client)
    {
        final OpenRGBSenderConfig cfg = OpenRGBSenderConfig.get();
        if(!cfg.enabled || !cfg.sendGpuPanoramaFrames || !cfg.useSharedMemory || cfg.sendRoomSampleFrames)
        {
            return;
        }
        if(client == null || client.player == null || client.level == null || client.isPaused())
        {
            return;
        }
        if(instance == null)
        {
            instance = new GpuPanoramaCapturer();
        }
        if(instance.tickCountdown-- > 0)
        {
            return;
        }
        instance.tickCountdown = TICKS_PER_UPDATE;
        try
        {
            instance.buildAndPublish(client, client.player, client.level);
        }
        catch(Exception ignored)
        {
        }
    }

    private void buildAndPublish(Minecraft client, LocalPlayer player, Level world)
    {
        final double ex = player.getX();
        final double ey = player.getEyeY();
        final double ez = player.getZ();
        final Vec3 eye = new Vec3(ex, ey, ez);

        final BlockPos eyeBlock = BlockPos.containing(ex, ey, ez);
        final int skyLight   = world.getBrightness(LightLayer.SKY, eyeBlock);
        final int blockLight = world.getBrightness(LightLayer.BLOCK, eyeBlock);
        final int combined   = Math.max(skyLight, blockLight);
        final float lightMul = 0.08f + 0.92f * (combined / 15.0f);

        final int rawSky = AtmosphereSampler.sampleSkyColor(client, 0x8EC0E0);
        final int rawFog = AtmosphereSampler.sampleFogColor(client, rawSky);
        final int skyR = (int)(((rawSky >> 16 & 0xFF) * 0.55f + (rawFog >> 16 & 0xFF) * 0.45f) * lightMul);
        final int skyG = (int)(((rawSky >> 8  & 0xFF) * 0.55f + (rawFog >> 8  & 0xFF) * 0.45f) * lightMul);
        final int skyB = (int)(((rawSky       & 0xFF) * 0.55f + (rawFog       & 0xFF) * 0.45f) * lightMul);

        for(int face = 0; face < FACE_COUNT; face++)
        {
            final int faceOff = face * FACE_SIZE * FACE_SIZE * 4;
            for(int fv = 0; fv < FACE_SIZE; fv++)
            {
                final float v = (fv + 0.5f) / FACE_SIZE * 2.0f - 1.0f;
                for(int fu = 0; fu < FACE_SIZE; fu++)
                {
                    final float u = (fu + 0.5f) / FACE_SIZE * 2.0f - 1.0f;

                    final double wx, wy, wz;
                    switch(face)
                    {
                        case 0: wx =  1; wy = -v; wz = -u; break;
                        case 1: wx = -1; wy = -v; wz =  u; break;
                        case 2: wx =  u; wy =  1; wz =  v; break;
                        case 3: wx =  u; wy = -1; wz = -v; break;
                        case 4: wx =  u; wy = -v; wz =  1; break;
                        default: wx = -u; wy = -v; wz = -1; break;
                    }

                    final double wlen = Math.sqrt(wx*wx + wy*wy + wz*wz);
                    final double ndx = wx / wlen;
                    final double ndy = wy / wlen;
                    final double ndz = wz / wlen;

                    final Vec3 target = new Vec3(ex + ndx * MAX_RANGE,
                                                   ey + ndy * MAX_RANGE,
                                                   ez + ndz * MAX_RANGE);

                    final BlockHitResult hit = world.clip(new ClipContext(
                            eye, target,
                            ClipContext.Block.COLLIDER,
                            ClipContext.Fluid.SOURCE_ONLY,
                            player));

                    int r, g, b;
                    if(hit.getType() == HitResult.Type.BLOCK)
                    {
                        final BlockPos pos = hit.getBlockPos();
                        final MapColor mc = world.getBlockState(pos).getMapColor(world, pos);
                        final int rgb = mc != null ? mc.col : 0x808080;
                        r = (int)(((rgb >> 16) & 0xFF) * lightMul);
                        g = (int)(((rgb >> 8)  & 0xFF) * lightMul);
                        b = (int)((rgb & 0xFF) * lightMul);
                    }
                    else
                    {
                        r = skyR;
                        g = skyG;
                        b = skyB;
                    }

                    final int off = faceOff + (fv * FACE_SIZE + fu) * 4;
                    faceBuffer[off    ] = (byte) Math.min(255, r);
                    faceBuffer[off + 1] = (byte) Math.min(255, g);
                    faceBuffer[off + 2] = (byte) Math.min(255, b);
                    faceBuffer[off + 3] = (byte) 0xFF;
                }
            }
        }

        publish((float)ex, (float)ey, (float)ez);
    }

    private void publish(float ex, float ey, float ez)
    {
        try
        {
            frameId++;
            final long now = System.currentTimeMillis();
            if(writer.writeFrame(frameId, now, ex, ey, ez,
                    FACE_SIZE, FACE_SIZE, FACE_COUNT, faceBuffer))
            {
                if(!loggedActive)
                {
                    loggedActive = true;
                    writer.ensurePath();
                    LOGGER.info("Raycast cubemap fallback active ({}x{} x{} faces, {} block range)",
                            FACE_SIZE, FACE_SIZE, FACE_COUNT, MAX_RANGE);
                }
                OpenRGBSenderMod.notifyGpuPanoramaShmFrame(frameId, now);
            }
        }
        catch(Exception ex2)
        {
            LOGGER.debug("Raycast cubemap SHM publish failed", ex2);
        }
    }
}
