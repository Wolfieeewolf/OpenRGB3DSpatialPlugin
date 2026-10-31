// SPDX-License-Identifier: GPL-2.0-only

#include "CaptureZonesWidget.h"
#include "ui_CaptureZonesWidget.h"
#include "DisplayPlane3D.h"
#include "EffectDoubleSliderRow.h"
#include "EffectSliderPromote.h"
#include "QtCompat.h"
#include "ScreenCaptureManager.h"
#include "ScreenMirrorCalibrationPattern.h"
#include "ScreenMirror_Internal.h"

#include <QVBoxLayout>
#include <QGroupBox>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QPainter>
#include <QMouseEvent>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QPushButton>
#include <QSlider>
#include <QTimer>
#include <algorithm>
#include <cstdint>
#include <functional>
#include <cmath>
#include <limits>

CaptureZone::CaptureZone()
    : u_min(0.0f)
    , u_max(1.0f)
    , v_min(0.0f)
    , v_max(1.0f)
    , enabled(true)
    , name("Zone")
{}

CaptureZone::CaptureZone(float u0, float u1, float v0, float v1)
    : u_min(std::min(u0, u1))
    , u_max(std::max(u0, u1))
    , v_min(std::min(v0, v1))
    , v_max(std::max(v0, v1))
    , enabled(true)
    , name("Zone")
{}

class CaptureAreaPreviewWidget : public QWidget
{
public:
    DisplayPlane3D* display_plane = nullptr;
    std::vector<CaptureZone>* capture_zones = nullptr;
    std::function<void()> valueChangedCallback;
    bool* show_calibration_pattern_ptr = nullptr;
    bool* show_screen_preview_ptr = nullptr;
    float* black_bar_top_percent_ptr = nullptr;
    float* black_bar_bottom_percent_ptr = nullptr;
    float* black_bar_left_percent_ptr = nullptr;
    float* black_bar_right_percent_ptr = nullptr;

    CaptureAreaPreviewWidget(std::vector<CaptureZone>* zones,
                             DisplayPlane3D* plane,
                             bool* calibration_pattern,
                             bool* screen_preview,
                             float* black_bar_top_percent,
                             float* black_bar_bottom_percent,
                             float* black_bar_left_percent,
                             float* black_bar_right_percent,
                             QWidget* parent = nullptr)
        : QWidget(parent)
        , display_plane(plane)
        , capture_zones(zones)
        , show_calibration_pattern_ptr(calibration_pattern)
        , show_screen_preview_ptr(screen_preview)
        , black_bar_top_percent_ptr(black_bar_top_percent)
        , black_bar_bottom_percent_ptr(black_bar_bottom_percent)
        , black_bar_left_percent_ptr(black_bar_left_percent)
        , black_bar_right_percent_ptr(black_bar_right_percent)
        , selected_zone_index(-1)
        , dragging(false)
        , drag_handle(None)
        , preview_last_frame_id_(std::numeric_limits<std::uint64_t>::max())
    {
        setMinimumHeight(200);
        setMaximumHeight(300);
        setToolTip(QStringLiteral(
            "Click a zone to select it. Drag corners to resize, drag the zone to move.\n"
            "Add: + button, Insert / +, or right-click → Add zone.\n"
            "Remove: − button, Delete / Backspace, or right-click → Remove zone "
            "(at least one zone must remain)."));
        setMouseTracking(true);
        setFocusPolicy(Qt::StrongFocus);
        setContextMenuPolicy(Qt::CustomContextMenu);
        connect(this, &QWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
            ShowZoneContextMenu(pos);
        });

        refresh_timer = new QTimer(this);
        connect(refresh_timer, &QTimer::timeout, this, [this]() {
            const bool show_preview = show_screen_preview_ptr && *show_screen_preview_ptr &&
                                    !(show_calibration_pattern_ptr && *show_calibration_pattern_ptr);
            if(show_preview && display_plane)
            {
                const std::string sid = display_plane->GetCaptureSourceId();
                if(!sid.empty())
                {
                    ScreenCaptureManager& capture_mgr = ScreenCaptureManager::Instance();
                    if(capture_mgr.IsInitialized() && capture_mgr.IsCapturing(sid))
                    {
                        std::shared_ptr<CapturedFrame> fr = capture_mgr.GetLatestFrame(sid);
                        if(fr && fr->valid && !fr->data.empty() && fr->frame_id == preview_last_frame_id_)
                        {
                            return;
                        }
                        if(fr && fr->valid && !fr->data.empty())
                        {
                            preview_last_frame_id_ = fr->frame_id;
                        }
                    }
                }
            }
            update();
        });
        refresh_timer->start(OpenRGB3DUi::kScreenPreviewTimerIntervalMs);
    }

    void SetDisplayPlane(DisplayPlane3D* plane)
    {
        display_plane = plane;
        update();
    }

    void SetValueChangedCallback(std::function<void()> callback)
    {
        valueChangedCallback = callback;
    }

    void AddZone()
    {
        if(!capture_zones) return;
        if(capture_zones->size() >= kMaxCaptureZonesPerMonitor) return;
        CaptureZone new_zone(0.4f, 0.6f, 0.4f, 0.6f);
        new_zone.name = "Zone " + std::to_string(capture_zones->size() + 1);
        capture_zones->push_back(new_zone);
        selected_zone_index = (int)capture_zones->size() - 1;
        if(valueChangedCallback) valueChangedCallback();
        update();
    }

    bool CanRemoveZone() const
    {
        return capture_zones && capture_zones->size() > 1;
    }

    void RemoveSelectedZone()
    {
        if(!CanRemoveZone()) return;
        int idx = selected_zone_index;
        if(idx < 0 || idx >= (int)capture_zones->size())
        {
            idx = (int)capture_zones->size() - 1;
        }
        capture_zones->erase(capture_zones->begin() + idx);
        if(selected_zone_index >= (int)capture_zones->size())
        {
            selected_zone_index = (int)capture_zones->size() - 1;
        }
        if(valueChangedCallback) valueChangedCallback();
        update();
    }

    int SelectedZoneIndex() const { return selected_zone_index; }

    void ShowZoneContextMenu(const QPoint& local_pos)
    {
        if(!capture_zones)
        {
            return;
        }
        /* Select zone under cursor when opening the menu. */
        int hit = -1;
        for(size_t i = 0; i < capture_zones->size(); i++)
        {
            const CaptureZone& zone = (*capture_zones)[i];
            if(!zone.enabled) continue;
            const int zone_left = preview_rect.left() + (int)(preview_rect.width() * zone.u_min);
            const int zone_right = preview_rect.left() + (int)(preview_rect.width() * zone.u_max);
            const int zone_top = preview_rect.top() + (int)(preview_rect.height() * (1.0f - zone.v_max));
            const int zone_bottom = preview_rect.top() + (int)(preview_rect.height() * (1.0f - zone.v_min));
            if(QRect(zone_left, zone_top, zone_right - zone_left, zone_bottom - zone_top).contains(local_pos))
            {
                hit = (int)i;
                break;
            }
        }
        if(hit >= 0)
        {
            selected_zone_index = hit;
            update();
        }

        QMenu menu(this);
        QAction* add_act = menu.addAction(QStringLiteral("Add zone"));
        add_act->setEnabled(capture_zones->size() < kMaxCaptureZonesPerMonitor);
        QAction* remove_act = menu.addAction(QStringLiteral("Remove zone"));
        remove_act->setEnabled(CanRemoveZone() && selected_zone_index >= 0);
        QAction* chosen = menu.exec(mapToGlobal(local_pos));
        if(chosen == add_act)
        {
            AddZone();
        }
        else if(chosen == remove_act)
        {
            RemoveSelectedZone();
        }
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        (void)event;
        if(!capture_zones || !display_plane) return;

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        QRect widget_rect = this->rect().adjusted(2, 2, -2, -2);

        float aspect_ratio = 16.0f / 9.0f;
        float width_mm = display_plane->GetWidthMM();
        float height_mm = display_plane->GetHeightMM();
        if(height_mm > 0.0f)
            aspect_ratio = width_mm / height_mm;

        QRect rect = widget_rect;
        float widget_aspect = (float)widget_rect.width() / (float)widget_rect.height();
        if(widget_aspect > aspect_ratio)
        {
            int new_width = (int)(widget_rect.height() * aspect_ratio);
            int x_offset = (widget_rect.width() - new_width) / 2;
            rect = QRect(widget_rect.left() + x_offset, widget_rect.top(), new_width, widget_rect.height());
        }
        else
        {
            int new_height = (int)(widget_rect.width() / aspect_ratio);
            int y_offset = (widget_rect.height() - new_height) / 2;
            rect = QRect(widget_rect.left(), widget_rect.top() + y_offset, widget_rect.width(), new_height);
        }

        preview_rect = rect;
        painter.setPen(QPen(QColor(100, 100, 100), 2));
        painter.setBrush(QBrush(QColor(30, 30, 30)));
        painter.drawRect(rect);

        bool show_calibration = show_calibration_pattern_ptr && *show_calibration_pattern_ptr;
        bool show_preview = show_screen_preview_ptr && *show_screen_preview_ptr && !show_calibration;

        if(show_calibration)
        {
            static QImage cal_image;
            static std::once_flag cal_init;
            std::call_once(cal_init, []() {
                std::vector<uint8_t> buf;
                ScreenMirrorFillCalibrationPatternBuffer(buf);
                cal_image = QImage(kScreenMirrorCalibrationPatternW, kScreenMirrorCalibrationPatternH, QImage::Format_RGBA8888);
                if(!cal_image.isNull())
                {
                    for(int y = 0; y < kScreenMirrorCalibrationPatternH; y++)
                    {
                        memcpy(cal_image.scanLine(y),
                               buf.data() + (size_t)y * (size_t)kScreenMirrorCalibrationPatternW * 4u,
                               (size_t)kScreenMirrorCalibrationPatternW * 4u);
                    }
                }
            });
            if(!cal_image.isNull())
            {
                painter.drawImage(rect, cal_image);
            }
        }
        else if(show_preview && display_plane)
        {
            std::string source_id = display_plane->GetCaptureSourceId();
            if(!source_id.empty())
            {
                ScreenCaptureManager& capture_mgr = ScreenCaptureManager::Instance();
                if(capture_mgr.IsInitialized() && capture_mgr.IsCapturing(source_id))
                {
                    std::shared_ptr<CapturedFrame> frame = capture_mgr.GetLatestFrame(source_id);
                    if(frame && frame->valid && !frame->data.empty())
                    {
                        const int stride = frame->width * 4;
                        QImage wrapped((const uchar*)frame->data.data(), frame->width, frame->height, stride,
                                       QImage::Format_RGBA8888);
                        QImage image = wrapped.copy();
                        if(!image.isNull())
                        {
                            painter.drawImage(rect, image);
                            if(black_bar_top_percent_ptr && black_bar_bottom_percent_ptr &&
                               black_bar_left_percent_ptr && black_bar_right_percent_ptr)
                            {
                                const float top_f =
                                    std::clamp(*black_bar_top_percent_ptr, 0.0f, 49.0f) / 100.0f;
                                const float bottom_f =
                                    std::clamp(*black_bar_bottom_percent_ptr, 0.0f, 49.0f) / 100.0f;
                                const float left_f =
                                    std::clamp(*black_bar_left_percent_ptr, 0.0f, 49.0f) / 100.0f;
                                const float right_f =
                                    std::clamp(*black_bar_right_percent_ptr, 0.0f, 49.0f) / 100.0f;
                                const int left = rect.left() + (int)(rect.width() * left_f);
                                const int right = rect.left() + (int)(rect.width() * (1.0f - right_f));
                                const int top = rect.top() + (int)(rect.height() * top_f);
                                const int bottom = rect.top() + (int)(rect.height() * (1.0f - bottom_f));
                                QRect bounds_rect(left, top, std::max(0, right - left), std::max(0, bottom - top));
                                painter.setPen(QPen(QColor(255, 255, 0), 2, Qt::DashLine));
                                painter.setBrush(Qt::NoBrush);
                                painter.drawRect(bounds_rect);
                            }
                        }
                    }
                }
            }
        }

        const int handle_size = 10;
        const int handle_half = handle_size / 2;
        QColor zone_color(0, 200, 255, 120);
        QColor zone_border(0, 200, 255, 255);
        QColor selected_zone_color(0, 255, 200, 150);
        QColor selected_zone_border(0, 255, 200, 255);
        QColor handle_color(100, 200, 255, 255);
        QColor handle_hover_color(150, 255, 255, 255);

        for(size_t i = 0; i < capture_zones->size(); i++)
        {
            const CaptureZone& zone = (*capture_zones)[i];
            if(!zone.enabled) continue;

            bool is_selected = ((int)i == selected_zone_index);
            int zone_left = rect.left() + (int)(rect.width() * zone.u_min);
            int zone_right = rect.left() + (int)(rect.width() * zone.u_max);
            int zone_top = rect.top() + (int)(rect.height() * (1.0f - zone.v_max));
            int zone_bottom = rect.top() + (int)(rect.height() * (1.0f - zone.v_min));

            QRect zone_rect(zone_left, zone_top, zone_right - zone_left, zone_bottom - zone_top);
            painter.setPen(QPen(is_selected ? selected_zone_border : zone_border, is_selected ? 3 : 2));
            painter.setBrush(QBrush(is_selected ? selected_zone_color : zone_color));
            painter.drawRect(zone_rect);
            if(is_selected)
            {
                QPoint corners[4] = {
                    QPoint(zone_left, zone_top),
                    QPoint(zone_right, zone_top),
                    QPoint(zone_right, zone_bottom),
                    QPoint(zone_left, zone_bottom)
                };
                for(int corner = 0; corner < 4; corner++)
                {
                    bool is_hover = (drag_handle == (CornerHandle)(TopLeft + corner) && (int)i == selected_zone_index);
                    painter.setPen(QPen(is_hover ? QColor(255, 255, 255) : QColor(0, 150, 200), 2));
                    painter.setBrush(QBrush(is_hover ? handle_hover_color : handle_color));
                    painter.drawEllipse(corners[corner].x() - handle_half, corners[corner].y() - handle_half, handle_size, handle_size);
                }
            }
        }
    }

    void mousePressEvent(QMouseEvent* event) override
    {
        if(!capture_zones || !display_plane) return;

        QPoint pos = event->pos();
        const int handle_size = 10;
        const int handle_half = handle_size / 2;

        setFocus(Qt::MouseFocusReason);

        if(event->button() != Qt::LeftButton) return;

        QRect calc_preview_rect = computePreviewRect();

        for(size_t i = 0; i < capture_zones->size(); i++)
        {
            const CaptureZone& zone = (*capture_zones)[i];
            if(!zone.enabled) continue;
            int zone_left = calc_preview_rect.left() + (int)(calc_preview_rect.width() * zone.u_min);
            int zone_right = calc_preview_rect.left() + (int)(calc_preview_rect.width() * zone.u_max);
            int zone_top = calc_preview_rect.top() + (int)(calc_preview_rect.height() * (1.0f - zone.v_max));
            int zone_bottom = calc_preview_rect.top() + (int)(calc_preview_rect.height() * (1.0f - zone.v_min));
            QPoint corners[4] = {
                QPoint(zone_left, zone_top),
                QPoint(zone_right, zone_top),
                QPoint(zone_right, zone_bottom),
                QPoint(zone_left, zone_bottom)
            };
            for(int corner = 0; corner < 4; corner++)
            {
                QRect handle_rect(corners[corner].x() - handle_half, corners[corner].y() - handle_half, handle_size, handle_size);
                if(handle_rect.contains(pos))
                {
                    selected_zone_index = (int)i;
                    drag_handle = (CornerHandle)(TopLeft + corner);
                    dragging = true;
                    drag_start_pos = pos;
                    drag_start_zone = (*capture_zones)[i];
                    update();
                    return;
                }
            }
        }

        for(size_t i = 0; i < capture_zones->size(); i++)
        {
            const CaptureZone& zone = (*capture_zones)[i];
            if(!zone.enabled) continue;
            int zone_left = calc_preview_rect.left() + (int)(calc_preview_rect.width() * zone.u_min);
            int zone_right = calc_preview_rect.left() + (int)(calc_preview_rect.width() * zone.u_max);
            int zone_top = calc_preview_rect.top() + (int)(calc_preview_rect.height() * (1.0f - zone.v_max));
            int zone_bottom = calc_preview_rect.top() + (int)(calc_preview_rect.height() * (1.0f - zone.v_min));
            QRect zone_rect(zone_left, zone_top, zone_right - zone_left, zone_bottom - zone_top);
            if(zone_rect.contains(pos))
            {
                selected_zone_index = (int)i;
                dragging = true;
                drag_handle = MoveZone;
                drag_start_pos = pos;
                drag_start_zone = (*capture_zones)[i];
                update();
                return;
            }
        }

        selected_zone_index = -1;
        update();
    }

    void mouseMoveEvent(QMouseEvent* event) override
    {
        if(!capture_zones || !display_plane) return;

        QPoint pos = event->pos();
        const int handle_size = 10;
        const int handle_half = handle_size / 2;

        QRect calc_preview_rect = computePreviewRect();
        preview_rect = calc_preview_rect;

        if(!dragging)
        {
            CornerHandle new_hover = None;
            for(size_t i = 0; i < capture_zones->size(); i++)
            {
                const CaptureZone& zone = (*capture_zones)[i];
                if(!zone.enabled || (int)i != selected_zone_index) continue;
                int zone_left = calc_preview_rect.left() + (int)(calc_preview_rect.width() * zone.u_min);
                int zone_right = calc_preview_rect.left() + (int)(calc_preview_rect.width() * zone.u_max);
                int zone_top = calc_preview_rect.top() + (int)(calc_preview_rect.height() * (1.0f - zone.v_max));
                int zone_bottom = calc_preview_rect.top() + (int)(calc_preview_rect.height() * (1.0f - zone.v_min));
                QPoint corners[4] = {
                    QPoint(zone_left, zone_top),
                    QPoint(zone_right, zone_top),
                    QPoint(zone_right, zone_bottom),
                    QPoint(zone_left, zone_bottom)
                };
                for(int corner = 0; corner < 4; corner++)
                {
                    QRect handle_rect(corners[corner].x() - handle_half, corners[corner].y() - handle_half, handle_size, handle_size);
                    if(handle_rect.contains(pos))
                    {
                        new_hover = (CornerHandle)(TopLeft + corner);
                        break;
                    }
                }
                if(new_hover != None) break;
            }
            if(new_hover != drag_handle)
            {
                drag_handle = new_hover;
                update();
            }
            return;
        }

        if(selected_zone_index < 0 || selected_zone_index >= (int)capture_zones->size())
        {
            dragging = false;
            return;
        }

        CaptureZone& zone = (*capture_zones)[selected_zone_index];
        if(calc_preview_rect.width() <= 0 || calc_preview_rect.height() <= 0)
            return;

        float normalized_x = (float)(pos.x() - calc_preview_rect.left()) / (float)calc_preview_rect.width();
        float normalized_y = (float)(pos.y() - calc_preview_rect.top()) / (float)calc_preview_rect.height();
        normalized_x = std::max(0.0f, std::min(1.0f, normalized_x));
        normalized_y = std::max(0.0f, std::min(1.0f, normalized_y));
        float v = 1.0f - normalized_y;

        const float min_size = 0.01f;

        if(drag_handle == MoveZone)
        {
            float delta_u = normalized_x - ((drag_start_zone.u_min + drag_start_zone.u_max) * 0.5f);
            float delta_v = v - ((drag_start_zone.v_min + drag_start_zone.v_max) * 0.5f);
            float new_u_min = drag_start_zone.u_min + delta_u;
            float new_u_max = drag_start_zone.u_max + delta_u;
            float new_v_min = drag_start_zone.v_min + delta_v;
            float new_v_max = drag_start_zone.v_max + delta_v;
            if(new_u_min < 0.0f) { new_u_max -= new_u_min; new_u_min = 0.0f; }
            if(new_u_max > 1.0f) { new_u_min -= (new_u_max - 1.0f); new_u_max = 1.0f; }
            if(new_v_min < 0.0f) { new_v_max -= new_v_min; new_v_min = 0.0f; }
            if(new_v_max > 1.0f) { new_v_min -= (new_v_max - 1.0f); new_v_max = 1.0f; }
            zone.u_min = std::max(0.0f, std::min(1.0f, new_u_min));
            zone.u_max = std::max(0.0f, std::min(1.0f, new_u_max));
            zone.v_min = std::max(0.0f, std::min(1.0f, new_v_min));
            zone.v_max = std::max(0.0f, std::min(1.0f, new_v_max));
            if(zone.u_min > zone.u_max) { float t = zone.u_min; zone.u_min = zone.u_max; zone.u_max = t; }
            if(zone.v_min > zone.v_max) { float t = zone.v_min; zone.v_min = zone.v_max; zone.v_max = t; }
        }
        else
        {
            switch(drag_handle)
            {
            case TopLeft:
                zone.u_max = drag_start_zone.u_max;
                zone.v_min = drag_start_zone.v_min;
                zone.u_min = normalized_x;
                zone.v_max = v;
                break;
            case TopRight:
                zone.u_min = drag_start_zone.u_min;
                zone.v_min = drag_start_zone.v_min;
                zone.u_max = normalized_x;
                zone.v_max = v;
                break;
            case BottomRight:
                zone.u_min = drag_start_zone.u_min;
                zone.v_max = drag_start_zone.v_max;
                zone.u_max = normalized_x;
                zone.v_min = v;
                break;
            case BottomLeft:
                zone.u_max = drag_start_zone.u_max;
                zone.v_max = drag_start_zone.v_max;
                zone.u_min = normalized_x;
                zone.v_min = v;
                break;
            default:
                break;
            }
            zone.u_min = std::max(0.0f, std::min(1.0f, zone.u_min));
            zone.u_max = std::max(0.0f, std::min(1.0f, zone.u_max));
            zone.v_min = std::max(0.0f, std::min(1.0f, zone.v_min));
            zone.v_max = std::max(0.0f, std::min(1.0f, zone.v_max));
            if(zone.u_max - zone.u_min < min_size)
            {
                if(drag_handle == TopLeft || drag_handle == BottomLeft)
                    zone.u_min = zone.u_max - min_size;
                else
                    zone.u_max = zone.u_min + min_size;
            }
            if(zone.v_max - zone.v_min < min_size)
            {
                if(drag_handle == TopLeft || drag_handle == TopRight)
                    zone.v_max = zone.v_min + min_size;
                else
                    zone.v_min = zone.v_max - min_size;
            }
            zone.u_min = std::max(0.0f, std::min(1.0f, zone.u_min));
            zone.u_max = std::max(0.0f, std::min(1.0f, zone.u_max));
            zone.v_min = std::max(0.0f, std::min(1.0f, zone.v_min));
            zone.v_max = std::max(0.0f, std::min(1.0f, zone.v_max));
        }

        if(valueChangedCallback) valueChangedCallback();
        update();
    }

    void mouseReleaseEvent(QMouseEvent* event) override
    {
        (void)event;
        if(dragging && selected_zone_index >= 0 && selected_zone_index < (int)capture_zones->size())
            drag_start_zone = (*capture_zones)[selected_zone_index];
        dragging = false;
        drag_handle = None;
        update();
    }

    void keyPressEvent(QKeyEvent* event) override
    {
        if(!event)
        {
            QWidget::keyPressEvent(event);
            return;
        }
        if(event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace)
        {
            if(CanRemoveZone())
            {
                RemoveSelectedZone();
                event->accept();
                return;
            }
        }
        if(event->key() == Qt::Key_Insert || event->key() == Qt::Key_Plus ||
           event->key() == Qt::Key_Equal)
        {
            AddZone();
            event->accept();
            return;
        }
        if(event->key() == Qt::Key_Menu ||
           (event->key() == Qt::Key_F10 && event->modifiers() & Qt::ShiftModifier))
        {
            const QPoint anchor = (selected_zone_index >= 0 && capture_zones &&
                                   selected_zone_index < (int)capture_zones->size())
                                      ? QPoint(preview_rect.center())
                                      : rect().center();
            ShowZoneContextMenu(anchor);
            event->accept();
            return;
        }
        QWidget::keyPressEvent(event);
    }

private:
    enum CornerHandle { None, TopLeft, TopRight, BottomRight, BottomLeft, MoveZone };

    QRect computePreviewRect() const
    {
        QRect widget_rect = this->rect().adjusted(2, 2, -2, -2);
        float aspect_ratio = 16.0f / 9.0f;
        if(display_plane)
        {
            float width_mm = display_plane->GetWidthMM();
            float height_mm = display_plane->GetHeightMM();
            if(height_mm > 0.0f)
                aspect_ratio = width_mm / height_mm;
        }
        float widget_aspect = (float)widget_rect.width() / (float)widget_rect.height();
        if(widget_aspect > aspect_ratio)
        {
            int new_width = (int)(widget_rect.height() * aspect_ratio);
            int x_offset = (widget_rect.width() - new_width) / 2;
            return QRect(widget_rect.left() + x_offset, widget_rect.top(), new_width, widget_rect.height());
        }
        int new_height = (int)(widget_rect.width() / aspect_ratio);
        int y_offset = (widget_rect.height() - new_height) / 2;
        return QRect(widget_rect.left(), widget_rect.top() + y_offset, widget_rect.width(), new_height);
    }

    int selected_zone_index;
    bool dragging;
    CornerHandle drag_handle;
    QPoint drag_start_pos;
    CaptureZone drag_start_zone;
    QRect preview_rect;
    QTimer* refresh_timer = nullptr;
    std::uint64_t preview_last_frame_id_;
};

CaptureZonesWidget::CaptureZonesWidget(
    std::vector<CaptureZone>* zones,
    DisplayPlane3D* plane,
    bool* show_calibration_pattern,
    bool* show_screen_preview,
    float* black_bar_top_percent,
    float* black_bar_bottom_percent,
    float* black_bar_left_percent,
    float* black_bar_right_percent,
    float propagation_speed,
    float wave_decay_ms,
    float wave_time_to_edge_sec,
    QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::CaptureZonesWidget)
{
    ui->setupUi(this);

    ui->propagationSpeedSlider->setValue((int)std::lround(propagation_speed));
    ui->waveDecaySlider->setValue((int)wave_decay_ms);

    propagation_row_ = PromoteSliderLabelToEffectRow(ui->propagationLayout, ui->propagationSpeedSlider,
                                                       ui->propagationSpeedLabel, QStringLiteral("%"));
    wave_decay_row_ = PromoteSliderLabelToEffectRow(ui->waveDecayLayout, ui->waveDecaySlider,
                                                      ui->waveDecayLabel, QStringLiteral(" ms"));

    const QString wave_time_tip = ui->waveTimeToEdgeSlider->toolTip();
    ui->waveTimeLayout->removeWidget(ui->waveTimeToEdgeSlider);
    ui->waveTimeLayout->removeWidget(ui->waveTimeToEdgeLabel);
    ui->waveTimeToEdgeSlider->hide();
    ui->waveTimeToEdgeSlider->deleteLater();
    ui->waveTimeToEdgeLabel->hide();
    ui->waveTimeToEdgeLabel->deleteLater();
    wave_time_row_ = new EffectDoubleSliderRow(ui->waveTimeRow);
    wave_time_row_->configure(0.0,
                              10.0,
                              std::clamp((double)wave_time_to_edge_sec, 0.0, 10.0),
                              /*decimals=*/1,
                              /*single_step=*/0.1,
                              wave_time_tip);
    wave_time_row_->setSuffix(QStringLiteral(" s"));
    ui->waveTimeLayout->addWidget(wave_time_row_, 1);
    AlignFormCaptions(ui->waveForm);
    syncWaveIntensityVisibility();

    auto* preview_layout = new QVBoxLayout(ui->previewHost);
    preview_layout->setContentsMargins(0, 0, 0, 0);
    preview_widget = new CaptureAreaPreviewWidget(
        zones, plane, show_calibration_pattern, show_screen_preview,
        black_bar_top_percent, black_bar_bottom_percent,
        black_bar_left_percent, black_bar_right_percent, ui->previewHost);
    preview_widget->SetValueChangedCallback([this]() {
        onInternalChange();
        syncZoneButtonStates();
    });
    preview_layout->addWidget(preview_widget);

    ui->addZoneButton->setAccessibleName(QStringLiteral("Add capture zone"));
    ui->removeZoneButton->setAccessibleName(QStringLiteral("Remove selected capture zone"));
    connect(ui->addZoneButton, &QPushButton::clicked, this, [this]() {
        if(preview_widget)
        {
            preview_widget->AddZone();
        }
        syncZoneButtonStates();
    });
    connect(ui->removeZoneButton, &QPushButton::clicked, this, [this]() {
        if(preview_widget)
        {
            preview_widget->RemoveSelectedZone();
        }
        syncZoneButtonStates();
    });
    syncZoneButtonStates();

    wireSliderConnections();
}

CaptureZonesWidget::~CaptureZonesWidget()
{
    delete ui;
}

void CaptureZonesWidget::wireSliderConnections()
{
    if(propagation_row_)
    {
        connect(propagation_row_->slider(), &QSlider::valueChanged, this, [this](int) {
            onInternalChange();
        });
    }
    if(wave_decay_row_)
    {
        connect(wave_decay_row_->slider(), &QSlider::valueChanged, this, [this](int) {
            onInternalChange();
        });
    }
    if(wave_time_row_ && wave_time_row_->spinBox())
    {
        connect(wave_time_row_->spinBox(), QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                [this](double) {
                    syncWaveIntensityVisibility();
                    onInternalChange();
                });
    }
}

void CaptureZonesWidget::SetDisplayPlane(DisplayPlane3D* plane)
{
    if(preview_widget)
        preview_widget->SetDisplayPlane(plane);
}

void CaptureZonesWidget::SetCaptureZones(std::vector<CaptureZone>* zones)
{
    if(preview_widget)
        preview_widget->capture_zones = zones;
    syncZoneButtonStates();
}

void CaptureZonesWidget::onInternalChange()
{
    emit valueChanged();
}

void CaptureZonesWidget::syncZoneButtonStates()
{
    if(!ui)
    {
        return;
    }
    const bool can_remove = preview_widget && preview_widget->CanRemoveZone();
    if(ui->removeZoneButton)
    {
        ui->removeZoneButton->setEnabled(can_remove);
    }
    if(ui->addZoneButton && preview_widget && preview_widget->capture_zones)
    {
        ui->addZoneButton->setEnabled(
            preview_widget->capture_zones->size() < kMaxCaptureZonesPerMonitor);
    }
}

QPushButton* CaptureZonesWidget::getAddZoneButton() const { return ui->addZoneButton; }
QPushButton* CaptureZonesWidget::getRemoveZoneButton() const { return ui->removeZoneButton; }
QWidget* CaptureZonesWidget::getPreviewWidget() const { return preview_widget; }
QSlider* CaptureZonesWidget::getPropagationSpeedSlider() const
{
    return propagation_row_ ? propagation_row_->slider() : nullptr;
}
QSlider* CaptureZonesWidget::getWaveDecaySlider() const
{
    return wave_decay_row_ ? wave_decay_row_->slider() : nullptr;
}
QDoubleSpinBox* CaptureZonesWidget::getWaveTimeToEdgeSpin() const
{
    return wave_time_row_ ? wave_time_row_->spinBox() : nullptr;
}

QGroupBox* CaptureZonesWidget::getWaveGroup() const { return ui->waveGroup; }
QGroupBox* CaptureZonesWidget::getZonesGroup() const { return ui->zonesGroup; }

void CaptureZonesWidget::syncWaveIntensityVisibility()
{
    if(!ui)
    {
        return;
    }
    QDoubleSpinBox* tte = getWaveTimeToEdgeSpin();
    const bool tte_on = tte && tte->value() > 0.0;
    if(ui->propagationCaption)
    {
        ui->propagationCaption->setVisible(!tte_on);
    }
    if(ui->propagationRow)
    {
        ui->propagationRow->setVisible(!tte_on);
    }
}
