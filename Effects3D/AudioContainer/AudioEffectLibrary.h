// SPDX-License-Identifier: GPL-2.0-only

#ifndef AUDIOEFFECTLIBRARY_H
#define AUDIOEFFECTLIBRARY_H

#include <string>

namespace AudioEffectLibrary
{

/** Unified audio layer: frequency ranges and per-range sub-effects live here. */
inline const char* HubClassName()
{
    return "AudioContainer";
}

}

#endif
