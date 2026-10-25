// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include "Actions/Action.h"
#include "Actions/ActionApplier.h"
#include "LEDPosition3D.h"
#include "filesystem.h"
#include <QMenu>
#include <QPixmap>
#include <QPoint>
#include <QSet>
#include <QString>
#include <QVector>
#include <QWidget>
#include <functional>
#include <memory>
#include <vector>

class QMimeData;
class ZoneManager3D;
struct ControllerTransform;

/**
 * Sequencer-style timeline: All (this pack) + selected controllers.
 * Click [+] to expand zones, then LEDs. Clips: move / resize / delete / right-click colour.
 */
class ActionTimelineWidget : public QWidget
{
    Q_OBJECT

public:
    struct Node
    {
        QString label;
        Action::Target target;
        QVector<Node> children;
        bool expanded = false;
        /** Total LEDs under this node (device/zone/All) for in-clip spatial preview. */
        int led_count = 1;
        /** Scene controller_transforms index when this node is a device under a scene zone. */
        int transform_index = -1;
        /** Parent scene zone name for drag-reorder of controllers. */
        QString scene_zone_name;
        bool reorderable = false;
    };

    struct Row
    {
        QString label;
        Action::Target target;
        int depth = 0;
        QVector<int> path;
        bool expandable = false;
        bool expanded = false;
        /** For LED rows under a zone/device: index within sibling LEDs for spatial preview. */
        int led_index = 0;
        int led_count = 1;
        bool single_led_row = false;
        int transform_index = -1;
        QString scene_zone_name;
        bool reorderable = false;
    };

    explicit ActionTimelineWidget(QWidget* parent = nullptr);

    void setPack(Action::Pack* pack);
    void setEffectFilesDir(const filesystem::path& dir);
    /** Scene transforms for world-space wipe/chase preview inside clips. */
    void setControllerTransforms(std::vector<std::unique_ptr<ControllerTransform>>* transforms);
    void setZoneManager(ZoneManager3D* zone_manager);
    /** Full room AABB for Room/field timeline rasters (never LED cluster). */
    void setRoomWorldBounds(const Action::ApplySpatialContext& room);
    void setModel(QVector<Node> roots);
    void setDurationMs(int duration_ms);
    void setPlayheadMs(int ms);
    /** Mark clip/row content stale and schedule a full timeline rebuild (not playhead-only). */
    void requestContentRepaint();
    int playheadMs() const { return playhead_ms_; }
    void setPixelsPerSecond(double pps);
    double pixelsPerSecond() const { return pixels_per_second_; }

    int rowHeight() const { return row_height_; }
    int headerHeight() const { return header_height_; }
    int gutterWidth() const { return gutter_width_; }
    int selectedTrackIndex() const { return selected_track_; }
    int selectedClipIndex() const { return selected_clip_; }
    int selectedRowIndex() const { return selected_row_; }
    bool isClipDragging() const { return drag_op_ == DragOp::Move
                                       || drag_op_ == DragOp::ResizeStart
                                       || drag_op_ == DragOp::ResizeEnd
                                       || drag_op_ == DragOp::ResizeVertical; }

    int trackIndexForRow(int row) const;
    const QVector<Row>& visibleRows() const { return visible_rows_; }
    const QVector<Row>& rows() const { return visible_rows_; }
    /** Targets for the row node and all descendants (including collapsed). */
    QVector<Action::Target> subtreeTargetsForRow(int row) const;
    bool hasClipboardClip() const { return clipboard_has_clip_; }

public slots:
    void setSelectedClip(int track_index, int clip_index);
    /** Abort in-progress move/resize so delete/swap cannot mutate stale indices. */
    void cancelDrag();

signals:
    void playheadChanged(int ms);
    void clipSelected(int track_index, int clip_index);
    void clipEdited(int track_index, int clip_index);
    void clipDeleteRequested(int track_index, int clip_index);
    /** Place a new effect on a row at time (from right-click menu / effects palette / drag-drop). */
    void effectAddRequested(int row_index, int ms, const QString& effect_id);
    /** Gradient preset id dropped or applied onto a clip (e.g. "rainbow"). */
    void gradientPresetApplied(int track_index, int clip_index, const QString& preset_id);
    void curvePresetApplied(int track_index, int clip_index, const QString& preset_id);
    void colorDropped(int row_index, int ms, unsigned int rgb);
    void gradientDropped(int row_index, int ms, const QString& preset_id);
    void rowSelected(int row_index);
    void contentHeightChanged(int height);
    void modelExpandedChanged();
    /** Controllers under a scene zone were reordered (new Zone3D controller index list). */
    void sceneZoneControllersReordered(const QString& scene_zone_name, const QVector<int>& controller_indices);
    /** Preview the whole pack starting at local time (0 = beginning). */
    void previewPackRequested(int start_ms);
    /** Preview only the selected row's track subtree starting at local time. */
    void previewRowRequested(int row_index, int start_ms);
    /** Preview a single pack track starting at local time. */
    void previewTrackRequested(int track_index, int start_ms);
    void stopPreviewRequested();
    /** A clip was added via paste/duplicate/vertical span (selection already updated). */
    void clipCreated(int track_index, int clip_index);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

private:
    enum class DragOp
    {
        None,
        Move,
        ResizeStart,
        ResizeEnd,
        ResizeVertical,
        ScrubHeader,
        Marquee
    };

    enum class ClipHit
    {
        None,
        Body,
        LeftEdge,
        RightEdge,
        TopEdge,
        BottomEdge
    };

    struct ClipId
    {
        int track = -1;
        int clip = -1;
        bool operator==(const ClipId& o) const { return track == o.track && clip == o.clip; }
    };

    struct DragMember
    {
        int track = -1;
        int clip = -1;
        int origin_start = 0;
        int origin_end = 0;
    };

    struct PaintClip
    {
        const Action::Clip* clip = nullptr;
        int track = -1;
        int clip_index = -1;
        int led_index = 0;
        int led_count = 1;
        bool single_led_row = false;
        Action::Target view_target;
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
    bool hitTestClip(int x, int y, int* out_row, int* out_track, int* out_clip,
                      ClipHit* out_hit = nullptr) const;
    QRect clipRect(int row, const Action::Clip& clip) const;
    /** Absolute rect of a clip on any row it covers (for marquee hit-testing). */
    QRect clipAbsRect(int track, int clip_index) const;
    Action::Clip* mutableClip(int track, int clip);
    void applyDrag(int mouse_x, int mouse_y);
    void finishDrag();
    void clearClipSelection();
    void setPrimarySelection(int track, int clip);
    void toggleClipSelection(int track, int clip);
    bool isClipSelected(int track, int clip) const;
    void selectClipsInRect(const QRect& rect, bool additive);
    void beginDragGroupFromSelection();
    void forkDragGroupCopies();
    void applyDragGroupTimeDelta(int primary_new_start);
    void applyDragGroupResizeStart(int primary_new_start);
    void applyDragGroupResizeEnd(int primary_new_end);
    bool deleteSelectedClips();
    /** Toggle/apply transform flags on every selected clip. */
    bool mutateSelectedClips(const std::function<void(Action::Clip&)>& mutator);
    /** Find pack track for a visible row, or create one matching that row's target. */
    int findOrCreateTrackForRow(int row);
    /** Move the in-progress drag clip onto dest_track; updates drag_/selection indices. */
    bool moveDragClipToTrack(int dest_track);
    /** Copy clip onto dest_track; returns new clip index or -1. */
    int copyClipToTrack(int src_track, int src_clip, int dest_track, int start_ms = -1);
    /** Expand one clip across rows as a single multi-LED / parent target (not per-row clones). */
    bool expandClipAcrossRows(int src_track, int src_clip, int row_a, int row_b,
                               int* out_track = nullptr, int* out_clip = nullptr);
    /** True if track's target paints onto this timeline row. */
    bool trackCoversRow(const Action::Track& track, const Row& row) const;
    /** Inclusive visible-row span covered by a track (for continuous clip paint). */
    bool coveredRowSpanForTrack(int track_index, int* out_lo, int* out_hi) const;
    void copySelectedClipToClipboard();
    bool pasteClipboardClipAt(int row, int ms);
    bool duplicateSelectedClip();
    void copyRowEffectsToClipboard(int row);
    bool pasteClipboardRowEffectsAt(int row);
    void updateHoverCursor(int x, int y);
    void editClipColorAt(int track, int clip, const QPoint& global_pos);
    void showRowContextMenu(int row, int ms, const QPoint& global_pos);
    void showClipContextMenu(int row, int track, int clip, const QPoint& global_pos);
    void showHeaderContextMenu(const QPoint& global_pos);
    void populateAddEffectMenu(QMenu* menu, int row, int ms);
    void applyColorToClip(int track, int clip, RGBColor color);
    bool dropAt(const QPoint& pos, const QMimeData* mime);
    QString targetKey(const Action::Target& t) const;
    void captureExpandState(QSet<QString>* expanded_keys) const;
    void restoreExpandState(const QSet<QString>& expanded_keys);
    QVector<PaintClip> paintClipsForRow(int row) const;
    void paintClipVisual(QPainter& p, const QRect& br, const PaintClip& pb, bool selected) const;
    void paintClipGradientBar(QPainter& p, const QRect& br, const Action::Clip& clip, int alpha) const;
    void paintClipSpatialRaster(QPainter& p, const QRect& br, const PaintClip& pb,
                                 const Action::Clip& sample) const;
    void paintClipIntensityCurve(QPainter& p, const QRect& br, const Action::Clip& clip) const;
    void paintTimelineContent(QPainter& p) const;

    Action::Pack* pack_ = nullptr;
    std::vector<std::unique_ptr<ControllerTransform>>* transforms_ = nullptr;
    ZoneManager3D* zone_manager_ = nullptr;
    Action::ApplySpatialContext room_bounds_;
    QVector<Node> roots_;
    QVector<Row> visible_rows_;
    int duration_ms_ = 5000;
    int playhead_ms_ = 0;
    /** Cached timeline without playhead — playhead ticks blit this instead of re-rasterizing clips. */
    mutable QPixmap content_cache_;
    mutable bool content_dirty_ = true;
    filesystem::path effect_files_dir_;
    double pixels_per_second_ = 80.0;
    int row_height_ = 28;
    int header_height_ = 24;
    int gutter_width_ = 200;
    int expand_hit_ = 18;
    int edge_hit_px_ = 8;
    int min_clip_ms_ = 50;
    int snap_ms_ = 10;
    int selected_track_ = -1;
    int selected_clip_ = -1;
    int selected_row_ = -1;
    QVector<ClipId> selection_;

    DragOp drag_op_ = DragOp::None;
    int drag_track_ = -1;
    int drag_clip_ = -1;
    int drag_origin_start_ = 0;
    int drag_origin_end_ = 0;
    int drag_grab_offset_ms_ = 0;
    bool drag_moved_ = false;
    bool drag_copy_ = false;
    /** Source track when Ctrl+drag started; never move the copy back onto it. */
    int drag_copy_source_track_ = -1;
    /** Row under cursor during Move; track created only on release if needed. */
    int drag_dest_row_ = -1;
    /** Row the move started on (ghost placeholder). */
    int drag_home_row_ = -1;
    /** Floating clip Y follows the pointer during Move. */
    int drag_float_y_ = 0;
    int drag_grab_offset_y_ = 0;
    int drag_span_anchor_row_ = -1;
    int drag_span_hover_row_ = -1;
    QVector<DragMember> drag_group_;
    QPoint marquee_origin_;
    QPoint marquee_current_;
    bool marquee_additive_ = false;

    /** Gutter drag to reorder controllers under a scene zone. */
    int row_reorder_from_ = -1;
    int row_reorder_hover_ = -1;

    bool clipboard_has_clip_ = false;
    Action::Clip clipboard_clip_;
    bool clipboard_has_row_ = false;
    QVector<Action::Clip> clipboard_row_clips_;
    QVector<Action::Clip> clipboard_multi_clips_;
    bool clipboard_has_multi_ = false;
};
