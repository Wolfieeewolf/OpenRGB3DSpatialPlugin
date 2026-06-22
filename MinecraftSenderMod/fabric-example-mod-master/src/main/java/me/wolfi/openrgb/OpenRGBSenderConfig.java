package me.wolfi.openrgb;

import com.google.gson.Gson;
import com.google.gson.GsonBuilder;
import net.fabricmc.loader.api.FabricLoader;

import java.io.IOException;
import java.io.Reader;
import java.io.Writer;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;

public final class OpenRGBSenderConfig
{
    private static final Gson GSON = new GsonBuilder().setPrettyPrinting().create();
    private static OpenRGBSenderConfig instance;

    public boolean enabled = true;
    public String host = "127.0.0.1";
    public int port = 9876;
    public float blocksPerMeter = 1.0f;
    public boolean sendVoxelFrames = true;
    /** Client ticks between telemetry batches (2 ≈ 10 Hz at 20 tps). */
    public int telemetryTickDivisor = 2;
    /** Telemetry batches between voxel_frame packets (1 = every batch, ~10 Hz with divisor 2). */
    public int voxelSendInterval = 1;

    public static OpenRGBSenderConfig get()
    {
        if(instance == null)
        {
            instance = new OpenRGBSenderConfig();
            instance.load();
        }
        return instance;
    }

    public void load()
    {
        Path path = configPath();
        if(!Files.isRegularFile(path))
        {
            clamp();
            return;
        }
        try(Reader reader = Files.newBufferedReader(path, StandardCharsets.UTF_8))
        {
            OpenRGBSenderConfig loaded = GSON.fromJson(reader, OpenRGBSenderConfig.class);
            if(loaded != null)
            {
                enabled = loaded.enabled;
                host = loaded.host != null ? loaded.host : host;
                port = loaded.port;
                blocksPerMeter = loaded.blocksPerMeter;
                sendVoxelFrames = loaded.sendVoxelFrames;
                telemetryTickDivisor = loaded.telemetryTickDivisor;
                voxelSendInterval = loaded.voxelSendInterval;
            }
        }
        catch(IOException | RuntimeException ignored)
        {
        }
        clamp();
    }

    public void save()
    {
        clamp();
        Path path = configPath();
        try
        {
            Files.createDirectories(path.getParent());
            try(Writer writer = Files.newBufferedWriter(path, StandardCharsets.UTF_8))
            {
                GSON.toJson(this, writer);
            }
        }
        catch(IOException ignored)
        {
        }
        OpenRGBSenderMod.invalidateUdpTarget();
    }

    public void clamp()
    {
        if(host == null || host.isBlank())
        {
            host = "127.0.0.1";
        }
        host = host.trim();
        port = Math.max(1, Math.min(65535, port));
        blocksPerMeter = Math.max(0.25f, Math.min(4.0f, blocksPerMeter));
        telemetryTickDivisor = Math.max(1, Math.min(20, telemetryTickDivisor));
        voxelSendInterval = Math.max(1, Math.min(32, voxelSendInterval));
    }

    private static Path configPath()
    {
        return FabricLoader.getInstance().getConfigDir().resolve("openrgb-minecraft-sender.json");
    }
}
