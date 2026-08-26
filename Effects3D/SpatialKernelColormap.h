// SPDX-License-Identifier: GPL-2.0-only

#ifndef SPATIALKERNELCOLORMAP_H
#define SPATIALKERNELCOLORMAP_H

#include "Game/StripPatternSurface.h"
#include "SpatialPatternKernels/SpatialPatternKernels.h"
#include "SpatialPatternKernels/SpatialPatternPalettes.h"
#include "SpatialEffect3D.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>

inline RGBColor ResolveStripKernelFinalColor(int kernel_id, float palette01, float time_sec)
{
    float p = std::fmod(palette01, 1.0f);
    if(p < 0.0f)
        p += 1.0f;
    kernel_id = SpatialPatternKernelClamp(kernel_id);
    return SampleKernelPatternPalette(kernel_id, p, time_sec);
}

/** Freq/detail scaling for strip-colormap prepare. */
inline void StripColormapClockScale(float freq_norm,
                                    float detail_norm,
                                    float kernel_rep,
                                    float phase01,
                                    float time_sec,
                                    float& phase_eff,
                                    float& time_eff,
                                    float& kernel_rep_eff)
{
    kernel_rep_eff = std::max(1.0f, kernel_rep * (0.65f + 0.70f * detail_norm));
    phase_eff = phase01 * (0.70f + 0.55f * freq_norm);
    time_eff = time_sec * (0.55f + 0.90f * freq_norm);
}

inline float StripColormapComputeS01(int unfold_mode,
                                     float dir_deg,
                                     float phase_eff,
                                     float time_eff,
                                     float lx,
                                     float ly,
                                     float lz,
                                     float& phase_out,
                                     float& time_out)
{
    auto mode = static_cast<StripPatternSurface::UnfoldMode>(
        std::clamp(unfold_mode, 0, (int)StripPatternSurface::UnfoldMode::COUNT - 1));
    phase_out = phase_eff;
    time_out = time_eff;
    float s01;
    if(mode == StripPatternSurface::UnfoldMode::EffectPhaseOnly)
    {
        s01 = std::fmod(phase_eff + time_eff * 0.12f, 1.0f);
        if(s01 < 0.0f)
            s01 += 1.0f;
        s01 = std::clamp(s01, 0.0f, 1.0f);
    }
    else if(mode == StripPatternSurface::UnfoldMode::StaticRoomPlane)
    {
        s01 = StripPatternSurface::StripCoord01(lx, ly, lz, StripPatternSurface::UnfoldMode::PlaneXZ, dir_deg);
        phase_out = 0.0f;
        time_out = 0.0f;
    }
    else
    {
        s01 = StripPatternSurface::StripCoord01(lx, ly, lz, mode, dir_deg);
    }
    return s01;
}

inline void StripColormapLocalAxes(const GridContext3D& grid,
                                   float normalized_scale,
                                   const Vector3D& origin,
                                   const Vector3D& rot,
                                   float& lx,
                                   float& ly,
                                   float& lz)
{
    float scale_eff = std::max(0.05f, normalized_scale);
    float sw = grid.width * 0.5f * scale_eff;
    float sh = grid.height * 0.5f * scale_eff;
    float sd = grid.depth * 0.5f * scale_eff;
    if(sw < 1e-5f)
        sw = 1.0f;
    if(sh < 1e-5f)
        sh = 1.0f;
    if(sd < 1e-5f)
        sd = 1.0f;
    lx = (rot.x - origin.x) / sw;
    ly = (rot.y - origin.y) / sh;
    lz = (rot.z - origin.z) / sd;
}

inline void StripColormapSaveCanonical(nlohmann::json& j,
                                      bool on,
                                      int kern,
                                      float rep,
                                      int unfold,
                                      float dir)
{
    j["strip_cmap_on"] = on;
    j["strip_cmap_kernel"] = kern;
    j["strip_cmap_rep"] = rep;
    j["strip_cmap_unfold"] = unfold;
    j["strip_cmap_dir"] = dir;
}

inline void StripColormapLoadCanonical(const nlohmann::json& settings,
                                       bool& on,
                                       int& kern,
                                       float& rep,
                                       int& unfold,
                                       float& dir)
{
    if(settings.contains("strip_cmap_on") && settings["strip_cmap_on"].is_boolean())
        on = settings["strip_cmap_on"].get<bool>();
    if(settings.contains("strip_cmap_kernel") && settings["strip_cmap_kernel"].is_number_integer())
        kern = std::clamp(settings["strip_cmap_kernel"].get<int>(), 0, SpatialPatternKernelCount() - 1);
    if(settings.contains("strip_cmap_rep") && settings["strip_cmap_rep"].is_number())
        rep = std::max(1.0f, std::min(40.0f, settings["strip_cmap_rep"].get<float>()));
    if(settings.contains("strip_cmap_unfold") && settings["strip_cmap_unfold"].is_number_integer())
        unfold = std::clamp(settings["strip_cmap_unfold"].get<int>(), 0, (int)StripPatternSurface::UnfoldMode::COUNT - 1);
    if(settings.contains("strip_cmap_dir") && settings["strip_cmap_dir"].is_number())
        dir = std::fmod(settings["strip_cmap_dir"].get<float>() + 360.0f, 360.0f);
}

#endif
