// SPDX-License-Identifier: GPL-2.0-only

#include "CaptureZonesWidget.h"
#include "DisplayPlane3D.h"
#include "ScreenCaptureManager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFormLayout>
#include <QPainter>
#include <QMouseEvent>
#include <algorithm>
#include <functional>
#include <cmath>

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
    bool* show_test_pattern_ptr = nullptr;
    bool* show_screen_preview_ptr = nullptr;
    float* black_bar_letterbox_percent_ptr = nullptr;
    float* black_bar_pillarbox_percent_ptr = nullptr;

    CaptureAreaPreviewWidget(std::vector<CaptureZone>* zones,
                             DisplayPlane3D* plane,
                             bool* test_pattern,
                             bool* screen_preview,
                             float* black_bar_letterbox_percent,
                             float* black_bar_pillarbox_percent,
                             QWidget* parent = nullptr)
        : QWidget(parent)
        , display_plane(plane)
        , capture_zones(zones)
        , show_test_pattern_ptr(test_pattern)
        , show_screen_preview_ptr(screen_preview)
        , black_bar_letterbox_percent_ptr(black_bar_letterbox_percent)
        , black_bar_pillarbox_percent_ptr(black_bar_pillarbox_percent)
        , selected_zone_index(-1)
        , dragging(false)
        , drag_handle(None)
    {
        setMinimumHeight(200);
        setMaximumHeight(300);
        setStyleSheet("QWidget { background-color: #1a1a1a; border: 1px solid #444; }");
        setToolTip("Click and drag corner handles to resize zones. Click and drag zone to move it. Right-click to delete.");
        setMouseTracking(true);
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

        bool show_test = show_test_pattern_ptr && *show_test_pattern_ptr;
        bool show_preview = show_screen_preview_ptr && *show_screen_preview_ptr && !show_test;

        if(show_test)
        {
            int center_x = rect.left() + rect.width() / 2;
            int center_y = rect.top() + rect.height() / 2;
            painter.fillRect(QRect(rect.left(), center_y, center_x - rect.left(), rect.bottom() - center_y), QColor(255, 0, 0, 200));
            painter.fillRect(QRect(center_x, center_y, rect.right() - center_x, rect.bottom() - center_y), QColor(0, 255, 0, 200));
            painter.fillRect(QRect(center_x, rect.top(), rect.right() - center_x, center_y - rect.top()), QColor(0, 0, 255, 200));
            painter.fillRect(QRect(rect.left(), rect.top(), center_x - rect.left(), center_y - rect.top()), QColor(255, 255, 0, 200));
        }
        else if(show_preview && display_plane)
        {
            std::string source_id = display_plane->GetCaptureSourceId();
            if(!source_id.empty())
            {
                ScreenCaptureManager& capture_mgr = ScreenCaptureManager::Instance();
                if(capture_mgr.IsInitialized() || capture_mgr.Initialize())
                {
                    if(!capture_mgr.IsCapturing(source_id))
                        capture_mgr.StartCapture(source_id);
                    std::shared_ptr<CapturedFrame> frame = capture_mgr.GetLatestFrame(source_id);
                    if(frame && frame->valid && !frame->data.empty())
                    {
                        QImage image(frame->data.data(), frame->width, frame->height, QImage::Format_RGBA8888);
                        if(!image.isNull())
                        {
                            painter.drawImage(rect, image);
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
};

CaptureZonesWidget::CaptureZonesWidget(
    std::vector<CaptureZone>* zones,
    DisplayPlane3D* plane,
    bool* show_test_pattern,
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
{
    QVBoxLayout* main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(0, 0, 0, 0);

    QGroupBox* direction_group = new QGroupBox("Wall / Direction Focus");
    QFormLayout* direction_form = new QFormLayout(direction_group);
    direction_form->setContentsMargins(8, 12, 8, 8);

    QWidget* front_back_widget = new QWidget();
    QHBoxLayout* front_back_layout = new QHBoxLayout(front_back_widget);
    front_back_layout->setContentsMargins(0, 0, 0, 0);
    front_back_balance_slider = new QSlider(Qt::Horizontal);
    front_back_balance_slider->setRange(-100, 100);
    front_back_balance_slider->setValue((int)std::lround(front_back_balance));
    front_back_balance_slider->setToolTip("Favor front (+) or back (-) LEDs. 0 = even.");
    front_back_layout->addWidget(front_back_balance_slider);
    front_back_balance_label = new QLabel(QString::number((int)std::lround(front_back_balance)));
    front_back_balance_label->setMinimumWidth(40);
    front_back_layout->addWidget(front_back_balance_label);
    direction_form->addRow("Front/Back:", front_back_widget);

    QWidget* left_right_widget = new QWidget();
    QHBoxLayout* left_right_layout = new QHBoxLayout(left_right_widget);
    left_right_layout->setContentsMargins(0, 0, 0, 0);
    left_right_balance_slider = new QSlider(Qt::Horizontal);
    left_right_balance_slider->setRange(-100, 100);
    left_right_balance_slider->setValue((int)std::lround(left_right_balance));
    left_right_balance_slider->setToolTip("Favor right (+) or left (-) LEDs. 0 = even.");
    left_right_layout->addWidget(left_right_balance_slider);
    left_right_balance_label = new QLabel(QString::number((int)std::lround(left_right_balance)));
    left_right_balance_label->setMinimumWidth(40);
    left_right_layout->addWidget(left_right_balance_label);
    direction_form->addRow("Left/Right:", left_right_widget);

    QWidget* top_bottom_widget = new QWidget();
    QHBoxLayout* top_bottom_layout = new QHBoxLayout(top_bottom_widget);
    top_bottom_layout->setContentsMargins(0, 0, 0, 0);
    top_bottom_balance_slider = new QSlider(Qt::Horizontal);
    top_bottom_balance_slider->setRange(-100, 100);
    top_bottom_balance_slider->setValue((int)std::lround(top_bottom_balance));
    top_bottom_balance_slider->setToolTip("Favor top (+) or bottom (-) LEDs. 0 = even.");
    top_bottom_layout->addWidget(top_bottom_balance_slider);
    top_bottom_balance_label = new QLabel(QString::number((int)std::lround(top_bottom_balance)));
    top_bottom_balance_label->setMinimumWidth(40);
    top_bottom_layout->addWidget(top_bottom_balance_label);
    direction_form->addRow("Top/Bottom:", top_bottom_widget);

    main_layout->addWidget(direction_group);

    QGroupBox* wave_group = new QGroupBox("Wave");
    QFormLayout* wave_form = new QFormLayout(wave_group);
    wave_form->setContentsMargins(8, 12, 8, 8);

    QWidget* propagation_widget = new QWidget();
    QHBoxLayout* propagation_layout = new QHBoxLayout(propagation_widget);
    propagation_layout->setContentsMargins(0, 0, 0, 0);
    propagation_speed_slider = new QSlider(Qt::Horizontal);
    propagation_speed_slider->setRange(0, 100);
    propagation_speed_slider->setValue((int)std::lround(propagation_speed));
    propagation_speed_slider->setTickPosition(QSlider::TicksBelow);
    propagation_speed_slider->setTickInterval(10);
    propagation_speed_slider->setToolTip("Wave strength (0-100%). 0 = instant, 50% = moderate trail, 100% = long trail.");
    propagation_layout->addWidget(propagation_speed_slider);
    propagation_speed_label = new QLabel(QString::number((int)std::lround(propagation_speed)) + "%");
    propagation_speed_label->setMinimumWidth(45);
    propagation_layout->addWidget(propagation_speed_label);
    wave_form->addRow("Wave Intensity:", propagation_widget);

    QWidget* wave_decay_widget = new QWidget();
    QHBoxLayout* wave_decay_layout = new QHBoxLayout(wave_decay_widget);
    wave_decay_layout->setContentsMargins(0, 0, 0, 0);
    wave_decay_slider = new QSlider(Qt::Horizontal);
    wave_decay_slider->setRange(0, 3000);
    wave_decay_slider->setValue((int)wave_decay_ms);
    wave_decay_slider->setTickPosition(QSlider::TicksBelow);
    wave_decay_slider->setTickInterval(300);
    wave_decay_slider->setToolTip("Wave tail length (0-3000ms). 0 = no tail, ~500-1500ms = visible trail.");
    wave_decay_layout->addWidget(wave_decay_slider);
    wave_decay_label = new QLabel(QString::number((int)wave_decay_ms) + "ms");
    wave_decay_label->setMinimumWidth(60);
    wave_decay_layout->addWidget(wave_decay_label);
    wave_form->addRow("Wave Decay:", wave_decay_widget);

    QWidget* wave_time_to_edge_widget = new QWidget();
    QHBoxLayout* wave_time_layout = new QHBoxLayout(wave_time_to_edge_widget);
    wave_time_layout->setContentsMargins(0, 0, 0, 0);
    wave_time_to_edge_slider = new QSlider(Qt::Horizontal);
    wave_time_to_edge_slider->setRange(0, 100);
    wave_time_to_edge_slider->setValue((int)(wave_time_to_edge_sec * 10.0f));
    wave_time_to_edge_slider->setTickPosition(QSlider::TicksBelow);
    wave_time_to_edge_slider->setTickInterval(10);
    wave_time_to_edge_slider->setToolTip("Time for wave to reach farthest LED (0-10s). 0 = use Wave Intensity.");
    wave_time_layout->addWidget(wave_time_to_edge_slider);
    wave_time_to_edge_label = new QLabel(wave_time_to_edge_sec <= 0.0f ? "Off" : QString::number(wave_time_to_edge_sec, 'f', 1) + "s");
    wave_time_to_edge_label->setMinimumWidth(45);
    wave_time_layout->addWidget(wave_time_to_edge_label);
    wave_form->addRow("Time to edge (s):", wave_time_to_edge_widget);

    main_layout->addWidget(wave_group);

    QGroupBox* zones_group = new QGroupBox("Capture Zones");
    QVBoxLayout* zones_layout = new QVBoxLayout();

    add_zone_button = new QPushButton("Add Capture Zone");
    add_zone_button->setToolTip("Add a new capture zone that can be positioned anywhere on the screen.");
    zones_layout->addWidget(add_zone_button);

    zones_layout->addWidget(new QLabel("Capture area (zones):"));
    preview_widget = new CaptureAreaPreviewWidget(
        zones, plane, show_test_pattern, show_screen_preview,
        black_bar_letterbox_percent, black_bar_pillarbox_percent, this);
    preview_widget->SetValueChangedCallback([this]() { onInternalChange(); });
    zones_layout->addWidget(preview_widget);

    zones_group->setLayout(zones_layout);
    main_layout->addWidget(zones_group);

    connect(add_zone_button, &QPushButton::clicked, this, [this]() {
        if(preview_widget) preview_widget->AddZone();
    });

    connect(propagation_speed_slider, &QSlider::valueChanged, this, [this](int v) {
        if(propagation_speed_label) propagation_speed_label->setText(QString::number(v) + "%");
        onInternalChange();
    });
    connect(wave_decay_slider, &QSlider::valueChanged, this, [this](int v) {
        if(wave_decay_label) wave_decay_label->setText(QString::number(v) + "ms");
        onInternalChange();
    });
    connect(wave_time_to_edge_slider, &QSlider::valueChanged, this, [this](int v) {
        if(wave_time_to_edge_label) wave_time_to_edge_label->setText(v == 0 ? "Off" : QString::number(v / 10.0, 'f', 1) + "s");
        onInternalChange();
    });
    connect(front_back_balance_slider, &QSlider::valueChanged, this, [this](int v) {
        if(front_back_balance_label) front_back_balance_label->setText(QString::number(v));
        onInternalChange();
    });
    connect(left_right_balance_slider, &QSlider::valueChanged, this, [this](int v) {
        if(left_right_balance_label) left_right_balance_label->setText(QString::number(v));
        onInternalChange();
    });
    connect(top_bottom_balance_slider, &QSlider::valueChanged, this, [this](int v) {
        if(top_bottom_balance_label) top_bottom_balance_label->setText(QString::number(v));
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

QPushButton* CaptureZonesWidget::getAddZoneButton() const { return add_zone_button; }
QWidget* CaptureZonesWidget::getPreviewWidget() const { return preview_widget; }
QSlider* CaptureZonesWidget::getPropagationSpeedSlider() const { return propagation_speed_slider; }
QLabel* CaptureZonesWidget::getPropagationSpeedLabel() const { return propagation_speed_label; }
QSlider* CaptureZonesWidget::getWaveDecaySlider() const { return wave_decay_slider; }
QLabel* CaptureZonesWidget::getWaveDecayLabel() const { return wave_decay_label; }
QSlider* CaptureZonesWidget::getWaveTimeToEdgeSlider() const { return wave_time_to_edge_slider; }
QLabel* CaptureZonesWidget::getWaveTimeToEdgeLabel() const { return wave_time_to_edge_label; }
QSlider* CaptureZonesWidget::getFrontBackBalanceSlider() const { return front_back_balance_slider; }
QLabel* CaptureZonesWidget::getFrontBackBalanceLabel() const { return front_back_balance_label; }
QSlider* CaptureZonesWidget::getLeftRightBalanceSlider() const { return left_right_balance_slider; }
QLabel* CaptureZonesWidget::getLeftRightBalanceLabel() const { return left_right_balance_label; }
QSlider* CaptureZonesWidget::getTopBottomBalanceSlider() const { return top_bottom_balance_slider; }
QLabel* CaptureZonesWidget::getTopBottomBalanceLabel() const { return top_bottom_balance_label; }
