// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include "EffectPacks/EffectPack.h"
#include "LEDPosition3D.h"
#include <QSet>
#include <QString>
#include <QVector>
#include <QWidget>
#include <memory>
#include <vector>

struct ControllerTransform;

/**
 * Sequencer-style timeline: All (this pack) + selected controllers.
 * Click [+] to expand zones, then LEDs. Blocks: move / resize / delete / right-click colour.
 */
class EffectPackTimelineWidget : public QWidget
{
    Q_OBJECT

public:
    struct Node
    {
        QString label;
        EffectPack::Target target;
        QVector<Node> children;
        bool expanded = false;
        /** Total LEDs under this node (device/zone/All) for in-block spatial preview. */
        int led_count = 1;
    };

    struct Row
    {
        QString label;
        EffectPack::Target target;
        int depth = 0;
        QVector<int> path;
        bool expandable = false;
        bool expanded = false;
        /** For LED rows under a zone/device: index within sibling LEDs for spatial preview. */
        int led_index = 0;
        int led_count = 1;
        bool single_led_row = false;
    };

    explicit EffectPackTimelineWidget(QWidget* parent = nullptr);

    void setPack(EffectPack::Pack* pack);
    /** Scene transforms for world-space wipe/chase preview inside blocks. */
    void setControllerTransforms(std::vector<std::unique_ptr<ControllerTransform>>* transforms);
    void setModel(QVector<Node> roots);
    void setDurationMs(int duration_ms);
    void setPlayheadMs(int ms);
    int playheadMs() const { return playhead_ms_; }
    void setPixelsPerSecond(double pps);
    double pixelsPerSecond() const { return pixels_per_second_; }

    int rowHeight() const { return row_height_; }
    int headerHeight() const { return header_height_; }
    int gutterWidth() const { return gutter_width_; }
    int selectedTrackIndex() const { return selected_track_; }
    int selectedBlockIndex() const { return selected_block_; }
    int selectedRowIndex() const { return selected_row_; }

    int trackIndexForRow(int row) const;
    const QVector<Row>& visibleRows() const { return visible_rows_; }
    const QVector<Row>& rows() const { return visible_rows_; }

public slots:
    void setSelectedBlock(int track_index, int block_index);

signals:
    void playheadChanged(int ms);
    void blockSelected(int track_index, int block_index);
    void blockEdited(int track_index, int block_index);
    void blockDeleteRequested(int track_index, int block_index);
    /** Place a new effect on a row at time (from right-click menu / effects palette). */
    void effectAddRequested(int row_index, int ms, int block_type);
    void rowSelected(int row_index);
    void contentHeightChanged(int height);
    void modelExpandedChanged();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

private:
    enum class DragOp
    {
        None,
        Move,
        ResizeStart,
        ResizeEnd,
        ScrubHeader
    };

    enum class BlockHit
    {
        None,
        Body,
        LeftEdge,
        RightEdge
    };

    struct PaintBlock
    {
        const EffectPack::Block* block = nullptr;
        int track = -1;
        int block_index = -1;
        int led_index = 0;
        int led_count = 1;
        bool single_led_row = false;
        EffectPack::Target view_target;
    };

    int timeToX(int ms) const;
    int xToTime(int x) const;
    int snapMs(int ms) const;
    int contentWidth() const;
    int contentHeight() const;
    void updateGeometrySize();
    void rebuildVisibleRows();
    void flattenNode(const Node& node, const QVector<int>& path, int depth, int led_index, int led_count);
    Node* nodeAtPath(const QVector<int>& path);
    const Node* nodeAtPath(const QVector<int>& path) const;
    void toggleExpand(const QVector<int>& path);
    bool hitTestBlock(int x, int y, int* out_row, int* out_track, int* out_block,
                      BlockHit* out_hit = nullptr) const;
    QRect blockRect(int row, const EffectPack::Block& block) const;
    EffectPack::Block* mutableBlock(int track, int block);
    void applyDrag(int mouse_x);
    void finishDrag();
    void updateHoverCursor(int x, int y);
    void editBlockColorAt(int track, int block, const QPoint& global_pos);
    void showAddEffectMenu(int row, int ms, const QPoint& global_pos);
    QString targetKey(const EffectPack::Target& t) const;
    void captureExpandState(QSet<QString>* expanded_keys) const;
    void restoreExpandState(const QSet<QString>& expanded_keys);
    QVector<PaintBlock> paintBlocksForRow(int row) const;
    void paintBlockVisual(QPainter& p, const QRect& br, const PaintBlock& pb, bool selected) const;
    void paintBlockGradientBar(QPainter& p, const QRect& br, const EffectPack::Block& block, int alpha) const;
    void paintBlockSpatialRaster(QPainter& p, const QRect& br, const PaintBlock& pb,
                                 const EffectPack::Block& sample) const;

    EffectPack::Pack* pack_ = nullptr;
    std::vector<std::unique_ptr<ControllerTransform>>* transforms_ = nullptr;
    QVector<Node> roots_;
    QVector<Row> visible_rows_;
    int duration_ms_ = 5000;
    int playhead_ms_ = 0;
    double pixels_per_second_ = 80.0;
    int row_height_ = 28;
    int header_height_ = 24;
    int gutter_width_ = 200;
    int expand_hit_ = 18;
    int edge_hit_px_ = 8;
    int min_block_ms_ = 50;
    int snap_ms_ = 10;
    int selected_track_ = -1;
    int selected_block_ = -1;
    int selected_row_ = -1;

    DragOp drag_op_ = DragOp::None;
    int drag_track_ = -1;
    int drag_block_ = -1;
    int drag_origin_start_ = 0;
    int drag_origin_end_ = 0;
    int drag_grab_offset_ms_ = 0;
    bool drag_moved_ = false;
};
