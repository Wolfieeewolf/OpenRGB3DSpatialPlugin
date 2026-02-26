// SPDX-License-Identifier: GPL-2.0-only

#ifndef AUDIOREACTIVECOMMON_H
#define AUDIOREACTIVECOMMON_H

#include <algorithm>
#include <cmath>
#include <vector>
#include <nlohmann/json.hpp>
#include "RGBController.h"

struct AudioGradientStop3D
{
    float position;   // 0..1
    RGBColor color;   // 0x00BBGGRR
};

struct AudioGradient3D
{
    std::vector<AudioGradientStop3D> stops;
};

struct AudioReactiveSettings3D
{
    int low_hz;
    int high_hz;
    float smoothing;
    float falloff;
    AudioGradient3D foreground;
    AudioGradient3D background;
    float background_mix;
    float peak_boost;
};

inline RGBColor MakeRGBColor(int r, int g, int b)
{
    if(r < 0) r = 0; if(r > 255) r = 255;
    if(g < 0) g = 0; if(g > 255) g = 255;
    if(b < 0) b = 0; if(b > 255) b = 255;
    return ((RGBColor)b << 16) | ((RGBColor)g << 8) | (RGBColor)r;
}

inline void NormalizeGradient(AudioGradient3D& grad)
{
    if(grad.stops.empty())
    {
        grad.stops.push_back({0.0f, MakeRGBColor(0, 0, 0)});
        grad.stops.push_back({1.0f, MakeRGBColor(255, 255, 255)});
    }
    std::sort(grad.stops.begin(), grad.stops.end(), [](const AudioGradientStop3D& a, const AudioGradientStop3D& b){
        return a.position < b.position;
    });
    if(grad.stops.front().position > 0.0f || grad.stops.back().position < 1.0f)
    {
        if(grad.stops.front().position > 0.0f)
        {
            grad.stops.insert(grad.stops.begin(), {0.0f, grad.stops.front().color});
        }
        if(grad.stops.back().position < 1.0f)
        {
            grad.stops.push_back({1.0f, grad.stops.back().color});
        }
    }
    grad.stops.front().position = 0.0f;
    grad.stops.back().position = 1.0f;
    for(size_t i = 1; i + 1 < grad.stops.size(); ++i)
    {
        if(grad.stops[i].position < 0.0f) grad.stops[i].position = 0.0f;
        if(grad.stops[i].position > 1.0f) grad.stops[i].position = 1.0f;
    }
}

inline RGBColor SampleGradient(const AudioGradient3D& grad, float t)
{
    if(grad.stops.empty())
    {
        return 0x00000000;
    }
    if(t <= 0.0f) return grad.stops.front().color;
    if(t >= 1.0f) return grad.stops.back().color;
    for(size_t i = 1; i < grad.stops.size(); ++i)
    {
        const AudioGradientStop3D& prev = grad.stops[i - 1];
        const AudioGradientStop3D& next = grad.stops[i];
        if(t <= next.position)
        {
            float span = next.position - prev.position;
            float local_t = (span <= 1e-5f) ? 0.0f : (t - prev.position) / span;
            int pr = prev.color & 0xFF;
            int pg = (prev.color >> 8) & 0xFF;
            int pb = (prev.color >> 16) & 0xFF;
            int nr = next.color & 0xFF;
            int ng = (next.color >> 8) & 0xFF;
            int nb = (next.color >> 16) & 0xFF;
            int r = pr + (int)((nr - pr) * local_t);
            int g = pg + (int)((ng - pg) * local_t);
            int b = pb + (int)((nb - pb) * local_t);
            return MakeRGBColor(r, g, b);
        }
    }
    return grad.stops.back().color;
}

inline AudioGradient3D MakeDefaultForegroundGradient()
{
    AudioGradient3D grad;
    grad.stops = {
        {0.0f, MakeRGBColor(255, 255, 255)},
        {1.0f, MakeRGBColor(200, 200, 200)}
    };
    NormalizeGradient(grad);
    return grad;
}

inline AudioGradient3D MakeDefaultBackgroundGradient()
{
    AudioGradient3D grad;
    grad.stops = {
        {0.0f, MakeRGBColor(48, 48, 48)},
        {1.0f, MakeRGBColor(24, 24, 24)}
    };
    NormalizeGradient(grad);
    return grad;
}

inline AudioReactiveSettings3D MakeDefaultAudioReactiveSettings3D(int low, int high)
{
    AudioReactiveSettings3D cfg;
    cfg.low_hz = low;
    cfg.high_hz = high;
    cfg.smoothing = 0.6f;
    cfg.falloff = 1.0f;
    cfg.foreground = MakeDefaultForegroundGradient();
    cfg.background = MakeDefaultBackgroundGradient();
    cfg.background_mix = 0.35f;
    cfg.peak_boost = 1.35f;
    return cfg;
}

inline void NormalizeAudioReactiveSettings(AudioReactiveSettings3D& cfg)
{
    if(cfg.low_hz < 1)
    {
        cfg.low_hz = 1;
    }
    if(cfg.high_hz <= cfg.low_hz)
    {
        cfg.high_hz = cfg.low_hz + 1;
    }
    if(cfg.smoothing < 0.0f)
    {
        cfg.smoothing = 0.0f;
    }
    if(cfg.smoothing > 0.99f)
    {
        cfg.smoothing = 0.99f;
    }
    if(cfg.falloff < 0.2f)
    {
        cfg.falloff = 0.2f;
    }
    if(cfg.falloff > 5.0f)
    {
        cfg.falloff = 5.0f;
    }
    if(cfg.background_mix < 0.0f)
    {
        cfg.background_mix = 0.0f;
    }
    if(cfg.background_mix > 1.0f)
    {
        cfg.background_mix = 1.0f;
    }
    if(cfg.peak_boost < 0.5f)
    {
        cfg.peak_boost = 0.5f;
    }
    if(cfg.peak_boost > 4.0f)
    {
        cfg.peak_boost = 4.0f;
    }
    NormalizeGradient(cfg.foreground);
    NormalizeGradient(cfg.background);
}

inline void AudioGradientSaveToJson(nlohmann::json& j, const std::string& key, const AudioGradient3D& grad)
{
    nlohmann::json arr = nlohmann::json::array();
    for(size_t i = 0; i < grad.stops.size(); i++)
    {
        nlohmann::json entry;
        entry["position"] = grad.stops[i].position;
        entry["color"] = grad.stops[i].color;
        arr.push_back(entry);
    }
    j[key] = arr;
}

inline void AudioGradientLoadFromJson(AudioGradient3D& grad, const nlohmann::json& j, const std::string& key)
{
    if(!j.contains(key))
    {
        return;
    }
    const nlohmann::json& arr = j.at(key);
    if(!arr.is_array())
    {
        return;
    }
    grad.stops.clear();
    for(size_t i = 0; i < arr.size(); i++)
    {
        const nlohmann::json& entry = arr[i];
        if(!entry.contains("position") || !entry.contains("color"))
        {
            continue;
        }
        float pos = entry["position"].get<float>();
        RGBColor color = entry["color"].get<RGBColor>();
        grad.stops.push_back({pos, color});
    }
    NormalizeGradient(grad);
}

inline void AudioReactiveSaveToJson(nlohmann::json& j, const AudioReactiveSettings3D& cfg)
{
    j["low_hz"] = cfg.low_hz;
    j["high_hz"] = cfg.high_hz;
    j["smoothing"] = cfg.smoothing;
    j["falloff"] = cfg.falloff;
    j["background_mix"] = cfg.background_mix;
    j["peak_boost"] = cfg.peak_boost;
    AudioGradientSaveToJson(j, "foreground_gradient", cfg.foreground);
    AudioGradientSaveToJson(j, "background_gradient", cfg.background);
}

inline void AudioReactiveLoadFromJson(AudioReactiveSettings3D& cfg, const nlohmann::json& settings)
{
    if(settings.contains("low_hz"))
    {
        cfg.low_hz = settings["low_hz"].get<int>();
    }
    if(settings.contains("high_hz"))
    {
        cfg.high_hz = settings["high_hz"].get<int>();
    }
    if(settings.contains("smoothing"))
    {
        cfg.smoothing = settings["smoothing"].get<float>();
    }
    if(settings.contains("falloff"))
    {
        cfg.falloff = settings["falloff"].get<float>();
    }
    if(settings.contains("background_mix"))
    {
        cfg.background_mix = settings["background_mix"].get<float>();
    }
    if(settings.contains("peak_boost"))
    {
        cfg.peak_boost = settings["peak_boost"].get<float>();
    }
    AudioGradientLoadFromJson(cfg.foreground, settings, "foreground_gradient");
    AudioGradientLoadFromJson(cfg.background, settings, "background_gradient");
    NormalizeAudioReactiveSettings(cfg);
}

inline float AudioReactiveShapeLevel(float value, float falloff)
{
    if(value < 0.0f)
    {
        value = 0.0f;
    }
    if(value > 1.0f)
    {
        value = 1.0f;
    }
    float expo = std::max(0.2f, std::min(5.0f, falloff));
    return std::pow(value, expo);
}

inline RGBColor BlendRGBColors(RGBColor a, RGBColor b, float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    float inv = 1.0f - t;
    int ar = a & 0xFF;
    int ag = (a >> 8) & 0xFF;
    int ab = (a >> 16) & 0xFF;
    int br = b & 0xFF;
    int bg = (b >> 8) & 0xFF;
    int bb = (b >> 16) & 0xFF;
    int r = (int)std::round(ar * inv + br * t);
    int g = (int)std::round(ag * inv + bg * t);
    int bch = (int)std::round(ab * inv + bb * t);
    r = std::clamp(r, 0, 255);
    g = std::clamp(g, 0, 255);
    bch = std::clamp(bch, 0, 255);
    return ((RGBColor)bch << 16) | ((RGBColor)g << 8) | (RGBColor)r;
}

inline RGBColor ScaleRGBColor(RGBColor color, float scale)
{
    scale = std::max(0.0f, scale);
    int r = (int)std::round((color & 0xFF) * scale);
    int g = (int)std::round(((color >> 8) & 0xFF) * scale);
    int b = (int)std::round(((color >> 16) & 0xFF) * scale);
    r = std::clamp(r, 0, 255);
    g = std::clamp(g, 0, 255);
    b = std::clamp(b, 0, 255);
    return ((RGBColor)b << 16) | ((RGBColor)g << 8) | (RGBColor)r;
}

inline RGBColor ModulateRGBColors(RGBColor color, RGBColor modifier)
{
    float mr = (modifier & 0xFF) / 255.0f;
    float mg = ((modifier >> 8) & 0xFF) / 255.0f;
    float mb = ((modifier >> 16) & 0xFF) / 255.0f;
    int r = (int)std::round((color & 0xFF) * mr);
    int g = (int)std::round(((color >> 8) & 0xFF) * mg);
    int b = (int)std::round(((color >> 16) & 0xFF) * mb);
    r = std::clamp(r, 0, 255);
    g = std::clamp(g, 0, 255);
    b = std::clamp(b, 0, 255);
    return ((RGBColor)b << 16) | ((RGBColor)g << 8) | (RGBColor)r;
}

inline float NormalizeRange(float value, float min, float max)
{
    float range = max - min;
    if(range <= 1e-5f)
    {
        return 0.5f;
    }
    float t = (value - min) / range;
    return std::clamp(t, 0.0f, 1.0f);
}

inline float ComputeRadialNormalized(float dx, float dy, float dz, float max_radius)
{
    float radius = std::sqrt(dx * dx + dy * dy + dz * dz);
    if(max_radius <= 1e-5f)
    {
        return 0.0f;
    }
    return std::clamp(radius / max_radius, 0.0f, 1.0f);
}

inline float ApplyAudioIntensity(float value, const AudioReactiveSettings3D& cfg)
{
    float boosted = std::clamp(value * cfg.peak_boost, 0.0f, 1.0f);
    return AudioReactiveShapeLevel(boosted, cfg.falloff);
}

inline RGBColor ComposeAudioGradientColor(const AudioReactiveSettings3D& cfg, float gradient_pos, float intensity)
{
    float gpos = std::clamp(gradient_pos, 0.0f, 1.0f);
    float shaped = std::clamp(intensity, 0.0f, 1.0f);
    RGBColor background = SampleGradient(cfg.background, gpos);
    RGBColor foreground = SampleGradient(cfg.foreground, gpos);
    RGBColor audio_mix = BlendRGBColors(background, foreground, shaped);
    float accent = std::clamp(1.0f - cfg.background_mix, 0.0f, 1.0f);
    return BlendRGBColors(background, audio_mix, accent);
}

#endif // AUDIOREACTIVECOMMON_H
