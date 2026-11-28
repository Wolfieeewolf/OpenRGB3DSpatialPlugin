// SPDX-License-Identifier: GPL-2.0-or-later

package me.wolfi.openrgb;

import com.mojang.blaze3d.pipeline.RenderTarget;
import com.mojang.blaze3d.systems.RenderSystem;
import com.mojang.renderpearl.api.GpuFormat;
import com.mojang.renderpearl.api.buffers.GpuBuffer;
import com.mojang.renderpearl.api.buffers.GpuBufferSlice;
import com.mojang.renderpearl.api.commands.CommandEncoder;
import com.mojang.renderpearl.api.commands.RenderPass;
import com.mojang.renderpearl.api.device.GpuDevice;
import com.mojang.renderpearl.api.textures.FilterMode;
import com.mojang.renderpearl.api.textures.GpuTexture;
import com.mojang.renderpearl.api.textures.GpuTextureView;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.List;
import java.util.Optional;
import net.minecraft.client.Minecraft;
import net.minecraft.client.renderer.GameRenderer;
import net.minecraft.client.renderer.RenderPipelines;
import net.minecraft.client.renderer.state.level.CameraRenderState;
import org.joml.Matrix4f;
import org.joml.Vector4f;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/**
 * Room Ambilight "front from the game picture": once the world is drawn (before the hand, screen
 * effects and HUD), the frame is shrunk on the GPU by halving passes (each an exact 2x2 average)
 * to about 192 px wide, copied to a buffer, and read a frame later when the GPU has finished, so
 * the game never waits. Kept with the camera it was drawn with, so an LED's direction can be
 * projected onto it. Works on both of Minecraft's back ends (OpenGL and Vulkan).
 */
final class ScreenFrameCapture
{
    private static final Logger LOGGER = LoggerFactory.getLogger("openrgb-sender");
    /** Picture width before read-back, from the plugin's "Picture resolution". */
    static volatile int targetWidth = 320;
    private static int builtFor;
    private static final long MIN_INTERVAL_NS = 1_000_000_000L / 240; /* at most 240 captures a second */

    /** One read-back picture and the camera it was drawn with. Rows: 0 = bottom of the screen. */
    static final class Frame
    {
        int width;
        int height;
        byte[] rgba = new byte[0];
        final Matrix4f viewProj = new Matrix4f();
        double camX;
        double camY;
        double camZ;
        long timeNs;
    }

    /** Set from the plugin's config each tick: capture only when someone uses it. */
    static volatile boolean enabled;

    private static final List<GpuTexture> chain = new ArrayList<>();
    private static final List<GpuTextureView> chainViews = new ArrayList<>();
    private static GpuBuffer readBuffer;
    private static int srcWidth;
    private static int srcHeight;
    private static int outWidth;
    private static int outHeight;
    private static boolean pending;
    private static volatile boolean copyDone;
    private static long lastCaptureNs;
    private static boolean failed;
    private static final Matrix4f pendingViewProj = new Matrix4f();
    private static double pendingX;
    private static double pendingY;
    private static double pendingZ;
    private static Frame latest;
    private static Frame spare = new Frame();
    private static Runnable onNewFrame;

    private ScreenFrameCapture()
    {
    }

    static void setOnNewFrame(Runnable r)
    {
        onNewFrame = r;
    }

    /** The newest picture if it is recent (else null: use rays). */
    static Frame latest(long maxAgeNs)
    {
        final Frame f = latest;
        return f != null && System.nanoTime() - f.timeNs <= maxAgeNs ? f : null;
    }

    /** Called from GameRenderer.renderLevel once the world is drawn (render thread). */
    static void onWorldRendered(GameRenderer renderer)
    {
        if(!enabled || failed)
        {
            return;
        }
        try
        {
            collect();
            final long now = System.nanoTime();
            if(!pending && now - lastCaptureNs >= MIN_INTERVAL_NS)
            {
                start(renderer, now);
            }
        }
        catch(Throwable t)
        {
            /* Never break the game's rendering: switch the capture off and fall back to rays. */
            failed = true;
            LOGGER.warn("Room Ambilight screen capture disabled: {}", t.toString());
            release();
        }
    }

    /** Read the previous capture once the GPU has finished copying it. */
    private static void collect()
    {
        if(!pending || !copyDone || readBuffer == null)
        {
            return;
        }
        pending = false;
        final Frame f = spare;
        final int bytes = outWidth * outHeight * 4;
        if(f.rgba.length != bytes)
        {
            f.rgba = new byte[bytes];
        }
        try(GpuBufferSlice.MappedView view = readBuffer.map(true, false))
        {
            final ByteBuffer data = view.data();
            data.get(0, f.rgba, 0, bytes);
        }
        f.width = outWidth;
        f.height = outHeight;
        f.viewProj.set(pendingViewProj);
        f.camX = pendingX;
        f.camY = pendingY;
        f.camZ = pendingZ;
        f.timeNs = System.nanoTime();
        spare = latest != null ? latest : new Frame();
        latest = f;
        final Runnable r = onNewFrame;
        if(r != null)
        {
            r.run();
        }
    }

    private static void start(GameRenderer renderer, long now)
    {
        final RenderTarget target = renderer.mainRenderTarget();
        if(target == null || target.getColorTextureView() == null || target.width <= 0 || target.height <= 0)
        {
            return;
        }
        if(target.width != srcWidth || target.height != srcHeight || chain.isEmpty() || builtFor != targetWidth)
        {
            rebuild(target.width, target.height);
        }
        final GpuDevice device = RenderSystem.getDevice();
        final CommandEncoder encoder = device.createCommandEncoder();
        GpuTextureView source = target.getColorTextureView();
        for(GpuTextureView dst : chainViews)
        {
            try(RenderPass pass = encoder.createRenderPass(() -> "OpenRGB ambilight shrink", dst, Optional.empty()))
            {
                RenderSystem.bindDefaultUniforms(pass);
                pass.setPipeline(RenderSystem.getCompiledPipeline(RenderPipelines.TRACY_BLIT));
                pass.setUniform("InSampler", source, RenderSystem.getSamplerCache().getClampToEdge(FilterMode.LINEAR));
                pass.draw(3, 1, 0, 0);
            }
            source = dst;
        }

        /* Camera this frame was drawn with (camera-relative: view rotation only, then position). */
        final CameraRenderState cam = renderer.gameRenderState().levelRenderState.cameraRenderState;
        pendingViewProj.set(cam.projectionMatrix).mul(cam.viewRotationMatrix);
        pendingX = cam.pos.x;
        pendingY = cam.pos.y;
        pendingZ = cam.pos.z;

        copyDone = false;
        pending = true;
        lastCaptureNs = now;
        encoder.copyTextureToBuffer(chain.get(chain.size() - 1), readBuffer, 0L, () -> copyDone = true, 0);
    }

    /** Halving passes down to about twice the output, then one pass to the output size. */
    private static void rebuild(int width, int height)
    {
        release();
        srcWidth = width;
        srcHeight = height;
        builtFor = targetWidth;
        outWidth = Math.min(Math.max(64, builtFor), width);
        outHeight = Math.max(1, Math.round(outWidth * (float)height / (float)width));
        final GpuDevice device = RenderSystem.getDevice();
        int w = width;
        int h = height;
        while(w / 2 >= outWidth * 2)
        {
            w /= 2;
            h = Math.max(1, h / 2);
            addStage(device, w, h, false);
        }
        addStage(device, outWidth, outHeight, true);
        readBuffer = device.createBuffer(() -> "OpenRGB ambilight read-back",
                GpuBuffer.USAGE_MAP_READ | GpuBuffer.USAGE_COPY_DST, (long)outWidth * outHeight * 4L);
        LOGGER.info("Room Ambilight screen capture: {}x{} -> {}x{} ({} passes)", width, height, outWidth, outHeight,
                chain.size());
    }

    private static void addStage(GpuDevice device, int w, int h, boolean last)
    {
        int usage = GpuTexture.USAGE_RENDER_ATTACHMENT | GpuTexture.USAGE_TEXTURE_BINDING;
        if(last)
        {
            usage |= GpuTexture.USAGE_COPY_SRC;
        }
        final GpuTexture tex = device.createTexture("OpenRGB ambilight " + w + "x" + h, usage, GpuFormat.RGBA8_UNORM, w, h, 1, 1);
        chain.add(tex);
        chainViews.add(device.createTextureView(tex));
    }

    private static void release()
    {
        for(GpuTextureView v : chainViews)
        {
            quiet(v::close);
        }
        for(GpuTexture t : chain)
        {
            quiet(t::close);
        }
        chainViews.clear();
        chain.clear();
        if(readBuffer != null)
        {
            final GpuBuffer b = readBuffer;
            quiet(b::close);
            readBuffer = null;
        }
        pending = false;
        srcWidth = 0;
        srcHeight = 0;
    }

    /**
     * How much of a world point is on screen (0 = off screen, 1 = well inside), and its picture
     * colour into rgbOut (0..255). edge: fraction of the screen from each edge that fades out.
     */
    static float sample(Frame f, double wx, double wy, double wz, float edge, Vector4f scratch, int[] rgbOut)
    {
        scratch.set((float)(wx - f.camX), (float)(wy - f.camY), (float)(wz - f.camZ), 1.0f);
        f.viewProj.transform(scratch);
        if(scratch.w <= 1e-4f)
        {
            return 0.0f; /* behind the camera */
        }
        final float u = (scratch.x / scratch.w) * 0.5f + 0.5f;
        final float v = (scratch.y / scratch.w) * 0.5f + 0.5f;
        if(u < 0.0f || u > 1.0f || v < 0.0f || v > 1.0f)
        {
            return 0.0f;
        }
        final float e = Math.min(Math.min(u, 1.0f - u), Math.min(v, 1.0f - v));
        float weight = edge <= 1e-4f ? 1.0f : Math.min(1.0f, e / edge);
        weight = weight * weight * (3.0f - 2.0f * weight); /* smooth */

        /* Bilinear from the small picture (row 0 = bottom, as v). */
        final float px = u * f.width - 0.5f;
        final float py = v * f.height - 0.5f;
        final int x0 = Math.max(0, Math.min(f.width - 1, (int)Math.floor(px)));
        final int y0 = Math.max(0, Math.min(f.height - 1, (int)Math.floor(py)));
        final int x1 = Math.min(f.width - 1, x0 + 1);
        final int y1 = Math.min(f.height - 1, y0 + 1);
        final float fx = Math.max(0.0f, Math.min(1.0f, px - x0));
        final float fy = Math.max(0.0f, Math.min(1.0f, py - y0));
        for(int c = 0; c < 3; c++)
        {
            final float a = texel(f, x0, y0, c) * (1 - fx) + texel(f, x1, y0, c) * fx;
            final float b = texel(f, x0, y1, c) * (1 - fx) + texel(f, x1, y1, c) * fx;
            rgbOut[c] = Math.round(a * (1 - fy) + b * fy);
        }

        return weight;
    }

    private static int texel(Frame f, int x, int y, int c)
    {
        return f.rgba[(y * f.width + x) * 4 + c] & 0xFF;
    }

    private static void quiet(Runnable r)
    {
        try
        {
            r.run();
        }
        catch(Throwable ignored)
        {
        }
    }

    static boolean firstPerson()
    {
        final Minecraft mc = Minecraft.getInstance();
        return mc != null && mc.options != null && mc.options.getCameraType().isFirstPerson();
    }
}
