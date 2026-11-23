// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef ROOMSAMPLESHMPATHS_H
#define ROOMSAMPLESHMPATHS_H

/* Folder for the room-sample files shared with the Minecraft mod. Must match the mod's
   OpenRGBShmPaths.java:
     Windows:         %ProgramData%\OpenRGB3DSpatial\ (or %LOCALAPPDATA%)
     Linux and macOS: $XDG_RUNTIME_DIR/OpenRGB3DSpatial/, else $TMPDIR/OpenRGB3DSpatial/,
                      else /tmp/OpenRGB3DSpatial-<user>/ */

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cstdlib>
#include <pwd.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include <string>

namespace RoomSampleShmPaths
{

#ifdef _WIN32

inline std::wstring BaseDirW()
{
    for(const wchar_t* var : {L"ProgramData", L"LOCALAPPDATA"})
    {
        wchar_t value[MAX_PATH]{};
        if(GetEnvironmentVariableW(var, value, MAX_PATH) > 0)
        {
            std::wstring base = value;
            if(!base.empty() && base.back() != L'\\')
            {
                base += L'\\';
            }
            return base + L"OpenRGB3DSpatial\\";
        }
    }
    return L"OpenRGB3DSpatial\\";
}

inline std::wstring ConfigFilePathW()
{
    return BaseDirW() + L"openrgb_mc_room_config.shm";
}

inline std::wstring FrameFilePathW()
{
    return BaseDirW() + L"openrgb_mc_room_sample.shm";
}

#else

inline std::string BaseDir()
{
    for(const char* var : {"XDG_RUNTIME_DIR", "TMPDIR"})
    {
        const char* value = std::getenv(var);
        if(value && *value)
        {
            std::string base = value;
            if(base.back() != '/')
            {
                base += '/';
            }
            return base + "OpenRGB3DSpatial/";
        }
    }
    const passwd* pw = getpwuid(getuid());
    return std::string("/tmp/OpenRGB3DSpatial-") + ((pw && pw->pw_name) ? pw->pw_name : "user") + "/";
}

/* Creates the folder private to this user. Refuses one owned by someone else, so another
   account on a shared /tmp cannot feed or read the files. */
inline bool EnsureBaseDir()
{
    const std::string dir = BaseDir();
    mkdir(dir.c_str(), 0700);
    struct stat st{};
    return stat(dir.c_str(), &st) == 0 && S_ISDIR(st.st_mode) && st.st_uid == getuid();
}

inline std::string ConfigFilePath()
{
    return BaseDir() + "openrgb_mc_room_config.shm";
}

inline std::string FrameFilePath()
{
    return BaseDir() + "openrgb_mc_room_sample.shm";
}

#endif

}

#endif
