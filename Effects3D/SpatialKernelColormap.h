// SPDX-License-Identifier: GPL-2.0-only

#ifndef SPATIALKERNELCOLORMAP_H
#define SPATIALKERNELCOLORMAP_H

#include "Game/StripPatternSurface.h"
#include "StripShellPattern/StripShellPatternKernels.h"
#include "StripShellPattern/StripKernelPatternPalettes.h"
#include "SpatialEffect3D.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>
#include <string>

enum class StripKernelColorStyle : int
{
    PatternPalette = 0,
    EffectColors = 1,
};

inline int StripKernelColorStyleClamp(int v)
{
    return std::clamp(v, 0, 1);
}

inline RGBColor ResolveStripKernelFinalColor(SpatialEffect3D& effect,
                                            int kernel_id,
                                            float palette01,
                                            int color_style,
                                            float time_sec,
                                            float /*rainbow_time_hue_mul*/)
{
    const int s = StripKernelColorStyleClamp(color_style);
    float p = std::fmod(palette01, 1.0f);
    if(p < 0.0f)
        p += 1.0f;
    kernel_id = StripShellKernelClamp(kernel_id);
    if(s == 0)
        return SampleKernelPatternPalette(kernel_id, p, time_sec);
    return effect.GetColorAtPosition(p);
}

inline float SampleStripKernelPalette01(int kernel_id,
                                        float kernel_rep,
                                        int unfold_mode,
                                        float dir_deg,
                                        float phase01,
                                        float time_sec,
                                        const GridContext3D& grid,
                                        float normalized_scale,
                                        const Vector3D& origin,
                                        const Vector3D& rot)
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
    float lx = (rot.x - origin.x) / sw;
    float ly = (rot.y - origin.y) / sh;
    float lz = (rot.z - origin.z) / sd;
    auto mode = static_cast<StripPatternSurface::UnfoldMode>(
        std::clamp(unfold_mode, 0, (int)StripPatternSurface::UnfoldMode::COUNT - 1));
    float phase_eff = phase01;
    float time_eff = time_sec;
    float s01;
    if(mode == StripPatternSurface::UnfoldMode::EffectPhaseOnly)
    {
        s01 = std::fmod(phase01 + time_sec * 0.12f, 1.0f);
        if(s01 < 0.0f)
        {
            s01 += 1.0f;
        }
        s01 = std::clamp(s01, 0.0f, 1.0f);
    }
    else if(mode == StripPatternSurface::UnfoldMode::StaticRoomPlane)
    {
        s01 = StripPatternSurface::StripCoord01(lx, ly, lz, StripPatternSurface::UnfoldMode::PlaneXZ, dir_deg);
        phase_eff = 0.0f;
        time_eff = 0.0f;
    }
    else
    {
        s01 = StripPatternSurface::StripCoord01(lx, ly, lz, mode, dir_deg);
    }
    float k = EvalStripShellKernel(kernel_id, s01, phase_eff, kernel_rep, time_eff);
    return std::clamp((k + 1.0f) * 0.5f, 0.0f, 1.0f);
}

inline void StripColormapSaveJson(nlohmann::json& j,
                                  const std::string& prefix,
                                  bool on,
                                  int kern,
                                  float rep,
                                  int unfold,
                                  float dir,
                                  int color_style)
{
    j[prefix + "_strip_cmap_on"] = on;
    j[prefix + "_strip_cmap_kernel"] = kern;
    j[prefix + "_strip_cmap_rep"] = rep;
    j[prefix + "_strip_cmap_unfold"] = unfold;
    j[prefix + "_strip_cmap_dir"] = dir;
    j[prefix + "_strip_cmap_color_style"] = StripKernelColorStyleClamp(color_style);
}

inline void StripColormapLoadJson(const nlohmann::json& settings,
                                  const std::string& prefix,
                                  bool& on,
                                  int& kern,
                                  float& rep,
                                  int& unfold,
                                  float& dir,
                                  int& color_style,
                                  bool /*legacy_rainbow_when_missing_key*/)
{
    const std::string k_on = prefix + "_strip_cmap_on";
    if(settings.contains(k_on) && settings[k_on].is_boolean())
        on = settings[k_on].get<bool>();
    const std::string k_k = prefix + "_strip_cmap_kernel";
    if(settings.contains(k_k) && settings[k_k].is_number_integer())
        kern = std::clamp(settings[k_k].get<int>(), 0, StripShellKernelCount() - 1);
    const std::string k_r = prefix + "_strip_cmap_rep";
    if(settings.contains(k_r) && settings[k_r].is_number())
        rep = std::max(1.0f, std::min(40.0f, settings[k_r].get<float>()));
    const std::string k_u = prefix + "_strip_cmap_unfold";
    if(settings.contains(k_u) && settings[k_u].is_number_integer())
        unfold = std::clamp(settings[k_u].get<int>(), 0, (int)StripPatternSurface::UnfoldMode::COUNT - 1);
    const std::string k_d = prefix + "_strip_cmap_dir";
    if(settings.contains(k_d) && settings[k_d].is_number())
        dir = std::fmod(settings[k_d].get<float>() + 360.0f, 360.0f);
    const std::string k_cs = prefix + "_strip_cmap_color_style";
    if(settings.contains(k_cs) && settings[k_cs].is_number_integer())
        color_style = StripKernelColorStyleClamp(settings[k_cs].get<int>());
    else
        color_style = 1;
}

#endif
