// SPDX-License-Identifier: GPL-2.0-only

#include "MatrixWiringOrder.h"

const char* MatrixWiringOrderName(MatrixWiringOrder order)
{
    switch(order)
    {
    case MatrixWiringOrder::HorizontalTopLeft:         return "Horizontal · top-left";
    case MatrixWiringOrder::HorizontalTopLeftZigzag:   return "Horizontal · top-left zigzag";
    case MatrixWiringOrder::HorizontalTopRight:        return "Horizontal · top-right";
    case MatrixWiringOrder::HorizontalTopRightZigzag:  return "Horizontal · top-right zigzag";
    case MatrixWiringOrder::HorizontalBottomLeft:      return "Horizontal · bottom-left";
    case MatrixWiringOrder::HorizontalBottomLeftZigzag:return "Horizontal · bottom-left zigzag";
    case MatrixWiringOrder::HorizontalBottomRight:     return "Horizontal · bottom-right";
    case MatrixWiringOrder::HorizontalBottomRightZigzag:return "Horizontal · bottom-right zigzag";
    case MatrixWiringOrder::VerticalTopLeft:           return "Vertical · top-left";
    case MatrixWiringOrder::VerticalTopLeftZigzag:     return "Vertical · top-left zigzag";
    case MatrixWiringOrder::VerticalTopRight:          return "Vertical · top-right";
    case MatrixWiringOrder::VerticalTopRightZigzag:    return "Vertical · top-right zigzag";
    case MatrixWiringOrder::VerticalBottomLeft:        return "Vertical · bottom-left";
    case MatrixWiringOrder::VerticalBottomLeftZigzag:  return "Vertical · bottom-left zigzag";
    case MatrixWiringOrder::VerticalBottomRight:       return "Vertical · bottom-right";
    case MatrixWiringOrder::VerticalBottomRightZigzag: return "Vertical · bottom-right zigzag";
    default:                                            return "Horizontal · top-left zigzag";
    }
}

void FillMatrixWiringMap(unsigned int height,
                         unsigned int width,
                         unsigned int led_count,
                         MatrixWiringOrder order,
                         std::vector<unsigned int>& out_map)
{
    out_map.assign(static_cast<size_t>(height) * static_cast<size_t>(width), kMatrixMapUnused);
    if(height == 0 || width == 0 || led_count == 0)
    {
        return;
    }

    bool outer_loop_y = true;
    bool outer_loop_inverted = false;
    bool inner_loop_inverted = false;
    bool zigzag = false;

    switch(order)
    {
    case MatrixWiringOrder::HorizontalTopLeft:
    case MatrixWiringOrder::HorizontalTopLeftZigzag:
        break;
    case MatrixWiringOrder::HorizontalTopRight:
    case MatrixWiringOrder::HorizontalTopRightZigzag:
        inner_loop_inverted = true;
        break;
    case MatrixWiringOrder::HorizontalBottomLeft:
    case MatrixWiringOrder::HorizontalBottomLeftZigzag:
        outer_loop_inverted = true;
        break;
    case MatrixWiringOrder::HorizontalBottomRight:
    case MatrixWiringOrder::HorizontalBottomRightZigzag:
        outer_loop_inverted = true;
        inner_loop_inverted = true;
        break;
    case MatrixWiringOrder::VerticalTopLeft:
    case MatrixWiringOrder::VerticalTopLeftZigzag:
        outer_loop_y = false;
        break;
    case MatrixWiringOrder::VerticalTopRight:
    case MatrixWiringOrder::VerticalTopRightZigzag:
        outer_loop_y = false;
        outer_loop_inverted = true;
        break;
    case MatrixWiringOrder::VerticalBottomLeft:
    case MatrixWiringOrder::VerticalBottomLeftZigzag:
        outer_loop_y = false;
        inner_loop_inverted = true;
        break;
    case MatrixWiringOrder::VerticalBottomRight:
    case MatrixWiringOrder::VerticalBottomRightZigzag:
        outer_loop_y = false;
        outer_loop_inverted = true;
        inner_loop_inverted = true;
        break;
    default:
        break;
    }

    switch(order)
    {
    case MatrixWiringOrder::HorizontalTopLeftZigzag:
    case MatrixWiringOrder::HorizontalTopRightZigzag:
    case MatrixWiringOrder::HorizontalBottomLeftZigzag:
    case MatrixWiringOrder::HorizontalBottomRightZigzag:
    case MatrixWiringOrder::VerticalTopLeftZigzag:
    case MatrixWiringOrder::VerticalTopRightZigzag:
    case MatrixWiringOrder::VerticalBottomLeftZigzag:
    case MatrixWiringOrder::VerticalBottomRightZigzag:
        zigzag = true;
        break;
    default:
        break;
    }

    unsigned int led_idx = 0;
    const int outer_max = static_cast<int>(outer_loop_y ? height : width);
    const int inner_max = static_cast<int>(outer_loop_y ? width : height);

    for(int outer = (outer_loop_inverted ? (outer_max - 1) : 0);
        outer_loop_inverted ? (outer >= 0) : (outer < outer_max);
        outer_loop_inverted ? (--outer) : (++outer))
    {
        for(int inner = (inner_loop_inverted ? (inner_max - 1) : 0);
            inner_loop_inverted ? (inner >= 0) : (inner < inner_max);
            inner_loop_inverted ? (--inner) : (++inner))
        {
            const unsigned int row = static_cast<unsigned int>(outer_loop_y ? outer : inner);
            const unsigned int col = static_cast<unsigned int>(outer_loop_y ? inner : outer);
            if(led_idx < led_count)
            {
                out_map[row * width + col] = led_idx;
            }
            ++led_idx;
        }
        if(zigzag)
        {
            inner_loop_inverted = !inner_loop_inverted;
        }
    }
}
