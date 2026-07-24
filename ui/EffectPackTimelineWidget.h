// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include "EffectPacks/EffectPack.h"
#include <QSet>
#include <QString>
#include <QVector>
#include <QWidget>

/**
 * Sequencer-style timeline: one row per controller (device).
 * Click [+] to expand into zone layers, then LED layers under a zone.
 * Blocks: drag middle to move, drag edges to resize, right-click to recolour.
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
    };

    /** Flattened visible row for hit-testing / block placement. */
    struct Row
    {
        QString label;
        EffectPack::Target target;
        int depth = 0;
        /** Index path into the node tree (empty = synthetic). */
        QVector<int> path;
        bool expandable = false;
        bool expanded = false;
    };

    explicit EffectPackTimelineWidget(QWidget* parent = nullptr);

    void setPack(EffectPack::Pack* pack);
    /** Replace the model tree (controllers → zones → LEDs). Preserves expand state by target key when possible. */
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
    /** Alias used by editor helpers. */
    const QVector<Row>& rows() const { return visible_rows_; }

public slots:
    void setSelectedBlock(int track_index, int block_index);

signals:
    void playheadChanged(int ms);
    void blockSelected(int track_index, int block_index);
    /** Fired after move/resize or in-place colour edit. */
    void blockEdited(int track_index, int block_index);
    void emptyCellClicked(int row_index, int ms);
    void rowSelected(int row_index);
    void contentHeightChanged(int height);
    void modelExpandedChanged();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
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

    int timeToX(int ms) const;
    int xToTime(int x) const;
    int snapMs(int ms) const;
    int contentWidth() const;
    int contentHeight() const;
    void updateGeometrySize();
    void rebuildVisibleRows();
    void flattenNode(const Node& node, const QVector<int>& path, int depth);
    Node* nodeAtPath(const QVector<int>& path);
    const Node* nodeAtPath(const QVector<int>& path) const;
    void toggleExpand(const QVector<int>& path);
    bool hitTestBlock(int x, int y, int* out_row, int* out_track, int* out_block, BlockHit* out_hit = nullptr) const;
    QRect blockRect(int row, const EffectPack::Block& block) const;
    EffectPack::Block* mutableBlock(int track, int block);
    void applyDrag(int mouse_x);
    void finishDrag();
    void updateHoverCursor(int x, int y);
    void editBlockColorAt(int track, int block, const QPoint& global_pos);
    QString targetKey(const EffectPack::Target& t) const;
    void captureExpandState(QSet<QString>* expanded_keys) const;
    void restoreExpandState(const QSet<QString>& expanded_keys);

    EffectPack::Pack* pack_ = nullptr;
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
