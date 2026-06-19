// SPDX-License-Identifier: GPL-2.0-only

#ifndef AUDIOREACTIVECOMMON_H
#define AUDIOREACTIVECOMMON_H

#include <algorithm>
#include <cmath>
#include <vector>
#include <nlohmann/json.hpp>
#include "RGBController.h"
#include "Audio/AudioInputManager.h"
#include "EffectStratumBlend.h"

struct AudioGradientStop3D
{
    float position;
    RGBColor color;
};

struct AudioGradient3D
{
    std::vector<AudioGradientStop3D> stops;
};

enum class AudioDriveMode : int
{
    Sustained = 0,
    Transient = 1,
    Beat = 2,
    BandOnset = 3,
};

enum class AudioStemTarget : int
{
    CustomHz = 0,
    StreamKick = 1,
    StreamSnare = 2,
    StreamHihat = 3,
    StreamBass = 4,
};

enum class AudioPulseColorMode : int
{
    PerBeatCycle = 0,
    Uniform = 1,
    SpatialAlongRing = 2,
    AudioGradient = 3,
};

enum class AudioBeatWaveMode : int
{
    ClassicWave = 0,
    FlashThenWave = 1,
    WaveOnBeat = 2,
    FlashOnBeat = 3,
    OffBeatWave = 4,
    RecedingClear = 5,
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
    int drive_mode = static_cast<int>(AudioDriveMode::Sustained);
    float sustain_reject = 0.65f;
    int stem_target = static_cast<int>(AudioStemTarget::CustomHz);
    int pulse_color_mode = static_cast<int>(AudioPulseColorMode::PerBeatCycle);
    int beat_wave_mode = static_cast<int>(AudioBeatWaveMode::ClassicWave);
    float wave_spread = 1.0f;
    float wave_decay = 1.0f;
};

inline RGBColor MakeRGBColor(int r, int g, int b)
{
    if(r < 0) r = 0;
    if(r > 255) r = 255;
    if(g < 0) g = 0;
    if(g > 255) g = 255;
    if(b < 0) b = 0;
    if(b > 255) b = 255;
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
    cfg.smoothing = 0.35f;
    cfg.falloff = 1.0f;
    cfg.foreground = MakeDefaultForegroundGradient();
    cfg.background = MakeDefaultBackgroundGradient();
    cfg.background_mix = 0.20f;
    cfg.peak_boost = 1.35f;
    cfg.drive_mode = static_cast<int>(AudioDriveMode::Sustained);
    cfg.sustain_reject = 0.65f;
    return cfg;
}

inline AudioReactiveSettings3D MakeDefaultBeatAudioReactiveSettings3D()
{
    AudioReactiveSettings3D cfg = MakeDefaultAudioReactiveSettings3D(55, 110);
    cfg.drive_mode = static_cast<int>(AudioDriveMode::Beat);
    cfg.sustain_reject = 0.72f;
    cfg.peak_boost = 1.65f;
    cfg.falloff = 0.88f;
    cfg.smoothing = 0.40f;
    cfg.pulse_color_mode = static_cast<int>(AudioPulseColorMode::PerBeatCycle);
    cfg.beat_wave_mode = static_cast<int>(AudioBeatWaveMode::ClassicWave);
    cfg.wave_spread = 1.0f;
    cfg.wave_decay = 1.0f;
    return cfg;
}

inline AudioReactiveSettings3D MakeDefaultLevelAudioReactiveSettings3D()
{
    AudioReactiveSettings3D cfg = MakeDefaultAudioReactiveSettings3D(55, 280);
    cfg.pulse_color_mode = static_cast<int>(AudioPulseColorMode::SpatialAlongRing);
    cfg.drive_mode = static_cast<int>(AudioDriveMode::Transient);
    cfg.smoothing = 0.28f;
    cfg.falloff = 0.82f;
    cfg.peak_boost = 1.55f;
    return cfg;
}

struct AudioPulseTriggerState
{
    float onset_smoothed = 0.0f;
    float onset_hold = 0.0f;
    bool beat_armed = true;
};

struct BeatShellHit
{
    float energy = 0.0f;
    size_t pulse_index = static_cast<size_t>(-1);

    bool valid() const
    {
        return pulse_index != static_cast<size_t>(-1) && energy > 0.0f;
    }
};

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
    if(cfg.peak_boost > 5.0f)
    {
        cfg.peak_boost = 5.0f;
    }
    if(cfg.drive_mode < 0)
    {
        cfg.drive_mode = 0;
    }
    if(cfg.drive_mode > 3)
    {
        cfg.drive_mode = 3;
    }
    if(cfg.sustain_reject < 0.0f)
    {
        cfg.sustain_reject = 0.0f;
    }
    if(cfg.sustain_reject > 1.0f)
    {
        cfg.sustain_reject = 1.0f;
    }
    if(cfg.stem_target < 0)
    {
        cfg.stem_target = 0;
    }
    if(cfg.stem_target > 4)
    {
        cfg.stem_target = 4;
    }
    if(cfg.pulse_color_mode < static_cast<int>(AudioPulseColorMode::PerBeatCycle)
       || cfg.pulse_color_mode > static_cast<int>(AudioPulseColorMode::AudioGradient))
    {
        cfg.pulse_color_mode = static_cast<int>(AudioPulseColorMode::PerBeatCycle);
    }
    if(cfg.beat_wave_mode < static_cast<int>(AudioBeatWaveMode::ClassicWave)
       || cfg.beat_wave_mode > static_cast<int>(AudioBeatWaveMode::RecedingClear))
    {
        cfg.beat_wave_mode = static_cast<int>(AudioBeatWaveMode::ClassicWave);
    }
    if(cfg.wave_spread < 0.25f)
    {
        cfg.wave_spread = 0.25f;
    }
    if(cfg.wave_spread > 8.0f)
    {
        cfg.wave_spread = 8.0f;
    }
    if(cfg.wave_decay < 0.12f)
    {
        cfg.wave_decay = 0.12f;
    }
    if(cfg.wave_decay > 8.0f)
    {
        cfg.wave_decay = 8.0f;
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
    j["drive_mode"] = cfg.drive_mode;
    j["sustain_reject"] = cfg.sustain_reject;
    j["stem_target"] = cfg.stem_target;
    j["pulse_color_mode"] = cfg.pulse_color_mode;
    j["beat_wave_mode"] = cfg.beat_wave_mode;
    j["wave_spread"] = cfg.wave_spread;
    j["wave_decay"] = cfg.wave_decay;
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
    if(settings.contains("drive_mode"))
    {
        cfg.drive_mode = settings["drive_mode"].get<int>();
    }
    if(settings.contains("sustain_reject"))
    {
        cfg.sustain_reject = settings["sustain_reject"].get<float>();
    }
    if(settings.contains("stem_target"))
    {
        cfg.stem_target = settings["stem_target"].get<int>();
    }
    if(settings.contains("pulse_color_mode"))
    {
        cfg.pulse_color_mode = settings["pulse_color_mode"].get<int>();
    }
    if(settings.contains("beat_wave_mode"))
    {
        cfg.beat_wave_mode = settings["beat_wave_mode"].get<int>();
    }
    if(settings.contains("wave_spread"))
    {
        cfg.wave_spread = settings["wave_spread"].get<float>();
    }
    if(settings.contains("wave_decay"))
    {
        cfg.wave_decay = settings["wave_decay"].get<float>();
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

inline float AudioEffectDisplayBrightness(float energy)
{
    energy = std::clamp(energy, 0.0f, 1.0f);
    const float lifted = std::pow(energy, 0.78f);
    return std::clamp(0.52f + 0.98f * lifted, 0.0f, 1.32f);
}

inline RGBColor BrightenAudioEffectColor(RGBColor color, float energy)
{
    return ScaleRGBColor(color, AudioEffectDisplayBrightness(energy));
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

inline float AudioReactiveOnsetSmoothAlpha(const AudioReactiveSettings3D& cfg)
{
    return std::clamp(cfg.smoothing, 0.0f, 0.85f);
}

inline float SampleAudioDriveLevel(const AudioReactiveSettings3D& cfg)
{
    AudioInputManager* audio = AudioInputManager::instance();
    if(!audio)
    {
        return 0.0f;
    }
    const AudioStemTarget stem = static_cast<AudioStemTarget>(cfg.stem_target);
    if(stem != AudioStemTarget::CustomHz)
    {
        const AudioInputManager::StreamStemLevels stems = audio->getStreamStemLevels();
        float v = 0.0f;
        switch(stem)
        {
        case AudioStemTarget::StreamKick:
            v = stems.kick;
            break;
        case AudioStemTarget::StreamSnare:
            v = stems.snare;
            break;
        case AudioStemTarget::StreamHihat:
            v = stems.hihat;
            break;
        case AudioStemTarget::StreamBass:
            v = stems.bass;
            break;
        default:
            break;
        }
        return std::clamp(v, 0.0f, 1.0f);
    }
    const float low = (float)cfg.low_hz;
    const float high = (float)cfg.high_hz;
    const float sustain = audio->getBandSlowEnergyHz(low, high);
    const AudioDriveMode mode = static_cast<AudioDriveMode>(cfg.drive_mode);
    switch(mode)
    {
    case AudioDriveMode::Transient:
        return audio->getBandTransientEnergyHz(low, high);
    case AudioDriveMode::BandOnset:
        return audio->getBandOnsetLevel(low, high);
    case AudioDriveMode::Beat:
    {
        const float trans = audio->getBandTransientEnergyHz(low, high);
        const float reject = std::clamp(cfg.sustain_reject, 0.0f, 1.0f) * sustain;
        return std::max(0.0f, trans - reject);
    }
    case AudioDriveMode::Sustained:
    default:
        return audio->getBandSlowEnergyHz(low, high);
    }
}

inline float SampleAudioOnsetLevel(const AudioReactiveSettings3D& cfg)
{
    AudioInputManager* audio = AudioInputManager::instance();
    if(!audio)
    {
        return 0.0f;
    }
    const AudioStemTarget stem = static_cast<AudioStemTarget>(cfg.stem_target);
    if(stem != AudioStemTarget::CustomHz)
    {
        return SampleAudioDriveLevel(cfg);
    }
    if(static_cast<AudioDriveMode>(cfg.drive_mode) == AudioDriveMode::Sustained)
    {
        return audio->getOnsetLevel();
    }
    return audio->getBandOnsetLevel((float)cfg.low_hz, (float)cfg.high_hz);
}

inline float SampleAudioVisualLevel(const AudioReactiveSettings3D& cfg)
{
    float level = SampleAudioDriveLevel(cfg);
    const AudioDriveMode mode = static_cast<AudioDriveMode>(cfg.drive_mode);
    if(mode != AudioDriveMode::Sustained)
    {
        const float onset = SampleAudioOnsetLevel(cfg);
        level = std::max(level, onset * 0.42f);
    }
    return std::clamp(level, 0.0f, 1.0f);
}

inline float ApplyAudioPulseIntensity(float value, const AudioReactiveSettings3D& cfg)
{
    float shaped = ApplyAudioIntensity(value, cfg);
    return std::clamp(std::sqrt(shaped), 0.0f, 1.0f);
}

inline float AudioReactiveBeatPulseHoldSec()
{
    return 0.20f;
}

inline float AudioReactiveSoftSaturate(float sum)
{
    return 1.0f - std::exp(-std::max(0.0f, sum));
}

inline float AudioReactivePulseFade(float strength, float age_sec, float decay_per_sec)
{
    return strength * std::exp(-decay_per_sec * age_sec);
}

inline float BeatWaveSpreadMul(const AudioReactiveSettings3D& cfg)
{
    return std::clamp(cfg.wave_spread, 0.25f, 8.0f);
}

inline float BeatWaveDecayMul(const AudioReactiveSettings3D& cfg)
{
    return std::clamp(cfg.wave_decay, 0.12f, 8.0f);
}

inline float BeatWaveBurstPhaseCap(const AudioReactiveSettings3D& cfg)
{
    return 1.35f * BeatWaveSpreadMul(cfg);
}

inline float BeatWaveScaledSpeed(float pulse_speed, const AudioReactiveSettings3D& cfg)
{
    return pulse_speed * BeatWaveSpreadMul(cfg);
}

inline float BeatWaveShellDecay(const AudioReactiveSettings3D& cfg, float base_decay)
{
    return base_decay * BeatWaveDecayMul(cfg);
}

inline bool TryTriggerAudioPulse(float dt,
                                 const AudioReactiveSettings3D& cfg,
                                 AudioPulseTriggerState& state,
                                 float onset_threshold,
                                 float onset_smooth_alpha,
                                 float hold_sec,
                                 float& out_strength)
{
    const float onset_raw = SampleAudioOnsetLevel(cfg);
    state.onset_smoothed =
        onset_smooth_alpha * state.onset_smoothed + (1.0f - onset_smooth_alpha) * onset_raw;

    if(state.onset_hold > 0.0f)
    {
        state.onset_hold = std::max(0.0f, state.onset_hold - dt);
        return false;
    }

    constexpr float kRearmRatio = 0.52f;
    if(state.onset_smoothed < onset_threshold * kRearmRatio)
    {
        state.beat_armed = true;
    }

    const float drive = SampleAudioDriveLevel(cfg);
    const float shaped_drive = ApplyAudioPulseIntensity(std::clamp(drive, 0.0f, 1.0f), cfg);
    const float shaped_onset =
        ApplyAudioPulseIntensity(std::clamp(state.onset_smoothed, 0.0f, 1.0f), cfg);

    const AudioDriveMode mode = static_cast<AudioDriveMode>(cfg.drive_mode);
    const bool allow_drive_only =
        (mode == AudioDriveMode::Transient || mode == AudioDriveMode::BandOnset);

    const bool onset_hit =
        state.beat_armed
        && (state.onset_smoothed >= onset_threshold || onset_raw >= onset_threshold * 1.08f);

    constexpr float kDriveTrigger = 0.22f;
    const bool drive_hit = shaped_drive >= kDriveTrigger;
    if(!onset_hit && !(allow_drive_only && drive_hit))
    {
        return false;
    }

    const float shaped_raw_onset =
        ApplyAudioPulseIntensity(std::clamp(onset_raw, 0.0f, 1.0f), cfg);
    float strength = shaped_onset;
    if(onset_hit)
    {
        strength = std::max(strength, shaped_raw_onset * 0.88f);
    }
    if(drive_hit)
    {
        strength = std::max(strength, shaped_drive * (onset_hit ? 0.72f : 1.0f));
    }
    if(onset_hit && drive_hit)
    {
        strength = std::max(strength,
                            (shaped_drive + std::max(shaped_onset, shaped_raw_onset)) * 0.48f);
    }
    strength = std::clamp(strength * 1.10f, 0.0f, 1.0f);
    if(strength < 0.04f)
    {
        return false;
    }

    out_strength = strength;
    state.onset_hold = hold_sec;
    state.beat_armed = false;
    return true;
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

inline float AudioReactiveSmoothstep(float edge0, float edge1, float x)
{
    const float denom = edge1 - edge0;
    float t = (std::fabs(denom) > 1e-8f) ? (x - edge0) / denom : (x >= edge0 ? 1.0f : 0.0f);
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

struct AudioExpandingRingParams
{
    float coord = 0.0f;
    float ring_radius = 0.0f;
    float half_width = 0.01f;
    float inner_cutoff_mul = 1.05f;
    bool allow_echo_trail = true;
    float echo_strength = 0.18f;
};

inline float BeatHitInstantFlash(float age_sec, float strength, float decay_per_sec = 24.0f)
{
    if(age_sec < 0.0f || strength <= 0.0f)
    {
        return 0.0f;
    }
    return strength * std::exp(-age_sec * decay_per_sec);
}

inline float SampleClassicExplosionBeatShell(float distance,
                                            float radius_basis,
                                            float age_sec,
                                            float pulse_speed,
                                            float falloff,
                                            float size_m,
                                            float detail,
                                            float tight_mul,
                                            const EffectStratumBlend::BandBlendScalars& bb,
                                            float stratum_mot01,
                                            const AudioReactiveSettings3D& cfg)
{
    const float tm = std::clamp(tight_mul, 0.25f, 4.0f);
    const float size_cl = std::clamp(size_m, 0.2f, 2.0f);

    float wave_thickness =
        radius_basis * (0.02f + 0.09f / std::max(0.2f, falloff)) * size_cl;
    wave_thickness /= std::max(0.25f, tm);
    wave_thickness *= std::clamp(0.85f + 0.15f * detail, 0.7f, 1.15f);

    const float freq_rip = detail * tm * 0.08f / std::max(0.08f, size_cl);
    const float travel_speed = BeatWaveScaledSpeed(pulse_speed, cfg);
    const float burst_phase = std::min(BeatWaveBurstPhaseCap(cfg), age_sec * travel_speed * 0.55f);
    const float explosion_radius = burst_phase * radius_basis * (0.14f + 0.86f * size_cl);

    float primary = 1.0f - AudioReactiveSmoothstep(explosion_radius - wave_thickness,
                                                   explosion_radius + wave_thickness,
                                                   distance);
    primary *= std::exp(-std::fabs(distance - explosion_radius)
                        * (7.0f / std::max(wave_thickness, radius_basis * 0.0015f)));

    const float secondary_radius = explosion_radius * 0.68f;
    float secondary = 1.0f - AudioReactiveSmoothstep(secondary_radius - wave_thickness * 0.45f,
                                                     secondary_radius + wave_thickness * 0.55f,
                                                     distance);
    secondary *= std::exp(-std::fabs(distance - secondary_radius) * 0.11f) * 0.62f;

    const float shock_age = std::exp(-age_sec * 2.4f);
    float shock = 0.09f * shock_age
                  * std::sin(distance * freq_rip * 10.0f - burst_phase * 6.2831853f
                             + EffectStratumBlend::ApplyMotionToAngleRad(
                                   EffectStratumBlend::PhaseShiftRad(bb), stratum_mot01));
    shock *= std::exp(-distance * 0.065f);

    float core = 0.0f;
    if(distance < explosion_radius * 0.24f)
    {
        core = (1.0f - distance / (explosion_radius * 0.24f + 1e-4f)) * 0.4f;
    }

    return std::min(1.0f, primary + secondary + shock + core);
}

inline float BeatWaveRingAgeSec(float age_sec, AudioBeatWaveMode mode)
{
    if(mode == AudioBeatWaveMode::OffBeatWave)
    {
        constexpr float kOffBeatDelaySec = 0.10f;
        return std::max(0.0f, age_sec - kOffBeatDelaySec);
    }
    return age_sec;
}

inline float BeatWaveInstantFlash(float age_sec,
                                  const AudioReactiveSettings3D& cfg,
                                  float strength = 1.0f)
{
    return BeatHitInstantFlash(age_sec, strength, 18.0f * BeatWaveDecayMul(cfg));
}

inline bool BeatWaveUsesRing(AudioBeatWaveMode mode)
{
    return mode != AudioBeatWaveMode::FlashOnBeat && mode != AudioBeatWaveMode::ClassicWave;
}

inline float SampleBeatWaveShell(AudioBeatWaveMode mode,
                                 float age_sec,
                                 float coord,
                                 float ring_radius,
                                 float half_width,
                                 float expanding_ring_band,
                                 const AudioReactiveSettings3D& cfg)
{
    const float instant = BeatWaveInstantFlash(age_sec, cfg);

    switch(mode)
    {
    case AudioBeatWaveMode::FlashOnBeat:
        return instant;
    case AudioBeatWaveMode::WaveOnBeat:
        return expanding_ring_band;
    case AudioBeatWaveMode::OffBeatWave:
        if(age_sec < 0.10f)
        {
            return 0.0f;
        }
        return expanding_ring_band;
    case AudioBeatWaveMode::RecedingClear:
    {
        const float lit_outside =
            AudioReactiveSmoothstep(0.0f, half_width * 1.35f, coord - ring_radius);
        return AudioReactiveSoftSaturate(std::max(instant, lit_outside));
    }
    case AudioBeatWaveMode::FlashThenWave:
        return AudioReactiveSoftSaturate(expanding_ring_band + instant * 0.9f);
    case AudioBeatWaveMode::ClassicWave:
    default:
        return expanding_ring_band;
    }
}

inline float SampleAudioExpandingRingBand(const AudioExpandingRingParams& p)
{
    if(p.half_width <= 0.0f)
    {
        return 0.0f;
    }
    const float dist_from_ring = std::fabs(p.coord - p.ring_radius);
    float band = 1.0f - AudioReactiveSmoothstep(0.0f, p.half_width, dist_from_ring);
    band *= band;
    if(p.coord < p.ring_radius - p.half_width * p.inner_cutoff_mul)
    {
        band = 0.0f;
    }

    float trail = 0.0f;
    if(p.allow_echo_trail && p.ring_radius > p.half_width * 2.5f)
    {
        const float echo_radius = p.ring_radius - p.half_width * 4.0f;
        const float echo_dist = std::fabs(p.coord - echo_radius);
        trail = p.echo_strength * (1.0f - AudioReactiveSmoothstep(0.0f, p.half_width * 0.55f, echo_dist));
        if(p.coord < echo_radius - p.half_width * p.inner_cutoff_mul)
        {
            trail = 0.0f;
        }
    }
    return std::max(band, trail);
}

inline float AudioRingHalfWidthFromFalloff(float span_units,
                                           float falloff,
                                           float size_mul = 1.0f,
                                           float tight_mul = 1.0f,
                                           float detail_mul = 1.0f)
{
    float half_w = span_units * (0.006f + 0.048f / std::max(0.25f, falloff)) * std::clamp(size_mul, 0.2f, 2.0f);
    half_w /= std::max(0.35f, tight_mul);
    half_w *= std::clamp(0.82f + 0.18f * detail_mul, 0.65f, 1.2f);
    return std::max(half_w, span_units * 0.0035f);
}

#endif
