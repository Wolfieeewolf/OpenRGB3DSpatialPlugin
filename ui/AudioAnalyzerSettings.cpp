// SPDX-License-Identifier: GPL-2.0-only

#include "AudioAnalyzerSettings.h"
#include "Audio/AudioInputManager.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr int kSmoothingDefault = 80;
constexpr int kDefaultBands = 16;
constexpr int kDefaultFft = 512;

int pctFromDecay(float coeff, float lo, float hi)
{
    if(hi <= lo + 1e-6f)
        return 0;
    const float t = std::clamp((coeff - lo) / (hi - lo), 0.0f, 1.0f);
    return static_cast<int>(std::lround(t * 100.0f));
}

float decayFromPct(int pct, float lo, float hi)
{
    const float t = std::clamp(pct / 100.0f, 0.0f, 1.0f);
    return lo + t * (hi - lo);
}
}

float AudioAnalyzerSettings::jsonToGain(const nlohmann::json& v)
{
    if(v.is_number_float() || v.is_number_integer() || v.is_number_unsigned())
        return std::clamp(v.get<float>(), 0.0f, 2.0f);
    return 1.0f;
}

float AudioAnalyzerSettings::resampleSavedEqGain(const nlohmann::json& arr, int band_index, int bands_count)
{
    if(!arr.is_array() || arr.empty() || bands_count <= 0 || band_index < 0 || band_index >= bands_count)
        return 1.0f;
    const int n = static_cast<int>(arr.size());
    if(n == bands_count)
        return jsonToGain(arr[band_index]);
    const float t = ((float)band_index + 0.5f) / (float)bands_count;
    const float ri = t * (float)n - 0.5f;
    const int i0 = std::clamp((int)std::floor(ri), 0, n - 1);
    const int i1 = std::min(i0 + 1, n - 1);
    const float frac = std::clamp(ri - (float)i0, 0.0f, 1.0f);
    const float g0 = jsonToGain(arr[i0]);
    const float g1 = jsonToGain(arr[i1]);
    return std::clamp(g0 * (1.0f - frac) + g1 * frac, 0.0f, 2.0f);
}

void AudioAnalyzerSettings::resetToFactoryDefaults()
{
    AudioInputManager* audio = AudioInputManager::instance();
    if(!audio)
        return;
    audio->resetAnalyzerTuning();
    audio->setSmoothing(decayFromPct(kSmoothingDefault, 0.0f, 0.99f));
    audio->setAutoLevelEnabled(true);
    audio->setBandsCount(kDefaultBands);
    audio->setFFTSize(kDefaultFft);
}

void AudioAnalyzerSettings::applyFromPluginSettings(const nlohmann::json& settings)
{
    AudioInputManager* audio = AudioInputManager::instance();
    if(!audio)
        return;
    if(settings.contains("AudioSmoothingPct"))
        audio->setSmoothing(decayFromPct(settings["AudioSmoothingPct"].get<int>(), 0.0f, 0.99f));
}

void AudioAnalyzerSettings::writeToPluginSettings(nlohmann::json& settings)
{
    AudioInputManager* audio = AudioInputManager::instance();
    if(!audio)
        return;
    settings["AudioSmoothingPct"] = pctFromDecay(audio->getSmoothing(), 0.0f, 0.99f);
}
