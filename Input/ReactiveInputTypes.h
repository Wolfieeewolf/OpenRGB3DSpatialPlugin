// SPDX-License-Identifier: GPL-2.0-only

#ifndef REACTIVEINPUTTYPES_H
#define REACTIVEINPUTTYPES_H

#include "LEDPosition3D.h"
#include "RGBControllerInterface.h"

#include <cctype>
#include <cstdint>
#include <string>
#include <vector>

enum class ReactiveSourceKind : std::uint8_t
{
    Keyboard = 0,
    Mouse    = 1,
    Gamepad  = 2,
};

struct ReactiveLedSample
{
    device_type     type = DEVICE_TYPE_UNKNOWN;
    std::string     name;
    Vector3D        room_position{};
    Vector3D        local_position{};
    std::uintptr_t  device_id = 0;
    std::uint16_t   vid = 0;
    std::uint16_t   pid = 0;
};

inline bool ParseHidVidPid(const std::string& text, std::uint16_t* vid, std::uint16_t* pid)
{
    if(!vid || !pid)
    {
        return false;
    }
    *vid = 0;
    *pid = 0;
    auto read_tag = [&text](const char* tag) -> std::uint16_t {
        const std::size_t tag_len = std::char_traits<char>::length(tag);
        for(std::size_t i = 0; i + tag_len + 4 <= text.size(); ++i)
        {
            bool match = true;
            for(std::size_t t = 0; t < tag_len; ++t)
            {
                const char have = static_cast<char>(std::tolower(static_cast<unsigned char>(text[i + t])));
                if(have != tag[t])
                {
                    match = false;
                    break;
                }
            }
            if(!match)
            {
                continue;
            }
            unsigned int value = 0;
            for(int d = 0; d < 4; ++d)
            {
                const char c = static_cast<char>(std::tolower(static_cast<unsigned char>(text[i + tag_len + d])));
                unsigned int nibble = 0;
                if(c >= '0' && c <= '9')
                {
                    nibble = static_cast<unsigned int>(c - '0');
                }
                else if(c >= 'a' && c <= 'f')
                {
                    nibble = static_cast<unsigned int>(c - 'a' + 10);
                }
                else
                {
                    value = 0;
                    break;
                }
                value = (value << 4) | nibble;
            }
            if(value != 0)
            {
                return static_cast<std::uint16_t>(value);
            }
        }
        return 0;
    };
    *vid = read_tag("vid_");
    *pid = read_tag("pid_");
    return *vid != 0 && *pid != 0;
}

/** Origin for lighting only. Does not identify the key or button. */
struct ReactiveOriginEvent
{
    Vector3D          room_position{};
    ReactiveSourceKind kind = ReactiveSourceKind::Keyboard;
    bool              down  = true;
    float             device_spread = 0.0f;
    float             led_spacing = 0.0f;
};

struct ReactiveHeldOrigin
{
    Vector3D          room_position{};
    ReactiveSourceKind kind = ReactiveSourceKind::Keyboard;
    float             device_spread = 0.0f;
    float             led_spacing = 0.0f;
};

#endif
