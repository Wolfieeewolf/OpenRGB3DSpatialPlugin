package me.wolfi.openrgb;

import com.mojang.blaze3d.platform.NativeImage;
import com.mojang.blaze3d.pipeline.RenderTarget;
import me.wolfi.openrgb.mixin.CameraAccessor;
import net.fabricmc.fabric.api.client.rendering.v1.level.LevelRenderEvents;
import net.minecraft.client.Camera;
import net.minecraft.client.Minecraft;
import net.minecraft.client.Screenshot;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/**
 * Captures GPU cubemap faces by briefly aiming the render camera at each world axis
 * for one normal render frame, then readback at END_MAIN. Never calls
 * {@code renderLevel} inside a render callback (that triggers PreparedFrame already in use).
 */
final class GpuFramebufferCapturer
{
    private static final Logger LOGGER = LoggerFactory.getLogger("openrgb-sender");
    private static final int TICKS_BETWEEN_CAPTURES = 20;
    private static final int FACE_SIZE = GpuCubemapCapture.FACE_SIZE;

    private static int tickCounter = 0;
    private static int nextFaceIndex = 0;
    private static boolean loggedPipeline = false;

    /** Scheduled on client tick; applied at START_MAIN, read back at END_MAIN. */
    private static int pendingFace = -1;
    private static float restoreYaw = 0.0f;
    private static float restorePitch = 0.0f;
    private static volatile boolean readbackPending = false;

    private static final byte[] scratchFace = new byte[FACE_SIZE * FACE_SIZE * 4];
    private static final float[] yawPitchScratch = new float[2];

    private GpuFramebufferCapturer()
    {
    }

    static void register()
    {
        LevelRenderEvents.START_MAIN.register(context -> onStartMain());
        LevelRenderEvents.END_MAIN.register(context -> onEndMain());
    }

    /** Called from the client tick thread to schedule the next face capture. */
    static void onClientTick(Minecraft client)
    {
        final OpenRGBSenderConfig cfg = OpenRGBSenderConfig.get();
        if(!cfg.enabled || !cfg.experimentalGpuReadback)
        {
            return;
        }
        if(client == null || client.player == null || client.level == null || client.isPaused())
        {
            return;
        }
        if(pendingFace >= 0 || readbackPending)
        {
            return;
        }
        if(++tickCounter % TICKS_BETWEEN_CAPTURES != 0)
        {
            return;
        }

        pendingFace = nextFaceIndex;
        nextFaceIndex = (nextFaceIndex + 1) % GpuCubemapCapture.FACE_COUNT;
    }

    private static boolean shouldCapture(Minecraft client)
    {
        final OpenRGBSenderConfig cfg = OpenRGBSenderConfig.get();
        return cfg.enabled
                && cfg.experimentalGpuReadback
                && client != null
                && client.player != null
                && client.level != null
                && !client.isPaused()
                && pendingFace >= 0;
    }

    private static void onStartMain()
    {
        final Minecraft client = Minecraft.getInstance();
        if(!shouldCapture(client))
        {
            return;
        }

        final Camera camera = client.gameRenderer.mainCamera();
        restoreYaw = camera.yaw();
        restorePitch = camera.xRot();
        GpuCubemapCapture.faceToYawPitch(pendingFace, yawPitchScratch);
        ((CameraAccessor)camera).openrgb$setRotation(yawPitchScratch[0], yawPitchScratch[1]);
    }

    private static void onEndMain()
    {
        final Minecraft client = Minecraft.getInstance();
        if(!shouldCapture(client) || readbackPending)
        {
            return;
        }

        readbackPending = true;
        final int faceIndex = pendingFace;
        final Camera camera = client.gameRenderer.mainCamera();
        final RenderTarget target = client.gameRenderer.mainRenderTarget();

        Screenshot.takeScreenshot(target, image ->
        {
            try
            {
                extractFaceFromScreenshot(image, scratchFace);
                GpuCubemapCapture.writeFace(faceIndex, scratchFace);

                if(!loggedPipeline)
                {
                    loggedPipeline = true;
                    LOGGER.info("GPU cubemap face {} captured ({}x{}, {} valid faces)",
                            faceIndex, FACE_SIZE, FACE_SIZE, GpuCubemapCapture.countValidFaces());
                }
                else if(faceIndex == GpuCubemapCapture.FACE_COUNT - 1)
                {
                    LOGGER.info("GPU cubemap sweep complete ({} valid faces)",
                            GpuCubemapCapture.countValidFaces());
                }
            }
            catch(Exception ex)
            {
                LOGGER.debug("GPU cubemap face capture failed", ex);
            }
            finally
            {
                image.close();
                readbackPending = false;
            }
        });

        // Restore player view immediately; readback already queued from current framebuffer.
        ((CameraAccessor)camera).openrgb$setRotation(restoreYaw, restorePitch);
        pendingFace = -1;
    }

    private static void extractFaceFromScreenshot(NativeImage image, byte[] out)
    {
        final int w = image.getWidth();
        final int h = image.getHeight();
        if(w <= 0 || h <= 0)
        {
            return;
        }

        final int cx = w / 2;
        final int cy = h / 2;
        final int patchScreen = Math.max(FACE_SIZE, Math.min(w, h) / 3);

        for(int fv = 0; fv < FACE_SIZE; fv++)
        {
            for(int fu = 0; fu < FACE_SIZE; fu++)
            {
                final int sx = cx - patchScreen / 2 + fu * patchScreen / FACE_SIZE;
                final int sy = cy - patchScreen / 2 + fv * patchScreen / FACE_SIZE;
                final int px = Math.max(0, Math.min(w - 1, sx));
                final int py = Math.max(0, Math.min(h - 1, sy));
                final int argb = image.getPixel(px, py);
                final int off = (fv * FACE_SIZE + fu) * 4;
                out[off    ] = (byte)((argb >> 16) & 0xFF);
                out[off + 1] = (byte)((argb >> 8) & 0xFF);
                out[off + 2] = (byte)(argb & 0xFF);
                out[off + 3] = (byte)0xFF;
            }
        }
    }
}
