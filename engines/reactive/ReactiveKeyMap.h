// SPDX-License-Identifier: GPL-2.0-only

#ifndef REACTIVEKEYMAP_H
#define REACTIVEKEYMAP_H

#include <cstdint>

/** Windows virtual-key / XInput bit → OpenRGB LED name ("Key: F"). No logging. */
namespace ReactiveKeyMap
{

const char* KeyboardNameForVk(unsigned int vk);
const char* KeypadPaddedNameForVk(unsigned int vk);
int FillKeyboardLedNames(unsigned int vk,
                         unsigned int make_code,
                         bool extended,
                         bool pause_prefix,
                         const char** out,
                         int cap);
const char* MousePrimaryName(unsigned int button);
const char* const* MouseAliasNames(unsigned int button);
const char* const* GamepadAliasNames(std::uint16_t xinput_button);

}

#endif
