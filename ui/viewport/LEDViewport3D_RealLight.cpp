// SPDX-License-Identifier: GPL-2.0-or-later
/* Room-grid "Real light" overlay: the room as the lights would actually light it.
   Built from the colour every LED is showing right now (whatever drives it: the effect
   stack, the Action Editor's stacked lanes and blends, triggers, Movie Sync), so it always
   matches the hardware.

   The lights and their ray maps come from the shared light engine (RoomLight /
   RoomLightScene, also used by Emitter + relay receivers): each light knows how far its light
   gets in every direction, so housings shape it and blockers cast shadows. The GPU lights each
   grid point only where the ray towards it got that far. */

#include "LEDViewport3D.h"
#include "viewport/MeshBatch.h"
#include "viewport/ViewportMath.h"
#include "GridSpaceUtils.h"
#include "SpatialLighting/LightOutput.h"
#include "SpatialLighting/RoomLight.h"
#include "SpatialLighting/RoomLightScene.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace
{
constexpr int kMaxRealLights = 768;
/* Texels per light row: position, linear colour, then the ray map (4 depths per texel). */
constexpr int kTexelsPerLight = 2 + RoomLight::kRayMapSize / 4;

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

int LEDViewport3D::buildRealLightTexels(float /*reach_units*/)
{
    real_light_texels_.clear();
    if(!controller_transforms)
    {
        return 0;
    }
    const float* lin = SrgbToLinearTable();

    /* Lights and ray maps from the shared engine. */
    RoomLightScene::Leds leds;
    RoomLightScene::CollectLeds(*controller_transforms, nullptr, leds);
    if(leds.pos.empty())
    {
        return 0;
    }
    RoomLightScene::Blockers blockers;
    RoomLightScene::Gather(controller_transforms, grid_scale_mm, blockers);
    std::vector<RoomLight::Light> lights;
    std::vector<int> led_light;
    std::vector<const float*> ray_maps;
    RoomLightScene::BuildLightsWithRayMaps(leds, blockers, grid_scale_mm, kMaxRealLights, lights, led_light, ray_maps);

    /* This frame's colour per light: the LEDs' own colours, scaled by their real output. */
    std::vector<float> colour(lights.size() * 3u, 0.0f);
    for(std::size_t i = 0; i < leds.led.size(); ++i)
    {
        const LEDPosition3D& led = *leds.led[i];
        RGBColor c = led.preview_color;
        unsigned int global = 0;
        if(c == 0x00FFFFFF && led.controller && GlobalLedIndex(led.controller, led.zone_idx, led.led_idx, &global))
        {
            c = led.controller->GetColor(global);
        }
        const float scale = LightOutput::LedScale((*controller_transforms)[(std::size_t)leds.owner[i]].get(), led);
        float* out = &colour[(std::size_t)led_light[i] * 3u];
        out[0] += lin[RGBGetRValue(c)] * scale;
        out[1] += lin[RGBGetGValue(c)] * scale;
        out[2] += lin[RGBGetBValue(c)] * scale;
    }

    /* Upload lit lights: per row, position, colour and its ray map. */
    const float range = RoomLightScene::RayRange(grid_scale_mm);
    int n = 0;
    real_light_texels_.reserve(lights.size() * (std::size_t)kTexelsPerLight * 4u);
    for(std::size_t k = 0; k < lights.size(); ++k)
    {
        const float* c = &colour[k * 3u];
        if(c[0] + c[1] + c[2] <= 1e-5f)
        {
            continue;
        }
        const RoomLight::Light& l = lights[k];
        const float row[8] = {l.pos.x, l.pos.y, l.pos.z, 1.0f, c[0], c[1], c[2], 1.0f};
        real_light_texels_.insert(real_light_texels_.end(), row, row + 8);
        for(int d = 0; d < RoomLight::kRayMapSize; ++d)
        {
            real_light_texels_.push_back(ray_maps[k] ? ray_maps[k][d] : range);
        }
        ++n;
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
    static const std::vector<float> none((size_t)kTexelsPerLight * 4u, 0.0f);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, kTexelsPerLight, std::max(1, lights), 0, GL_RGBA, GL_FLOAT,
                 lights > 0 ? real_light_texels_.data() : none.data());
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
    gl_prog_real_light_point_.SetUniform1f("u_exposure", 1.4f * std::max(0.05f, room_grid_brightness) * room_exposure_);
    gl_prog_real_light_point_.SetUniform1f("u_floor", 0.02f);
    room_grid_overlay_batch_.Draw(MeshBatch::Primitive::Points);
    gl_prog_real_light_point_.Unbind();

    glBindTexture(GL_TEXTURE_2D, 0);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_TRUE);
    glDisable(GL_PROGRAM_POINT_SIZE);
}
