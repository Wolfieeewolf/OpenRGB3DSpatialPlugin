// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include "EffectPacks/EffectPack.h"
#include <QVector>
#include <QWidget>

class EffectPackTimelineWidget : public QWidget
{
    Q_OBJECT

public:
    struct Row
    {
        QString label;
        EffectPack::Target target;
    };

    explicit EffectPackTimelineWidget(QWidget* parent = nullptr);

    void setPack(EffectPack::Pack* pack);
    void setRows(const QVector<Row>& rows);
    void setDurationMs(int duration_ms);
    void setPlayheadMs(int ms);
    int playheadMs() const { return playhead_ms_; }
    void setPixelsPerSecond(double pps);
    double pixelsPerSecond() const { return pixels_per_second_; }

    int rowHeight() const { return row_height_; }
    int headerHeight() const { return header_height_; }
    int selectedTrackIndex() const { return selected_track_; }
    int selectedBlockIndex() const { return selected_block_; }

    /** Resolve pack track index for a timeline row (-1 if none). */
    int trackIndexForRow(int row) const;
    const QVector<Row>& rows() const { return rows_; }

public slots:
    void setSelectedBlock(int track_index, int block_index);

signals:
    void playheadChanged(int ms);
    void blockSelected(int track_index, int block_index);
    void emptyCellClicked(int row_index, int ms);
    void contentHeightChanged(int height);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

private:
    int timeToX(int ms) const;
    int xToTime(int x) const;
    int contentWidth() const;
    int contentHeight() const;
    void updateGeometrySize();
    bool hitTestBlock(int x, int y, int* out_row, int* out_track, int* out_block) const;

    EffectPack::Pack* pack_ = nullptr;
    QVector<Row> rows_;
    int duration_ms_ = 5000;
    int playhead_ms_ = 0;
    double pixels_per_second_ = 80.0;
    int row_height_ = 28;
    int header_height_ = 24;
    int selected_track_ = -1;
    int selected_block_ = -1;
};
