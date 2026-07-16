// SPDX-License-Identifier: GPL-2.0-only

#ifndef SCREENMIRROR_INTERNAL_H
#define SCREENMIRROR_INTERNAL_H

#include "ScreenCaptureManager.h"
#include "DisplayPlane3D.h"
#include "DisplayPlaneManager.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

constexpr int kWhiteRolloffSliderMax = 125;
constexpr float kWhiteRolloffStoredMax = static_cast<float>(kWhiteRolloffSliderMax) / 100.0f;

constexpr int kScreenMapRollTicksPerDegree = 2;
constexpr int kScreenMapRollSliderMax = 180 * kScreenMapRollTicksPerDegree;

inline float RadialMapUiToInternal(int ui_0_100)
{
    return (float)std::clamp(ui_0_100, 0, 100) - 50.0f;
}

/* Single definition in ScreenMirror_Render.cpp (std::call_once / static buffer). */
const uint8_t* GetCalibrationPatternBuffer(int& out_w, int& out_h);

inline bool CaptureSourceIdIsPrimary(const std::string& source_id)
{
    if(source_id.empty())
    {
        return false;
    }
    ScreenCaptureManager& mgr = ScreenCaptureManager::Instance();
    if(!mgr.IsInitialized())
    {
        return false;
    }
    for(const CaptureSourceInfo& info : mgr.GetAvailableSources())
    {
        if(info.id == source_id)
        {
            return info.is_primary;
        }
    }
    return false;
}

inline bool DefaultMonitorEnabledForPlane(DisplayPlane3D* plane)
{
    std::vector<DisplayPlane3D*> planes = DisplayPlaneManager::instance()->GetDisplayPlanes();
    if(!plane || planes.empty())
    {
        return true;
    }
    DisplayPlane3D* primary_plane = nullptr;
    for(DisplayPlane3D* p : planes)
    {
        if(!p) continue;
        if(CaptureSourceIdIsPrimary(p->GetCaptureSourceId()))
        {
            primary_plane = p;
            break;
        }
    }
    if(primary_plane)
    {
        return plane == primary_plane;
    }
    return plane == planes[0];
}

inline DisplayPlane3D* FindDisplayPlaneByName(const std::string& name)
{
    for(DisplayPlane3D* p : DisplayPlaneManager::instance()->GetDisplayPlanes())
    {
        if(p && p->GetName() == name)
        {
            return p;
        }
    }
    return nullptr;
}

#endif // SCREENMIRROR_INTERNAL_H
