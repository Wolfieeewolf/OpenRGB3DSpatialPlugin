// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include <vector>

/** One FolderVolume (or similar) atlas layer sampled on the GPU for the room-grid overlay.
 *  Prefer atlas_rgba (CPU export from the offscreen pool) so the viewport can upload a
 *  durable owned texture — pool FBO tex ids go stale after the next PrepareGpuFields.
 *  Atlas texels are volumeMain packed samples — finish_mode maps them to display RGB
 *  the same way FolderVolumeEffect::CalculateColorGrid does on the CPU. */
struct GpuRoomGridOverlayLayer
{
    static constexpr int kMaxLayers = 4;
    static constexpr int kMaxPalette = 4;

    /** How to interpret atlas RGB → display RGB (see ViewportShaders::VolumeAtlasPoint). */
    enum Finish : int
    {
        FinishRgb = 0,       /* atlas already display RGB */
        FinishHsv = 1,       /* (s,v,h) → HSV */
        FinishSurface = 2,   /* r=intensity, g=plasma → rainbow/palette * intensity */
        FinishScalarRainbow = 3, /* r=value → rainbow(value) * value */
        FinishScalarPalette = 4, /* r=value → palette(value) * value */
        FinishHex = 5,       /* r=value, g=hue01 → rainbow(hue) * value */
        FinishHexAtlas = 6,  /* xy→hue via atan2, z=value */
    };

    /** Viewport-owned GL texture after SetGpuRoomGridOverlayLayers; 0 while staging. */
    unsigned int atlas_tex = 0;
    /** RGBA8 packed atlas (width=n, height=n*n) from the volume pool — survives Stop/idle. */
    std::vector<unsigned char> atlas_rgba;
    int atlas_n = 0;
    int blend_mode = 0; /* BlendMode as int */
    float brightness = 1.0f;
    int finish_mode = FinishRgb;
    float hue_scroll_deg = 0.0f;
    int palette_count = 0;
    float palette[kMaxPalette][3] = {};

    bool sample_room = false;
    float origin[3] = {0.0f, 0.0f, 0.0f};
    /** Row-major R; uploaded as QMatrix3x3 → GLSL R^T * delta (VolumeWorldDeltaToLocal). */
    float rot_inv[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
    float half_ext[3] = {1.0f, 1.0f, 1.0f};
    float room_min[3] = {0.0f, 0.0f, 0.0f};
    float room_max[3] = {1.0f, 1.0f, 1.0f};
    float shear_xy = 0.0f;
    float shear_xz = 0.0f;
    float shear_yz = 0.0f;
    int volume_shape = 0; /* VOLUME_SHAPE_* */
    bool volume_tools = false;
};
