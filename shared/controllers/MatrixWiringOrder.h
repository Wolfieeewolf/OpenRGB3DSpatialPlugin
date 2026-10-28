// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include <vector>

/** Serpentine / row-wrap orders matching OpenRGB's Matrix Map Editor presets.
 *  Used when dumping linear zones into a grid (Custom Controller + layout gen). */
enum class MatrixWiringOrder : int
{
    HorizontalTopLeft = 0,
    HorizontalTopLeftZigzag,
    HorizontalTopRight,
    HorizontalTopRightZigzag,
    HorizontalBottomLeft,
    HorizontalBottomLeftZigzag,
    HorizontalBottomRight,
    HorizontalBottomRightZigzag,
    VerticalTopLeft,
    VerticalTopLeftZigzag,
    VerticalTopRight,
    VerticalTopRightZigzag,
    VerticalBottomLeft,
    VerticalBottomLeftZigzag,
    VerticalBottomRight,
    VerticalBottomRightZigzag,
    Count
};

inline constexpr unsigned int kMatrixMapUnused = 0xFFFFFFFFu;

const char* MatrixWiringOrderName(MatrixWiringOrder order);

/** Fill out_map (height * width) with zone-local LED indices 0..led_count-1.
 *  Unused cells are kMatrixMapUnused — same convention as OpenRGB matrix_map. */
void FillMatrixWiringMap(unsigned int height,
                         unsigned int width,
                         unsigned int led_count,
                         MatrixWiringOrder order,
                         std::vector<unsigned int>& out_map);
