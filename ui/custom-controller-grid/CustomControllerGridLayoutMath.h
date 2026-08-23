// SPDX-License-Identifier: GPL-2.0-only

#ifndef CUSTOMCONTROLLERGRIDLAYOUTMATH_H
#define CUSTOMCONTROLLERGRIDLAYOUTMATH_H

#include <QVector>
#include <QtGlobal>
#include <algorithm>
#include <cmath>

#include "GridSpaceUtils.h"

namespace CustomControllerGridLayoutMath
{

constexpr qreal kHeaderSizeMinScene = 0.85;
constexpr qreal kHeaderSizeMaxScene = 5.0;
constexpr qreal kHeaderSizeRatio    = 0.32;
constexpr float  kMinCellSizeMm       = 0.5f;
constexpr float  kDefaultCellSizeMm   = 10.0f;

inline float MmPerSceneUnit(float grid_scale_mm)
{
    return SafeGridScaleMm(grid_scale_mm);
}

inline qreal MmToScene(float mm, float mm_per_unit)
{
    return static_cast<qreal>(MMToGridUnits(mm, mm_per_unit));
}

inline float SceneToMm(qreal scene, float mm_per_unit)
{
    return GridUnitsToMM(static_cast<float>(scene), mm_per_unit);
}

inline float SumSizes(const QVector<float>& sizes)
{
    float total = 0.0f;
    for(float size : sizes)
    {
        total += size;
    }
    return total;
}

inline qreal SumSceneSizes(const QVector<float>& sizes_mm, float mm_per_unit)
{
    qreal total = 0.0;
    for(float mm : sizes_mm)
    {
        total += MmToScene(mm, mm_per_unit);
    }
    return total;
}

inline qreal AvgSceneSize(const QVector<float>& sizes_mm, float mm_per_unit)
{
    if(sizes_mm.isEmpty())
    {
        return MmToScene(kDefaultCellSizeMm, mm_per_unit);
    }
    return SumSceneSizes(sizes_mm, mm_per_unit) / sizes_mm.size();
}

inline qreal ColHeaderHeightScene(const QVector<float>& column_widths_mm, float mm_per_unit)
{
    return std::clamp(AvgSceneSize(column_widths_mm, mm_per_unit) * kHeaderSizeRatio,
                      kHeaderSizeMinScene,
                      kHeaderSizeMaxScene);
}

inline qreal RowHeaderWidthScene(const QVector<float>& row_heights_mm, float mm_per_unit)
{
    return std::clamp(AvgSceneSize(row_heights_mm, mm_per_unit) * kHeaderSizeRatio,
                      kHeaderSizeMinScene,
                      kHeaderSizeMaxScene);
}

inline float ColumnWidthMm(const QVector<float>& column_widths_mm, int column)
{
    return (column >= 0 && column < column_widths_mm.size())
        ? column_widths_mm[column]
        : kDefaultCellSizeMm;
}

inline float RowHeightMm(const QVector<float>& row_heights_mm, int row)
{
    return (row >= 0 && row < row_heights_mm.size())
        ? row_heights_mm[row]
        : kDefaultCellSizeMm;
}

inline qreal ColumnWidthScene(const QVector<float>& column_widths_mm, int column, float mm_per_unit)
{
    return MmToScene(ColumnWidthMm(column_widths_mm, column), mm_per_unit);
}

inline qreal RowHeightScene(const QVector<float>& row_heights_mm, int row, float mm_per_unit)
{
    return MmToScene(RowHeightMm(row_heights_mm, row), mm_per_unit);
}

inline qreal ColumnLeftScene(const QVector<float>& column_widths_mm,
                             const QVector<float>& row_heights_mm,
                             int column,
                             float mm_per_unit)
{
    qreal x = RowHeaderWidthScene(row_heights_mm, mm_per_unit);
    for(int i = 0; i < column; ++i)
    {
        x += ColumnWidthScene(column_widths_mm, i, mm_per_unit);
    }
    return x;
}

inline qreal RowTopScene(const QVector<float>& row_heights_mm,
                         const QVector<float>& column_widths_mm,
                         int row,
                         float mm_per_unit)
{
    qreal y = ColHeaderHeightScene(column_widths_mm, mm_per_unit);
    for(int i = 0; i < row; ++i)
    {
        y += RowHeightScene(row_heights_mm, i, mm_per_unit);
    }
    return y;
}

inline qreal TotalGridWidthScene(const QVector<float>& column_widths_mm,
                                 const QVector<float>& row_heights_mm,
                                 float mm_per_unit)
{
    return RowHeaderWidthScene(row_heights_mm, mm_per_unit)
           + SumSceneSizes(column_widths_mm, mm_per_unit);
}

inline qreal TotalGridHeightScene(const QVector<float>& row_heights_mm,
                                  const QVector<float>& column_widths_mm,
                                  float mm_per_unit)
{
    return ColHeaderHeightScene(column_widths_mm, mm_per_unit)
           + SumSceneSizes(row_heights_mm, mm_per_unit);
}

inline bool CellAtScenePos(const QVector<float>& column_widths_mm,
                           const QVector<float>& row_heights_mm,
                           int grid_width,
                           int grid_height,
                           const QPointF& scene_pos,
                           float mm_per_unit,
                           int* column,
                           int* row)
{
    if(!column || !row || grid_width <= 0 || grid_height <= 0)
    {
        return false;
    }

    const qreal row_header_w = RowHeaderWidthScene(row_heights_mm, mm_per_unit);
    const qreal col_header_h = ColHeaderHeightScene(column_widths_mm, mm_per_unit);
    if(scene_pos.x() < row_header_w || scene_pos.y() < col_header_h)
    {
        return false;
    }

    const qreal local_x = scene_pos.x() - row_header_w;
    const qreal local_y = scene_pos.y() - col_header_h;

    qreal x_cursor = 0.0;
    int col        = -1;
    for(int c = 0; c < grid_width; ++c)
    {
        const qreal width = ColumnWidthScene(column_widths_mm, c, mm_per_unit);
        if(local_x >= x_cursor && local_x < x_cursor + width)
        {
            col = c;
            break;
        }
        x_cursor += width;
    }

    qreal y_cursor = 0.0;
    int r          = -1;
    for(int y = 0; y < grid_height; ++y)
    {
        const qreal height = RowHeightScene(row_heights_mm, y, mm_per_unit);
        if(local_y >= y_cursor && local_y < y_cursor + height)
        {
            r = y;
            break;
        }
        y_cursor += height;
    }

    if(col < 0 || r < 0)
    {
        return false;
    }

    *column = col;
    *row    = r;
    return true;
}

inline int ColumnResizeIndexAtScenePos(const QVector<float>& column_widths_mm,
                                       const QVector<float>& row_heights_mm,
                                       int grid_width,
                                       const QPointF& scene_pos,
                                       qreal hit_slop_scene,
                                       float mm_per_unit)
{
    if(grid_width <= 0 || scene_pos.y() < ColHeaderHeightScene(column_widths_mm, mm_per_unit))
    {
        return -1;
    }

    qreal best_distance = hit_slop_scene + 1.0;
    int   best_column   = -1;
    qreal border_x      = RowHeaderWidthScene(row_heights_mm, mm_per_unit);
    for(int col = 0; col < grid_width; ++col)
    {
        border_x += ColumnWidthScene(column_widths_mm, col, mm_per_unit);
        const qreal distance = std::abs(scene_pos.x() - border_x);
        if(distance <= hit_slop_scene && distance < best_distance)
        {
            best_distance = distance;
            best_column   = col;
        }
    }

    return best_column;
}

inline int RowResizeIndexAtScenePos(const QVector<float>& row_heights_mm,
                                    const QVector<float>& column_widths_mm,
                                    int grid_height,
                                    const QPointF& scene_pos,
                                    qreal hit_slop_scene,
                                    float mm_per_unit)
{
    if(grid_height <= 0 || scene_pos.x() < RowHeaderWidthScene(row_heights_mm, mm_per_unit))
    {
        return -1;
    }

    qreal best_distance = hit_slop_scene + 1.0;
    int   best_row      = -1;
    qreal border_y      = ColHeaderHeightScene(column_widths_mm, mm_per_unit);
    for(int row = 0; row < grid_height; ++row)
    {
        border_y += RowHeightScene(row_heights_mm, row, mm_per_unit);
        const qreal distance = std::abs(scene_pos.y() - border_y);
        if(distance <= hit_slop_scene && distance < best_distance)
        {
            best_distance = distance;
            best_row      = row;
        }
    }

    return best_row;
}

inline int ColumnBorderIndexAtScenePos(const QVector<float>& column_widths_mm,
                                       const QVector<float>& row_heights_mm,
                                       int grid_width,
                                       const QPointF& scene_pos,
                                       qreal hit_slop_scene,
                                       float mm_per_unit)
{
    return ColumnResizeIndexAtScenePos(
        column_widths_mm, row_heights_mm, grid_width, scene_pos, hit_slop_scene, mm_per_unit);
}

inline int RowBorderIndexAtScenePos(const QVector<float>& row_heights_mm,
                                    const QVector<float>& column_widths_mm,
                                    int grid_height,
                                    const QPointF& scene_pos,
                                    qreal hit_slop_scene,
                                    float mm_per_unit)
{
    return RowResizeIndexAtScenePos(
        row_heights_mm, column_widths_mm, grid_height, scene_pos, hit_slop_scene, mm_per_unit);
}

inline int ColumnHeaderIndexAtScenePos(const QVector<float>& column_widths_mm,
                                       const QVector<float>& row_heights_mm,
                                       int grid_width,
                                       const QPointF& scene_pos,
                                       float mm_per_unit)
{
    const qreal row_header_w = RowHeaderWidthScene(row_heights_mm, mm_per_unit);
    const qreal col_header_h = ColHeaderHeightScene(column_widths_mm, mm_per_unit);
    if(grid_width <= 0 || scene_pos.y() < 0.0 || scene_pos.y() >= col_header_h
       || scene_pos.x() < row_header_w)
    {
        return -1;
    }

    const qreal local_x  = scene_pos.x() - row_header_w;
    qreal       x_cursor = 0.0;
    for(int col = 0; col < grid_width; ++col)
    {
        const qreal width = ColumnWidthScene(column_widths_mm, col, mm_per_unit);
        if(local_x >= x_cursor && local_x < x_cursor + width)
        {
            return col;
        }
        x_cursor += width;
    }
    return -1;
}

inline int RowHeaderIndexAtScenePos(const QVector<float>& row_heights_mm,
                                    const QVector<float>& column_widths_mm,
                                    int grid_height,
                                    const QPointF& scene_pos,
                                    float mm_per_unit)
{
    const qreal row_header_w = RowHeaderWidthScene(row_heights_mm, mm_per_unit);
    const qreal col_header_h = ColHeaderHeightScene(column_widths_mm, mm_per_unit);
    if(grid_height <= 0 || scene_pos.x() < 0.0 || scene_pos.x() >= row_header_w
       || scene_pos.y() < col_header_h)
    {
        return -1;
    }

    const qreal local_y  = scene_pos.y() - col_header_h;
    qreal       y_cursor = 0.0;
    for(int row = 0; row < grid_height; ++row)
    {
        const qreal height = RowHeightScene(row_heights_mm, row, mm_per_unit);
        if(local_y >= y_cursor && local_y < y_cursor + height)
        {
            return row;
        }
        y_cursor += height;
    }
    return -1;
}

} // namespace CustomControllerGridLayoutMath

#endif
