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
        case BlockType::ColorWash: return "colorwash";
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
    return false;
}

std::string DirectionToString(Direction d)
{
    switch(d)
    {
        case Direction::Left: return "left";
        case Direction::Right: return "right";
        case Direction::Up: return "up";
        case Direction::Down: return "down";
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
    if(s == "left") { *out = Direction::Left; return true; }
    if(s == "right") { *out = Direction::Right; return true; }
    if(s == "up") { *out = Direction::Up; return true; }
    if(s == "down") { *out = Direction::Down; return true; }
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

float AxisPos(Direction dir, int led_index, int led_count)
{
    if(led_count <= 1)
    {
        return 0.0f;
    }
    const float t = (float)led_index / (float)(led_count - 1);
    switch(dir)
    {
        case Direction::Left:
        case Direction::Up:
            return 1.0f - t;
        case Direction::Right:
        case Direction::Down:
        default:
            return t;
    }
}

float NormOnAxis(float v, float vmin, float vmax, bool invert)
{
    const float span = vmax - vmin;
    if(span <= 1e-5f)
    {
        return invert ? 1.0f : 0.0f;
    }
    float t = std::clamp((v - vmin) / span, 0.0f, 1.0f);
    return invert ? (1.0f - t) : t;
}

float BlockProgress(const Block& block, int local_ms)
{
    const float dur = (float)std::max(1, block.end_ms - block.start_ms);
    float t = (float)(local_ms - block.start_ms) / dur;
    t = std::clamp(t, 0.0f, 1.0f);
    const float speed = std::max(0.05f, block.speed);
    // speed > 1 = multiple cycles within the block; still fills the resized span.
    const float scaled = t * speed;
    if(scaled <= 0.0f)
    {
        return 0.0f;
    }
    // Exact cycle boundaries must read as "complete" (1.0), not wrap to 0.0.
    const float wrapped = scaled - std::floor(scaled);
    if(wrapped <= 1e-6f)
    {
        return 1.0f;
    }
    return wrapped;
}

unsigned int HashLed(int led_index, int local_ms, int period)
{
    unsigned int x = (unsigned int)(led_index * 374761393u + (local_ms / std::max(1, period)) * 668265263u);
    x = (x ^ (x >> 13)) * 1274126177u;
    return x ^ (x >> 16);
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

const char* BlockTypeDisplayName(BlockType t)
{
    switch(t)
    {
        case BlockType::Solid: return "Set Level";
        case BlockType::Fade: return "Fade";
        case BlockType::Pulse: return "Pulse";
        case BlockType::Wipe: return "Wipe";
        case BlockType::Chase: return "Chase";
        case BlockType::Twinkle: return "Twinkle";
        case BlockType::ColorWash: return "ColorWash";
        default: return "Effect";
    }
}

void EnsureBlockGradient(Block* block)
{
    if(!block || !block->gradient.empty())
    {
        return;
    }
    if(block->type == BlockType::Fade)
    {
        block->gradient.push_back({0.0f, block->color_from});
        block->gradient.push_back({1.0f, block->color_to});
        return;
    }
    block->gradient.push_back({0.0f, block->color});
    block->gradient.push_back({1.0f, block->color});
}

RGBColor SampleGradient(const Block& block, float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    if(block.gradient.empty())
    {
        if(block.type == BlockType::Fade)
        {
            return LerpColor(block.color_from, block.color_to, t);
        }
        return block.color;
    }
    if(block.gradient.size() == 1)
    {
        return block.gradient.front().color;
    }
    if(t <= block.gradient.front().pos)
    {
        return block.gradient.front().color;
    }
    if(t >= block.gradient.back().pos)
    {
        return block.gradient.back().color;
    }
    for(size_t i = 1; i < block.gradient.size(); ++i)
    {
        const GradientStop& a = block.gradient[i - 1];
        const GradientStop& b = block.gradient[i];
        if(t <= b.pos)
        {
            const float span = std::max(1e-6f, b.pos - a.pos);
            return LerpColor(a.color, b.color, (t - a.pos) / span);
        }
    }
    return block.gradient.back().color;
}

bool EvaluateBlock(const Block& block, int local_ms, RGBColor* out_color, float* out_intensity)
{
    return EvaluateBlockAtLed(block, local_ms, 0, 1, out_color, out_intensity);
}

float WorldAxisPos(Direction dir,
                   float x, float y, float z,
                   float min_x, float max_x,
                   float min_y, float max_y,
                   float min_z, float max_z)
{
    const float sx = max_x - min_x;
    const float sy = max_y - min_y;
    const float sz = max_z - min_z;
    const float diag = std::max(1e-5f, std::sqrt(sx * sx + sy * sy + sz * sz));
    const float eps = diag * 0.02f;

    const bool horiz = (dir == Direction::Left || dir == Direction::Right);
    const bool invert = (dir == Direction::Left || dir == Direction::Down);

    // Preferred room axes: Left/Right → X, Up/Down → Y.
    if(horiz)
    {
        if(sx > eps)
        {
            return NormOnAxis(x, min_x, max_x, invert);
        }
        if(sz >= sy && sz > eps)
        {
            return NormOnAxis(z, min_z, max_z, invert);
        }
        if(sy > eps)
        {
            return NormOnAxis(y, min_y, max_y, invert);
        }
        return invert ? 1.0f : 0.0f;
    }

    if(sy > eps)
    {
        return NormOnAxis(y, min_y, max_y, invert);
    }
    if(sz >= sx && sz > eps)
    {
        return NormOnAxis(z, min_z, max_z, invert);
    }
    if(sx > eps)
    {
        return NormOnAxis(x, min_x, max_x, invert);
    }
    return invert ? 1.0f : 0.0f;
}

bool EvaluateBlockAtAxis(const Block& block,
                         int local_ms,
                         float axis_pos,
                         int twinkle_seed,
                         RGBColor* out_color,
                         float* out_intensity)
{
    if(local_ms < block.start_ms || local_ms >= block.end_ms || block.end_ms <= block.start_ms)
    {
        return false;
    }

    float intensity = std::clamp(block.intensity, 0.0f, 1.0f);
    RGBColor color = block.color;
    const float axis = std::clamp(axis_pos, 0.0f, 1.0f);
    const float progress = BlockProgress(block, local_ms);

    switch(block.type)
    {
        case BlockType::Solid:
            color = SampleGradient(block, 0.0f);
            break;
        case BlockType::Fade:
        {
            const float t = (float)(local_ms - block.start_ms) / (float)(block.end_ms - block.start_ms);
            color = SampleGradient(block, t);
            break;
        }
        case BlockType::Pulse:
        {
            const float speed = std::max(0.05f, block.speed);
            const int period = std::max(1, (int)std::lround((float)std::max(1, block.period_ms) / speed));
            const float phase = (float)((local_ms - block.start_ms) % period) / (float)period;
            const float wave = 0.5f - 0.5f * std::cos(phase * 6.28318530718f);
            const float lo = std::clamp(block.min_intensity, 0.0f, 1.0f);
            const float hi = std::clamp(block.max_intensity, 0.0f, 1.0f);
            intensity *= lo + (hi - lo) * wave;
            color = SampleGradient(block, phase);
            break;
        }
        case BlockType::Wipe:
        {
            const float edge = 0.08f;
            const float front = progress * (1.0f + 2.0f * edge) - edge;
            const float d = front - axis;
            float cover = 0.0f;
            if(d >= edge)
            {
                cover = 1.0f;
            }
            else if(d > -edge)
            {
                cover = (d + edge) / (2.0f * edge);
            }
            if(cover <= 0.001f)
            {
                return false;
            }
            intensity *= cover;
            color = SampleGradient(block, progress);
            break;
        }
        case BlockType::Chase:
        {
            const float head = std::clamp(block.pulse_length, 0.02f, 1.0f);
            float delta = std::fabs(axis - progress);
            delta = std::min(delta, 1.0f - delta);
            if(delta > head)
            {
                return false;
            }
            intensity *= 1.0f - (delta / head);
            color = SampleGradient(block, progress);
            break;
        }
        case BlockType::Twinkle:
        {
            // Sparse per-LED flashes: most of the time near floor, occasional smooth peaks.
            const float speed = std::max(0.05f, block.speed);
            const int period = std::max(80, (int)std::lround((float)std::max(80, block.period_ms) / speed));
            const unsigned int h0 = HashLed(twinkle_seed, 0, 1);
            const float phase0 = (float)(h0 & 0xFFFF) / 65535.0f;
            // Higher block intensity → more LEDs flash each cycle (density).
            const float density = 0.12f + 0.55f * std::clamp(block.intensity, 0.0f, 1.0f);

            const int local = std::max(0, local_ms - block.start_ms);
            const int epoch = local / period;
            const unsigned int he = HashLed(twinkle_seed ^ 0xA5A5, epoch * period, period);
            const float roll = (float)((he >> 8) & 0xFF) / 255.0f;
            const bool active_cycle = (roll < density);

            const float lo = std::clamp(block.min_intensity, 0.0f, 1.0f);
            const float hi = std::clamp(block.max_intensity, lo, 1.0f);

            float flash = 0.0f;
            if(active_cycle)
            {
                const int phase_ms = (local + (int)std::lround(phase0 * (float)period)) % period;
                const float phase = (float)phase_ms / (float)period;
                // Flash occupies the first ~30% of the cycle, smooth in/out.
                const float win = 0.30f;
                if(phase < win)
                {
                    flash = std::sin((phase / win) * 3.14159265358979323846f);
                }
            }

            intensity *= lo + (hi - lo) * flash;
            // Per-LED colour from gradient identity; brighten toward white end when flashing hard.
            const RGBColor base = SampleGradient(block, phase0);
            const RGBColor peak = SampleGradient(block, std::min(1.0f, phase0 + 0.35f));
            color = LerpColor(base, peak, flash);
            break;
        }
        case BlockType::ColorWash:
        {
            float t = progress + axis * 0.35f;
            t -= std::floor(t);
            color = SampleGradient(block, t);
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

bool EvaluateBlockAtLed(const Block& block,
                        int local_ms,
                        int led_index,
                        int led_count,
                        RGBColor* out_color,
                        float* out_intensity)
{
    led_count = std::max(1, led_count);
    led_index = std::clamp(led_index, 0, led_count - 1);
    const float axis = AxisPos(block.direction, led_index, led_count);
    return EvaluateBlockAtAxis(block, local_ms, axis, led_index, out_color, out_intensity);
}

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
    return EvaluateTrackColorAtLed(track, local_ms, 0, 1, out_color, out_intensity);
}

const Block* FindActiveBlock(const Track& track, int local_ms)
{
    const Block* top = nullptr;
    for(const Block& block : track.blocks)
    {
        if(local_ms >= block.start_ms && local_ms < block.end_ms && block.end_ms > block.start_ms)
        {
            top = &block;
        }
    }
    return top;
}

bool EvaluateTrackColorAtLed(const Track& track,
                             int local_ms,
                             int led_index,
                             int led_count,
                             RGBColor* out_color,
                             float* out_intensity)
{
    const Block* top = FindActiveBlock(track, local_ms);
    if(!top)
    {
        return false;
    }
    return EvaluateBlockAtLed(*top, local_ms, led_index, led_count, out_color, out_intensity);
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
            bj["speed"] = block.speed;
            bj["pulse_length"] = block.pulse_length;
            if(!block.gradient.empty())
            {
                bj["gradient"] = GradientToJson(block.gradient);
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
    if(ver < 1 || ver > kFormatVersion)
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
        EnsureBlockGradient(&block);
        track.blocks.push_back(block);
    }
    pack.tracks.push_back(std::move(track));
    return pack;
}

} // namespace EffectPack
