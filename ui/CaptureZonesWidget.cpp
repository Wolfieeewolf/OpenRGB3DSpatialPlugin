// SPDX-License-Identifier: GPL-2.0-only

#include "CaptureZonesWidget.h"
#include "ui_CaptureZonesWidget.h"
#include "DisplayPlane3D.h"
#include "QtCompat.h"
#include "ScreenCaptureManager.h"
#include "ScreenMirror/ScreenMirrorCalibrationPattern.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFormLayout>
#include <QLabel>
#include <QPainter>
#ifdef Q_OS_WIN
#include <QFontMetrics>
#endif
#include <QMouseEvent>
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

bool CaptureZone::Contains(float u, float v) const
{
    return enabled && u >= u_min && u <= u_max && v >= v_min && v <= v_max;
}

class CaptureAreaPreviewWidget : public QWidget
{
public:
    DisplayPlane3D* display_plane = nullptr;
    std::vector<CaptureZone>* capture_zones = nullptr;
    std::function<void()> on_value_changed;
    bool* show_calibration_pattern_ptr = nullptr;
    bool* show_screen_preview_ptr = nullptr;
    float* black_bar_letterbox_percent_ptr = nullptr;
    float* black_bar_pillarbox_percent_ptr = nullptr;

    CaptureAreaPreviewWidget(std::vector<CaptureZone>* zones,
                             DisplayPlane3D* plane,
                             bool* calibration_pattern,
                             bool* screen_preview,
                             float* black_bar_letterbox_percent,
                             float* black_bar_pillarbox_percent,
                             QWidget* parent = nullptr)
        : QWidget(parent)
        , display_plane(plane)
        , capture_zones(zones)
        , show_calibration_pattern_ptr(calibration_pattern)
        , show_screen_preview_ptr(screen_preview)
        , black_bar_letterbox_percent_ptr(black_bar_letterbox_percent)
        , black_bar_pillarbox_percent_ptr(black_bar_pillarbox_percent)
        , selected_zone_index(-1)
        , dragging(false)
        , drag_handle(None)
        , preview_last_frame_id_(std::numeric_limits<std::uint64_t>::max())
    {
        setMinimumHeight(200);
        setMaximumHeight(300);
        setToolTip("Click and drag corner handles to resize zones. Click and drag zone to move it. Right-click to delete.");
        setMouseTracking(true);

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
        on_value_changed = callback;
    }

    void AddZone()
    {
        if(!capture_zones) return;
        CaptureZone new_zone(0.4f, 0.6f, 0.4f, 0.6f);
        new_zone.name = "Zone " + std::to_string(capture_zones->size() + 1);
        capture_zones->push_back(new_zone);
        selected_zone_index = (int)capture_zones->size() - 1;
        if(on_value_changed) on_value_changed();
        update();
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
#ifdef Q_OS_WIN
                            {
                                const QString tag = frame->used_gdi_capture ? QStringLiteral("GDI")
                                                                            : QStringLiteral("DXGI");
                                QFont font = painter.font();
                                font.setPointSize(9);
                                font.setBold(true);
                                painter.setFont(font);
                                QFontMetrics fm(font);
                                const QSize ts = fm.size(0, tag);
                                QRect badge(rect.right() - ts.width() - 14, rect.bottom() - ts.height() - 10,
                                            ts.width() + 8, ts.height() + 4);
                                painter.fillRect(badge, QColor(0, 0, 0, 185));
                                painter.setPen(frame->used_gdi_capture ? QColor(255, 190, 90)
                                                                     : QColor(120, 255, 160));
                                painter.drawText(badge, Qt::AlignCenter, tag);
                            }
#endif
                            if(black_bar_letterbox_percent_ptr && black_bar_pillarbox_percent_ptr)
                            {
                                float lp = std::clamp(*black_bar_letterbox_percent_ptr, 0.0f, 49.0f) / 100.0f;
                                float pp = std::clamp(*black_bar_pillarbox_percent_ptr, 0.0f, 49.0f) / 100.0f;
                                float u_min = pp, u_max = 1.0f - pp;
                                float v_min = lp, v_max = 1.0f - lp;
                                int left   = rect.left() + (int)(rect.width() * u_min);
                                int right  = rect.left() + (int)(rect.width() * u_max);
                                int top    = rect.top() + (int)(rect.height() * v_min);
                                int bottom = rect.top() + (int)(rect.height() * v_max);
                                QRect bounds_rect(left, top, right - left, bottom - top);
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

        if(event->button() == Qt::RightButton)
        {
            for(size_t i = 0; i < capture_zones->size(); i++)
            {
                const CaptureZone& zone = (*capture_zones)[i];
                if(!zone.enabled) continue;
                int zone_left = preview_rect.left() + (int)(preview_rect.width() * zone.u_min);
                int zone_right = preview_rect.left() + (int)(preview_rect.width() * zone.u_max);
                int zone_top = preview_rect.top() + (int)(preview_rect.height() * (1.0f - zone.v_max));
                int zone_bottom = preview_rect.top() + (int)(preview_rect.height() * (1.0f - zone.v_min));
                QRect zone_rect(zone_left, zone_top, zone_right - zone_left, zone_bottom - zone_top);
                if(zone_rect.contains(pos))
                {
                    if(capture_zones->size() > 1)
                    {
                        capture_zones->erase(capture_zones->begin() + i);
                        if(selected_zone_index >= (int)capture_zones->size())
                            selected_zone_index = (int)capture_zones->size() - 1;
                        if(on_value_changed) on_value_changed();
                        update();
                    }
                    return;
                }
            }
            return;
        }

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

        if(on_value_changed) on_value_changed();
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
    float* black_bar_letterbox_percent,
    float* black_bar_pillarbox_percent,
    float propagation_speed,
    float wave_decay_ms,
    float wave_time_to_edge_sec,
    float front_back_balance,
    float left_right_balance,
    float top_bottom_balance,
    QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::CaptureZonesWidget)
{
    ui->setupUi(this);

    ui->frontBackBalanceSlider->setValue((int)std::lround(front_back_balance));
    ui->frontBackBalanceLabel->setText(QString::number((int)std::lround(front_back_balance)));
    ui->leftRightBalanceSlider->setValue((int)std::lround(left_right_balance));
    ui->leftRightBalanceLabel->setText(QString::number((int)std::lround(left_right_balance)));
    ui->topBottomBalanceSlider->setValue((int)std::lround(top_bottom_balance));
    ui->topBottomBalanceLabel->setText(QString::number((int)std::lround(top_bottom_balance)));

    ui->propagationSpeedSlider->setValue((int)std::lround(propagation_speed));
    ui->propagationSpeedLabel->setText(QString::number((int)std::lround(propagation_speed)) + "%");
    ui->waveDecaySlider->setValue((int)wave_decay_ms);
    ui->waveDecayLabel->setText(QString::number((int)wave_decay_ms) + "ms");
    ui->waveTimeToEdgeSlider->setValue((int)(wave_time_to_edge_sec * 10.0f));
    ui->waveTimeToEdgeLabel->setText(wave_time_to_edge_sec <= 0.0f ? "Off"
                                                                   : QString::number(wave_time_to_edge_sec, 'f', 1) + "s");

    auto* preview_layout = new QVBoxLayout(ui->previewHost);
    preview_layout->setContentsMargins(0, 0, 0, 0);
    preview_widget = new CaptureAreaPreviewWidget(
        zones, plane, show_calibration_pattern, show_screen_preview, black_bar_letterbox_percent,
        black_bar_pillarbox_percent, ui->previewHost);
    preview_widget->SetValueChangedCallback([this]() { onInternalChange(); });
    preview_layout->addWidget(preview_widget);

    connect(ui->addZoneButton, &QPushButton::clicked, this, [this]() {
        if(preview_widget)
        {
            preview_widget->AddZone();
        }
    });

    wireSliderConnections();
}

CaptureZonesWidget::~CaptureZonesWidget()
{
    delete ui;
}

void CaptureZonesWidget::wireSliderConnections()
{
    connect(ui->propagationSpeedSlider, &QSlider::valueChanged, this, [this](int v) {
        ui->propagationSpeedLabel->setText(QString::number(v) + "%");
        onInternalChange();
    });
    connect(ui->waveDecaySlider, &QSlider::valueChanged, this, [this](int v) {
        ui->waveDecayLabel->setText(QString::number(v) + "ms");
        onInternalChange();
    });
    connect(ui->waveTimeToEdgeSlider, &QSlider::valueChanged, this, [this](int v) {
        ui->waveTimeToEdgeLabel->setText(v == 0 ? "Off" : QString::number(v / 10.0, 'f', 1) + "s");
        onInternalChange();
    });
    connect(ui->frontBackBalanceSlider, &QSlider::valueChanged, this, [this](int v) {
        ui->frontBackBalanceLabel->setText(QString::number(v));
        onInternalChange();
    });
    connect(ui->leftRightBalanceSlider, &QSlider::valueChanged, this, [this](int v) {
        ui->leftRightBalanceLabel->setText(QString::number(v));
        onInternalChange();
    });
    connect(ui->topBottomBalanceSlider, &QSlider::valueChanged, this, [this](int v) {
        ui->topBottomBalanceLabel->setText(QString::number(v));
        onInternalChange();
    });
}

void CaptureZonesWidget::SetDisplayPlane(DisplayPlane3D* plane)
{
    if(preview_widget)
        preview_widget->SetDisplayPlane(plane);
}

void CaptureZonesWidget::SetValueChangedCallback(std::function<void()> callback)
{
    value_changed_callback = callback;
}

void CaptureZonesWidget::SetCaptureZones(std::vector<CaptureZone>* zones)
{
    if(preview_widget)
        preview_widget->capture_zones = zones;
}

void CaptureZonesWidget::onInternalChange()
{
    if(value_changed_callback)
        value_changed_callback();
    emit valueChanged();
}

QPushButton* CaptureZonesWidget::getAddZoneButton() const { return ui->addZoneButton; }
QWidget* CaptureZonesWidget::getPreviewWidget() const { return preview_widget; }
QSlider* CaptureZonesWidget::getPropagationSpeedSlider() const { return ui->propagationSpeedSlider; }
QLabel* CaptureZonesWidget::getPropagationSpeedLabel() const { return ui->propagationSpeedLabel; }
QSlider* CaptureZonesWidget::getWaveDecaySlider() const { return ui->waveDecaySlider; }
QLabel* CaptureZonesWidget::getWaveDecayLabel() const { return ui->waveDecayLabel; }
QSlider* CaptureZonesWidget::getWaveTimeToEdgeSlider() const { return ui->waveTimeToEdgeSlider; }
QLabel* CaptureZonesWidget::getWaveTimeToEdgeLabel() const { return ui->waveTimeToEdgeLabel; }
QSlider* CaptureZonesWidget::getFrontBackBalanceSlider() const { return ui->frontBackBalanceSlider; }
QLabel* CaptureZonesWidget::getFrontBackBalanceLabel() const { return ui->frontBackBalanceLabel; }
QSlider* CaptureZonesWidget::getLeftRightBalanceSlider() const { return ui->leftRightBalanceSlider; }
QLabel* CaptureZonesWidget::getLeftRightBalanceLabel() const { return ui->leftRightBalanceLabel; }
QSlider* CaptureZonesWidget::getTopBottomBalanceSlider() const { return ui->topBottomBalanceSlider; }
QLabel* CaptureZonesWidget::getTopBottomBalanceLabel() const { return ui->topBottomBalanceLabel; }
