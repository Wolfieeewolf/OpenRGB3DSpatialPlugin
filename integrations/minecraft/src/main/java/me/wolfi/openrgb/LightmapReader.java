// SPDX-License-Identifier: GPL-2.0-or-later

package me.wolfi.openrgb;

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
import java.util.Optional;
import net.minecraft.client.renderer.GameRenderer;
import net.minecraft.client.renderer.RenderPipelines;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/**
 * The game's own lightmap: 16 x 16 final light colours, one per (block light, sky light) pair,
 * with the player's Brightness setting, night vision, dimension ambient, torch flicker and time
 * of day already in it. Ray-sampled LEDs use it so they are lit exactly as the game lights the
 * world. Copied (drawn into our own texture, then read back a frame later) every other frame.
 */
final class LightmapReader
{
    private static final Logger LOGGER = LoggerFactory.getLogger("openrgb-sender");

    private static GpuTexture texture;
    private static GpuTextureView view;
    private static GpuBuffer buffer;
    private static boolean pending;
    private static volatile boolean copyDone;
    private static boolean failed;
    private static int frameCounter;
    /** Packed 0xRRGGBB, index [sky * 16 + block]. Null until the first read. */
    private static volatile int[] colours;

    private LightmapReader()
    {
    }

    /** Final light colour for these light levels (0xRRGGBB), or -1 if not read yet. */
    static int colour(int block, int sky)
    {
        final int[] c = colours;
        if(c == null)
        {
            return -1;
        }
        final int b = Math.max(0, Math.min(15, block));
        final int s = Math.max(0, Math.min(15, sky));
        return c[s * 16 + b];
    }

    /** Render thread, once the world is drawn. */
    static void update(GameRenderer renderer)
    {
        if(failed)
        {
            return;
        }
        try
        {
            collect();
            if(!pending && (++frameCounter & 1) == 0)
            {
                start(renderer);
            }
        }
        catch(Throwable t)
        {
            failed = true;
            LOGGER.warn("Lightmap read disabled (LEDs use a built-in light curve): {}", t.toString());
        }
    }

    private static void start(GameRenderer renderer)
    {
        final GpuTextureView source = renderer.levelLightmap();
        if(source == null)
        {
            return;
        }
        final GpuDevice device = RenderSystem.getDevice();
        if(texture == null)
        {
            texture = device.createTexture("OpenRGB lightmap copy",
                    GpuTexture.USAGE_RENDER_ATTACHMENT | GpuTexture.USAGE_TEXTURE_BINDING | GpuTexture.USAGE_COPY_SRC,
                    GpuFormat.RGBA8_UNORM, 16, 16, 1, 1);
            view = device.createTextureView(texture);
            buffer = device.createBuffer(() -> "OpenRGB lightmap read-back",
                    GpuBuffer.USAGE_MAP_READ | GpuBuffer.USAGE_COPY_DST, 16L * 16L * 4L);
        }
        final CommandEncoder encoder = device.createCommandEncoder();
        /* The game's lightmap texture cannot be copied out directly: draw it 1:1 into ours. */
        try(RenderPass pass = encoder.createRenderPass(() -> "OpenRGB lightmap copy", view, Optional.empty()))
        {
            RenderSystem.bindDefaultUniforms(pass);
            pass.setPipeline(RenderSystem.getCompiledPipeline(RenderPipelines.TRACY_BLIT));
            pass.setUniform("InSampler", source, RenderSystem.getSamplerCache().getClampToEdge(FilterMode.NEAREST));
            pass.draw(3, 1, 0, 0);
        }
        copyDone = false;
        pending = true;
        encoder.copyTextureToBuffer(texture, buffer, 0L, () -> copyDone = true, 0);
    }

    private static void collect()
    {
        if(!pending || !copyDone)
        {
            return;
        }
        pending = false;
        final byte[] raw = new byte[16 * 16 * 4];
        try(GpuBufferSlice.MappedView v = buffer.map(true, false))
        {
            final ByteBuffer data = v.data();
            data.get(0, raw, 0, raw.length);
        }
        /* Rows are sky light. Which end of the read-back is sky 15 depends on the back end:
           sky 15 (no block light) is never darker than sky 0, so use the brighter end. */
        final int lumRow0 = luma(raw, 0, 0);
        final int lumRow15 = luma(raw, 0, 15);
        final boolean flip = lumRow0 > lumRow15;
        final int[] out = new int[256];
        for(int row = 0; row < 16; row++)
        {
            final int sky = flip ? 15 - row : row;
            for(int block = 0; block < 16; block++)
            {
                final int o = (row * 16 + block) * 4;
                out[sky * 16 + block] = ((raw[o] & 0xFF) << 16) | ((raw[o + 1] & 0xFF) << 8) | (raw[o + 2] & 0xFF);
            }
        }
        colours = out;
    }

    private static int luma(byte[] raw, int block, int row)
    {
        final int o = (row * 16 + block) * 4;
        return (raw[o] & 0xFF) + (raw[o + 1] & 0xFF) + (raw[o + 2] & 0xFF);
    }
}
