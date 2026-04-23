// SPDX-License-Identifier: GPL-2.0-only

#include "SpatialLayerCore.h"

#include <algorithm>
#include <cmath>

namespace SpatialLayerCore
{
namespace
{
constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = 6.28318530717958647692f;

static inline float Clamp01(float x)
{
    return std::clamp(x, 0.0f, 1.0f);
}

static RGBColor LerpColor(RGBColor a, RGBColor b, float t)
{
    const float k = Clamp01(t);
    const int ar = (int)(a & 0xFF);
    const int ag = (int)((a >> 8) & 0xFF);
    const int ab = (int)((a >> 16) & 0xFF);
    const int br = (int)(b & 0xFF);
    const int bg = (int)((b >> 8) & 0xFF);
    const int bb = (int)((b >> 16) & 0xFF);
    const int rr = (int)std::lround(ar + (br - ar) * k);
    const int rg = (int)std::lround(ag + (bg - ag) * k);
    const int rb = (int)std::lround(ab + (bb - ab) * k);
    return (RGBColor)((rb << 16) | (rg << 8) | rr);
}

static RGBColor WeightedColorFrom(const RGBColor* cols, const float* ws, int n)
{
    float wr = 0.0f;
    float wg = 0.0f;
    float wb = 0.0f;
    float wsum = 0.0f;
    for(int i = 0; i < n; i++)
    {
        const float w = std::max(0.0f, ws[i]);
        wr += w * (float)(cols[i] & 0xFF);
        wg += w * (float)((cols[i] >> 8) & 0xFF);
        wb += w * (float)((cols[i] >> 16) & 0xFF);
        wsum += w;
    }
    if(wsum <= 1e-6f)
    {
        return (RGBColor)0x00000000;
    }
    const float inv = 1.0f / wsum;
    const int r = std::clamp((int)std::lround(wr * inv), 0, 255);
    const int g = std::clamp((int)std::lround(wg * inv), 0, 255);
    const int b = std::clamp((int)std::lround(wb * inv), 0, 255);
    return (RGBColor)((b << 16) | (g << 8) | r);
}

static float RampWeight(float x, float lo, float hi, float softness)
{
    if(hi <= lo + 1e-6f)
    {
        return 0.0f;
    }
    const float s = std::max(1e-4f, softness);
    if(x <= lo - s || x >= hi + s)
    {
        return 0.0f;
    }
    if(x >= lo + s && x <= hi - s)
    {
        return 1.0f;
    }
    if(x < lo + s)
    {
        return Clamp01((x - (lo - s)) / (2.0f * s));
    }
    return Clamp01(((hi + s) - x) / (2.0f * s));
}

static int ResolveLayerCount(const LayeredProbeSet& layered, LayerProfileMode mode)
{
    if(layered.has_layered && (layered.layer_count == 3 || layered.layer_count == 4))
    {
        return layered.layer_count;
    }
    if(mode == LayerProfileMode::ThreeLayer)
    {
        return 3;
    }
    if(mode == LayerProfileMode::FourLayer)
    {
        return 4;
    }
    return 4;
}

static void ComputeLayerWeights(float y_norm, const MapperSettings& settings, int layer_count, float* out_w)
{
    for(int i = 0; i < kMaxLayerCount; i++)
    {
        out_w[i] = 0.0f;
    }

    const float y = Clamp01(y_norm);
    const float soft = std::clamp(settings.blend_softness, 0.02f, 0.35f);
    const float b0 = std::clamp(settings.floor_end, 0.08f, 0.55f);

    if(layer_count == 3)
    {
        const float b1 = std::clamp(std::max(b0 + 0.08f, settings.upper_end), b0 + 0.08f, 0.92f);
        out_w[0] = RampWeight(y, 0.0f, b0, soft);
        out_w[1] = RampWeight(y, b0, b1, soft);
        out_w[2] = RampWeight(y, b1, 1.0f, soft);
    }
    else
    {
        const float b1 = std::clamp(std::max(b0 + 0.06f, settings.desk_end), b0 + 0.06f, 0.88f);
        const float b2 = std::clamp(std::max(b1 + 0.06f, settings.upper_end), b1 + 0.06f, 0.94f);
        out_w[0] = RampWeight(y, 0.0f, b0, soft);
        out_w[1] = RampWeight(y, b0, b1, soft);
        out_w[2] = RampWeight(y, b1, b2, soft);
        out_w[3] = RampWeight(y, b2, 1.0f, soft);
    }

    float sum = 0.0f;
    for(int i = 0; i < layer_count; i++)
    {
        sum += out_w[i];
    }
    if(sum <= 1e-5f)
    {
        out_w[(layer_count == 4) ? 1 : 1] = 1.0f;
        return;
    }
    const float inv = 1.0f / sum;
    for(int i = 0; i < layer_count; i++)
    {
        out_w[i] *= inv;
    }
}

static bool ComputeLocalDir(const Basis& basis, const SamplePoint& sample, float& out_lx, float& out_ly, float& out_lz)
{
    float fx = basis.forward_x;
    float fy = basis.forward_y;
    float fz = basis.forward_z;

    float ux = basis.up_x;
    float uy = basis.up_y;
    float uz = basis.up_z;
    float ul = std::sqrt(ux * ux + uy * uy + uz * uz);
    if(ul <= 1e-5f)
    {
        ux = 0.0f;
        uy = 1.0f;
        uz = 0.0f;
    }
    else
    {
        ux /= ul;
        uy /= ul;
        uz /= ul;
    }

    // Keep compass orientation stable with look pitch by projecting forward onto
    // the horizontal plane of the current up vector (amBX-style focal north).
    float fl = std::sqrt(fx * fx + fy * fy + fz * fz);
    if(fl <= 1e-5f)
    {
        fx = 0.0f;
        fy = 0.0f;
        fz = 1.0f;
    }
    else
    {
        fx /= fl;
        fy /= fl;
        fz /= fl;
    }
    const float fup = fx * ux + fy * uy + fz * uz;
    fx -= fup * ux;
    fy -= fup * uy;
    fz -= fup * uz;
    fl = std::sqrt(fx * fx + fy * fy + fz * fz);
    if(fl <= 1e-5f)
    {
        // If looking straight up/down, retain a stable forward fallback.
        fx = 0.0f;
        fy = 0.0f;
        fz = 1.0f;
    }
    else
    {
        fx /= fl;
        fy /= fl;
        fz /= fl;
    }

    float rx = fy * uz - fz * uy;
    float ry = fz * ux - fx * uz;
    float rz = fx * uy - fy * ux;
    float rl = std::sqrt(rx * rx + ry * ry + rz * rz);
    if(rl <= 1e-5f)
    {
        rx = 1.0f;
        ry = 0.0f;
        rz = 0.0f;
    }
    else
    {
        rx /= rl;
        ry /= rl;
        rz /= rl;
    }

    float ox = sample.grid_x - sample.origin_x;
    float oy = sample.grid_y - sample.origin_y;
    float oz = sample.grid_z - sample.origin_z;
    float ol = std::sqrt(ox * ox + oy * oy + oz * oz);
    if(ol <= 1e-5f)
    {
        return false;
    }
    ox /= ol;
    oy /= ol;
    oz /= ol;

    out_lx = ox * rx + oy * ry + oz * rz;
    out_ly = ox * ux + oy * uy + oz * uz;
    out_lz = ox * fx + oy * fy + oz * fz;
    return true;
}

}

RGBColor ComputeProjectedProbeColor(const ProbeInput& probes,
                                    const Basis& basis,
                                    const SamplePoint& sample,
                                    const MapperSettings& settings,
                                    bool* out_used_layered)
{
    if(out_used_layered)
    {
        *out_used_layered = false;
    }
    if(!basis.valid)
    {
        return (RGBColor)0x00000000;
    }

    float lx = 0.0f;
    float ly = 0.0f;
    float lz = 1.0f;
    if(!ComputeLocalDir(basis, sample, lx, ly, lz))
    {
        return (RGBColor)0x00000000;
    }

    const int layer_count = ResolveLayerCount(probes.layered, settings.profile_mode);
    if(probes.layered.has_layered && probes.layered.layer_count >= 3)
    {
        const float az = std::atan2(lx, lz) + settings.compass_azimuth_offset_rad;
        float pos = az * (8.0f / kTwoPi);
        if(pos < 0.0f)
        {
            pos += 8.0f;
        }
        const int i0 = ((int)std::floor(pos)) & 7;
        const int i1 = (i0 + 1) & 7;
        float t = pos - std::floor(pos);
        float w0 = 1.0f - t;
        float w1 = t;
        const float sharp = std::max(0.8f, settings.directional_sharpness);
        if(sharp > 1.001f)
        {
            w0 = std::pow(w0, sharp);
            w1 = std::pow(w1, sharp);
            const float s = w0 + w1;
            if(s > 1e-6f)
            {
                w0 /= s;
                w1 /= s;
            }
        }

        const float horiz = std::sqrt(std::max(0.0f, lx * lx + lz * lz));
        const float center_size = std::clamp(settings.center_size, 0.02f, 0.65f);
        const float center_soft = std::max(0.01f, std::clamp(settings.blend_softness, 0.02f, 0.35f) * 0.75f);
        const float center_w = Clamp01((center_size + center_soft - horiz) / (2.0f * center_soft));
        const float compass_scale = 1.0f - center_w;

        float layer_w[kMaxLayerCount]{};
        ComputeLayerWeights(sample.y_norm, settings, layer_count, layer_w);

        RGBColor layer_cols[kMaxLayerCount]{};
        for(int l = 0; l < layer_count; l++)
        {
            const int base = l * kSectorCount;
            const RGBColor c0 = probes.layered.colors[(size_t)(base + i0)];
            const RGBColor c1 = probes.layered.colors[(size_t)(base + i1)];
            const RGBColor cc = probes.layered.colors[(size_t)(base + 8)];
            RGBColor az_col = LerpColor(c0, c1, w1);
            if(compass_scale < 0.999f)
            {
                az_col = LerpColor(cc, az_col, compass_scale);
            }
            layer_cols[l] = az_col;
        }

        RGBColor out = WeightedColorFrom(layer_cols, layer_w, layer_count);
        if(out_used_layered)
        {
            *out_used_layered = true;
        }
        return out;
    }

    return (RGBColor)0x00000000;
}

}
