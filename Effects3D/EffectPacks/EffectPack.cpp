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
    return DirectionInvertsAxis(dir) ? (1.0f - t) : t;
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

float BlockProgress(const Block& block, int local_ms)
{
    const float dur = (float)std::max(1, block.end_ms - block.start_ms);
    float t = (float)(local_ms - block.start_ms) / dur;
    t = std::clamp(t, 0.0f, 1.0f);
    const float speed = std::max(0.05f, block.speed);
    const float scaled = t * speed;
    if(scaled <= 0.0f)
    {
        return 0.0f;
    }
    const float wrapped = scaled - std::floor(scaled);
    if(wrapped <= 1e-6f)
    {
        return 1.0f;
    }
    return wrapped;
}

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
bool DirectionInvertsAxis(Direction dir)
{
    switch(dir)
    {
        case Direction::Left:
        case Direction::Down:
        case Direction::Back:
        case Direction::NegX:
        case Direction::NegY:
        case Direction::NegZ:
            return true;
        case Direction::Right:
        case Direction::Up:
        case Direction::Forward:
        case Direction::PosX:
        case Direction::PosY:
        case Direction::PosZ:
            return false;
        default:
        {
            const Direction unused = dir;
            (void)unused;
            return false;
        }
    }
}

/** Preferred room axis: 0=X, 1=Y, 2=Z. */
int DirectionPreferredAxis(Direction dir)
{
    switch(dir)
    {
        case Direction::Left:
        case Direction::Right:
        case Direction::PosX:
        case Direction::NegX:
            return 0;
        case Direction::Up:
        case Direction::Down:
        case Direction::PosY:
        case Direction::NegY:
            return 1;
        case Direction::Forward:
        case Direction::Back:
        case Direction::PosZ:
        case Direction::NegZ:
            return 2;
        default:
        {
            const Direction unused = dir;
            (void)unused;
            return 0;
        }
    }
}

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
        case BlockType::Alternating: return "Alternating";
        case BlockType::Strobe: return "Strobe";
        case BlockType::Spin: return "Spin";
        case BlockType::Candle: return "Candle Flicker";
        case BlockType::Dissolve: return "Dissolve";
        case BlockType::Wave: return "Wave";
        case BlockType::Plasma: return "Plasma";
        case BlockType::Snow: return "Snow";
        case BlockType::Fire: return "Fire";
        case BlockType::Balls: return "Balls";
        case BlockType::Bars: return "Bars";
        case BlockType::Scanner: return "Scanner";
        case BlockType::SphereWipe: return "Sphere Wipe";
        case BlockType::Orbit: return "Orbit";
        case BlockType::Ripple: return "Ripple";
        case BlockType::Meteor: return "Meteor";
        case BlockType::Noise3D: return "Noise 3D";
        case BlockType::Burst: return "Burst";
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
    const bool invert = DirectionInvertsAxis(dir);
    const int preferred = DirectionPreferredAxis(dir);

    auto span_of = [&](int axis) -> float {
        return (axis == 0) ? sx : ((axis == 1) ? sy : sz);
    };
    auto sample = [&](int axis) -> float {
        if(axis == 0)
        {
            return NormOnAxis(x, min_x, max_x, invert);
        }
        if(axis == 1)
        {
            return NormOnAxis(y, min_y, max_y, invert);
        }
        return NormOnAxis(z, min_z, max_z, invert);
    };

    // Preferred axis first; Up/Down fall back Y→Z→X (never X before Z).
    int order[3] = {preferred, 0, 0};
    if(preferred == 0)
    {
        order[1] = 2;
        order[2] = 1;
    }
    else if(preferred == 1)
    {
        order[1] = 2;
        order[2] = 0;
    }
    else
    {
        order[1] = 0;
        order[2] = 1;
    }

    for(int i = 0; i < 3; ++i)
    {
        if(span_of(order[i]) > eps)
        {
            return sample(order[i]);
        }
    }
    return invert ? 1.0f : 0.0f;
}

float WorldSpinAngle(Direction dir,
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
    const bool invert = DirectionInvertsAxis(dir);
    const int axis = DirectionPreferredAxis(dir);

    const float cx = 0.5f * (min_x + max_x);
    const float cy = 0.5f * (min_y + max_y);
    const float cz = 0.5f * (min_z + max_z);
    float u = 0.0f;
    float v = 0.0f;
    float su = 0.0f;
    float sv = 0.0f;
    if(axis == 0)
    {
        u = y - cy;
        v = z - cz;
        su = sy;
        sv = sz;
    }
    else if(axis == 1)
    {
        u = x - cx;
        v = z - cz;
        su = sx;
        sv = sz;
    }
    else
    {
        u = x - cx;
        v = y - cy;
        su = sx;
        sv = sy;
    }

    if(su <= eps || sv <= eps)
    {
        return WorldAxisPos(dir, x, y, z, min_x, max_x, min_y, max_y, min_z, max_z);
    }

    u /= std::max(su, eps);
    v /= std::max(sv, eps);
    float ang = std::atan2(v, u); // −π..π
    float t = ang / (2.0f * 3.14159265358979323846f) + 0.5f;
    t -= std::floor(t);
    return invert ? (1.0f - t) : t;
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
            const float speed = std::max(0.05f, block.speed);
            const int period = std::max(80, (int)std::lround((float)std::max(80, block.period_ms) / speed));
            const unsigned int h0 = HashLed(twinkle_seed, 0, 1);
            const float phase0 = (float)(h0 & 0xFFFF) / 65535.0f;
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
                const float win = 0.30f;
                if(phase < win)
                {
                    flash = std::sin((phase / win) * 3.14159265358979323846f);
                }
            }
            intensity *= lo + (hi - lo) * flash;
            const RGBColor base = SampleGradient(block, phase0);
            const RGBColor peak = SampleGradient(block, std::min(1.0f, phase0 + 0.35f));
            color = LerpColor(base, peak, flash);
            break;
        }
        case BlockType::Alternating:
        {
            const float speed = std::max(0.05f, block.speed);
            const int period = std::max(50, (int)std::lround((float)std::max(50, block.period_ms) / speed));
            const int phase_bit = ((local_ms - block.start_ms) / period) & 1;
            const int led_bit = (twinkle_seed ^ (int)std::lround(axis * 1024.0f)) & 1;
            const bool a = (led_bit ^ phase_bit) == 0;
            color = a ? SampleGradient(block, 0.0f) : SampleGradient(block, 1.0f);
            break;
        }
        case BlockType::Strobe:
        {
            const float speed = std::max(0.05f, block.speed);
            const int period = std::max(40, (int)std::lround((float)std::max(40, block.period_ms) / speed));
            const float phase = (float)((local_ms - block.start_ms) % period) / (float)period;
            const float duty = std::clamp(block.pulse_length, 0.05f, 0.95f);
            if(phase > duty)
            {
                return false;
            }
            color = SampleGradient(block, progress);
            break;
        }
        case BlockType::Spin:
        {
            const float width = std::clamp(block.pulse_length, 0.04f, 0.55f);
            float delta = axis - progress;
            delta -= std::floor(delta + 0.5f);
            const float lead = width * 0.22f;
            const float trail = width;
            float cover = 0.0f;
            if(delta >= 0.0f && delta <= lead)
            {
                cover = 1.0f - (delta / lead);
            }
            else if(delta < 0.0f && -delta <= trail)
            {
                cover = 1.0f + (delta / trail);
            }
            if(width <= 0.28f)
            {
                float delta2 = delta - ((delta >= 0.0f) ? 0.5f : -0.5f);
                delta2 -= std::floor(delta2 + 0.5f);
                float cover2 = 0.0f;
                if(delta2 >= 0.0f && delta2 <= lead)
                {
                    cover2 = 1.0f - (delta2 / lead);
                }
                else if(delta2 < 0.0f && -delta2 <= trail)
                {
                    cover2 = 1.0f + (delta2 / trail);
                }
                cover = std::max(cover, cover2 * 0.85f);
            }
            if(cover <= 0.001f)
            {
                return false;
            }
            intensity *= cover;
            color = SampleGradient(block, progress);
            break;
        }
        case BlockType::Candle:
        {
            const unsigned int h = HashLed(twinkle_seed, local_ms / 30, 30);
            const float n1 = (float)(h & 0xFF) / 255.0f;
            const float n2 = (float)((h >> 8) & 0xFF) / 255.0f;
            const float flicker = 0.55f + 0.45f * (0.65f * n1 + 0.35f * n2);
            const float lo = std::clamp(block.min_intensity, 0.0f, 1.0f);
            const float hi = std::clamp(block.max_intensity, lo, 1.0f);
            intensity *= lo + (hi - lo) * flicker;
            color = SampleGradient(block, 0.15f + 0.7f * n1);
            break;
        }
        case BlockType::Dissolve:
        {
            const unsigned int h = HashLed(twinkle_seed ^ 0x5F3759DF, 0, 1);
            const float threshold = (float)(h & 0xFFFF) / 65535.0f;
            if(progress + 0.001f < threshold)
            {
                return false;
            }
            float cover = 1.0f;
            if(progress < threshold + 0.08f)
            {
                cover = (progress - threshold) / 0.08f;
            }
            intensity *= std::clamp(cover, 0.0f, 1.0f);
            color = SampleGradient(block, threshold);
            break;
        }
        case BlockType::Wave:
        {
            const float cycles = std::max(0.25f, block.speed);
            const float phase = progress * cycles * 6.2831853f;
            const float wave = 0.5f + 0.5f * std::sin((axis * 6.2831853f * std::max(0.5f, block.pulse_length * 4.0f)) - phase);
            intensity *= std::clamp(wave, 0.0f, 1.0f);
            color = SampleGradient(block, axis);
            break;
        }
        case BlockType::Scanner:
        {
            float head = progress * 2.0f;
            if(head > 1.0f)
            {
                head = 2.0f - head;
            }
            if(DirectionInvertsAxis(block.direction))
            {
                head = 1.0f - head;
            }
            const float half_w = std::max(0.02f, block.pulse_length * 0.5f);
            const float dist = std::fabs(axis - head);
            float cover = 0.0f;
            if(dist <= half_w)
            {
                cover = 1.0f - (dist / half_w);
            }
            if(cover <= 0.001f)
            {
                return false;
            }
            intensity *= cover;
            color = SampleGradient(block, head);
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
            if(BlockNeedsWorldEval(block.type))
            {
                // Axis-only fallback: treat as a line along X in a unit cube.
                return EvaluateBlockAtWorld(block, local_ms,
                                            axis, 0.5f, 0.5f,
                                            0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f,
                                            twinkle_seed, out_color, out_intensity);
            }
            break;
    }

    if(!block.intensity_curve.empty())
    {
        intensity *= SampleCurve(block.intensity_curve, progress);
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

            // Multi-device group rows default to Room; device rows default to Device (v3+).
            if(ver < 3)
            {
                block.axis_space = AxisSpace::Room;
            }
            else if(TargetIsMultiDeviceGroup(track.target))
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

Pack MakeExampleDeskRipple()
{
    Pack pack;
    pack.id = "desk_ripple";
    pack.name = "Desk ripple";
    pack.duration_ms = 4000;
    pack.loop = LoopMode::Once;
    pack.priority = 20;

    Track track;
    track.name = "All LEDs";
    track.target.kind = TargetKind::All;

    Block ripple;
    ripple.type = BlockType::Ripple;
    ripple.start_ms = 0;
    ripple.end_ms = 3500;
    ripple.axis_space = AxisSpace::Room;
    ripple.pulse_length = 0.18f;
    ripple.intensity = 1.0f;
    ApplyGradientPresetId(&ripple, "ice");
    track.blocks.push_back(ripple);

    Block burst;
    burst.type = BlockType::Burst;
    burst.start_ms = 800;
    burst.end_ms = 2800;
    burst.axis_space = AxisSpace::Room;
    burst.pulse_length = 0.22f;
    burst.intensity = 0.85f;
    ApplyGradientPresetId(&burst, "cyber");
    track.blocks.push_back(burst);

    pack.tracks.push_back(std::move(track));
    return pack;
}

Pack MakeExampleSequenceWipe()
{
    Pack pack;
    pack.id = "sequence_wipe";
    pack.name = "Sequence wipe";
    pack.duration_ms = 3000;
    pack.loop = LoopMode::Once;
    pack.priority = 15;

    Track track;
    track.name = "All LEDs";
    track.target.kind = TargetKind::All;

    Block wipe;
    wipe.type = BlockType::Wipe;
    wipe.start_ms = 0;
    wipe.end_ms = 2200;
    wipe.axis_space = AxisSpace::Sequence;
    wipe.direction = Direction::Right;
    wipe.intensity = 1.0f;
    ApplyGradientPresetId(&wipe, "sunset");
    ApplyBuiltinIntensityCurve(&wipe, "ease_out");
    track.blocks.push_back(wipe);

    Block scanner;
    scanner.type = BlockType::Scanner;
    scanner.start_ms = 400;
    scanner.end_ms = 3000;
    scanner.axis_space = AxisSpace::Sequence;
    scanner.direction = Direction::Right;
    scanner.pulse_length = 0.12f;
    scanner.intensity = 1.0f;
    ApplyGradientPresetId(&scanner, "fire");
    track.blocks.push_back(scanner);

    pack.tracks.push_back(std::move(track));
    return pack;
}

} // namespace EffectPack
