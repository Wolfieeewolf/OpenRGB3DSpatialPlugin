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
#include "SpatialLighting/RoomLight.h"

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

    std::vector<RoomLight::LitPoint> lit;
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

    /* Same light model as Emitter + relay receivers (shared/SpatialLighting/RoomLight). */
    std::vector<RoomLight::LitPoint> lights;
    RoomLight::ClusterLights(lit, reach_units, kMaxRealLights, lights);
    const int n = (int)lights.size();
    real_light_texels_.resize((size_t)n * 8u, 0.0f);
    for(int i = 0; i < n; ++i)
    {
        const RoomLight::LitPoint& k = lights[(size_t)i];
        float* t = &real_light_texels_[(size_t)i * 8u];
        t[0] = k.x;
        t[1] = k.y;
        t[2] = k.z;
        t[3] = 1.0f;
        t[4] = k.r;
        t[5] = k.g;
        t[6] = k.b;
        t[7] = 1.0f;
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
