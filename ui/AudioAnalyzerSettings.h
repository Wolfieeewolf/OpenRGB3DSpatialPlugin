// SPDX-License-Identifier: GPL-2.0-only

#ifndef AUDIOANALYZERSETTINGS_H
#define AUDIOANALYZERSETTINGS_H

#include <nlohmann/json.hpp>

class AudioAnalyzerSettings
{
public:
    static void applyFromPluginSettings(const nlohmann::json& settings);
    static void writeToPluginSettings(nlohmann::json& settings);
    static void resetToFactoryDefaults();

    /** Clamp a saved gain JSON value to [0, 2]. */
    static float jsonToGain(const nlohmann::json& v);

    /** Map a saved EQ gain array (any length) onto the live band index. */
    static float resampleSavedEqGain(const nlohmann::json& arr, int band_index, int bands_count);
};

#endif
