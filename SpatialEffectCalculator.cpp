/*---------------------------------------------------------*\
| SpatialEffectCalculator.cpp                              |
|                                                           |
|   Calculator for all 3D spatial lighting effects        |
|                                                           |
|   Date: 2025-09-27                                        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#include "SpatialEffectCalculator.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

RGBColor SpatialEffectCalculator::CalculateColor(Vector3D position, float time_offset, const SpatialEffectParams& params, unsigned int led_idx)
{
    switch(params.type)
    {
        case SPATIAL_EFFECT_WAVE_X:
        case SPATIAL_EFFECT_WAVE_Y:
        case SPATIAL_EFFECT_WAVE_Z:
            return CalculateWaveColor(position, time_offset, params);

        case SPATIAL_EFFECT_WAVE_RADIAL:
            return CalculateRadialWaveColor(position, time_offset, params);

        case SPATIAL_EFFECT_RAIN:
            return CalculateRainColor(position, time_offset, params);

        case SPATIAL_EFFECT_FIRE:
            return CalculateFireColor(position, time_offset, params);

        case SPATIAL_EFFECT_PLASMA:
            return CalculatePlasmaColor(position, time_offset, params);

        case SPATIAL_EFFECT_RIPPLE:
            return CalculateRippleColor(position, time_offset, params);

        case SPATIAL_EFFECT_SPIRAL:
            return CalculateSpiralColor(position, time_offset, params);

        case SPATIAL_EFFECT_ORBIT:
            return CalculateOrbitColor(position, time_offset, params);

        case SPATIAL_EFFECT_SPHERE_PULSE:
            return CalculateSpherePulseColor(position, time_offset, params);

        case SPATIAL_EFFECT_CUBE_ROTATE:
            return CalculateCubeRotateColor(position, time_offset, params);

        case SPATIAL_EFFECT_METEOR:
            return CalculateMeteorColor(position, time_offset, params);

        case SPATIAL_EFFECT_DNA_HELIX:
            return CalculateDNAHelixColor(position, time_offset, params);

        case SPATIAL_EFFECT_ROOM_SWEEP:
            return CalculateRoomSweepColor(position, time_offset, params);

        case SPATIAL_EFFECT_CORNERS:
            return CalculateCornersColor(position, time_offset, params);

        case SPATIAL_EFFECT_VERTICAL_BARS:
            return CalculateVerticalBarsColor(position, time_offset, params);

        case SPATIAL_EFFECT_BREATHING_SPHERE:
            return CalculateBreathingSphereColor(position, time_offset, params);

        case SPATIAL_EFFECT_EXPLOSION:
            return CalculateExplosionColor(position, time_offset, params);

        case SPATIAL_EFFECT_WIPE_TOP_BOTTOM:
            return CalculateWipeTopBottomColor(position, time_offset, params);

        case SPATIAL_EFFECT_WIPE_LEFT_RIGHT:
            return CalculateWipeLeftRightColor(position, time_offset, params);

        case SPATIAL_EFFECT_WIPE_FRONT_BACK:
            return CalculateWipeFrontBackColor(position, time_offset, params);

        case SPATIAL_EFFECT_LED_SPARKLE:
            return CalculateLEDSparkleColor(position, time_offset, params, led_idx);

        case SPATIAL_EFFECT_LED_CHASE:
            return CalculateLEDChaseColor(position, time_offset, params, led_idx);

        case SPATIAL_EFFECT_LED_TWINKLE:
            return CalculateLEDTwinkleColor(position, time_offset, params, led_idx);

        default:
            return 0;
    }
}

RGBColor SpatialEffectCalculator::CalculateWaveColor(Vector3D pos, float time_offset, const SpatialEffectParams& params)
{
    float position_val = 0;

    if(params.type == SPATIAL_EFFECT_WAVE_X)
    {
        position_val = pos.x * params.scale_3d.x;
    }
    else if(params.type == SPATIAL_EFFECT_WAVE_Y)
    {
        position_val = pos.y * params.scale_3d.y;
    }
    else if(params.type == SPATIAL_EFFECT_WAVE_Z)
    {
        position_val = pos.z * params.scale_3d.z;
    }

    if(params.reverse)
    {
        position_val = -position_val;
    }

    float wave = (sinf((position_val + time_offset) / 10.0f) + 1.0f) / 2.0f;

    if(params.use_gradient)
    {
        return LerpColor(params.color_start, params.color_end, wave, params);
    }
    else
    {
        return params.color_start;
    }
}

RGBColor SpatialEffectCalculator::CalculateRadialWaveColor(Vector3D pos, float time_offset, const SpatialEffectParams& params)
{
    float dist = Distance3D(pos, params.origin);

    if(params.reverse)
    {
        dist = -dist;
    }

    float wave = (sinf((dist * params.scale_3d.x + time_offset) / 10.0f) + 1.0f) / 2.0f;

    if(params.use_gradient)
    {
        return LerpColor(params.color_start, params.color_end, wave, params);
    }
    else
    {
        return params.color_start;
    }
}

RGBColor SpatialEffectCalculator::CalculateRainColor(Vector3D pos, float time_offset, const SpatialEffectParams& params)
{
    float y_pos = pos.y + time_offset;
    float intensity = fmodf(y_pos * params.scale_3d.y, 10.0f) / 10.0f;

    return LerpColor(0x000000, params.color_start, intensity, params);
}

RGBColor SpatialEffectCalculator::CalculateFireColor(Vector3D pos, float time_offset, const SpatialEffectParams& params)
{
    float base = sinf(pos.x * 0.5f + time_offset * 0.1f) * 0.3f;
    float flicker = sinf(time_offset * 0.8f + pos.x) * 0.2f;
    float height_factor = 1.0f - (pos.y / 10.0f);

    float intensity = (base + flicker + height_factor) / 2.0f;
    intensity = fmaxf(0.0f, fminf(1.0f, intensity));

    /*---------------------------------------------------------*\
    | Using BGR format: 0x00BBGGRR                             |
    | Orange = RGB(255,69,0) -> BGR(0,69,255)                 |
    \*---------------------------------------------------------*/
    RGBColor orange = 0x0045FF;
    RGBColor yellow = 0x00FFFF;

    return LerpColor(orange, yellow, intensity, params);
}

RGBColor SpatialEffectCalculator::CalculatePlasmaColor(Vector3D pos, float time_offset, const SpatialEffectParams& params)
{
    float scale = params.scale_3d.x;
    float t = time_offset * 0.01f;

    float plasma1 = sinf(pos.x * scale * 0.1f + t);
    float plasma2 = sinf(pos.y * scale * 0.1f + t * 1.3f);
    float plasma3 = sinf((pos.x + pos.y) * scale * 0.05f + t * 0.8f);
    float plasma4 = sinf(sqrtf(pos.x * pos.x + pos.y * pos.y) * scale * 0.1f + t * 1.7f);

    float plasma = (plasma1 + plasma2 + plasma3 + plasma4) / 4.0f;
    plasma = (plasma + 1.0f) / 2.0f;

    if(params.use_gradient)
    {
        return LerpColor(params.color_start, params.color_end, plasma, params);
    }
    else
    {
        /*---------------------------------------------------------*\
        | Create rainbow effect when not using gradient           |
        \*---------------------------------------------------------*/
        float hue = fmodf(plasma * 360.0f, 360.0f);

        float c = 1.0f;
        float x = c * (1.0f - fabsf(fmodf(hue / 60.0f, 2.0f) - 1.0f));

        float r = 0, g = 0, b = 0;
        if(hue < 60) { r = c; g = x; b = 0; }
        else if(hue < 120) { r = x; g = c; b = 0; }
        else if(hue < 180) { r = 0; g = c; b = x; }
        else if(hue < 240) { r = 0; g = x; b = c; }
        else if(hue < 300) { r = x; g = 0; b = c; }
        else { r = c; g = 0; b = x; }

        /*---------------------------------------------------------*\
        | Apply brightness and return in BGR format               |
        \*---------------------------------------------------------*/
        float brightness_scale = params.brightness / 100.0f;
        unsigned char r_out = (unsigned char)(r * 255 * brightness_scale);
        unsigned char g_out = (unsigned char)(g * 255 * brightness_scale);
        unsigned char b_out = (unsigned char)(b * 255 * brightness_scale);

        return (b_out << 16) | (g_out << 8) | r_out;
    }
}

RGBColor SpatialEffectCalculator::CalculateRippleColor(Vector3D pos, float time_offset, const SpatialEffectParams& params)
{
    float dist = Distance3D(pos, params.origin);

    float ripple1 = sinf((dist * params.scale_3d.x - time_offset) / 5.0f);
    float ripple2 = sinf((dist * params.scale_3d.x - time_offset * 1.5f) / 7.0f);

    float intensity = (ripple1 + ripple2 + 2.0f) / 4.0f;

    return LerpColor(params.color_start, params.color_end, intensity, params);
}

RGBColor SpatialEffectCalculator::CalculateSpiralColor(Vector3D pos, float time_offset, const SpatialEffectParams& params)
{
    float angle = atan2f(pos.y - params.origin.y, pos.x - params.origin.x);
    float dist = Distance3D(pos, params.origin);

    float spiral = angle + (dist * params.scale_3d.x / 5.0f) - (time_offset / 10.0f);
    float value = (sinf(spiral * 2.0f * (float)M_PI) + 1.0f) / 2.0f;

    return LerpColor(params.color_start, params.color_end, value, params);
}

RGBColor SpatialEffectCalculator::CalculateOrbitColor(Vector3D pos, float time_offset, const SpatialEffectParams& params)
{
    float angle = atan2f(pos.z, pos.x) + time_offset / 20.0f;
    float radius = sqrtf(pos.x * pos.x + pos.z * pos.z);

    float orbit_x = cosf(angle) * radius;
    float orbit_z = sinf(angle) * radius;

    float dist = sqrtf((pos.x - orbit_x) * (pos.x - orbit_x) + (pos.z - orbit_z) * (pos.z - orbit_z));
    float brightness = fmaxf(0.0f, 1.0f - dist / 5.0f);

    return LerpColor(0x000000, params.color_start, brightness, params);
}

RGBColor SpatialEffectCalculator::CalculateSpherePulseColor(Vector3D pos, float time_offset, const SpatialEffectParams& params)
{
    float dist = Distance3D(pos, params.origin);
    float pulse_radius = fmodf(time_offset / 5.0f, 50.0f);

    float diff = fabsf(dist - pulse_radius);
    float brightness = fmaxf(0.0f, 1.0f - diff / 3.0f);

    if(params.use_gradient)
    {
        float grad = (sinf(time_offset / 20.0f) + 1.0f) / 2.0f;
        return LerpColor(params.color_start, params.color_end, grad * brightness, params);
    }

    return LerpColor(0x000000, params.color_start, brightness, params);
}

RGBColor SpatialEffectCalculator::CalculateCubeRotateColor(Vector3D pos, float time_offset, const SpatialEffectParams& params)
{
    float angle = time_offset / 30.0f;

    float rotated_x = pos.x * cosf(angle) - pos.z * sinf(angle);
    float rotated_z = pos.x * sinf(angle) + pos.z * cosf(angle);

    float max_abs = fmaxf(fmaxf(fabsf(rotated_x), fabsf(pos.y)), fabsf(rotated_z));

    if(max_abs > 20.0f && max_abs < 25.0f)
    {
        return params.color_start;
    }

    return 0x000000;
}

RGBColor SpatialEffectCalculator::CalculateMeteorColor(Vector3D pos, float time_offset, const SpatialEffectParams& params)
{
    float meteor_y = 50.0f - fmodf(time_offset / 3.0f, 100.0f);
    float meteor_x = time_offset / 5.0f;

    float dist_x = pos.x - meteor_x;
    float dist_y = pos.y - meteor_y;
    float dist_z = pos.z;

    float trail_length = 15.0f;
    float dist = sqrtf(dist_x * dist_x + dist_z * dist_z);

    if(dist_y > 0 && dist_y < trail_length && dist < 3.0f)
    {
        float brightness = 1.0f - (dist_y / trail_length);
        return LerpColor(params.color_end, params.color_start, brightness, params);
    }

    return 0x000000;
}

RGBColor SpatialEffectCalculator::CalculateDNAHelixColor(Vector3D pos, float time_offset, const SpatialEffectParams& params)
{
    float y_offset = time_offset / 10.0f;

    float angle1 = (pos.y + y_offset) / 5.0f;
    float helix1_x = cosf(angle1) * 10.0f;
    float helix1_z = sinf(angle1) * 10.0f;

    float angle2 = angle1 + (float)M_PI;
    float helix2_x = cosf(angle2) * 10.0f;
    float helix2_z = sinf(angle2) * 10.0f;

    float dist1 = sqrtf((pos.x - helix1_x) * (pos.x - helix1_x) + (pos.z - helix1_z) * (pos.z - helix1_z));
    float dist2 = sqrtf((pos.x - helix2_x) * (pos.x - helix2_x) + (pos.z - helix2_z) * (pos.z - helix2_z));

    if(dist1 < 3.0f)
    {
        return params.color_start;
    }
    else if(dist2 < 3.0f)
    {
        return params.color_end;
    }

    return 0x000000;
}

RGBColor SpatialEffectCalculator::CalculateRoomSweepColor(Vector3D pos, float time_offset, const SpatialEffectParams& params)
{
    float sweep_pos = fmodf(time_offset / 5.0f, 100.0f) - 50.0f;

    float dist = fabsf(pos.x - sweep_pos);

    if(dist < 5.0f)
    {
        float brightness = 1.0f - (dist / 5.0f);
        return LerpColor(0x000000, params.color_start, brightness, params);
    }

    return 0x000000;
}

RGBColor SpatialEffectCalculator::CalculateCornersColor(Vector3D pos, float time_offset, const SpatialEffectParams& params)
{
    int corner = (int)(time_offset / 30.0f) % 8;

    float target_x = (corner & 1) ? 25.0f : -25.0f;
    float target_y = (corner & 2) ? 25.0f : -25.0f;
    float target_z = (corner & 4) ? 25.0f : -25.0f;

    float dist = sqrtf(
        (pos.x - target_x) * (pos.x - target_x) +
        (pos.y - target_y) * (pos.y - target_y) +
        (pos.z - target_z) * (pos.z - target_z)
    );

    if(dist < 10.0f)
    {
        float brightness = 1.0f - (dist / 10.0f);
        return LerpColor(0x000000, params.color_start, brightness, params);
    }

    return 0x000000;
}

RGBColor SpatialEffectCalculator::CalculateVerticalBarsColor(Vector3D pos, float time_offset, const SpatialEffectParams& params)
{
    float offset = time_offset / 10.0f;
    float bar_width = 5.0f;
    float spacing = 15.0f;

    float x_mod = fmodf(pos.x + offset, spacing);

    if(x_mod < bar_width)
    {
        float t = x_mod / bar_width;
        return LerpColor(params.color_start, params.color_end, t, params);
    }

    return 0x000000;
}

RGBColor SpatialEffectCalculator::CalculateBreathingSphereColor(Vector3D pos, float time_offset, const SpatialEffectParams& params)
{
    float breath = (sinf(time_offset / 30.0f) + 1.0f) / 2.0f;
    float radius = 15.0f + breath * 15.0f;

    float dist = Distance3D(pos, params.origin);

    if(fabsf(dist - radius) < 3.0f)
    {
        float brightness = 1.0f - fabsf(dist - radius) / 3.0f;
        return LerpColor(params.color_start, params.color_end, breath * brightness, params);
    }

    return 0x000000;
}

RGBColor SpatialEffectCalculator::CalculateExplosionColor(Vector3D pos, float time_offset, const SpatialEffectParams& params)
{
    float dist = Distance3D(pos, params.origin);
    float explosion_radius = time_offset / 3.0f;

    float diff = fabsf(dist - explosion_radius);

    if(diff < 5.0f)
    {
        float brightness = 1.0f - (diff / 5.0f);
        float fade = fmaxf(0.0f, 1.0f - (explosion_radius / 50.0f));
        brightness *= fade;

        return LerpColor(params.color_end, params.color_start, brightness, params);
    }

    return 0x000000;
}

RGBColor SpatialEffectCalculator::CalculateWipeTopBottomColor(Vector3D pos, float time_offset, const SpatialEffectParams& params)
{
    float wipe_pos = 50.0f - (time_offset / 3.0f);

    float diff = pos.y - wipe_pos;

    if(diff > 0 && diff < 8.0f)
    {
        float brightness = 1.0f - (diff / 8.0f);
        return LerpColor(0x000000, params.color_start, brightness, params);
    }

    if(diff >= 8.0f)
    {
        return params.color_end;
    }

    return 0x000000;
}

RGBColor SpatialEffectCalculator::CalculateWipeLeftRightColor(Vector3D pos, float time_offset, const SpatialEffectParams& params)
{
    float wipe_pos = -50.0f + (time_offset / 3.0f);

    float diff = wipe_pos - pos.x;

    if(diff > 0 && diff < 8.0f)
    {
        float brightness = 1.0f - (diff / 8.0f);
        return LerpColor(0x000000, params.color_start, brightness, params);
    }

    if(diff >= 8.0f)
    {
        return params.color_end;
    }

    return 0x000000;
}

RGBColor SpatialEffectCalculator::CalculateWipeFrontBackColor(Vector3D pos, float time_offset, const SpatialEffectParams& params)
{
    float wipe_pos = -50.0f + (time_offset / 3.0f);

    float diff = wipe_pos - pos.z;

    if(diff > 0 && diff < 8.0f)
    {
        float brightness = 1.0f - (diff / 8.0f);
        return LerpColor(0x000000, params.color_start, brightness, params);
    }

    if(diff >= 8.0f)
    {
        return params.color_end;
    }

    return 0x000000;
}

RGBColor SpatialEffectCalculator::CalculateLEDSparkleColor(Vector3D /*pos*/, float time_offset, const SpatialEffectParams& params, unsigned int led_idx)
{
    unsigned int seed = led_idx + (unsigned int)(time_offset / 10.0f);
    srand(seed);

    if((rand() % 100) < 5)
    {
        return params.color_start;
    }

    return 0x000000;
}

RGBColor SpatialEffectCalculator::CalculateLEDChaseColor(Vector3D /*pos*/, float time_offset, const SpatialEffectParams& params, unsigned int led_idx)
{
    unsigned int chase_pos = (unsigned int)(time_offset / 2.0f) % 10;

    if(led_idx % 10 == chase_pos)
    {
        return params.color_start;
    }
    else if(led_idx % 10 == (chase_pos + 9) % 10)
    {
        return LerpColor(0x000000, params.color_start, 0.3f, params);
    }

    return 0x000000;
}

RGBColor SpatialEffectCalculator::CalculateLEDTwinkleColor(Vector3D /*pos*/, float time_offset, const SpatialEffectParams& params, unsigned int led_idx)
{
    float led_phase = (led_idx * 0.37f);
    float twinkle = (sinf((time_offset / 20.0f) + led_phase) + 1.0f) / 2.0f;

    if(twinkle > 0.7f)
    {
        float brightness = (twinkle - 0.7f) / 0.3f;
        return LerpColor(0x000000, params.color_start, brightness, params);
    }

    return 0x000000;
}

RGBColor SpatialEffectCalculator::LerpColor(RGBColor a, RGBColor b, float t, const SpatialEffectParams& params)
{
    t = fmaxf(0.0f, fminf(1.0f, t));

    /*---------------------------------------------------------*\
    | OpenRGB uses BGR format: 0x00BBGGRR                     |
    \*---------------------------------------------------------*/
    unsigned char r_a = a & 0xFF;
    unsigned char g_a = (a >> 8) & 0xFF;
    unsigned char b_a = (a >> 16) & 0xFF;

    unsigned char r_b = b & 0xFF;
    unsigned char g_b = (b >> 8) & 0xFF;
    unsigned char b_b = (b >> 16) & 0xFF;

    unsigned char r = (unsigned char)(r_a + (r_b - r_a) * t);
    unsigned char g = (unsigned char)(g_a + (g_b - g_a) * t);
    unsigned char b_out = (unsigned char)(b_a + (b_b - b_a) * t);

    /*---------------------------------------------------------*\
    | Apply brightness scaling                                 |
    \*---------------------------------------------------------*/
    float brightness_scale = params.brightness / 100.0f;
    r = (unsigned char)(r * brightness_scale);
    g = (unsigned char)(g * brightness_scale);
    b_out = (unsigned char)(b_out * brightness_scale);

    return (b_out << 16) | (g << 8) | r;
}

float SpatialEffectCalculator::Distance3D(Vector3D a, Vector3D b)
{
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    float dz = a.z - b.z;

    return sqrtf(dx * dx + dy * dy + dz * dz);
}