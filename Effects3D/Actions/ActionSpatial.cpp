// SPDX-License-Identifier: GPL-2.0-only
#include "Action.h"
#include "ActionClipEval.h"
#include "ActionMedia.h"
#include "ActionScript.h"

#include <algorithm>
#include <cmath>

namespace Action
{
namespace
{

RGBColor ScaleI(RGBColor c, float intensity)
{
    intensity = std::clamp(intensity, 0.0f, 1.0f);
    return ToRGBColor(
        (int)std::lround(RGBGetRValue(c) * intensity),
        (int)std::lround(RGBGetGValue(c) * intensity),
        (int)std::lround(RGBGetBValue(c) * intensity));
}

float ProjectOnUnitAxis(float x, float y, float z,
                        float min_x, float max_x,
                        float min_y, float max_y,
                        float min_z, float max_z,
                        float ux, float uy, float uz)
{
    const float cx = 0.5f * (min_x + max_x);
    const float cy = 0.5f * (min_y + max_y);
    const float cz = 0.5f * (min_z + max_z);
    float t_min = 1e9f, t_max = -1e9f;
    for(int i = 0; i < 8; ++i)
    {
        const float px = (i & 1) ? max_x : min_x;
        const float py = (i & 2) ? max_y : min_y;
        const float pz = (i & 4) ? max_z : min_z;
        const float t = (px - cx) * ux + (py - cy) * uy + (pz - cz) * uz;
        t_min = std::min(t_min, t);
        t_max = std::max(t_max, t);
    }
    const float span = std::max(1e-5f, t_max - t_min);
    const float t = (x - cx) * ux + (y - cy) * uy + (z - cz) * uz;
    return std::clamp((t - t_min) / span, 0.0f, 1.0f);
}

using clip_eval::WorldNorm;

WorldNorm MakeNormSample(float x, float y, float z,
                         float min_x, float max_x,
                         float min_y, float max_y,
                         float min_z, float max_z)
{
    WorldNorm s;
    s.span_x = max_x - min_x;
    s.span_y = max_y - min_y;
    s.span_z = max_z - min_z;
    const float diag = std::max(1e-5f, std::sqrt(s.span_x * s.span_x + s.span_y * s.span_y + s.span_z * s.span_z));
    const float eps = diag * 0.02f;

    s.nx = (s.span_x > eps) ? std::clamp((x - min_x) / s.span_x, 0.0f, 1.0f) : 0.5f;
    s.ny = (s.span_y > eps) ? std::clamp((y - min_y) / s.span_y, 0.0f, 1.0f) : 0.5f;
    s.nz = (s.span_z > eps) ? std::clamp((z - min_z) / s.span_z, 0.0f, 1.0f) : 0.5f;

    const float dx = (s.span_x > eps) ? (s.nx - 0.5f) : 0.0f;
    const float dy = (s.span_y > eps) ? (s.ny - 0.5f) : 0.0f;
    const float dz = (s.span_z > eps) ? (s.nz - 0.5f) : 0.0f;
    float max_d2 = 0.0f;
    if(s.span_x > eps) { max_d2 += 0.25f; }
    if(s.span_y > eps) { max_d2 += 0.25f; }
    if(s.span_z > eps) { max_d2 += 0.25f; }
    const float d2 = dx * dx + dy * dy + dz * dz;
    s.radius = (max_d2 > 1e-8f) ? std::sqrt(d2 / max_d2) : 0.0f;

    if(s.span_y > eps)
    {
        s.height = s.ny;
    }
    else if(s.span_z > eps)
    {
        s.height = s.nz;
    }
    else
    {
        s.height = s.nx;
    }
    return s;
}

bool CurvesEqual(const std::vector<CurvePoint>& a, const std::vector<CurvePoint>& b)
{
    if(a.size() != b.size())
    {
        return false;
    }
    for(size_t i = 0; i < a.size(); ++i)
    {
        if(std::fabs(a[i].pos - b[i].pos) > 1e-4f || std::fabs(a[i].value - b[i].value) > 1e-4f)
        {
            return false;
        }
    }
    return true;
}

} // namespace

float SampleCurve(const std::vector<CurvePoint>& curve, float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    if(curve.empty())
    {
        return 1.0f;
    }
    if(curve.size() == 1)
    {
        return std::clamp(curve.front().value, 0.0f, 1.0f);
    }
    if(t <= curve.front().pos)
    {
        return std::clamp(curve.front().value, 0.0f, 1.0f);
    }
    if(t >= curve.back().pos)
    {
        return std::clamp(curve.back().value, 0.0f, 1.0f);
    }
    for(size_t i = 1; i < curve.size(); ++i)
    {
        const CurvePoint& a = curve[i - 1];
        const CurvePoint& b = curve[i];
        if(t <= b.pos)
        {
            const float span = std::max(1e-6f, b.pos - a.pos);
            const float u = (t - a.pos) / span;
            return std::clamp(a.value + (b.value - a.value) * u, 0.0f, 1.0f);
        }
    }
    return std::clamp(curve.back().value, 0.0f, 1.0f);
}

void ApplyBuiltinIntensityCurve(Clip* clip, const char* preset_id)
{
    if(!clip || !preset_id)
    {
        return;
    }
    const std::string id(preset_id);
    clip->intensity_curve.clear();
    if(id == "triangle" || id == "seesaw")
    {
        clip->intensity_curve = {{0.0f, 0.0f}, {0.5f, 1.0f}, {1.0f, 0.0f}};
    }
    else if(id == "ease_in")
    {
        clip->intensity_curve = {{0.0f, 0.0f}, {0.6f, 0.25f}, {1.0f, 1.0f}};
    }
    else if(id == "ease_out")
    {
        clip->intensity_curve = {{0.0f, 1.0f}, {0.4f, 0.75f}, {1.0f, 0.0f}};
    }
    else if(id == "pulse_curve")
    {
        clip->intensity_curve = {{0.0f, 0.15f}, {0.2f, 1.0f}, {0.4f, 0.15f}, {1.0f, 0.15f}};
    }
    else if(id == "hold_peak")
    {
        clip->intensity_curve = {{0.0f, 0.0f}, {0.2f, 1.0f}, {0.8f, 1.0f}, {1.0f, 0.0f}};
    }
    else if(id == "snap")
    {
        clip->intensity_curve = {{0.0f, 0.0f}, {0.08f, 1.0f}, {0.92f, 1.0f}, {1.0f, 0.0f}};
    }
    else if(id == "flat")
    {
        return;
    }
    else
    {
        clip->intensity_curve = {{0.0f, 1.0f}, {1.0f, 1.0f}};
    }
}

const char* MatchBuiltinIntensityCurve(const std::vector<CurvePoint>& curve)
{
    if(curve.empty())
    {
        return "flat";
    }
    static const char* kIds[] = {
        "triangle", "ease_in", "ease_out", "pulse_curve", "hold_peak", "snap"
    };
    for(const char* id : kIds)
    {
        Clip probe;
        ApplyBuiltinIntensityCurve(&probe, id);
        if(CurvesEqual(curve, probe.intensity_curve))
        {
            return id;
        }
    }
    return nullptr;
}

void ApplyBuiltinPeriodCurve(Clip* clip, const char* preset_id)
{
    if(!clip || !preset_id)
    {
        return;
    }
    const std::string id(preset_id);
    clip->period_curve.clear();
    if(id == "square" || id == "blink")
    {
        // Hard on for first half, off for second (blink / strobe).
        clip->period_curve = {
            {0.0f, 1.0f}, {0.499f, 1.0f}, {0.5f, 0.0f}, {1.0f, 0.0f}
        };
    }
    else if(id == "duty25")
    {
        clip->period_curve = {
            {0.0f, 1.0f}, {0.24f, 1.0f}, {0.25f, 0.0f}, {1.0f, 0.0f}
        };
    }
    else if(id == "duty75")
    {
        clip->period_curve = {
            {0.0f, 1.0f}, {0.74f, 1.0f}, {0.75f, 0.0f}, {1.0f, 0.0f}
        };
    }
    else if(id == "triangle" || id == "seesaw")
    {
        clip->period_curve = {{0.0f, 0.0f}, {0.5f, 1.0f}, {1.0f, 0.0f}};
    }
    else if(id == "saw")
    {
        clip->period_curve = {{0.0f, 0.0f}, {1.0f, 1.0f}};
    }
    else if(id == "pulse_curve")
    {
        clip->period_curve = {
            {0.0f, 0.0f}, {0.08f, 1.0f}, {0.22f, 1.0f}, {0.35f, 0.0f}, {1.0f, 0.0f}
        };
    }
    else if(id == "sine")
    {
        // Coarse sine approximation for soft breathing within each period.
        clip->period_curve = {
            {0.0f, 0.0f}, {0.25f, 0.7f}, {0.5f, 1.0f}, {0.75f, 0.7f}, {1.0f, 0.0f}
        };
    }
    else if(id == "flat")
    {
        return;
    }
    else
    {
        clip->period_curve = {{0.0f, 1.0f}, {1.0f, 1.0f}};
    }
}

const char* MatchBuiltinPeriodCurve(const std::vector<CurvePoint>& curve)
{
    if(curve.empty())
    {
        return "flat";
    }
    static const char* kIds[] = {
        "square", "duty25", "duty75", "triangle", "saw", "pulse_curve", "sine"
    };
    for(const char* id : kIds)
    {
        Clip probe;
        ApplyBuiltinPeriodCurve(&probe, id);
        if(CurvesEqual(curve, probe.period_curve))
        {
            return id;
        }
    }
    return nullptr;
}

bool ApplyGradientPresetId(Clip* clip, const char* preset_id, RGBColor accent)
{
    if(!clip || !preset_id)
    {
        return false;
    }
    const std::string id(preset_id);
    if(id == "solid")
    {
        clip->color = accent;
        clip->color_from = accent;
        clip->color_to = accent;
        clip->gradient = {{0.0f, accent}};
    }
    else if(id == "rainbow")
    {
        clip->gradient = {
            {0.0f, ToRGBColor(255, 0, 0)},
            {0.2f, ToRGBColor(255, 128, 0)},
            {0.4f, ToRGBColor(255, 255, 0)},
            {0.6f, ToRGBColor(0, 255, 0)},
            {0.8f, ToRGBColor(0, 128, 255)},
            {1.0f, ToRGBColor(128, 0, 255)},
        };
    }
    else if(id == "red_blue")
    {
        clip->gradient = {{0.0f, ToRGBColor(255, 0, 0)}, {1.0f, ToRGBColor(0, 80, 255)}};
    }
    else if(id == "white_color")
    {
        clip->gradient = {{0.0f, ToRGBColor(255, 255, 255)}, {1.0f, accent}};
    }
    else if(id == "fire")
    {
        clip->gradient = {
            {0.0f, ToRGBColor(20, 0, 0)},
            {0.35f, ToRGBColor(255, 40, 0)},
            {0.7f, ToRGBColor(255, 160, 0)},
            {1.0f, ToRGBColor(255, 255, 180)},
        };
    }
    else if(id == "ice")
    {
        clip->gradient = {
            {0.0f, ToRGBColor(20, 40, 80)},
            {0.5f, ToRGBColor(120, 200, 255)},
            {1.0f, ToRGBColor(240, 250, 255)},
        };
    }
    else if(id == "forest")
    {
        clip->gradient = {
            {0.0f, ToRGBColor(10, 40, 10)},
            {0.5f, ToRGBColor(40, 160, 60)},
            {1.0f, ToRGBColor(180, 255, 120)},
        };
    }
    else if(id == "sunset")
    {
        clip->gradient = {
            {0.0f, ToRGBColor(40, 20, 80)},
            {0.4f, ToRGBColor(255, 80, 40)},
            {0.75f, ToRGBColor(255, 180, 60)},
            {1.0f, ToRGBColor(255, 240, 200)},
        };
    }
    else if(id == "cyber")
    {
        clip->gradient = {
            {0.0f, ToRGBColor(0, 255, 180)},
            {0.5f, ToRGBColor(0, 120, 255)},
            {1.0f, ToRGBColor(200, 0, 255)},
        };
    }
    else
    {
        return false;
    }
    if(!clip->gradient.empty())
    {
        clip->color = clip->gradient.front().color;
        clip->color_from = clip->gradient.front().color;
        clip->color_to = clip->gradient.back().color;
    }
    return true;
}

void AxisUnitVector(const Clip& clip, float* out_x, float* out_y, float* out_z)
{
    float x = 1.0f, y = 0.0f, z = 0.0f;
    if(clip.axis_mode == AxisMode::Custom)
    {
        const float deg = 3.14159265358979323846f / 180.0f;
        const float yaw = clip.axis_yaw_deg * deg;
        const float pitch = clip.axis_pitch_deg * deg;
        const float cp = std::cos(pitch);
        x = std::cos(yaw) * cp;
        y = std::sin(pitch);
        z = std::sin(yaw) * cp;
    }
    else
    {
        const int axis = DirectionPreferredAxis(clip.direction);
        const bool inv = DirectionInvertsAxis(clip.direction);
        x = (axis == 0) ? (inv ? -1.0f : 1.0f) : 0.0f;
        y = (axis == 1) ? (inv ? -1.0f : 1.0f) : 0.0f;
        z = (axis == 2) ? (inv ? -1.0f : 1.0f) : 0.0f;
    }
    const float len = std::max(1e-6f, std::sqrt(x * x + y * y + z * z));
    if(out_x) { *out_x = x / len; }
    if(out_y) { *out_y = y / len; }
    if(out_z) { *out_z = z / len; }
}

float SampleAxisPos(const Clip& clip,
                    float x, float y, float z,
                    float min_x, float max_x,
                    float min_y, float max_y,
                    float min_z, float max_z)
{
    if(clip.axis_mode == AxisMode::Custom)
    {
        float ux, uy, uz;
        AxisUnitVector(clip, &ux, &uy, &uz);
        return ProjectOnUnitAxis(x, y, z, min_x, max_x, min_y, max_y, min_z, max_z, ux, uy, uz);
    }
    return WorldAxisPos(clip.direction, x, y, z, min_x, max_x, min_y, max_y, min_z, max_z);
}

float SampleSpinAngle(const Clip& clip,
                      float x, float y, float z,
                      float min_x, float max_x,
                      float min_y, float max_y,
                      float min_z, float max_z)
{
    if(clip.axis_mode == AxisMode::Custom)
    {
        float ux, uy, uz;
        AxisUnitVector(clip, &ux, &uy, &uz);
        float rx = 0.0f, ry = 1.0f, rz = 0.0f;
        if(std::fabs(uy) > 0.9f)
        {
            rx = 1.0f; ry = 0.0f; rz = 0.0f;
        }
        float tx = ry * uz - rz * uy;
        float ty = rz * ux - rx * uz;
        float tz = rx * uy - ry * ux;
        float tlen = std::max(1e-6f, std::sqrt(tx * tx + ty * ty + tz * tz));
        tx /= tlen; ty /= tlen; tz /= tlen;
        float bx = uy * tz - uz * ty;
        float by = uz * tx - ux * tz;
        float bz = ux * ty - uy * tx;
        const float cx = 0.5f * (min_x + max_x);
        const float cy = 0.5f * (min_y + max_y);
        const float cz = 0.5f * (min_z + max_z);
        const float dx = x - cx, dy = y - cy, dz = z - cz;
        const float u = dx * tx + dy * ty + dz * tz;
        const float v = dx * bx + dy * by + dz * bz;
        if(std::fabs(u) < 1e-6f && std::fabs(v) < 1e-6f)
        {
            return SampleAxisPos(clip, x, y, z, min_x, max_x, min_y, max_y, min_z, max_z);
        }
        float ang = std::atan2(v, u);
        float t = ang / (2.0f * 3.14159265358979323846f) + 0.5f;
        t -= std::floor(t);
        return t;
    }
    return WorldSpinAngle(clip.direction, x, y, z, min_x, max_x, min_y, max_y, min_z, max_z);
}

bool ClipUsesSequenceAxis(const Clip& clip)
{
    if(clip.axis_space != AxisSpace::Sequence)
    {
        return false;
    }
    return !script::UsesWorld(script::FileId(clip));
}

bool ClipUsesSharedWorldBounds(const Clip& clip)
{
    if(clip.axis_space == AxisSpace::Room)
    {
        return true;
    }
    // GPU field / CPU particle tools sample shared room space — always use world AABB.
    const std::string effect_id = script::FileId(clip);
    if(script::UsesField(effect_id) || script::UsesParticles(effect_id)
       || script::UsesRibbons(effect_id))
    {
        return true;
    }
    return clip.axis_space == AxisSpace::Sequence && !ClipUsesSequenceAxis(clip);
}

namespace
{
ClipOrigin01 g_active_origin;
}

void SetActiveClipOrigin01(const ClipOrigin01& origin)
{
    g_active_origin = origin;
    g_active_origin.size = std::clamp(g_active_origin.size, 0.05f, 2.0f);
}

ClipOrigin01 GetActiveClipOrigin01()
{
    return g_active_origin;
}

ClipOrigin01 ResolveClipOrigin01(const Clip& clip,
                                 float min_x, float max_x,
                                 float min_y, float max_y,
                                 float min_z, float max_z,
                                 const std::vector<NamedWorldPoint>& refs)
{
    ClipOrigin01 out;
    out.size = std::clamp(clip.effect_size, 0.05f, 2.0f);

    const float span_x = max_x - min_x;
    const float span_y = max_y - min_y;
    const float span_z = max_z - min_z;
    float wx = 0.5f * (min_x + max_x);
    float wy = 0.5f * (min_y + max_y);
    float wz = 0.5f * (min_z + max_z);

    if(!clip.origin_ref.empty())
    {
        for(const NamedWorldPoint& p : refs)
        {
            if(p.name == clip.origin_ref)
            {
                wx = p.x;
                wy = p.y;
                wz = p.z;
                break;
            }
        }
    }

    const float half_w = span_x * 0.5f;
    const float half_h = span_y * 0.5f;
    const float half_d = span_z * 0.5f;
    wx += (std::clamp(clip.origin_offset_x, -100.0f, 100.0f) / 100.0f) * half_w;
    wy += (std::clamp(clip.origin_offset_y, -100.0f, 100.0f) / 100.0f) * half_h;
    wz += (std::clamp(clip.origin_offset_z, -100.0f, 100.0f) / 100.0f) * half_d;

    constexpr float eps = 1e-6f;
    out.ox = (span_x > eps) ? std::clamp((wx - min_x) / span_x, 0.0f, 1.0f) : 0.5f;
    out.oy = (span_y > eps) ? std::clamp((wy - min_y) / span_y, 0.0f, 1.0f) : 0.5f;
    out.oz = (span_z > eps) ? std::clamp((wz - min_z) / span_z, 0.0f, 1.0f) : 0.5f;
    return out;
}

using clip_eval::WorldCtx;


bool EvaluateClipAtWorld(const Clip& clip,
                          int local_ms,
                          float x, float y, float z,
                          float min_x, float max_x,
                          float min_y, float max_y,
                          float min_z, float max_z,
                          int twinkle_seed,
                          RGBColor* out_color,
                          float* out_intensity)
{
    if(local_ms < clip.start_ms || local_ms >= clip.end_ms || clip.end_ms <= clip.start_ms)
    {
        return false;
    }

    if(IsMediaClip(clip))
    {
        const float sx = max_x - min_x;
        const float sy = max_y - min_y;
        const float sz = max_z - min_z;
        const float diag = std::max(1e-5f, std::sqrt(sx * sx + sy * sy + sz * sz));
        const float eps = diag * 0.02f;
        const float tx = (sx > eps) ? std::clamp((x - min_x) / sx, 0.0f, 1.0f) : 0.5f;
        const float ty = (sy > eps) ? std::clamp((y - min_y) / sy, 0.0f, 1.0f) : 0.5f;
        const float tz = (sz > eps) ? std::clamp((z - min_z) / sz, 0.0f, 1.0f) : 0.5f;
        const float spans[3] = {sx, sy, sz};
        const float coords[3] = {tx, ty, tz};
        int order[3] = {0, 1, 2};
        // Sort axes by span descending (stable enough for three elements).
        if(spans[order[1]] > spans[order[0]]) { std::swap(order[0], order[1]); }
        if(spans[order[2]] > spans[order[1]]) { std::swap(order[1], order[2]); }
        if(spans[order[1]] > spans[order[0]]) { std::swap(order[0], order[1]); }
        int u_axis = order[0];
        int v_axis = order[1];
        // Prefer world X as U when it is one of the two largest (left→right reading).
        if(order[1] == 0)
        {
            u_axis = 0;
            v_axis = order[0];
        }
        const bool collapse_v = (spans[v_axis] <= eps);
        const float u = coords[u_axis];
        const float v = collapse_v ? 0.5f : coords[v_axis];
        return EvaluateMediaAtUv(clip, local_ms, u, v, out_color, out_intensity, collapse_v);
    }

    WorldCtx ctx;
    ctx.clip = &clip;
    ctx.local_ms = local_ms;
    ctx.progress = ClipProgress(clip, local_ms);
    ctx.twinkle_seed = twinkle_seed;
    ctx.x = x;
    ctx.y = y;
    ctx.z = z;
    ctx.min_x = min_x;
    ctx.max_x = max_x;
    ctx.min_y = min_y;
    ctx.max_y = max_y;
    ctx.min_z = min_z;
    ctx.max_z = max_z;
    ctx.s = MakeNormSample(x, y, z, min_x, max_x, min_y, max_y, min_z, max_z);
    ApplyClipUvTransform(clip, &ctx.s.nx, &ctx.s.ny);
    ctx.s.height = ctx.s.ny;
    ctx.dx = ctx.s.nx - 0.5f;
    ctx.dy = ctx.s.ny - 0.5f;
    ctx.dz = ctx.s.nz - 0.5f;
    ctx.intensity = std::clamp(clip.intensity, 0.0f, 1.0f);
    ctx.color = clip.color;

    script::LedView led;
    led.clip = &clip;
    led.local_ms = local_ms;
    led.progress = ctx.progress;
    led.axis = ctx.s.nx;
    led.seed = twinkle_seed;
    led.intensity = ctx.intensity;
    led.color = ctx.color;
    led.x = x;
    led.y = y;
    led.z = z;
    led.min_x = min_x;
    led.max_x = max_x;
    led.min_y = min_y;
    led.max_y = max_y;
    led.min_z = min_z;
    led.max_z = max_z;
    led.nx = ctx.s.nx;
    led.ny = ctx.s.ny;
    led.nz = ctx.s.nz;
    led.radius = ctx.s.radius;
    led.height = ctx.s.height;
    led.span_x = ctx.s.span_x;
    led.span_y = ctx.s.span_y;
    led.span_z = ctx.s.span_z;
    led.dx = ctx.dx;
    led.dy = ctx.dy;
    led.dz = ctx.dz;
    const std::string id = script::FileId(clip);
    led.turned_off = false;
    if(!script::Run(id, &led))
    {
        if(led.turned_off)
        {
            return false;
        }
        if(script::Has(id))
        {
            return false;
        }
        Clip sample = clip;
        EnsureClipGradient(&sample);
        led.color = sample.gradient.empty() ? sample.color : SampleGradient(sample, ctx.progress);
        led.intensity = std::clamp(clip.intensity, 0.0f, 1.0f);
    }
    ctx.color = led.color;
    ctx.intensity = led.intensity;

    ModulateClipIntensity(clip, ctx.progress, ClipPeriodProgress(clip, local_ms), &ctx.intensity);

    if(out_color)
    {
        *out_color = ScaleI(ctx.color, ctx.intensity);
    }
    if(out_intensity)
    {
        *out_intensity = ctx.intensity;
    }
    return true;
}

} // namespace Action
