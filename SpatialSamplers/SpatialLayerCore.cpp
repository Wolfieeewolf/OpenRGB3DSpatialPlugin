// SPDX-License-Identifier: GPL-2.0-only

#include "SpatialLayerCore.h"
#include "SpatialBasisUtils.h"

#include <algorithm>
#include <cmath>

namespace SpatialLayerCore
{
namespace detail
{
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

int ResolveLayerCount(const LayeredProbeSet& layered, LayerProfileMode mode)
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

void ComputeLayerWeights(float y_norm, const MapperSettings& settings, int layer_count, float* out_w)
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
        const int mid = std::clamp(layer_count / 2, 0, layer_count - 1);
        out_w[mid] = 1.0f;
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
    SpatialBasisUtils::BasisVectors b = SpatialBasisUtils::BuildOrthonormalBasis(basis.forward_x,
                                                                                  basis.forward_y,
                                                                                  basis.forward_z,
                                                                                  basis.up_x,
                                                                                  basis.up_y,
                                                                                  basis.up_z);

    float ox = sample.grid_x - sample.origin_x;
    float oy = sample.grid_y - sample.origin_y;
    float oz = sample.grid_z - sample.origin_z;
    if(!SpatialBasisUtils::NormalizeDirection(ox, oy, oz, ox, oy, oz))
    {
        return false;
    }
    SpatialBasisUtils::ToLocal(b, ox, oy, oz, out_lx, out_ly, out_lz);
    return true;
}

static void RotateDirectionEuler(float x,
                                 float y,
                                 float z,
                                 float yaw_deg,
                                 float pitch_deg,
                                 float roll_deg,
                                 float& ox,
                                 float& oy,
                                 float& oz)
{
    const float k = 3.14159265359f / 180.0f;
    const float yaw = yaw_deg * k;
    float nx = x * cosf(yaw) - z * sinf(yaw);
    float nz = x * sinf(yaw) + z * cosf(yaw);
    float ny = y;

    const float pitch = pitch_deg * k;
    float py = ny * cosf(pitch) - nz * sinf(pitch);
    float pz = ny * sinf(pitch) + nz * cosf(pitch);
    float px = nx;

    const float roll = roll_deg * k;
    ox = px * cosf(roll) - py * sinf(roll);
    oy = px * sinf(roll) + py * cosf(roll);
    oz = pz;
}

} // namespace detail

void ComputeVerticalStratumWeights(float y_norm, const MapperSettings& settings, int layer_count, float* out_w)
{
    if(!out_w || (layer_count != 3 && layer_count != 4))
    {
        return;
    }
    detail::ComputeLayerWeights(y_norm, settings, layer_count, out_w);
}

void MakeBasisFromEffectEulerDegrees(float yaw_deg, float pitch_deg, float roll_deg, Basis& out)
{
    detail::RotateDirectionEuler(0.0f, 0.0f, 1.0f, yaw_deg, pitch_deg, roll_deg, out.forward_x, out.forward_y, out.forward_z);
    detail::RotateDirectionEuler(0.0f, 1.0f, 0.0f, yaw_deg, pitch_deg, roll_deg, out.up_x, out.up_y, out.up_z);
    out.valid = true;
}

bool MapPositionToCompassStrata(const Basis& basis,
                                const SamplePoint& sample,
                                const MapperSettings& settings,
                                int layer_count,
                                CompassStratumSample& out)
{
    out = CompassStratumSample{};
    if(!basis.valid || (layer_count != 3 && layer_count != 4))
    {
        return false;
    }

    float lx = 0.0f;
    float ly = 0.0f;
    float lz = 1.0f;
    if(!detail::ComputeLocalDir(basis, sample, lx, ly, lz))
    {
        return false;
    }

    const float az = std::atan2(lx, lz) + settings.compass_azimuth_offset_rad;
    float pos = az * (8.0f / detail::kTwoPi);
    if(pos < 0.0f)
    {
        pos += 8.0f;
    }
    out.sector_i0 = ((int)std::floor(pos)) & 7;
    out.sector_i1 = (out.sector_i0 + 1) & 7;
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
    (void)w0;
    out.sector_t = w1;

    const float horiz = std::sqrt(std::max(0.0f, lx * lx + lz * lz));
    const float center_size = std::clamp(settings.center_size, 0.02f, 0.65f);
    const float center_soft = std::max(0.01f, std::clamp(settings.blend_softness, 0.02f, 0.35f) * 0.75f);
    const float center_w = detail::Clamp01((center_size + center_soft - horiz) / (2.0f * center_soft));
    out.directional_blend = 1.0f - center_w;

    detail::ComputeLayerWeights(sample.y_norm, settings, layer_count, out.layer_w);
    out.layer_count = layer_count;
    out.valid = true;
    return true;
}

float CompassStratumPalettePosition01(const CompassStratumSample& css,
                                      const int layer_spin_sign[3],
                                      float time_scroll,
                                      float plane_mix01,
                                      float plane_angle01)
{
    if(!css.valid || !layer_spin_sign)
    {
        return std::clamp(plane_angle01, 0.0f, 1.0f);
    }
    const float sector01 = ((float)css.sector_i0 + css.sector_t) * (1.0f / 8.0f);
    const float sector_mixed =
        css.directional_blend * sector01 + (1.0f - css.directional_blend) * 0.5f;
    float spin_w = 0.0f;
    const int lc = std::clamp(css.layer_count, 1, 4);
    const int n = std::min(lc, 3);
    for(int i = 0; i < n; i++)
    {
        spin_w += css.layer_w[i] * (float)layer_spin_sign[i];
    }
    const float scroll = spin_w * time_scroll;
    const float pm = std::clamp(plane_mix01, 0.0f, 1.0f);
    const float pa = std::clamp(plane_angle01, 0.0f, 1.0f);
    const float local = pm * (pa - 0.5f);
    float p = sector_mixed + scroll + local;
    p = std::fmod(p, 1.0f);
    if(p < 0.0f)
    {
        p += 1.0f;
    }
    return p;
}

float CompassStratumHueOffsetDegrees(const Basis& basis,
                                     const SamplePoint& sample,
                                     const MapperSettings& settings,
                                     int layer_count)
{
    int lc = (layer_count == 4) ? 4 : 3;
    CompassStratumSample css{};
    if(!MapPositionToCompassStrata(basis, sample, settings, lc, css) || !css.valid)
    {
        return 0.0f;
    }
    float h = css.directional_blend * ((float)css.sector_i0 * 16.0f + css.sector_t * 16.0f);
    for(int i = 0; i < lc; i++)
    {
        h += css.layer_w[i] * (float)(i * 12);
    }
    return h;
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

    const int layer_count = detail::ResolveLayerCount(probes.layered, settings.profile_mode);
    if(!(probes.layered.has_layered && probes.layered.layer_count >= 3))
    {
        return (RGBColor)0x00000000;
    }

    CompassStratumSample css;
    if(!MapPositionToCompassStrata(basis, sample, settings, layer_count, css) || !css.valid)
    {
        return (RGBColor)0x00000000;
    }

    const int i0 = css.sector_i0;
    const int i1 = css.sector_i1;
    const float w1 = css.sector_t;
    const float compass_scale = css.directional_blend;

    RGBColor layer_cols[kMaxLayerCount]{};
    for(int l = 0; l < layer_count; l++)
    {
        const int base = l * kSectorCount;
        const RGBColor c0 = probes.layered.colors[(size_t)(base + i0)];
        const RGBColor c1 = probes.layered.colors[(size_t)(base + i1)];
        const RGBColor cc = probes.layered.colors[(size_t)(base + 8)];
        RGBColor az_col = detail::LerpColor(c0, c1, w1);
        if(compass_scale < 0.999f)
        {
            az_col = detail::LerpColor(cc, az_col, compass_scale);
        }
        layer_cols[l] = az_col;
    }

    RGBColor out = detail::WeightedColorFrom(layer_cols, css.layer_w, layer_count);
    if(out_used_layered)
    {
        *out_used_layered = true;
    }
    return out;
}

}
