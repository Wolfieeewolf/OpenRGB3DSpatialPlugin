package me.wolfi.openrgb;

import com.google.gson.Gson;
import com.google.gson.GsonBuilder;
import net.fabricmc.loader.api.FabricLoader;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.IOException;
import java.io.Reader;
import java.io.Writer;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;

public final class OpenRGBSenderConfig
{
    private static final Logger LOGGER = LoggerFactory.getLogger("openrgb-sender");
    private static final Gson GSON = new GsonBuilder().setPrettyPrinting().create();
    private static OpenRGBSenderConfig instance;

    public boolean enabled = true;
    public String host = "127.0.0.1";
    public int port = 9876;
    public float blocksPerMeter = 4.0f;
    /** Per-LED room sample grid — 1:1 block colours for each physical LED position. */
    public boolean sendRoomSampleFrames = true;
    /** Client ticks between telemetry batches (1 = 20 Hz at 20 tps; room samples always run every tick). */
    public int telemetryTickDivisor = 1;
    /** Telemetry batches between room_sample frames (auto-increases for large grids). */
    public int roomSampleSendInterval = 1;

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
                sendRoomSampleFrames = loaded.sendRoomSampleFrames;
                telemetryTickDivisor = loaded.telemetryTickDivisor > 0 ? loaded.telemetryTickDivisor : 1;
                if(loaded.roomSampleSendInterval > 0)
                {
                    roomSampleSendInterval = loaded.roomSampleSendInterval;
                }
            }
        }
        catch(IOException | RuntimeException e)
        {
            LOGGER.warn("Failed to load OpenRGB sender config from {}: {}", path, e.toString());
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
        catch(IOException e)
        {
            LOGGER.warn("Failed to save OpenRGB sender config to {}: {}", path, e.toString());
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
        blocksPerMeter = Math.max(0.25f, Math.min(16.0f, blocksPerMeter));
        telemetryTickDivisor = Math.max(1, Math.min(20, telemetryTickDivisor));
        roomSampleSendInterval = Math.max(1, Math.min(32, roomSampleSendInterval));
    }

    private static Path configPath()
    {
        return FabricLoader.getInstance().getConfigDir().resolve("openrgb-minecraft-sender.json");
    }
}
