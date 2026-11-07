// SPDX-License-Identifier: GPL-2.0-only

#include "LaserFrame.h"

#include <algorithm>
#include <cmath>

void LaserFrame::RebuildSegments(bool link_lit_dwells)
{
    segments.clear();
    dots.clear();
    if(points.empty())
    {
        return;
    }

    segments.reserve(points.size());
    dots.reserve(points.size());

    auto push_dot = [this](const LaserPoint& p) {
        if(!dots.empty())
        {
            const LaserDot& last = dots.back();
            const float dx = p.x - last.x;
            const float dy = p.y - last.y;
            const float dz = p.z - last.z;
            if((dx * dx + dy * dy + dz * dz) < 1.0e-12f)
            {
                dots.back().r = p.r;
                dots.back().g = p.g;
                dots.back().b = p.b;
                dots.back().dwell += 1.0f;
                return;
            }
        }
        LaserDot d;
        d.x = p.x;
        d.y = p.y;
        d.z = p.z;
        d.r = p.r;
        d.g = p.g;
        d.b = p.b;
        d.dwell = 1.0f;
        dots.push_back(d);
    };

    if(points.size() == 1)
    {
        if(points[0].lit)
        {
            push_dot(points[0]);
        }
        return;
    }

    for(size_t i = 1; i < points.size(); ++i)
    {
        const LaserPoint& a = points[i - 1];
        const LaserPoint& b = points[i];
        if(!b.lit)
        {
            continue;
        }

        const float dx = b.x - a.x;
        const float dy = b.y - a.y;
        const float dz = b.z - a.z;
        const float len2 = dx * dx + dy * dy + dz * dz;

        if(len2 < 1.0e-12f)
        {
            // Sparkle / dwell: blanked travel, then lit hold (RING / SPRKL style).
            push_dot(b);
            continue;
        }

        // True lit stroke (continuous vector art) — moving beam, no extra dwell fat.
        LaserSegment seg;
        seg.x0 = a.x;
        seg.y0 = a.y;
        seg.z0 = a.z;
        seg.x1 = b.x;
        seg.y1 = b.y;
        seg.z1 = b.z;
        seg.r = b.r;
        seg.g = b.g;
        seg.b = b.b;
        seg.dwell = 1.0f;
        segments.push_back(seg);
        push_dot(b);
    }

    if(!link_lit_dwells || dots.size() < 2)
    {
        return;
    }

    // Editor-style outline: join successive dwell centers (RING's octagon/circle).
    const size_t n = dots.size();
    for(size_t i = 0; i + 1 < n; ++i)
    {
        const LaserDot& a = dots[i];
        const LaserDot& b = dots[i + 1];
        LaserSegment seg;
        seg.x0 = a.x;
        seg.y0 = a.y;
        seg.z0 = a.z;
        seg.x1 = b.x;
        seg.y1 = b.y;
        seg.z1 = b.z;
        seg.r = b.r;
        seg.g = b.g;
        seg.b = b.b;
        seg.dwell = std::max(a.dwell, b.dwell);
        segments.push_back(seg);
    }

    // Close the loop when first/last are clearly part of a ring (not a long open path).
    const LaserDot& first = dots.front();
    const LaserDot& last = dots.back();
    const float cdx = first.x - last.x;
    const float cdy = first.y - last.y;
    const float cdz = first.z - last.z;
    const float close_len2 = cdx * cdx + cdy * cdy + cdz * cdz;
    if(close_len2 > 1.0e-12f && n >= 3)
    {
        // Close if chord is not huge vs figure radius (avoid closing sparse open paths).
        float max_r2 = 0.0f;
        for(const LaserDot& d : dots)
        {
            max_r2 = std::max(max_r2, d.x * d.x + d.y * d.y + d.z * d.z);
        }
        if(close_len2 <= max_r2 * 2.5f + 0.05f)
        {
            LaserSegment seg;
            seg.x0 = last.x;
            seg.y0 = last.y;
            seg.z0 = last.z;
            seg.x1 = first.x;
            seg.y1 = first.y;
            seg.z1 = first.z;
            seg.r = first.r;
            seg.g = first.g;
            seg.b = first.b;
            seg.dwell = std::max(last.dwell, first.dwell);
            segments.push_back(seg);
        }
    }
}
