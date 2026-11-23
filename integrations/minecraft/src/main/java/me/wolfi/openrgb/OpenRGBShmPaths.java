// SPDX-License-Identifier: GPL-2.0-or-later

package me.wolfi.openrgb;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.nio.file.attribute.PosixFilePermissions;

/**
 * Folder holding the room-sample files shared with the OpenRGB 3D Spatial plugin.
 * Must match engines/game/Game/RoomSampleShmPaths.h in the plugin.
 *
 * Windows: %ProgramData%\OpenRGB3DSpatial (or %LOCALAPPDATA%).
 * Linux and macOS: $XDG_RUNTIME_DIR/OpenRGB3DSpatial, else $TMPDIR/OpenRGB3DSpatial,
 * else /tmp/OpenRGB3DSpatial-<user>.
 */
final class OpenRGBShmPaths
{
    private OpenRGBShmPaths()
    {
    }

    private static boolean isSet(String value)
    {
        return value != null && !value.isBlank();
    }

    static Path baseDir() throws IOException
    {
        final boolean windows = System.getProperty("os.name", "").toLowerCase().startsWith("windows");
        Path dir;
        if(windows)
        {
            final String programData = System.getenv("ProgramData");
            final String local = System.getenv("LOCALAPPDATA");
            if(isSet(programData))
            {
                dir = Paths.get(programData, "OpenRGB3DSpatial");
            }
            else if(isSet(local))
            {
                dir = Paths.get(local, "OpenRGB3DSpatial");
            }
            else
            {
                throw new IOException("ProgramData/LOCALAPPDATA not set");
            }
            Files.createDirectories(dir);
            return dir;
        }

        final String runtime = System.getenv("XDG_RUNTIME_DIR");
        final String tmp = System.getenv("TMPDIR");
        if(isSet(runtime))
        {
            dir = Paths.get(runtime, "OpenRGB3DSpatial");
        }
        else if(isSet(tmp))
        {
            dir = Paths.get(tmp, "OpenRGB3DSpatial");
        }
        else
        {
            dir = Paths.get("/tmp", "OpenRGB3DSpatial-" + System.getProperty("user.name", "user"));
        }
        if(!Files.isDirectory(dir))
        {
            Files.createDirectories(dir, PosixFilePermissions.asFileAttribute(PosixFilePermissions.fromString("rwx------")));
        }
        return dir;
    }

    static Path resolveFile(String name) throws IOException
    {
        return baseDir().resolve(name);
    }
}
