// SPDX-License-Identifier: GPL-2.0-only

#ifndef ROOMSAMPLESHMPATHS_H
#define ROOMSAMPLESHMPATHS_H

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include <string>

namespace RoomSampleShmPaths
{

inline std::wstring BaseDirW()
{
#ifdef _WIN32
    wchar_t program_data[MAX_PATH]{};
    if(GetEnvironmentVariableW(L"ProgramData", program_data, MAX_PATH) > 0)
    {
        std::wstring base = program_data;
        if(!base.empty() && base.back() != L'\\')
        {
            base += L'\\';
        }
        return base + L"OpenRGB3DSpatial\\";
    }

    wchar_t local_app[MAX_PATH]{};
    if(GetEnvironmentVariableW(L"LOCALAPPDATA", local_app, MAX_PATH) > 0)
    {
        std::wstring base = local_app;
        if(!base.empty() && base.back() != L'\\')
        {
            base += L'\\';
        }
        return base + L"OpenRGB3DSpatial\\";
    }
#endif
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

}

#endif
