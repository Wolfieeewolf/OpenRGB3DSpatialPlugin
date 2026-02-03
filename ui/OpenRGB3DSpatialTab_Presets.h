// SPDX-License-Identifier: GPL-2.0-only

#ifndef OPENRGB3DSPATIALTAB_PRESETS_H
#define OPENRGB3DSPATIALTAB_PRESETS_H

#include <cstddef>

/** Embedded default monitor presets (JSON array). Written to monitors folder when missing. */
extern const char* kDefaultMonitorPresetJson;

struct DefaultControllerPreset
{
    const char* filename;
    const char* content;
};

/** Embedded default controller presets. Written to controller_presets folder when missing. */
extern const DefaultControllerPreset kDefaultControllerPresets[];
extern const size_t kDefaultControllerPresetCount;

#endif
