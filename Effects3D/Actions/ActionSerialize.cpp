// SPDX-License-Identifier: GPL-2.0-only

#include "Action.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <sstream>

namespace Action
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
        tj["clips"] = nlohmann::json::array();
        for(const Clip& clip : track.clips)
        {
            nlohmann::json bj;
            const std::string effect_id = clip.effect_id.empty() ? "solid" : clip.effect_id;
            // Keep a stable, readable field order for hand-editing.
            bj["type"] = effect_id;
            bj["effect"] = effect_id;
            bj["start_ms"] = clip.start_ms;
            bj["end_ms"] = clip.end_ms;
            bj["intensity"] = clip.intensity;
            bj["min_intensity"] = clip.min_intensity;
            bj["max_intensity"] = clip.max_intensity;
            bj["color"] = ColorToHex(clip.color);
            bj["color_from"] = ColorToHex(clip.color_from);
            bj["color_to"] = ColorToHex(clip.color_to);
            if(!clip.gradient.empty())
            {
                bj["gradient"] = GradientToJson(clip.gradient);
            }
            bj["direction"] = DirectionToString(clip.direction);
            bj["axis_space"] = AxisSpaceToString(clip.axis_space);
            bj["axis_mode"] = (clip.axis_mode == AxisMode::Custom) ? "custom" : "preset";
            bj["axis_yaw_deg"] = clip.axis_yaw_deg;
            bj["axis_pitch_deg"] = clip.axis_pitch_deg;
            bj["speed"] = clip.speed;
            bj["pulse_length"] = clip.pulse_length;
            if(!clip.origin_ref.empty())
            {
                bj["origin_ref"] = clip.origin_ref;
            }
            if(clip.origin_offset_x != 0.0f || clip.origin_offset_y != 0.0f || clip.origin_offset_z != 0.0f)
            {
                bj["origin_offset_x"] = clip.origin_offset_x;
                bj["origin_offset_y"] = clip.origin_offset_y;
                bj["origin_offset_z"] = clip.origin_offset_z;
            }
            if(std::fabs(clip.effect_size - 1.0f) > 1e-4f)
            {
                bj["effect_size"] = clip.effect_size;
            }
            bj["period_ms"] = clip.period_ms;
            bj["reverse"] = clip.reverse;
            if(clip.flip_h)
            {
                bj["flip_h"] = true;
            }
            if(clip.flip_v)
            {
                bj["flip_v"] = true;
            }
            if(clip.rotate_quarters != 0)
            {
                bj["rotate_quarters"] = ((clip.rotate_quarters % 4) + 4) % 4;
            }
            if(!clip.media_path.empty())
            {
                bj["media_path"] = clip.media_path;
            }
            if(!clip.media_text.empty())
            {
                bj["media_text"] = clip.media_text;
            }
            if(clip.media_scroll)
            {
                bj["media_scroll"] = true;
            }
            if(!clip.intensity_curve.empty())
            {
                nlohmann::json carr = nlohmann::json::array();
                for(const CurvePoint& cp : clip.intensity_curve)
                {
                    nlohmann::json cj;
                    cj["pos"] = cp.pos;
                    cj["value"] = cp.value;
                    carr.push_back(cj);
                }
                bj["intensity_curve"] = carr;
            }
            if(!clip.period_curve.empty())
            {
                nlohmann::json carr = nlohmann::json::array();
                for(const CurvePoint& cp : clip.period_curve)
                {
                    nlohmann::json cj;
                    cj["pos"] = cp.pos;
                    cj["value"] = cp.value;
                    carr.push_back(cj);
                }
                bj["period_curve"] = carr;
            }
            tj["clips"].push_back(bj);
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

        if(!tj.contains("clips") || !tj["clips"].is_array())
        {
            if(error)
            {
                *error = "track clips must be an array";
            }
            return false;
        }
        for(const auto& bj : tj["clips"])
        {
            Clip clip;
            const std::string type_name = bj.value("type", std::string("solid"));
            clip.effect_id = bj.value("effect", type_name);
            clip.start_ms = std::max(0, bj.value("start_ms", 0));
            clip.end_ms = std::max(clip.start_ms + 1, bj.value("end_ms", clip.start_ms + 1));
            clip.intensity = std::clamp(bj.value("intensity", 1.0f), 0.0f, 1.0f);
            clip.period_ms = std::max(1, bj.value("period_ms", 1000));
            clip.min_intensity = std::clamp(bj.value("min_intensity", 0.15f), 0.0f, 1.0f);
            clip.max_intensity = std::clamp(bj.value("max_intensity", 1.0f), 0.0f, 1.0f);
            clip.speed = std::max(0.05f, bj.value("speed", 1.0f));
            clip.pulse_length = std::clamp(bj.value("pulse_length", 0.25f), 0.02f, 1.0f);
            if(bj.contains("origin_ref") && bj["origin_ref"].is_string())
            {
                clip.origin_ref = bj["origin_ref"].get<std::string>();
            }
            clip.origin_offset_x = std::clamp(bj.value("origin_offset_x", 0.0f), -100.0f, 100.0f);
            clip.origin_offset_y = std::clamp(bj.value("origin_offset_y", 0.0f), -100.0f, 100.0f);
            clip.origin_offset_z = std::clamp(bj.value("origin_offset_z", 0.0f), -100.0f, 100.0f);
            clip.effect_size = std::clamp(bj.value("effect_size", 1.0f), 0.05f, 2.0f);
            clip.reverse = bj.value("reverse", false);
            clip.flip_h = bj.value("flip_h", false);
            clip.flip_v = bj.value("flip_v", false);
            clip.rotate_quarters = bj.value("rotate_quarters", 0) % 4;
            if(clip.rotate_quarters < 0)
            {
                clip.rotate_quarters += 4;
            }
            if(bj.contains("media_path") && bj["media_path"].is_string())
            {
                clip.media_path = bj["media_path"].get<std::string>();
            }
            if(bj.contains("media_text") && bj["media_text"].is_string())
            {
                clip.media_text = bj["media_text"].get<std::string>();
            }
            clip.media_scroll = bj.value("media_scroll", false);

            Direction dir = Direction::Right;
            if(bj.contains("direction") && bj["direction"].is_string())
            {
                DirectionFromString(bj["direction"].get<std::string>(), &dir);
            }
            clip.direction = dir;

            if(TargetIsMultiDeviceGroup(track.target))
            {
                clip.axis_space = AxisSpace::Room;
            }
            else
            {
                clip.axis_space = AxisSpace::Device;
            }
            if(bj.contains("axis_space") && bj["axis_space"].is_string())
            {
                AxisSpace parsed = clip.axis_space;
                if(AxisSpaceFromString(bj["axis_space"].get<std::string>(), &parsed))
                {
                    clip.axis_space = parsed;
                }
            }
            clip.axis_mode = AxisMode::Preset;
            if(bj.contains("axis_mode") && bj["axis_mode"].is_string()
               && bj["axis_mode"].get<std::string>() == "custom")
            {
                clip.axis_mode = AxisMode::Custom;
            }
            clip.axis_yaw_deg = bj.value("axis_yaw_deg", 0.0f);
            clip.axis_pitch_deg = bj.value("axis_pitch_deg", 0.0f);

            if(bj.contains("intensity_curve") && bj["intensity_curve"].is_array())
            {
                for(const auto& cj : bj["intensity_curve"])
                {
                    CurvePoint cp;
                    cp.pos = std::clamp(cj.value("pos", 0.0f), 0.0f, 1.0f);
                    cp.value = std::clamp(cj.value("value", 1.0f), 0.0f, 1.0f);
                    clip.intensity_curve.push_back(cp);
                }
                std::sort(clip.intensity_curve.begin(), clip.intensity_curve.end(),
                          [](const CurvePoint& a, const CurvePoint& b) { return a.pos < b.pos; });
            }
            if(bj.contains("period_curve") && bj["period_curve"].is_array())
            {
                for(const auto& cj : bj["period_curve"])
                {
                    CurvePoint cp;
                    cp.pos = std::clamp(cj.value("pos", 0.0f), 0.0f, 1.0f);
                    cp.value = std::clamp(cj.value("value", 1.0f), 0.0f, 1.0f);
                    clip.period_curve.push_back(cp);
                }
                std::sort(clip.period_curve.begin(), clip.period_curve.end(),
                          [](const CurvePoint& a, const CurvePoint& b) { return a.pos < b.pos; });
            }

            RGBColor color = ToRGBColor(255, 0, 0);
            if(bj.contains("color") && bj["color"].is_string())
            {
                ColorFromHex(bj["color"].get<std::string>(), &color);
            }
            clip.color = color;
            clip.color_from = color;
            clip.color_to = ToRGBColor(255, 255, 255);
            if(bj.contains("color_from") && bj["color_from"].is_string())
            {
                ColorFromHex(bj["color_from"].get<std::string>(), &clip.color_from);
            }
            if(bj.contains("color_to") && bj["color_to"].is_string())
            {
                ColorFromHex(bj["color_to"].get<std::string>(), &clip.color_to);
            }
            if(bj.contains("gradient"))
            {
                GradientFromJson(bj["gradient"], &clip.gradient);
            }
            EnsureClipGradient(&clip);
            track.clips.push_back(clip);
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
    std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    nlohmann::json j;
    try
    {
        // Allow // and /* */ comments so hand-authored packs can document the layout.
        j = nlohmann::json::parse(text, nullptr, true, true);
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

} // namespace Action
