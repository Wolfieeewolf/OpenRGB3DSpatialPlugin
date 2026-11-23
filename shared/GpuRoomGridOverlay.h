// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <vector>

/** One FolderVolume (or similar) atlas layer sampled on the GPU for the room-grid overlay.
 *  Prefer atlas_rgba (CPU export from the offscreen pool) so the viewport can upload a
 *  durable owned texture — pool FBO tex ids go stale after the next PrepareGpuFields.
 *  Atlas texels are volumeMain packed samples — finish_mode maps them to display RGB
 *  the same way FolderVolumeEffect::CalculateColorGrid does on the CPU. */
struct GpuRoomGridOverlayLayer
{
    /** ABI guard — MUST stay the first member. Each translation unit stamps its own
     *  sizeof; fillers check it (OverlayLayerAbiOk) before writing. A partially rebuilt
     *  plugin (one .obj compiled against an older version of this struct) then skips the
     *  layer instead of overwriting memory (crash seen 2026-11-22 after a mixed build). */
    unsigned int abi_size = (unsigned int)sizeof(GpuRoomGridOverlayLayer);

    static constexpr int kMaxLayers = 4;
    static constexpr int kMaxPalette = 8;

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
        FinishAudio = 7,     /* r=intensity, g=gradient → palette/rainbow(g) * AudioEffectDisplayBrightness(r) */
        FinishSpiral = 8,    /* r=value → palette/rainbow(value + scroll) * value^0.85 */
        FinishHexPalette = 9, /* r=value, g=hue01 → palette(hue01) * value (finish_hex, Rainbow off) */
    };
    /** finish_depth: multiply by the anchor-distance shade FolderVolume applies on the LEDs. */
    bool depth_shade = false;
    /** Rainbow (vs palette) for FinishAudio / FinishSpiral. */
    bool rainbow = false;

    /** What the texture holds and how the overlay shader reads it. */
    enum Kind : int
    {
        KindVolumeAtlas = 0, /* n×n×n packed volume atlas (FolderVolume) */
        KindProjected2D = 1, /* 2D RGBA frame (row 0 = top) projected onto the effect box
                                with `projection` — Shader Field, same SampleUv as the LEDs */
        KindScreenMirror = 2, /* capture frames of up to kMaxScreenPlanes display planes stacked
                                 top-to-bottom; per-plane SpatialMapToScreen + zones + crop +
                                 Reach falloff + grade in GLSL (ScreenMirror::CalculateColorGrid).
                                 Live frame only: wave delay and smoothing are temporal. */
        KindReactive = 3,     /* wave list in a 4 × N RGBA32F texture (data_f32); per-point
                                 spread distance / look / glow / colour in GLSL, mirroring
                                 Reactive::CalculateColorGrid. Per-wave scalars come from the
                                 same ComputeWaveFrame the LEDs use. */
        KindMinecraft = 4,    /* vitals bars + damage flash + Room Ambilight cubemap; per-frame
                                 scalars in `mc`, cubemap faces stacked in the RGBA8 texture
                                 (width S, height 6S). Mirrors MinecraftGame::RenderColor. */
    };

    /** KindMinecraft scalars (see MinecraftGame::FillOverlayLayer for the slot map). */
    static constexpr int kMinecraftSlots = 18;
    float mc[kMinecraftSlots][3] = {};

    /** KindReactive: rows = waves, 4 RGBA32F texels per row (see Reactive overlay fill). */
    std::vector<float> data_f32;
    int data_rows = 0;
    /** KindReactive colour: rainbow cycles (Size) and cyclic palette blend. */
    float reactive_cycles = 1.0f;
    static constexpr int kMaxReactiveWaves = 32;

    static constexpr int kMaxScreenPlanes = 4;
    static constexpr int kMaxScreenZones = 4;
    /** KindScreenMirror: one display plane's mapping and grade. */
    struct ScreenPlane
    {
        int frame_w = 0;
        int frame_h = 0;
        int row0 = 0;                 /* first texture row of this plane's frame */
        float pos[3] = {};            /* plane centre (grid units) = Reach origin */
        float rot[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1}; /* world→local, row-major, from
                                         TransformDisplayPlaneWorldToLocal */
        float inv_scale[3] = {1, 1, 1};
        float half_w = 1.0f;          /* grid units */
        float half_h = 1.0f;
        int zone_count = 0;           /* enabled zones; 0 = whole screen */
        float zones[kMaxScreenZones][4] = {}; /* u_min, u_max, v_min, v_max */
        float crop[4] = {0, 1, 0, 1}; /* u_lo, u_hi, v_lo, v_hi (black bars) */
        float max_mm = 0.0f;
        float coverage = 1.0f;
        float softness = 0.0f;
        float curve = 1.0f;
        bool inverted = false;
        float brightness = 1.0f;
        float threshold = 0.0f;
        float white_rolloff = 0.0f;
        float white_rolloff_max = 1.0f;
        float vibrance = 1.0f;
        float gain[3] = {1, 1, 1};
    };
    std::vector<ScreenPlane> screen_planes;
    float grid_scale_mm = 10.0f;
    int kind = KindVolumeAtlas;
    /** KindProjected2D only: frame size and ShaderField::ProjectionMode. */
    int tex_w = 0;
    int tex_h = 0;
    int projection = 0;

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

    /** Colors and patterns: the strip pattern supplies the colour and the effect value stays
     *  the brightness mask — same rule as SpatialEffect3D::ResolvePatternColor on the LEDs. */
    static constexpr int kPatternWidth = 256;
    bool pattern_on = false;
    int pattern_unfold = 0;          /* StripPatternSurface::UnfoldMode */
    float pattern_dir_deg = 0.0f;
    float pattern_feature = 1.0f;    /* Size: zoom of the strip axes */
    float pattern_phase_s01 = 0.0f;  /* EffectPhaseOnly: one strip position for every point */
    float pattern_strip[kPatternWidth] = {};   /* baked kernel strip, 0..1 */
    float pattern_lut[kPatternWidth][3] = {};  /* kernel palette, RGB 0..1 */
    /** Viewport-owned 256 × 2 RGBA32F texture (row 0 strip, row 1 palette); 0 while staging. */
    unsigned int pattern_tex = 0;

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

/** True when the caller's layer object has this translation unit's layout (see abi_size). */
inline bool OverlayLayerAbiOk(const GpuRoomGridOverlayLayer& layer)
{
    return layer.abi_size == (unsigned int)sizeof(GpuRoomGridOverlayLayer);
}
