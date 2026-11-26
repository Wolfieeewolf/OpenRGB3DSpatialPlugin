// SPDX-License-Identifier: GPL-2.0-or-later
/* Room-grid "Real light" overlay: the room as the lights would actually light it.
   Built from the colour every LED is showing right now (whatever drives it: the effect
   stack, the Action Editor's stacked lanes and blends, triggers, Movie Sync), so it always
   matches the hardware. LEDs are clustered on the CPU into a few hundred lights; each
   grid point gathers their spill on the GPU. */

#include "LEDViewport3D.h"
#include "viewport/MeshBatch.h"
#include "viewport/ViewportMath.h"
#include "GridSpaceUtils.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <unordered_map>

namespace
{
constexpr int kMaxRealLights = 768;

/* sRGB byte -> linear light (table: called for every lit LED every frame). */
const float* SrgbToLinearTable()
{
    static float table[256];
    static bool ready = false;
    if(!ready)
    {
        for(int i = 0; i < 256; ++i)
        {
            const float c = (float)i / 255.0f;
            table[i] = c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f);
        }
        ready = true;
    }
    return table;
}

bool GlobalLedIndex(RGBControllerInterface* c, unsigned int zone, unsigned int led, unsigned int* out)
{
    if(!c || zone >= c->GetZoneCount() || led >= c->GetZoneLEDsCount(zone))
    {
        return false;
    }
    *out = c->GetZoneStartIndex(zone) + led;
    return *out < c->GetLEDCount();
}

struct Cluster
{
    float px = 0.0f, py = 0.0f, pz = 0.0f, w = 0.0f;
    float r = 0.0f, g = 0.0f, b = 0.0f;
};
} // namespace

void LEDViewport3D::SetRealLightReachMm(float mm)
{
    real_light_reach_mm_ = std::clamp(mm, 50.0f, 10000.0f);
    update();
}

int LEDViewport3D::buildRealLightTexels(float reach_units)
{
    real_light_texels_.clear();
    if(!controller_transforms)
    {
        return 0;
    }
    const float* lin = SrgbToLinearTable();

    struct Lit
    {
        float x, y, z, r, g, b;
    };
    std::vector<Lit> lit;
    for(const auto& t : *controller_transforms)
    {
        if(!t || t->hidden_by_virtual)
        {
            continue;
        }
        for(const LEDPosition3D& led : t->led_positions)
        {
            /* Same colour the LED points draw (preview colour, or the device's own when unset). */
            RGBColor c = led.preview_color;
            unsigned int global = 0;
            if(c == 0x00FFFFFF && led.controller && GlobalLedIndex(led.controller, led.zone_idx, led.led_idx, &global))
            {
                c = led.controller->GetColor(global);
            }
            const int r = RGBGetRValue(c);
            const int g = RGBGetGValue(c);
            const int b = RGBGetBValue(c);
            if(r + g + b < 3)
            {
                continue;
            }
            const Vector3D& p = led.world_position;
            lit.push_back({p.x, p.y, p.z, lin[r], lin[g], lin[b]});
        }
    }
    if(lit.empty())
    {
        return 0;
    }

    /* Merge neighbours into lights a fraction of the reach apart; grow the cell until the
       light count fits the shader loop budget. The spill is smooth, so this is invisible. */
    float cell = std::max(1e-3f, reach_units * 0.22f);
    std::unordered_map<uint64_t, Cluster> cells;
    for(int attempt = 0; attempt < 8; ++attempt)
    {
        cells.clear();
        cells.reserve(std::min<size_t>(lit.size(), (size_t)kMaxRealLights * 2u));
        const float inv = 1.0f / cell;
        for(const Lit& l : lit)
        {
            const int64_t ix = (int64_t)std::floor(l.x * inv);
            const int64_t iy = (int64_t)std::floor(l.y * inv);
            const int64_t iz = (int64_t)std::floor(l.z * inv);
            const uint64_t key = ((uint64_t)(ix & 0x1FFFFF) << 42) | ((uint64_t)(iy & 0x1FFFFF) << 21)
                                 | (uint64_t)(iz & 0x1FFFFF);
            Cluster& k = cells[key];
            const float w = l.r + l.g + l.b + 1e-3f;
            k.px += l.x * w;
            k.py += l.y * w;
            k.pz += l.z * w;
            k.w += w;
            k.r += l.r;
            k.g += l.g;
            k.b += l.b;
        }
        if((int)cells.size() <= kMaxRealLights)
        {
            break;
        }
        cell *= 1.6f;
    }

    const int n = (int)std::min<size_t>(cells.size(), (size_t)kMaxRealLights);
    real_light_texels_.resize((size_t)n * 8u, 0.0f);
    int i = 0;
    for(const auto& kv : cells)
    {
        if(i >= n)
        {
            break;
        }
        const Cluster& k = kv.second;
        float* t = &real_light_texels_[(size_t)i * 8u];
        t[0] = k.px / k.w;
        t[1] = k.py / k.w;
        t[2] = k.pz / k.w;
        t[3] = 1.0f;
        t[4] = k.r;
        t[5] = k.g;
        t[6] = k.b;
        t[7] = 1.0f;
        ++i;
    }
    return n;
}

void LEDViewport3D::drawRealLightOverlay()
{
    if(!room_grid_overlay_batch_.IsValid())
    {
        return;
    }
    const float reach = std::max(0.05f, MMToGridUnits(real_light_reach_mm_, grid_scale_mm));
    const int lights = buildRealLightTexels(reach);

    if(real_light_tex_ == 0)
    {
        GLuint id = 0;
        glGenTextures(1, &id);
        real_light_tex_ = id;
    }
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, real_light_tex_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    const float none[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 2, std::max(1, lights), 0, GL_RGBA, GL_FLOAT,
                 lights > 0 ? real_light_texels_.data() : none);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    ViewportMat4 projection;
    ViewportMat4 view;
    std::memcpy(projection.m, pick_projection_, sizeof(float) * 16);
    std::memcpy(view.m, pick_scene_modelview_, sizeof(float) * 16);
    const ViewportMat4 mvp = ViewportMath::ModelViewProjection(projection, view, ViewportMath::Identity());

    /* Additive, no depth writes: dots read as lit haze and add up where light overlaps,
       while devices in front still hide what is behind them. */
    glEnable(GL_PROGRAM_POINT_SIZE);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);

    gl_prog_real_light_point_.Bind();
    gl_prog_real_light_point_.SetUniformMat4("u_mvp", mvp.m);
    gl_prog_real_light_point_.SetUniform1f("u_point_size", std::clamp(room_grid_point_size * 2.2f, 2.0f, 40.0f));
    gl_prog_real_light_point_.SetUniform1i("u_lights", 0);
    gl_prog_real_light_point_.SetUniform1i("u_light_count", lights);
    gl_prog_real_light_point_.SetUniform1f("u_reach", reach);
    gl_prog_real_light_point_.SetUniform1f("u_exposure", 1.4f * std::max(0.05f, room_grid_brightness));
    gl_prog_real_light_point_.SetUniform1f("u_floor", 0.02f);
    room_grid_overlay_batch_.Draw(MeshBatch::Primitive::Points);
    gl_prog_real_light_point_.Unbind();

    glBindTexture(GL_TEXTURE_2D, 0);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_TRUE);
    glDisable(GL_PROGRAM_POINT_SIZE);
}
