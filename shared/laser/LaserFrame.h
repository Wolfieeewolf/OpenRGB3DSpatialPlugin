// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include <cstdint>
#include <string>
#include <vector>

/** One laser sample point in normalized projector space (~[-1,1]). */
struct LaserPoint
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
    /** True when the stroke TO this point is lit (ILDA blanking bit clear). */
    bool lit = false;
};

/** Lit polyline stroke (projector space). Drawn when blanking is clear on the destination. */
struct LaserSegment
{
    float x0 = 0.0f, y0 = 0.0f, z0 = 0.0f;
    float x1 = 0.0f, y1 = 0.0f, z1 = 0.0f;
    float r = 1.0f, g = 1.0f, b = 1.0f;
    /**
     * Fatness proxy carried from the file — not an ILDA field.
     * True strokes stay ~1; joined dwell outlines use max endpoint dwell so
     * parked corners read thicker than quick mid-path hops in the same frame.
     */
    float dwell = 1.0f;
    /** ShowEditor Spur/track or ILDA scanner head (0 = default / single). */
    int track = 0;
};

/** Lit dwell / sparkle point (zero-length stroke or intentional point output). */
struct LaserDot
{
    float x = 0.0f, y = 0.0f, z = 0.0f;
    float r = 1.0f, g = 1.0f, b = 1.0f;
    /** How many consecutive lit samples parked here (ILDA “dwell”) — proxies brightness/fatness. */
    float dwell = 1.0f;
    /** ShowEditor Spur/track or ILDA scanner head (0 = default / single). */
    int track = 0;
};

struct LaserFrame
{
    std::string name;
    /** ILDA header "scanner head" (0–255): which projector/scanner pair this frame targets. */
    int scanner = 0;
    std::vector<LaserPoint> points;
    std::vector<LaserSegment> segments;
    std::vector<LaserDot> dots;

    /**
     * Build lit strokes + sparkle dwells.
     * @param link_lit_dwells If true, also connect successive dwell centers with strokes
     *        (editor-style ring/outline). Real ILDA leaves those moves blanked.
     */
    void RebuildSegments(bool link_lit_dwells = false);
};

struct LaserShow
{
    std::vector<LaserFrame> frames;
    /** Distinct scanner-head IDs seen in the file (sorted). */
    std::vector<int> scanners;
    std::string source_path;
    std::string error;
};
