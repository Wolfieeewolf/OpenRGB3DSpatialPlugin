// SPDX-License-Identifier: GPL-2.0-only

#include "EffectPack.h"

#include <algorithm>
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
    if(s == "once") { *out = LoopMode::Once; return true; }
    if(s == "forever") { *out = LoopMode::Forever; return true; }
    if(s == "while_active") { *out = LoopMode::WhileActive; return true; }
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
        case TargetKind::SceneZone: return "scene_zone";
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
    if(s == "all") { *out = TargetKind::All; return true; }
    if(s == "device") { *out = TargetKind::Device; return true; }
    if(s == "zone") { *out = TargetKind::Zone; return true; }
    if(s == "leds") { *out = TargetKind::Leds; return true; }
    if(s == "scene_zone") { *out = TargetKind::SceneZone; return true; }
    return false;
}

std::string AxisSpaceToString(AxisSpace s)
{
    switch(s)
    {
        case AxisSpace::Room: return "room";
        case AxisSpace::Device: return "device";
        case AxisSpace::Sequence: return "sequence";
        default:
        {
            const AxisSpace unused = s;
            (void)unused;
            return "device";
        }
    }
}

bool AxisSpaceFromString(const std::string& s, AxisSpace* out)
{
    if(!out)
    {
        return false;
    }
    if(s == "room") { *out = AxisSpace::Room; return true; }
    if(s == "device") { *out = AxisSpace::Device; return true; }
    if(s == "sequence") { *out = AxisSpace::Sequence; return true; }
    return false;
}

std::string BlockTypeToString(BlockType t)
{
    switch(t)
    {
        case BlockType::Solid: return "solid";
        case BlockType::Fade: return "fade";
        case BlockType::Pulse: return "pulse";
        case BlockType::Wipe: return "wipe";
        case BlockType::Chase: return "chase";
        case BlockType::Twinkle: return "twinkle";
        case BlockType::Alternating: return "alternating";
        case BlockType::Strobe: return "strobe";
        case BlockType::Spin: return "spin";
        case BlockType::Candle: return "candle";
        case BlockType::Dissolve: return "dissolve";
        case BlockType::Wave: return "wave";
        case BlockType::ColorWash: return "colorwash";
        case BlockType::Plasma: return "plasma";
        case BlockType::Snow: return "snow";
        case BlockType::Fire: return "fire";
        case BlockType::Balls: return "balls";
        case BlockType::Bars: return "bars";
        case BlockType::Scanner: return "scanner";
        case BlockType::SphereWipe: return "spherewipe";
        case BlockType::Orbit: return "orbit";
        case BlockType::Ripple: return "ripple";
        case BlockType::Meteor: return "meteor";
        case BlockType::Noise3D: return "noise3d";
        case BlockType::Burst: return "burst";
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
    if(s == "solid" || s == "set_level") { *out = BlockType::Solid; return true; }
    if(s == "fade") { *out = BlockType::Fade; return true; }
    if(s == "pulse") { *out = BlockType::Pulse; return true; }
    if(s == "wipe") { *out = BlockType::Wipe; return true; }
    if(s == "chase") { *out = BlockType::Chase; return true; }
    if(s == "twinkle") { *out = BlockType::Twinkle; return true; }
    if(s == "colorwash" || s == "color_wash") { *out = BlockType::ColorWash; return true; }
    if(s == "alternating") { *out = BlockType::Alternating; return true; }
    if(s == "strobe") { *out = BlockType::Strobe; return true; }
    if(s == "spin") { *out = BlockType::Spin; return true; }
    if(s == "candle" || s == "candle_flicker") { *out = BlockType::Candle; return true; }
    if(s == "dissolve") { *out = BlockType::Dissolve; return true; }
    if(s == "wave") { *out = BlockType::Wave; return true; }
    if(s == "plasma") { *out = BlockType::Plasma; return true; }
    if(s == "snow" || s == "meteors") { *out = BlockType::Snow; return true; }
    if(s == "fire") { *out = BlockType::Fire; return true; }
    if(s == "balls") { *out = BlockType::Balls; return true; }
    if(s == "bars") { *out = BlockType::Bars; return true; }
    if(s == "scanner") { *out = BlockType::Scanner; return true; }
    if(s == "spherewipe" || s == "sphere_wipe") { *out = BlockType::SphereWipe; return true; }
    if(s == "orbit") { *out = BlockType::Orbit; return true; }
    if(s == "ripple") { *out = BlockType::Ripple; return true; }
    if(s == "meteor") { *out = BlockType::Meteor; return true; }
    if(s == "noise3d" || s == "plasma3d") { *out = BlockType::Noise3D; return true; }
    if(s == "burst") { *out = BlockType::Burst; return true; }
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
    *out = ToRGBColor((int)((value >> 16) & 0xFF), (int)((value >> 8) & 0xFF), (int)(value & 0xFF));
    return true;
}
nlohmann::json GradientToJson(const std::vector<GradientStop>& stops)
{
    nlohmann::json arr = nlohmann::json::array();
    for(const GradientStop& s : stops)
    {
        nlohmann::json j;
        j["pos"] = s.pos;
        j["color"] = ColorToHex(s.color);
        arr.push_back(j);
    }
    return arr;
}

void GradientFromJson(const nlohmann::json& j, std::vector<GradientStop>* out)
{
    if(!out || !j.is_array())
    {
        return;
    }
    out->clear();
    for(const auto& s : j)
    {
        if(!s.is_object())
        {
            continue;
        }
        GradientStop stop;
        stop.pos = std::clamp(s.value("pos", 0.0f), 0.0f, 1.0f);
        if(s.contains("color") && s["color"].is_string())
        {
            ColorFromHex(s["color"].get<std::string>(), &stop.color);
        }
        out->push_back(stop);
    }
    std::sort(out->begin(), out->end(),
              [](const GradientStop& a, const GradientStop& b) { return a.pos < b.pos; });
}

} // namespace

std::string DirectionToString(Direction d)
{
    switch(d)
    {
        case Direction::Left: return "left";
        case Direction::Right: return "right";
        case Direction::Up: return "up";
        case Direction::Down: return "down";
        case Direction::Forward: return "forward";
        case Direction::Back: return "back";
        case Direction::PosX: return "+x";
        case Direction::NegX: return "-x";
        case Direction::PosY: return "+y";
        case Direction::NegY: return "-y";
        case Direction::PosZ: return "+z";
        case Direction::NegZ: return "-z";
        default:
        {
            const Direction unused = d;
            (void)unused;
            return "right";
        }
    }
}

bool DirectionFromString(const std::string& s, Direction* out)
{
    if(!out)
    {
        return false;
    }
    if(s == "left") { *out = Direction::Left; return true; }
    if(s == "right") { *out = Direction::Right; return true; }
    if(s == "up") { *out = Direction::Up; return true; }
    if(s == "down") { *out = Direction::Down; return true; }
    if(s == "forward" || s == "front") { *out = Direction::Forward; return true; }
    if(s == "back" || s == "backward") { *out = Direction::Back; return true; }
    if(s == "+x" || s == "pos_x" || s == "x+") { *out = Direction::PosX; return true; }
    if(s == "-x" || s == "neg_x" || s == "x-") { *out = Direction::NegX; return true; }
    if(s == "+y" || s == "pos_y" || s == "y+") { *out = Direction::PosY; return true; }
    if(s == "-y" || s == "neg_y" || s == "y-") { *out = Direction::NegY; return true; }
    if(s == "+z" || s == "pos_z" || s == "z+") { *out = Direction::PosZ; return true; }
    if(s == "-z" || s == "neg_z" || s == "z-") { *out = Direction::NegZ; return true; }
    return false;
}

/** True when wipe progresses toward the negative end of the chosen axis. */
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
    if(!pack.devices.empty())
    {
        j["devices"] = pack.devices;
    }
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
        if(!track.target.scene_zone_name.empty())
        {
            target["scene_zone_name"] = track.target.scene_zone_name;
        }
        if(track.target.flatten_leds)
        {
            target["flatten_leds"] = true;
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
            bj["color"] = ColorToHex(block.color);
            bj["color_from"] = ColorToHex(block.color_from);
            bj["color_to"] = ColorToHex(block.color_to);
            bj["period_ms"] = block.period_ms;
            bj["min_intensity"] = block.min_intensity;
            bj["max_intensity"] = block.max_intensity;
            bj["direction"] = DirectionToString(block.direction);
            bj["axis_space"] = AxisSpaceToString(block.axis_space);
            bj["axis_mode"] = (block.axis_mode == AxisMode::Custom) ? "custom" : "preset";
            bj["axis_yaw_deg"] = block.axis_yaw_deg;
            bj["axis_pitch_deg"] = block.axis_pitch_deg;
            bj["speed"] = block.speed;
            bj["pulse_length"] = block.pulse_length;
            if(!block.gradient.empty())
            {
                bj["gradient"] = GradientToJson(block.gradient);
            }
            if(!block.intensity_curve.empty())
            {
                nlohmann::json carr = nlohmann::json::array();
                for(const CurvePoint& cp : block.intensity_curve)
                {
                    nlohmann::json cj;
                    cj["pos"] = cp.pos;
                    cj["value"] = cp.value;
                    carr.push_back(cj);
                }
                bj["intensity_curve"] = carr;
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
    if(!j.contains("version") || !j["version"].is_number_integer())
    {
        if(error)
        {
            *error = "unsupported or missing pack version";
        }
        return false;
    }
    const int ver = j["version"].get<int>();
    if(ver != kFormatVersion)
    {
        if(error)
        {
            *error = "unsupported pack version";
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

    if(j.contains("devices") && j["devices"].is_array())
    {
        for(const auto& d : j["devices"])
        {
            if(d.is_string())
            {
                const std::string name = d.get<std::string>();
                if(!name.empty())
                {
                    pack.devices.push_back(name);
                }
            }
        }
    }

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
            track.target.scene_zone_name = target.value("scene_zone_name", std::string());
            track.target.flatten_leds = target.value("flatten_leds", false);
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
            block.speed = std::max(0.05f, bj.value("speed", 1.0f));
            block.pulse_length = std::clamp(bj.value("pulse_length", 0.25f), 0.02f, 1.0f);

            Direction dir = Direction::Right;
            if(bj.contains("direction") && bj["direction"].is_string())
            {
                DirectionFromString(bj["direction"].get<std::string>(), &dir);
            }
            block.direction = dir;

            if(TargetIsMultiDeviceGroup(track.target))
            {
                block.axis_space = AxisSpace::Room;
            }
            else
            {
                block.axis_space = AxisSpace::Device;
            }
            if(bj.contains("axis_space") && bj["axis_space"].is_string())
            {
                AxisSpace parsed = block.axis_space;
                if(AxisSpaceFromString(bj["axis_space"].get<std::string>(), &parsed))
                {
                    block.axis_space = parsed;
                }
            }
            block.axis_mode = AxisMode::Preset;
            if(bj.contains("axis_mode") && bj["axis_mode"].is_string()
               && bj["axis_mode"].get<std::string>() == "custom")
            {
                block.axis_mode = AxisMode::Custom;
            }
            block.axis_yaw_deg = bj.value("axis_yaw_deg", 0.0f);
            block.axis_pitch_deg = bj.value("axis_pitch_deg", 0.0f);

            if(bj.contains("intensity_curve") && bj["intensity_curve"].is_array())
            {
                for(const auto& cj : bj["intensity_curve"])
                {
                    CurvePoint cp;
                    cp.pos = std::clamp(cj.value("pos", 0.0f), 0.0f, 1.0f);
                    cp.value = std::clamp(cj.value("value", 1.0f), 0.0f, 1.0f);
                    block.intensity_curve.push_back(cp);
                }
                std::sort(block.intensity_curve.begin(), block.intensity_curve.end(),
                          [](const CurvePoint& a, const CurvePoint& b) { return a.pos < b.pos; });
            }

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
            if(bj.contains("gradient"))
            {
                GradientFromJson(bj["gradient"], &block.gradient);
            }
            EnsureBlockGradient(&block);
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

bool LoadFromFile(const filesystem::path& path, Pack* out, std::string* error)
{
    std::ifstream in(path, std::ios::binary);
    if(!in)
    {
        if(error)
        {
            *error = "failed to open pack file: " + path.string();
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

bool SaveToFile(const filesystem::path& path, const Pack& pack, std::string* error)
{
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if(!out)
    {
        if(error)
        {
            *error = "failed to write pack file: " + path.string();
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
    return static_cast<bool>(out);
}

} // namespace EffectPack
