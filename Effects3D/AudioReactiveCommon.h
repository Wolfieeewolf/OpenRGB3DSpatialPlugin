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

enum class AudioDriveMode : int
{
    Sustained = 0,
    Beat = 2,
};

/** Low = kick/bass; Mid = voice/melody; High = sparkle/notes; Mixed = overview. */
enum class AudioRegisterRole : int
{
    Low = 0,
    Mid = 1,
    High = 2,
    Mixed = 3,
};

enum class AudioPulseColorMode : int
{
    PerBeatCycle = 0,
    Uniform = 1,
    SpatialAlongRing = 2,
    FollowNotes = 4,
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
    float peak_boost;
    int drive_mode = static_cast<int>(AudioDriveMode::Sustained);
    float sustain_reject = 0.65f;
    int register_role = static_cast<int>(AudioRegisterRole::Mixed);
    int pulse_color_mode = static_cast<int>(AudioPulseColorMode::SpatialAlongRing);
    int beat_wave_mode = static_cast<int>(AudioBeatWaveMode::ClassicWave);
    float wave_spread = 1.0f;
    float wave_decay = 1.0f;
};

inline AudioReactiveSettings3D MakeDefaultAudioReactiveSettings3D(int low, int high)
{
    AudioReactiveSettings3D cfg;
    cfg.low_hz = low;
    cfg.high_hz = high;
    cfg.smoothing = 0.45f;
    cfg.falloff = 1.0f;
    cfg.peak_boost = 1.25f;
    cfg.drive_mode = static_cast<int>(AudioDriveMode::Sustained);
    cfg.sustain_reject = 0.65f;
    cfg.register_role = static_cast<int>(AudioRegisterRole::Mixed);
    cfg.pulse_color_mode = static_cast<int>(AudioPulseColorMode::SpatialAlongRing);
    return cfg;
}

inline void ApplyAudioRegisterRole(AudioReactiveSettings3D& cfg)
{
    if(cfg.register_role < static_cast<int>(AudioRegisterRole::Low)
       || cfg.register_role > static_cast<int>(AudioRegisterRole::Mixed))
    {
        cfg.register_role = static_cast<int>(AudioRegisterRole::Mixed);
    }

    const auto role = static_cast<AudioRegisterRole>(cfg.register_role);
    switch(role)
    {
    case AudioRegisterRole::Low:
        cfg.low_hz = 40;
        cfg.high_hz = 180;
        cfg.drive_mode = static_cast<int>(AudioDriveMode::Beat);
        cfg.sustain_reject = 0.72f;
        cfg.smoothing = 0.48f;
        cfg.peak_boost = 1.35f;
        cfg.falloff = std::max(cfg.falloff, 0.85f);
        break;
    case AudioRegisterRole::Mid:
        /* Voice + pitched instruments (ColorChord-weighted band). */
        cfg.low_hz = 200;
        cfg.high_hz = 4000;
        cfg.drive_mode = static_cast<int>(AudioDriveMode::Sustained);
        cfg.smoothing = 0.58f;
        cfg.peak_boost = 1.18f;
        break;
    case AudioRegisterRole::High:
        cfg.low_hz = 2800;
        cfg.high_hz = 14000;
        cfg.drive_mode = static_cast<int>(AudioDriveMode::Sustained);
        cfg.smoothing = 0.68f;
        cfg.peak_boost = 1.45f;
        break;
    case AudioRegisterRole::Mixed:
        cfg.low_hz = 40;
        cfg.high_hz = 12000;
        cfg.drive_mode = static_cast<int>(AudioDriveMode::Sustained);
        cfg.smoothing = 0.45f;
        cfg.peak_boost = 1.20f;
        break;
    }
}

inline AudioReactiveSettings3D MakeDefaultBeatAudioReactiveSettings3D()
{
    AudioReactiveSettings3D cfg = MakeDefaultAudioReactiveSettings3D(40, 180);
    cfg.register_role = static_cast<int>(AudioRegisterRole::Low);
    ApplyAudioRegisterRole(cfg);
    cfg.beat_wave_mode = static_cast<int>(AudioBeatWaveMode::ClassicWave);
    cfg.wave_spread = 1.0f;
    cfg.wave_decay = 1.15f;
    cfg.pulse_color_mode = static_cast<int>(AudioPulseColorMode::SpatialAlongRing);
    return cfg;
}

inline AudioReactiveSettings3D MakeDefaultLevelAudioReactiveSettings3D()
{
    AudioReactiveSettings3D cfg = MakeDefaultAudioReactiveSettings3D(200, 4000);
    cfg.register_role = static_cast<int>(AudioRegisterRole::Mid);
    ApplyAudioRegisterRole(cfg);
    cfg.pulse_color_mode = static_cast<int>(AudioPulseColorMode::SpatialAlongRing);
    cfg.falloff = 0.82f;
    return cfg;
}

inline AudioReactiveSettings3D MakeDefaultSpectrumAudioReactiveSettings3D()
{
    AudioReactiveSettings3D cfg = MakeDefaultAudioReactiveSettings3D(40, 12000);
    cfg.register_role = static_cast<int>(AudioRegisterRole::Mixed);
    ApplyAudioRegisterRole(cfg);
    cfg.pulse_color_mode = static_cast<int>(AudioPulseColorMode::SpatialAlongRing);
    return cfg;
}

/** Dedicated Low stack layer — kick/bass punch, not melody. */
inline AudioReactiveSettings3D MakeDefaultLowPunchAudioReactiveSettings3D()
{
    AudioReactiveSettings3D cfg = MakeDefaultBeatAudioReactiveSettings3D();
    cfg.register_role = static_cast<int>(AudioRegisterRole::Low);
    ApplyAudioRegisterRole(cfg);
    cfg.peak_boost = 1.55f;
    cfg.sustain_reject = 0.70f;
    cfg.falloff = 0.88f;
    cfg.smoothing = 0.42f;
    cfg.pulse_color_mode = static_cast<int>(AudioPulseColorMode::SpatialAlongRing);
    cfg.beat_wave_mode = static_cast<int>(AudioBeatWaveMode::ClassicWave);
    cfg.wave_spread = 1.05f;
    cfg.wave_decay = 1.05f;
    return cfg;
}

/** Dedicated High stack layer — note sparkle (ColorChord hue). */
inline AudioReactiveSettings3D MakeDefaultHighSparkleAudioReactiveSettings3D()
{
    AudioReactiveSettings3D cfg = MakeDefaultLevelAudioReactiveSettings3D();
    cfg.register_role = static_cast<int>(AudioRegisterRole::High);
    ApplyAudioRegisterRole(cfg);
    cfg.smoothing = 0.72f;
    cfg.peak_boost = 1.55f;
    cfg.falloff = 0.88f;
    cfg.pulse_color_mode = static_cast<int>(AudioPulseColorMode::FollowNotes);
    return cfg;
}

struct AudioPulseTriggerState
{
    float onset_smoothed = 0.0f;
    float onset_hold = 0.0f;
    bool beat_armed = true;
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
    if(cfg.peak_boost < 0.25f)
    {
        cfg.peak_boost = 0.25f;
    }
    if(cfg.peak_boost > 5.0f)
    {
        cfg.peak_boost = 5.0f;
    }
    if(cfg.drive_mode != static_cast<int>(AudioDriveMode::Sustained)
       && cfg.drive_mode != static_cast<int>(AudioDriveMode::Beat))
    {
        cfg.drive_mode = static_cast<int>(AudioDriveMode::Sustained);
    }
    if(cfg.sustain_reject < 0.0f)
    {
        cfg.sustain_reject = 0.0f;
    }
    if(cfg.sustain_reject > 1.0f)
    {
        cfg.sustain_reject = 1.0f;
    }
    if(cfg.register_role < static_cast<int>(AudioRegisterRole::Low)
       || cfg.register_role > static_cast<int>(AudioRegisterRole::Mixed))
    {
        cfg.register_role = static_cast<int>(AudioRegisterRole::Mixed);
    }
    if(cfg.pulse_color_mode != static_cast<int>(AudioPulseColorMode::PerBeatCycle)
       && cfg.pulse_color_mode != static_cast<int>(AudioPulseColorMode::Uniform)
       && cfg.pulse_color_mode != static_cast<int>(AudioPulseColorMode::SpatialAlongRing)
       && cfg.pulse_color_mode != static_cast<int>(AudioPulseColorMode::FollowNotes))
    {
        cfg.pulse_color_mode = static_cast<int>(AudioPulseColorMode::SpatialAlongRing);
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
}

inline void AudioReactiveSaveToJson(nlohmann::json& j, const AudioReactiveSettings3D& cfg)
{
    j["low_hz"] = cfg.low_hz;
    j["high_hz"] = cfg.high_hz;
    j["smoothing"] = cfg.smoothing;
    j["falloff"] = cfg.falloff;
    j["peak_boost"] = cfg.peak_boost;
    j["drive_mode"] = cfg.drive_mode;
    j["sustain_reject"] = cfg.sustain_reject;
    j["register_role"] = cfg.register_role;
    j["pulse_color_mode"] = cfg.pulse_color_mode;
    j["beat_wave_mode"] = cfg.beat_wave_mode;
    j["wave_spread"] = cfg.wave_spread;
    j["wave_decay"] = cfg.wave_decay;
}

inline void AudioReactiveLoadFromJson(AudioReactiveSettings3D& cfg, const nlohmann::json& settings)
{
    if(settings.contains("low_hz"))
        cfg.low_hz = settings["low_hz"].get<int>();
    if(settings.contains("high_hz"))
        cfg.high_hz = settings["high_hz"].get<int>();
    if(settings.contains("smoothing"))
        cfg.smoothing = settings["smoothing"].get<float>();
    if(settings.contains("falloff"))
        cfg.falloff = settings["falloff"].get<float>();
    if(settings.contains("peak_boost"))
        cfg.peak_boost = settings["peak_boost"].get<float>();
    if(settings.contains("drive_mode"))
        cfg.drive_mode = settings["drive_mode"].get<int>();
    if(settings.contains("sustain_reject"))
        cfg.sustain_reject = settings["sustain_reject"].get<float>();
    if(settings.contains("register_role"))
        cfg.register_role = settings["register_role"].get<int>();
    if(settings.contains("pulse_color_mode"))
        cfg.pulse_color_mode = settings["pulse_color_mode"].get<int>();
    if(settings.contains("beat_wave_mode"))
        cfg.beat_wave_mode = settings["beat_wave_mode"].get<int>();
    if(settings.contains("wave_spread"))
        cfg.wave_spread = settings["wave_spread"].get<float>();
    if(settings.contains("wave_decay"))
        cfg.wave_decay = settings["wave_decay"].get<float>();
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

inline float ApplyAudioIntensity(float value, const AudioReactiveSettings3D& cfg)
{
    /* Wider effective gain so Effect sensitivity / Feel is obvious on LEDs. */
    const float boost = std::clamp(cfg.peak_boost, 0.25f, 4.0f);
    float boosted = std::clamp(value * boost, 0.0f, 1.0f);
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
    const float low = (float)cfg.low_hz;
    const float high = (float)cfg.high_hz;
    const float sustain = audio->getBandSlowEnergyHz(low, high);
    const AudioDriveMode mode = static_cast<AudioDriveMode>(cfg.drive_mode);
    const AudioRegisterRole role = static_cast<AudioRegisterRole>(cfg.register_role);

    float level = 0.0f;
    switch(mode)
    {
    case AudioDriveMode::Beat:
    {
        const float trans = audio->getBandTransientEnergyHz(low, high);
        const float reject = std::clamp(cfg.sustain_reject, 0.0f, 1.0f) * sustain;
        level = std::max(0.0f, trans - reject);
        break;
    }
    case AudioDriveMode::Sustained:
    default:
        level = sustain;
        break;
    }

    /* Notes tint motion; Hz band stays primary so Low/High Hz sliders actually matter. */
    if(role == AudioRegisterRole::High)
    {
        const float note = audio->getNoteDrive01();
        const float note_band = audio->getNoteEnergyInHz(low, high);
        const float note_mix = std::max(note, note_band);
        level = std::min(1.0f, 0.62f * level + 0.38f * note_mix);
    }
    else if(role == AudioRegisterRole::Mid)
    {
        const float note_band = audio->getNoteEnergyInHz(low, high);
        const float note_drive = audio->getNoteDrive01();
        const float vocal = std::max(note_band, note_drive * 0.88f);
        level = std::min(1.0f, 0.58f * level + 0.42f * vocal);
    }
    else if(role == AudioRegisterRole::Mixed)
    {
        const float note = audio->getNoteDrive01();
        level = std::min(1.0f, 0.85f * level + 0.15f * note);
    }
    return std::clamp(level, 0.0f, 1.0f);
}

inline float SampleAudioOnsetLevel(const AudioReactiveSettings3D& cfg)
{
    AudioInputManager* audio = AudioInputManager::instance();
    if(!audio)
    {
        return 0.0f;
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
    const AudioRegisterRole role = static_cast<AudioRegisterRole>(cfg.register_role);
    const AudioDriveMode mode = static_cast<AudioDriveMode>(cfg.drive_mode);

    /* Anti-flash: Mid/High/Sustained never get global-onset pumped in.
       Low beat/transient may take a small onset lift only. */
    if(role == AudioRegisterRole::Low && mode == AudioDriveMode::Beat)
    {
        const float onset = SampleAudioOnsetLevel(cfg);
        level = std::max(level, onset * 0.22f);
    }
    return std::clamp(level, 0.0f, 1.0f);
}

inline float ApplyAudioVisualIntensity(float value, const AudioReactiveSettings3D& cfg)
{
    const AudioRegisterRole role = static_cast<AudioRegisterRole>(cfg.register_role);
    float shaped = ApplyAudioIntensity(value, cfg);
    if(role == AudioRegisterRole::Low)
        return std::clamp(std::sqrt(shaped), 0.0f, 1.0f);
    return std::clamp(shaped, 0.0f, 1.0f);
}

inline float AudioReactiveBeatPulseHoldSec()
{
    return 0.20f;
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
    const float shaped_drive = ApplyAudioVisualIntensity(std::clamp(drive, 0.0f, 1.0f), cfg);
    const float shaped_onset =
        ApplyAudioVisualIntensity(std::clamp(state.onset_smoothed, 0.0f, 1.0f), cfg);

    const AudioDriveMode mode = static_cast<AudioDriveMode>(cfg.drive_mode);
    const bool allow_drive_only = (mode == AudioDriveMode::Beat);

    const bool onset_hit =
        state.beat_armed
        && (state.onset_smoothed >= onset_threshold || onset_raw >= onset_threshold * 1.08f);

    const float drive_trigger =
        (mode == AudioDriveMode::Beat) ? 0.16f : 0.22f;
    const bool drive_hit = shaped_drive >= drive_trigger;
    if(!onset_hit && !(allow_drive_only && drive_hit))
    {
        return false;
    }

    const float shaped_raw_onset =
        ApplyAudioVisualIntensity(std::clamp(onset_raw, 0.0f, 1.0f), cfg);
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
