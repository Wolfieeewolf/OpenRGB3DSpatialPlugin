// SPDX-License-Identifier: GPL-2.0-only

#include "ReactiveKeyMap.h"

namespace ReactiveKeyMap
{
namespace
{
/* Match OpenRGB RGBControllerKeyNames.cpp LED labels. Do not log these. */

const char* kMouseLeftAliases[] = {
    "Left Click", "Mouse Left", "LMB", "Button 1", nullptr
};
const char* kMouseRightAliases[] = {
    "Right Click", "Mouse Right", "RMB", "Button 2", nullptr
};
const char* kMouseMiddleAliases[] = {
    "Middle Click", "Scroll Wheel", "Mouse Wheel", "Wheel", "Button 3", nullptr
};
const char* kMouseX1Aliases[] = {
    "Back", "X1", "Side", nullptr
};
const char* kMouseX2Aliases[] = {
    "Forward", "X2", "Side", nullptr
};

const char* kPadA[] = { "A", "Button A", "A Button", "Cross", "South", nullptr };
const char* kPadB[] = { "B", "Button B", "B Button", "Circle", "East", nullptr };
const char* kPadX[] = { "X", "Button X", "X Button", "Square", "West", nullptr };
const char* kPadY[] = { "Y", "Button Y", "Y Button", "Triangle", "North", nullptr };
const char* kPadLb[] = { "LB", "Left Bumper", "Left Shoulder", "L1", "Bumper Left", nullptr };
const char* kPadRb[] = { "RB", "Right Bumper", "Right Shoulder", "R1", "Bumper Right", nullptr };
const char* kPadStart[] = { "Start", "Menu", "Options", nullptr };
const char* kPadBack[] = { "Back", "Select", "View", "Share", nullptr };
const char* kPadLs[] = { "Left Stick", "L3", "LS", nullptr };
const char* kPadRs[] = { "Right Stick", "R3", "RS", nullptr };
const char* kPadUp[] = { "D-Pad Up", "Up", "DPAD Up", nullptr };
const char* kPadDown[] = { "D-Pad Down", "Down", "DPAD Down", nullptr };
const char* kPadLeft[] = { "D-Pad Left", "Left", "DPAD Left", nullptr };
const char* kPadRight[] = { "D-Pad Right", "Right", "DPAD Right", nullptr };
const char* kPadLt[] = { "LT", "Left Trigger", "L2", nullptr };
const char* kPadRt[] = { "RT", "Right Trigger", "R2", nullptr };
} // namespace

const char* KeyboardNameForVk(unsigned int vk)
{
    switch(vk)
    {
    case 0x08: return "Key: Backspace";
    case 0x09: return "Key: Tab";
    case 0x0D: return "Key: Enter";
    case 0x13: return "Key: Pause/Break";
    case 0x14: return "Key: Caps Lock";
    case 0x1B: return "Key: Escape";
    case 0x20: return "Key: Space";
    case 0x21: return "Key: Page Up";
    case 0x22: return "Key: Page Down";
    case 0x23: return "Key: End";
    case 0x24: return "Key: Home";
    case 0x25: return "Key: Left Arrow";
    case 0x26: return "Key: Up Arrow";
    case 0x27: return "Key: Right Arrow";
    case 0x28: return "Key: Down Arrow";
    case 0x2C: return "Key: Print Screen";
    case 0x2D: return "Key: Insert";
    case 0x2E: return "Key: Delete";
    case 0x30: return "Key: 0";
    case 0x31: return "Key: 1";
    case 0x32: return "Key: 2";
    case 0x33: return "Key: 3";
    case 0x34: return "Key: 4";
    case 0x35: return "Key: 5";
    case 0x36: return "Key: 6";
    case 0x37: return "Key: 7";
    case 0x38: return "Key: 8";
    case 0x39: return "Key: 9";
    case 0x41: return "Key: A";
    case 0x42: return "Key: B";
    case 0x43: return "Key: C";
    case 0x44: return "Key: D";
    case 0x45: return "Key: E";
    case 0x46: return "Key: F";
    case 0x47: return "Key: G";
    case 0x48: return "Key: H";
    case 0x49: return "Key: I";
    case 0x4A: return "Key: J";
    case 0x4B: return "Key: K";
    case 0x4C: return "Key: L";
    case 0x4D: return "Key: M";
    case 0x4E: return "Key: N";
    case 0x4F: return "Key: O";
    case 0x50: return "Key: P";
    case 0x51: return "Key: Q";
    case 0x52: return "Key: R";
    case 0x53: return "Key: S";
    case 0x54: return "Key: T";
    case 0x55: return "Key: U";
    case 0x56: return "Key: V";
    case 0x57: return "Key: W";
    case 0x58: return "Key: X";
    case 0x59: return "Key: Y";
    case 0x5A: return "Key: Z";
    case 0x5B: return "Key: Left Windows";
    case 0x5C: return "Key: Right Windows";
    case 0x5D: return "Key: Menu";
    case 0x60: return "Key: Number Pad 0";
    case 0x61: return "Key: Number Pad 1";
    case 0x62: return "Key: Number Pad 2";
    case 0x63: return "Key: Number Pad 3";
    case 0x64: return "Key: Number Pad 4";
    case 0x65: return "Key: Number Pad 5";
    case 0x66: return "Key: Number Pad 6";
    case 0x67: return "Key: Number Pad 7";
    case 0x68: return "Key: Number Pad 8";
    case 0x69: return "Key: Number Pad 9";
    case 0x6A: return "Key: Number Pad *";
    case 0x6B: return "Key: Number Pad +";
    case 0x6D: return "Key: Number Pad -";
    case 0x6E: return "Key: Number Pad .";
    case 0x6F: return "Key: Number Pad /";
    case 0x70: return "Key: F1";
    case 0x71: return "Key: F2";
    case 0x72: return "Key: F3";
    case 0x73: return "Key: F4";
    case 0x74: return "Key: F5";
    case 0x75: return "Key: F6";
    case 0x76: return "Key: F7";
    case 0x77: return "Key: F8";
    case 0x78: return "Key: F9";
    case 0x79: return "Key: F10";
    case 0x7A: return "Key: F11";
    case 0x7B: return "Key: F12";
    case 0x7C: return "Key: F13";
    case 0x7D: return "Key: F14";
    case 0x7E: return "Key: F15";
    case 0x7F: return "Key: F16";
    case 0x80: return "Key: F17";
    case 0x81: return "Key: F18";
    case 0x82: return "Key: F19";
    case 0x83: return "Key: F20";
    case 0x84: return "Key: F21";
    case 0x85: return "Key: F22";
    case 0x86: return "Key: F23";
    case 0x87: return "Key: F24";
    case 0x90: return "Key: Num Lock";
    case 0x91: return "Key: Scroll Lock";
    case 0xA0: return "Key: Left Shift";
    case 0xA1: return "Key: Right Shift";
    case 0xA2: return "Key: Left Control";
    case 0xA3: return "Key: Right Control";
    case 0xA4: return "Key: Left Alt";
    case 0xA5: return "Key: Right Alt";
    case 0xBA: return "Key: ;";
    case 0xBB: return "Key: =";
    case 0xBC: return "Key: ,";
    case 0xBD: return "Key: -";
    case 0xBE: return "Key: .";
    case 0xBF: return "Key: /";
    case 0xC0: return "Key: `";
    case 0xDB: return "Key: [";
    case 0xDC: return "Key: \\";
    case 0xDD: return "Key: ]";
    case 0xDE: return "Key: '";
    default:   return nullptr;
    }
}

const char* KeypadPaddedNameForVk(unsigned int vk)
{
    switch(vk)
    {
    case 0x30: return "Key: 00";
    case 0x31: return "Key: 01";
    case 0x32: return "Key: 02";
    case 0x33: return "Key: 03";
    case 0x34: return "Key: 04";
    case 0x35: return "Key: 05";
    case 0x36: return "Key: 06";
    case 0x37: return "Key: 07";
    case 0x38: return "Key: 08";
    case 0x39: return "Key: 09";
    case 0x60: return "Key: 00";
    case 0x61: return "Key: 01";
    case 0x62: return "Key: 02";
    case 0x63: return "Key: 03";
    case 0x64: return "Key: 04";
    case 0x65: return "Key: 05";
    case 0x66: return "Key: 06";
    case 0x67: return "Key: 07";
    case 0x68: return "Key: 08";
    case 0x69: return "Key: 09";
    case 0x70: return "Key: 01";
    case 0x71: return "Key: 02";
    case 0x72: return "Key: 03";
    case 0x73: return "Key: 04";
    case 0x74: return "Key: 05";
    case 0x75: return "Key: 06";
    case 0x76: return "Key: 07";
    case 0x77: return "Key: 08";
    case 0x78: return "Key: 09";
    case 0x79: return "Key: 10";
    case 0x7A: return "Key: 11";
    case 0x7B: return "Key: 12";
    case 0x7C: return "Key: 13";
    case 0x7D: return "Key: 14";
    case 0x7E: return "Key: 15";
    case 0x7F: return "Key: 16";
    case 0x80: return "Key: 17";
    case 0x81: return "Key: 18";
    case 0x82: return "Key: 19";
    case 0x83: return "Key: 20";
    default:   return nullptr;
    }
}

int FillKeyboardLedNames(unsigned int vk,
                         unsigned int make_code,
                         bool extended,
                         bool pause_prefix,
                         const char** out,
                         int cap)
{
    if(!out || cap <= 0)
    {
        return 0;
    }
    int n = 0;
    auto add = [&](const char* name) {
        if(!name || n >= cap)
        {
            return;
        }
        for(int i = 0; i < n; ++i)
        {
            if(out[i] == name)
            {
                return;
            }
        }
        out[n++] = name;
    };

    /* Scan code + E0 is the physical key. VKey is the OS mapping and can alias twins. */
    const unsigned int make = make_code & 0xFFu;
    if(pause_prefix)
    {
        add("Key: Pause/Break");
    }
    else if(make != 0)
    {
        switch(make)
        {
        case 0x01: add("Key: Escape"); break;
        case 0x0E: add("Key: Backspace"); break;
        case 0x0F: add("Key: Tab"); break;
        case 0x1C: add(extended ? "Key: Number Pad Enter" : "Key: Enter"); break;
        case 0x1D: add(extended ? "Key: Right Control" : "Key: Left Control"); break;
        case 0x2A: add("Key: Left Shift"); break;
        case 0x36: add("Key: Right Shift"); break;
        case 0x37:
            if(extended)
            {
                add("Key: Print Screen");
                add("Key: PrtSc");
                add("Key: SysRq");
            }
            else
            {
                add("Key: Number Pad *");
            }
            break;
        case 0x38: add(extended ? "Key: Right Alt" : "Key: Left Alt"); break;
        case 0x39: add("Key: Space"); break;
        case 0x3A: add("Key: Caps Lock"); break;
        case 0x3B: add("Key: F1"); break;
        case 0x3C: add("Key: F2"); break;
        case 0x3D: add("Key: F3"); break;
        case 0x3E: add("Key: F4"); break;
        case 0x3F: add("Key: F5"); break;
        case 0x40: add("Key: F6"); break;
        case 0x41: add("Key: F7"); break;
        case 0x42: add("Key: F8"); break;
        case 0x43: add("Key: F9"); break;
        case 0x44: add("Key: F10"); break;
        case 0x45: add(extended ? "Key: Pause/Break" : "Key: Num Lock"); break;
        case 0x46: add(extended ? "Key: Pause/Break" : "Key: Scroll Lock"); break;
        case 0x47: add(extended ? "Key: Home" : "Key: Number Pad 7"); break;
        case 0x48: add(extended ? "Key: Up Arrow" : "Key: Number Pad 8"); break;
        case 0x49: add(extended ? "Key: Page Up" : "Key: Number Pad 9"); break;
        case 0x4A: add("Key: Number Pad -"); break;
        case 0x4B: add(extended ? "Key: Left Arrow" : "Key: Number Pad 4"); break;
        case 0x4C: add("Key: Number Pad 5"); break;
        case 0x4D: add(extended ? "Key: Right Arrow" : "Key: Number Pad 6"); break;
        case 0x4E: add("Key: Number Pad +"); break;
        case 0x4F: add(extended ? "Key: End" : "Key: Number Pad 1"); break;
        case 0x50: add(extended ? "Key: Down Arrow" : "Key: Number Pad 2"); break;
        case 0x51: add(extended ? "Key: Page Down" : "Key: Number Pad 3"); break;
        case 0x52: add(extended ? "Key: Insert" : "Key: Number Pad 0"); break;
        case 0x53: add(extended ? "Key: Delete" : "Key: Number Pad ."); break;
        case 0x35: add(extended ? "Key: Number Pad /" : "Key: /"); break;
        case 0x57: add("Key: F11"); break;
        case 0x58: add("Key: F12"); break;
        case 0x5B: add("Key: Left Windows"); break;
        case 0x5C: add("Key: Right Windows"); break;
        case 0x5D: add("Key: Menu"); break;
        default: break;
        }
    }

    const int from_scan = n;
    if(from_scan == 0)
    {
        add(KeyboardNameForVk(vk));
        switch(vk)
        {
        case 0x10: add("Key: Left Shift"); add("Key: Right Shift"); break;
        case 0x11: add("Key: Left Control"); add("Key: Right Control"); break;
        case 0x12: add("Key: Left Alt"); add("Key: Right Alt"); break;
        default: break;
        }
    }
    add(KeypadPaddedNameForVk(vk));
    if(vk == 0x2C)
    {
        add("Key: Print Screen");
        add("Key: PrtSc");
        add("Key: SysRq");
    }
    return n;
}

const char* MousePrimaryName(unsigned int button)
{
    switch(button)
    {
    case 1: return "Left Click";
    case 2: return "Right Click";
    case 3: return "Middle Click";
    case 4: return "Back";
    case 5: return "Forward";
    default: return nullptr;
    }
}

const char* const* MouseAliasNames(unsigned int button)
{
    switch(button)
    {
    case 1: return kMouseLeftAliases;
    case 2: return kMouseRightAliases;
    case 3: return kMouseMiddleAliases;
    case 4: return kMouseX1Aliases;
    case 5: return kMouseX2Aliases;
    default: return nullptr;
    }
}

const char* const* GamepadAliasNames(std::uint16_t xinput_button)
{
    switch(xinput_button)
    {
    case 0x1000: return kPadA;
    case 0x2000: return kPadB;
    case 0x4000: return kPadX;
    case 0x8000: return kPadY;
    case 0x0100: return kPadLb;
    case 0x0200: return kPadRb;
    case 0x0010: return kPadStart;
    case 0x0020: return kPadBack;
    case 0x0040: return kPadLs;
    case 0x0080: return kPadRs;
    case 0x0001: return kPadUp;
    case 0x0002: return kPadDown;
    case 0x0004: return kPadLeft;
    case 0x0008: return kPadRight;
    case 0x0400: return kPadLt; /* synthetic; not an XInput wButtons bit */
    case 0x0800: return kPadRt;
    default:     return nullptr;
    }
}

} // namespace ReactiveKeyMap
