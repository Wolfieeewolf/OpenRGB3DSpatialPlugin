// SPDX-License-Identifier: GPL-2.0-only

#include "EffectPack.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>

namespace EffectPack
{
namespace
{

std::string LoopToString(LoopMode m)
{
    switch(m)
    {
        case LoopMode::Once: return "once";
        case LoopMode::Forever: return "forever";
        case LoopMode::WhileActive: return "while_active";
        default:
        {
            const LoopMode unused = m;
            (void)unused;
            return "once";
        }
    }
}

bool LoopFromString(const std::string& s, LoopMode* out)
{
    if(s == "once")
    {
        *out = LoopMode::Once;
        return true;
    }
    if(s == "forever")
    {
        *out = LoopMode::Forever;
        return true;
    }
    if(s == "while_active")
    {
        *out = LoopMode::WhileActive;
        return true;
    }
    return false;
}

std::string TargetKindToString(TargetKind k)
{
    switch(k)
    {
        case TargetKind::All: return "all";
        case TargetKind::Device: return "device";
        case TargetKind::Zone: return "zone";
        case TargetKind::Leds: return "leds";
        default:
        {
            const TargetKind unused = k;
            (void)unused;
            return "all";
        }
    }
}

bool TargetKindFromString(const std::string& s, TargetKind* out)
{
    if(s == "all")
    {
        *out = TargetKind::All;
        return true;
    }
    if(s == "device")
    {
        *out = TargetKind::Device;
        return true;
    }
    if(s == "zone")
    {
        *out = TargetKind::Zone;
        return true;
    }
    if(s == "leds")
    {
        *out = TargetKind::Leds;
        return true;
    }
    return false;
}

std::string BlockTypeToString(BlockType t)
{
    switch(t)
    {
        case BlockType::Solid: return "solid";
        case BlockType::Fade: return "fade";
        case BlockType::Pulse: return "pulse";
        default:
        {
            const BlockType unused = t;
            (void)unused;
            return "solid";
        }
    }
}

bool BlockTypeFromString(const std::string& s, BlockType* out)
{
    if(s == "solid")
    {
        *out = BlockType::Solid;
        return true;
    }
    if(s == "fade")
    {
        *out = BlockType::Fade;
        return true;
    }
    if(s == "pulse")
    {
        *out = BlockType::Pulse;
        return true;
    }
    return false;
}

std::string ColorToHex(RGBColor c)
{
    char buf[8];
    std::snprintf(buf, sizeof(buf), "#%02X%02X%02X",
                  RGBGetRValue(c), RGBGetGValue(c), RGBGetBValue(c));
    return std::string(buf);
}

bool ColorFromHex(const std::string& s, RGBColor* out)
{
    if(!out)
    {
        return false;
    }
    std::string hex = s;
    if(!hex.empty() && hex[0] == '#')
    {
        hex = hex.substr(1);
    }
    if(hex.size() != 6)
    {
        return false;
    }
    unsigned int value = 0;
    if(std::sscanf(hex.c_str(), "%06x", &value) != 1)
    {
        return false;
    }
    const int r = (int)((value >> 16) & 0xFF);
    const int g = (int)((value >> 8) & 0xFF);
    const int b = (int)(value & 0xFF);
    *out = ToRGBColor(r, g, b);
    return true;
}

RGBColor ScaleIntensity(RGBColor c, float intensity)
{
    intensity = std::clamp(intensity, 0.0f, 1.0f);
    return ToRGBColor(
        (int)std::lround(RGBGetRValue(c) * intensity),
        (int)std::lround(RGBGetGValue(c) * intensity),
        (int)std::lround(RGBGetBValue(c) * intensity));
}

RGBColor LerpColor(RGBColor a, RGBColor b, float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    const float u = 1.0f - t;
    return ToRGBColor(
        (int)std::lround(RGBGetRValue(a) * u + RGBGetRValue(b) * t),
        (int)std::lround(RGBGetGValue(a) * u + RGBGetGValue(b) * t),
        (int)std::lround(RGBGetBValue(a) * u + RGBGetBValue(b) * t));
}

bool EvaluateBlock(const Block& block, int local_ms, RGBColor* out_color, float* out_intensity)
{
    if(local_ms < block.start_ms || local_ms >= block.end_ms || block.end_ms <= block.start_ms)
    {
        return false;
    }

    float intensity = std::clamp(block.intensity, 0.0f, 1.0f);
    RGBColor color = block.color;

    switch(block.type)
    {
        case BlockType::Solid:
            break;
        case BlockType::Fade:
        {
            const float t = (float)(local_ms - block.start_ms) / (float)(block.end_ms - block.start_ms);
            color = LerpColor(block.color_from, block.color_to, t);
            break;
        }
        case BlockType::Pulse:
        {
            const int period = std::max(1, block.period_ms);
            const float phase = (float)((local_ms - block.start_ms) % period) / (float)period;
            const float wave = 0.5f - 0.5f * std::cos(phase * 6.28318530718f);
            const float lo = std::clamp(block.min_intensity, 0.0f, 1.0f);
            const float hi = std::clamp(block.max_intensity, 0.0f, 1.0f);
            intensity *= lo + (hi - lo) * wave;
            color = block.color;
            break;
        }
        default:
        {
            const BlockType unused = block.type;
            (void)unused;
            break;
        }
    }

    if(out_color)
    {
        *out_color = ScaleIntensity(color, intensity);
    }
    if(out_intensity)
    {
        *out_intensity = intensity;
    }
    return true;
}

} // namespace

bool MapPlaybackTime(const Pack& pack, int elapsed_ms, bool event_active, int* out_local_ms)
{
    if(!out_local_ms || pack.duration_ms <= 0)
    {
        return false;
    }
    if(elapsed_ms < 0)
    {
        elapsed_ms = 0;
    }

    switch(pack.loop)
    {
        case LoopMode::Once:
            if(elapsed_ms >= pack.duration_ms)
            {
                return false;
            }
            *out_local_ms = elapsed_ms;
            return true;
        case LoopMode::Forever:
            *out_local_ms = elapsed_ms % pack.duration_ms;
            return true;
        case LoopMode::WhileActive:
            if(!event_active)
            {
                return false;
            }
            *out_local_ms = elapsed_ms % pack.duration_ms;
            return true;
        default:
        {
            const LoopMode unused = pack.loop;
            (void)unused;
            return false;
        }
    }
}

bool EvaluateTrackColor(const Track& track, int local_ms, RGBColor* out_color, float* out_intensity)
{
    // Last overlapping block wins (later timeline edits override).
    bool found = false;
    RGBColor color = ToRGBColor(0, 0, 0);
    float intensity = 0.0f;
    for(const Block& block : track.blocks)
    {
        RGBColor c;
        float i = 0.0f;
        if(EvaluateBlock(block, local_ms, &c, &i))
        {
            color = c;
            intensity = i;
            found = true;
        }
    }
    if(!found)
    {
        return false;
    }
    if(out_color)
    {
        *out_color = color;
    }
    if(out_intensity)
    {
        *out_intensity = intensity;
    }
    return true;
}

nlohmann::json ToJson(const Pack& pack)
{
    nlohmann::json j;
    j["format"] = kFormatId;
    j["version"] = kFormatVersion;
    j["id"] = pack.id;
    j["name"] = pack.name;
    j["duration_ms"] = pack.duration_ms;
    j["loop"] = LoopToString(pack.loop);
    j["priority"] = pack.priority;
    j["tracks"] = nlohmann::json::array();
    for(const Track& track : pack.tracks)
    {
        nlohmann::json tj;
        tj["name"] = track.name;
        nlohmann::json target;
        target["kind"] = TargetKindToString(track.target.kind);
        if(!track.target.device_name.empty())
        {
            target["device_name"] = track.target.device_name;
        }
        if(!track.target.zone_name.empty())
        {
            target["zone_name"] = track.target.zone_name;
        }
        if(!track.target.led_indices.empty())
        {
            target["led_indices"] = track.target.led_indices;
        }
        tj["target"] = target;
        tj["blocks"] = nlohmann::json::array();
        for(const Block& block : track.blocks)
        {
            nlohmann::json bj;
            bj["type"] = BlockTypeToString(block.type);
            bj["start_ms"] = block.start_ms;
            bj["end_ms"] = block.end_ms;
            bj["intensity"] = block.intensity;
            switch(block.type)
            {
                case BlockType::Solid:
                    bj["color"] = ColorToHex(block.color);
                    break;
                case BlockType::Fade:
                    bj["color_from"] = ColorToHex(block.color_from);
                    bj["color_to"] = ColorToHex(block.color_to);
                    break;
                case BlockType::Pulse:
                    bj["color"] = ColorToHex(block.color);
                    bj["period_ms"] = block.period_ms;
                    bj["min_intensity"] = block.min_intensity;
                    bj["max_intensity"] = block.max_intensity;
                    break;
                default:
                {
                    const BlockType unused = block.type;
                    (void)unused;
                    bj["color"] = ColorToHex(block.color);
                    break;
                }
            }
            tj["blocks"].push_back(bj);
        }
        j["tracks"].push_back(tj);
    }
    return j;
}

bool FromJson(const nlohmann::json& j, Pack* out, std::string* error)
{
    if(!out)
    {
        if(error)
        {
            *error = "null output pack";
        }
        return false;
    }
    if(!j.is_object())
    {
        if(error)
        {
            *error = "pack root must be an object";
        }
        return false;
    }
    if(!j.contains("format") || j["format"] != kFormatId)
    {
        if(error)
        {
            *error = "unsupported or missing format id";
        }
        return false;
    }
    if(!j.contains("version") || !j["version"].is_number_integer() || j["version"].get<int>() != kFormatVersion)
    {
        if(error)
        {
            *error = "unsupported or missing pack version";
        }
        return false;
    }

    Pack pack;
    pack.id = j.value("id", std::string());
    pack.name = j.value("name", pack.id);
    pack.duration_ms = std::clamp(j.value("duration_ms", 1000), 1, kMaxDurationMs);
    pack.priority = j.value("priority", 0);
    LoopMode loop = LoopMode::Once;
    if(!LoopFromString(j.value("loop", std::string("once")), &loop))
    {
        if(error)
        {
            *error = "invalid loop mode";
        }
        return false;
    }
    pack.loop = loop;

    if(!j.contains("tracks") || !j["tracks"].is_array())
    {
        if(error)
        {
            *error = "tracks must be an array";
        }
        return false;
    }

    for(const auto& tj : j["tracks"])
    {
        Track track;
        track.name = tj.value("name", std::string("Track"));
        TargetKind kind = TargetKind::All;
        if(tj.contains("target") && tj["target"].is_object())
        {
            const auto& target = tj["target"];
            if(!TargetKindFromString(target.value("kind", std::string("all")), &kind))
            {
                if(error)
                {
                    *error = "invalid target kind";
                }
                return false;
            }
            track.target.kind = kind;
            track.target.device_name = target.value("device_name", std::string());
            track.target.zone_name = target.value("zone_name", std::string());
            if(target.contains("led_indices") && target["led_indices"].is_array())
            {
                for(const auto& idx : target["led_indices"])
                {
                    if(idx.is_number_integer())
                    {
                        track.target.led_indices.push_back(idx.get<int>());
                    }
                }
            }
        }

        if(!tj.contains("blocks") || !tj["blocks"].is_array())
        {
            if(error)
            {
                *error = "track blocks must be an array";
            }
            return false;
        }
        for(const auto& bj : tj["blocks"])
        {
            Block block;
            BlockType type = BlockType::Solid;
            if(!BlockTypeFromString(bj.value("type", std::string("solid")), &type))
            {
                if(error)
                {
                    *error = "invalid block type";
                }
                return false;
            }
            block.type = type;
            block.start_ms = std::max(0, bj.value("start_ms", 0));
            block.end_ms = std::max(block.start_ms + 1, bj.value("end_ms", block.start_ms + 1));
            block.intensity = std::clamp(bj.value("intensity", 1.0f), 0.0f, 1.0f);
            block.period_ms = std::max(1, bj.value("period_ms", 1000));
            block.min_intensity = std::clamp(bj.value("min_intensity", 0.15f), 0.0f, 1.0f);
            block.max_intensity = std::clamp(bj.value("max_intensity", 1.0f), 0.0f, 1.0f);

            RGBColor color = ToRGBColor(255, 0, 0);
            if(bj.contains("color") && bj["color"].is_string())
            {
                ColorFromHex(bj["color"].get<std::string>(), &color);
            }
            block.color = color;
            block.color_from = color;
            block.color_to = ToRGBColor(255, 255, 255);
            if(bj.contains("color_from") && bj["color_from"].is_string())
            {
                ColorFromHex(bj["color_from"].get<std::string>(), &block.color_from);
            }
            if(bj.contains("color_to") && bj["color_to"].is_string())
            {
                ColorFromHex(bj["color_to"].get<std::string>(), &block.color_to);
            }
            track.blocks.push_back(block);
        }
        pack.tracks.push_back(std::move(track));
    }

    if(pack.id.empty())
    {
        if(error)
        {
            *error = "pack id is required";
        }
        return false;
    }
    *out = std::move(pack);
    return true;
}

bool LoadFromFile(const std::string& path, Pack* out, std::string* error)
{
    std::ifstream in(path);
    if(!in)
    {
        if(error)
        {
            *error = "failed to open pack file";
        }
        return false;
    }
    nlohmann::json j;
    try
    {
        in >> j;
    }
    catch(const std::exception& ex)
    {
        if(error)
        {
            *error = std::string("json parse failed: ") + ex.what();
        }
        return false;
    }
    return FromJson(j, out, error);
}

bool SaveToFile(const std::string& path, const Pack& pack, std::string* error)
{
    std::ofstream out(path);
    if(!out)
    {
        if(error)
        {
            *error = "failed to write pack file";
        }
        return false;
    }
    try
    {
        out << ToJson(pack).dump(2);
    }
    catch(const std::exception& ex)
    {
        if(error)
        {
            *error = std::string("json write failed: ") + ex.what();
        }
        return false;
    }
    return true;
}

Pack MakeExampleRainbowWash()
{
    Pack pack;
    pack.id = "rainbow_wash";
    pack.name = "Rainbow wash";
    pack.duration_ms = 60000;
    pack.loop = LoopMode::Forever;
    pack.priority = 10;

    Track track;
    track.name = "All LEDs";
    track.target.kind = TargetKind::All;

    const RGBColor stops[] = {
        ToRGBColor(255, 0, 0),
        ToRGBColor(255, 128, 0),
        ToRGBColor(255, 255, 0),
        ToRGBColor(0, 255, 0),
        ToRGBColor(0, 128, 255),
        ToRGBColor(128, 0, 255),
        ToRGBColor(255, 0, 0),
    };
    const int segments = 6;
    const int seg_ms = pack.duration_ms / segments;
    for(int i = 0; i < segments; ++i)
    {
        Block block;
        block.type = BlockType::Fade;
        block.start_ms = i * seg_ms;
        block.end_ms = (i == segments - 1) ? pack.duration_ms : (i + 1) * seg_ms;
        block.color_from = stops[i];
        block.color_to = stops[i + 1];
        block.intensity = 1.0f;
        track.blocks.push_back(block);
    }
    pack.tracks.push_back(std::move(track));
    return pack;
}

} // namespace EffectPack
