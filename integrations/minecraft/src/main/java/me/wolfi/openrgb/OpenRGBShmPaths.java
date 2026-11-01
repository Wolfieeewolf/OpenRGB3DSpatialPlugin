package me.wolfi.openrgb;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;

/** Shared-memory directory for OpenRGB3DSpatial game telemetry (Windows). */
final class OpenRGBShmPaths
{
    private OpenRGBShmPaths()
    {
    }

    static Path baseDir() throws IOException
    {
        final String programData = System.getenv("ProgramData");
        Path dir;
        if(programData != null && !programData.isBlank())
        {
            dir = Paths.get(programData, "OpenRGB3DSpatial");
        }
        else
        {
            final String local = System.getenv("LOCALAPPDATA");
            if(local == null || local.isBlank())
            {
                throw new IOException("ProgramData/LOCALAPPDATA not set");
            }
            dir = Paths.get(local, "OpenRGB3DSpatial");
        }
        Files.createDirectories(dir);
        return dir;
    }

    static Path resolveFile(String name) throws IOException
    {
        return baseDir().resolve(name);
    }
}
