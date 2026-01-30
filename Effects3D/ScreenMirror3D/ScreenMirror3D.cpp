// SPDX-License-Identifier: GPL-2.0-only

#include "ScreenMirror3D.h"
#include "ScreenCaptureManager.h"
#include "DisplayPlane3D.h"
#include "DisplayPlaneManager.h"
#include "Geometry3DUtils.h"
#include "GridSpaceUtils.h"
#include "LogManager.h"
#include "VirtualReferencePoint3D.h"

/*---------------------------------------------------------*\
| Register this effect with the effect manager             |
\*---------------------------------------------------------*/
REGISTER_EFFECT_3D(ScreenMirror3D);

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFormLayout>
#include <QSlider>
#include <QTimer>
#include <QSignalBlocker>
#include <QPaintEvent>
#include <QPainter>
#include <QEvent>
#include <QMouseEvent>
#include <QPushButton>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <array>
#include <limits>
#include <functional>

// Forward declaration
class ScreenMirror3D;

// Custom widget to display multiple capture zones with interactive corner handles
class CaptureAreaPreviewWidget : public QWidget
{
public:
    DisplayPlane3D* display_plane;
    std::vector<ScreenMirror3D::CaptureZone>* capture_zones;  // Pointer to vector of zones
    std::function<void()> on_value_changed;  // Callback when values change
    bool* show_test_pattern_ptr;  // Pointer to test pattern state
    bool* show_screen_preview_ptr;  // Pointer to screen preview state
    
    CaptureAreaPreviewWidget(std::vector<ScreenMirror3D::CaptureZone>* zones,
                             DisplayPlane3D* plane = nullptr,
                             bool* test_pattern = nullptr,
                             bool* screen_preview = nullptr,
                             QWidget* parent = nullptr)
        : QWidget(parent)
        , display_plane(plane)
        , capture_zones(zones)
        , show_test_pattern_ptr(test_pattern)
        , show_screen_preview_ptr(screen_preview)
        , selected_zone_index(-1)
        , dragging(false)
        , drag_handle(None)
        , preview_rect(0, 0, 0, 0)
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
        // Add a new zone in the center, 20% of screen size
        ScreenMirror3D::CaptureZone new_zone(0.4f, 0.6f, 0.4f, 0.6f);
        new_zone.name = "Zone " + std::to_string(capture_zones->size() + 1);
        capture_zones->push_back(new_zone);
        selected_zone_index = (int)capture_zones->size() - 1;
        if(on_value_changed) on_value_changed();
        update();
    }
    
    void DeleteSelectedZone()
    {
        if(!capture_zones || selected_zone_index < 0 || selected_zone_index >= (int)capture_zones->size())
            return;
        if(capture_zones->size() <= 1) return; // Keep at least one zone
        
        capture_zones->erase(capture_zones->begin() + selected_zone_index);
        if(selected_zone_index >= (int)capture_zones->size())
            selected_zone_index = (int)capture_zones->size() - 1;
        if(on_value_changed) on_value_changed();
        update();
    }
    
protected:
    void paintEvent(QPaintEvent* event) override
    {
        Q_UNUSED(event);
        if(!capture_zones || !display_plane) return;
        
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        
        QRect widget_rect = this->rect().adjusted(2, 2, -2, -2);
        
        // Calculate aspect ratio from display plane
        float aspect_ratio = 16.0f / 9.0f; // Default
        float width_mm = display_plane->GetWidthMM();
        float height_mm = display_plane->GetHeightMM();
        if(height_mm > 0.0f)
        {
            aspect_ratio = width_mm / height_mm;
        }
        
        // Calculate preview rect maintaining aspect ratio
        QRect rect = widget_rect;
        float widget_aspect = (float)widget_rect.width() / (float)widget_rect.height();
        if(widget_aspect > aspect_ratio)
        {
            // Widget is wider, fit to height
            int new_width = (int)(widget_rect.height() * aspect_ratio);
            int x_offset = (widget_rect.width() - new_width) / 2;
            rect = QRect(widget_rect.left() + x_offset, widget_rect.top(), new_width, widget_rect.height());
        }
        else
        {
            // Widget is taller, fit to width
            int new_height = (int)(widget_rect.width() / aspect_ratio);
            int y_offset = (widget_rect.height() - new_height) / 2;
            rect = QRect(widget_rect.left(), widget_rect.top() + y_offset, widget_rect.width(), new_height);
        }
        
        // Store rect for mouse calculations
        preview_rect = rect;
        
        // Draw full screen area (background)
        painter.setPen(QPen(QColor(100, 100, 100), 2));
        painter.setBrush(QBrush(QColor(30, 30, 30)));
        painter.drawRect(rect);
        
        // Draw test pattern or screen preview if enabled
        bool show_test = show_test_pattern_ptr && *show_test_pattern_ptr;
        bool show_preview = show_screen_preview_ptr && *show_screen_preview_ptr && !show_test;
        
        if(show_test)
        {
            // Draw test pattern: 4 quadrants (Red, Green, Blue, Yellow)
            int center_x = rect.left() + rect.width() / 2;
            int center_y = rect.top() + rect.height() / 2;
            
            // Bottom-left quadrant: RED
            painter.fillRect(QRect(rect.left(), center_y, center_x - rect.left(), rect.bottom() - center_y), QColor(255, 0, 0, 200));
            
            // Bottom-right quadrant: GREEN
            painter.fillRect(QRect(center_x, center_y, rect.right() - center_x, rect.bottom() - center_y), QColor(0, 255, 0, 200));
            
            // Top-right quadrant: BLUE
            painter.fillRect(QRect(center_x, rect.top(), rect.right() - center_x, center_y - rect.top()), QColor(0, 0, 255, 200));
            
            // Top-left quadrant: YELLOW
            painter.fillRect(QRect(rect.left(), rect.top(), center_x - rect.left(), center_y - rect.top()), QColor(255, 255, 0, 200));
        }
        else if(show_preview && display_plane)
        {
            // Try to get screen capture texture for this plane
            std::string source_id = display_plane->GetCaptureSourceId();
            if(!source_id.empty())
            {
                ScreenCaptureManager& capture_mgr = ScreenCaptureManager::Instance();
                if(capture_mgr.IsInitialized() || capture_mgr.Initialize())
                {
                    if(!capture_mgr.IsCapturing(source_id))
                    {
                        capture_mgr.StartCapture(source_id);
                    }
                    
                    std::shared_ptr<CapturedFrame> frame = capture_mgr.GetLatestFrame(source_id);
                    if(frame && frame->valid && !frame->data.empty())
                    {
                        // Convert frame data to QImage and draw it
                        QImage image(frame->data.data(), frame->width, frame->height, QImage::Format_RGBA8888);
                        if(!image.isNull())
                        {
                            painter.drawImage(rect, image);
                        }
                    }
                }
            }
        }
        
        // Draw each capture zone
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
            const ScreenMirror3D::CaptureZone& zone = (*capture_zones)[i];
            if(!zone.enabled) continue;
            
            bool is_selected = ((int)i == selected_zone_index);
            
            // Convert UV to screen coordinates
            // UV: u_min=left, u_max=right, v_min=bottom(0.0), v_max=top(1.0)
            // Screen: X increases right, Y increases down (top=small Y, bottom=large Y)
            int zone_left = rect.left() + (int)(rect.width() * zone.u_min);
            int zone_right = rect.left() + (int)(rect.width() * zone.u_max);
            int zone_top = rect.top() + (int)(rect.height() * (1.0f - zone.v_max)); // v_max=top -> smaller screen Y
            int zone_bottom = rect.top() + (int)(rect.height() * (1.0f - zone.v_min)); // v_min=bottom -> larger screen Y
            
            QRect zone_rect(zone_left, zone_top, zone_right - zone_left, zone_bottom - zone_top);
            
            // Draw zone fill
            painter.setPen(QPen(is_selected ? selected_zone_border : zone_border, is_selected ? 3 : 2));
            painter.setBrush(QBrush(is_selected ? selected_zone_color : zone_color));
            painter.drawRect(zone_rect);
            
            // Draw corner handles
            if(is_selected)
            {
                QPoint corners[4] = {
                    QPoint(zone_left, zone_top),           // Top-left
                    QPoint(zone_right, zone_top),          // Top-right
                    QPoint(zone_right, zone_bottom),       // Bottom-right
                    QPoint(zone_left, zone_bottom)          // Bottom-left
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
            // Right-click: delete zone if clicking on one
            for(size_t i = 0; i < capture_zones->size(); i++)
            {
                const ScreenMirror3D::CaptureZone& zone = (*capture_zones)[i];
                if(!zone.enabled) continue;
                
                int zone_left = preview_rect.left() + (int)(preview_rect.width() * zone.u_min);
                int zone_right = preview_rect.left() + (int)(preview_rect.width() * zone.u_max);
                int zone_top = preview_rect.top() + (int)(preview_rect.height() * (1.0f - zone.v_max)); // v_max=top
                int zone_bottom = preview_rect.top() + (int)(preview_rect.height() * (1.0f - zone.v_min)); // v_min=bottom
                
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
        
        // Calculate preview rect (same logic as paintEvent)
        QRect widget_rect = this->rect().adjusted(2, 2, -2, -2);
        float aspect_ratio = 16.0f / 9.0f;
        if(display_plane)
        {
            float width_mm = display_plane->GetWidthMM();
            float height_mm = display_plane->GetHeightMM();
            if(height_mm > 0.0f)
            {
                aspect_ratio = width_mm / height_mm;
            }
        }
        QRect calc_preview_rect = widget_rect;
        float widget_aspect = (float)widget_rect.width() / (float)widget_rect.height();
        if(widget_aspect > aspect_ratio)
        {
            int new_width = (int)(widget_rect.height() * aspect_ratio);
            int x_offset = (widget_rect.width() - new_width) / 2;
            calc_preview_rect = QRect(widget_rect.left() + x_offset, widget_rect.top(), new_width, widget_rect.height());
        }
        else
        {
            int new_height = (int)(widget_rect.width() / aspect_ratio);
            int y_offset = (widget_rect.height() - new_height) / 2;
            calc_preview_rect = QRect(widget_rect.left(), widget_rect.top() + y_offset, widget_rect.width(), new_height);
        }
        
        // Check for corner handle clicks first
        for(size_t i = 0; i < capture_zones->size(); i++)
        {
            const ScreenMirror3D::CaptureZone& zone = (*capture_zones)[i];
            if(!zone.enabled) continue;
            
            int zone_left = calc_preview_rect.left() + (int)(calc_preview_rect.width() * zone.u_min);
            int zone_right = calc_preview_rect.left() + (int)(calc_preview_rect.width() * zone.u_max);
            int zone_top = calc_preview_rect.top() + (int)(calc_preview_rect.height() * (1.0f - zone.v_max)); // v_max=top
            int zone_bottom = calc_preview_rect.top() + (int)(calc_preview_rect.height() * (1.0f - zone.v_min)); // v_min=bottom
            
            QPoint corners[4] = {
                QPoint(zone_left, zone_top),           // TopLeft
                QPoint(zone_right, zone_top),          // TopRight
                QPoint(zone_right, zone_bottom),       // BottomRight
                QPoint(zone_left, zone_bottom)         // BottomLeft
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
                    // Always use the CURRENT zone state from the vector (not the const reference)
                    drag_start_zone = (*capture_zones)[i];
                    update();
                    return;
                }
            }
        }
        
        // Check for zone body clicks (to select/move)
        for(size_t i = 0; i < capture_zones->size(); i++)
        {
            const ScreenMirror3D::CaptureZone& zone = (*capture_zones)[i];
            if(!zone.enabled) continue;
            
            int zone_left = calc_preview_rect.left() + (int)(calc_preview_rect.width() * zone.u_min);
            int zone_right = calc_preview_rect.left() + (int)(calc_preview_rect.width() * zone.u_max);
            int zone_top = calc_preview_rect.top() + (int)(calc_preview_rect.height() * (1.0f - zone.v_max)); // v_max=top
            int zone_bottom = calc_preview_rect.top() + (int)(calc_preview_rect.height() * (1.0f - zone.v_min)); // v_min=bottom
            
            QRect zone_rect(zone_left, zone_top, zone_right - zone_left, zone_bottom - zone_top);
            if(zone_rect.contains(pos))
            {
                selected_zone_index = (int)i;
                dragging = true;
                drag_handle = MoveZone;
                drag_start_pos = pos;
                // Always use the CURRENT zone state from the vector (not the const reference)
                drag_start_zone = (*capture_zones)[i];
                update();
                return;
            }
        }
        
        // Clicked outside all zones - deselect
        selected_zone_index = -1;
        update();
    }
    
    void mouseMoveEvent(QMouseEvent* event) override
    {
        if(!capture_zones || !display_plane) return;
        
        QPoint pos = event->pos();
        const int handle_size = 10;
        const int handle_half = handle_size / 2;
        
        // Calculate preview rect (same logic as paintEvent)
        QRect widget_rect = this->rect().adjusted(2, 2, -2, -2);
        float aspect_ratio = 16.0f / 9.0f;
        if(display_plane)
        {
            float width_mm = display_plane->GetWidthMM();
            float height_mm = display_plane->GetHeightMM();
            if(height_mm > 0.0f)
            {
                aspect_ratio = width_mm / height_mm;
            }
        }
        QRect calc_preview_rect = widget_rect;
        float widget_aspect = (float)widget_rect.width() / (float)widget_rect.height();
        if(widget_aspect > aspect_ratio)
        {
            int new_width = (int)(widget_rect.height() * aspect_ratio);
            int x_offset = (widget_rect.width() - new_width) / 2;
            calc_preview_rect = QRect(widget_rect.left() + x_offset, widget_rect.top(), new_width, widget_rect.height());
        }
        else
        {
            int new_height = (int)(widget_rect.width() / aspect_ratio);
            int y_offset = (widget_rect.height() - new_height) / 2;
            calc_preview_rect = QRect(widget_rect.left(), widget_rect.top() + y_offset, widget_rect.width(), new_height);
        }
        preview_rect = calc_preview_rect;  // Cache for paintEvent
        
        if(!dragging)
        {
            // Update hover state for handles
            CornerHandle new_hover = None;
            for(size_t i = 0; i < capture_zones->size(); i++)
            {
                const ScreenMirror3D::CaptureZone& zone = (*capture_zones)[i];
                if(!zone.enabled || (int)i != selected_zone_index) continue;
                
                int zone_left = calc_preview_rect.left() + (int)(calc_preview_rect.width() * zone.u_min);
                int zone_right = calc_preview_rect.left() + (int)(calc_preview_rect.width() * zone.u_max);
                int zone_top = calc_preview_rect.top() + (int)(calc_preview_rect.height() * (1.0f - zone.v_max)); // v_max=top
                int zone_bottom = calc_preview_rect.top() + (int)(calc_preview_rect.height() * (1.0f - zone.v_min)); // v_min=bottom
                
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
        
        // Handle dragging
        if(selected_zone_index < 0 || selected_zone_index >= (int)capture_zones->size())
        {
            dragging = false;
            return;
        }
        
        ScreenMirror3D::CaptureZone& zone = (*capture_zones)[selected_zone_index];
        
        // Calculate normalized position (0-1) within the preview rect
        // Ensure we have valid dimensions
        if(calc_preview_rect.width() <= 0 || calc_preview_rect.height() <= 0)
        {
            return; // Can't calculate if rect is invalid
        }
        
        float normalized_x = (float)(pos.x() - calc_preview_rect.left()) / (float)calc_preview_rect.width();
        float normalized_y = (float)(pos.y() - calc_preview_rect.top()) / (float)calc_preview_rect.height();
        
        // Clamp to valid range (but allow slight overflow for edge cases)
        normalized_x = std::max(-0.01f, std::min(1.01f, normalized_x));
        normalized_y = std::max(-0.01f, std::min(1.01f, normalized_y));
        // Then clamp to actual valid range
        normalized_x = std::max(0.0f, std::min(1.0f, normalized_x));
        normalized_y = std::max(0.0f, std::min(1.0f, normalized_y));
        
        // V is inverted (0=bottom, 1=top in UV, but screen Y increases downward)
        float v = 1.0f - normalized_y;
        
        if(drag_handle == MoveZone)
        {
            // Move the entire zone
            float delta_u = normalized_x - ((drag_start_zone.u_min + drag_start_zone.u_max) * 0.5f);
            float delta_v = v - ((drag_start_zone.v_min + drag_start_zone.v_max) * 0.5f);
            
            float new_u_min = drag_start_zone.u_min + delta_u;
            float new_u_max = drag_start_zone.u_max + delta_u;
            float new_v_min = drag_start_zone.v_min + delta_v;
            float new_v_max = drag_start_zone.v_max + delta_v;
            
            // Clamp to screen bounds
            if(new_u_min < 0.0f) { new_u_max -= new_u_min; new_u_min = 0.0f; }
            if(new_u_max > 1.0f) { new_u_min -= (new_u_max - 1.0f); new_u_max = 1.0f; }
            if(new_v_min < 0.0f) { new_v_max -= new_v_min; new_v_min = 0.0f; }
            if(new_v_max > 1.0f) { new_v_min -= (new_v_max - 1.0f); new_v_max = 1.0f; }
            
            zone.u_min = std::max(0.0f, std::min(1.0f, new_u_min));
            zone.u_max = std::max(0.0f, std::min(1.0f, new_u_max));
            zone.v_min = std::max(0.0f, std::min(1.0f, new_v_min));
            zone.v_max = std::max(0.0f, std::min(1.0f, new_v_max));
            
            // Ensure min < max
            if(zone.u_min > zone.u_max) { float temp = zone.u_min; zone.u_min = zone.u_max; zone.u_max = temp; }
            if(zone.v_min > zone.v_max) { float temp = zone.v_min; zone.v_min = zone.v_max; zone.v_max = temp; }
        }
        else
        {
            // Resize by dragging a corner (Windows-style):
            // drag a corner, the opposite corner stays fixed.
            const float min_size = 0.01f; // 1% minimum size

            // Clamp mouse position to valid range
            normalized_x = std::max(0.0f, std::min(1.0f, normalized_x));
            v            = std::max(0.0f, std::min(1.0f, v));

            switch(drag_handle)
            {
                case TopLeft:
                {
                    // Anchor: bottom-right corner stays fixed
                    zone.u_max = drag_start_zone.u_max;  // Right edge fixed
                    zone.v_min = drag_start_zone.v_min;  // Bottom edge fixed
                    zone.u_min = normalized_x;  // Left edge follows mouse X
                    zone.v_max = v;  // Top edge follows mouse V
                    break;
                }
                case TopRight:
                {
                    // Anchor: bottom-left corner stays fixed
                    zone.u_min = drag_start_zone.u_min;  // Left edge fixed
                    zone.v_min = drag_start_zone.v_min;  // Bottom edge fixed
                    zone.u_max = normalized_x;  // Right edge follows mouse X
                    zone.v_max = v;  // Top edge follows mouse V
                    break;
                }
                case BottomRight:
                {
                    // Anchor: top-left corner stays fixed
                    zone.u_min = drag_start_zone.u_min;  // Left edge fixed
                    zone.v_max = drag_start_zone.v_max;  // Top edge fixed
                    zone.u_max = normalized_x;  // Right edge follows mouse X
                    zone.v_min = v;  // Bottom edge follows mouse V
                    break;
                }
                case BottomLeft:
                {
                    // Anchor: top-right corner stays fixed
                    zone.u_max = drag_start_zone.u_max;  // Right edge fixed
                    zone.v_max = drag_start_zone.v_max;  // Top edge fixed
                    zone.u_min = normalized_x;  // Left edge follows mouse X
                    zone.v_min = v;  // Bottom edge follows mouse V
                    break;
                }
                default:
                    break;
            }

            // Final clamp + sanity (avoid any flip/degenerate)
            // Clamp to screen bounds first
            zone.u_min = std::max(0.0f, std::min(1.0f, zone.u_min));
            zone.u_max = std::max(0.0f, std::min(1.0f, zone.u_max));
            zone.v_min = std::max(0.0f, std::min(1.0f, zone.v_min));
            zone.v_max = std::max(0.0f, std::min(1.0f, zone.v_max));
            
            // Enforce minimum size by adjusting the dragged edge (not the anchor)
            // This preserves the anchor point while ensuring minimum size
            switch(drag_handle)
            {
                case TopLeft:
                    // Adjust u_min and v_max to maintain minimum size
                    if(zone.u_max - zone.u_min < min_size) zone.u_min = zone.u_max - min_size;
                    if(zone.v_max - zone.v_min < min_size) zone.v_max = zone.v_min + min_size;
                    break;
                case TopRight:
                    // Adjust u_max and v_max to maintain minimum size
                    if(zone.u_max - zone.u_min < min_size) zone.u_max = zone.u_min + min_size;
                    if(zone.v_max - zone.v_min < min_size) zone.v_max = zone.v_min + min_size;
                    break;
                case BottomRight:
                    // Adjust u_max and v_min to maintain minimum size
                    if(zone.u_max - zone.u_min < min_size) zone.u_max = zone.u_min + min_size;
                    if(zone.v_max - zone.v_min < min_size) zone.v_min = zone.v_max - min_size;
                    break;
                case BottomLeft:
                    // Adjust u_min and v_min to maintain minimum size
                    if(zone.u_max - zone.u_min < min_size) zone.u_min = zone.u_max - min_size;
                    if(zone.v_max - zone.v_min < min_size) zone.v_min = zone.v_max - min_size;
                    break;
                default:
                    break;
            }
            
            // Final safety check (shouldn't be needed, but just in case)
            if(zone.u_max <= zone.u_min) zone.u_max = std::min(1.0f, zone.u_min + min_size);
            if(zone.v_max <= zone.v_min) zone.v_min = std::max(0.0f, zone.v_max - min_size);
            
            // Re-clamp after minimum size adjustment
            zone.u_min = std::max(0.0f, std::min(1.0f, zone.u_min));
            zone.u_max = std::max(0.0f, std::min(1.0f, zone.u_max));
            zone.v_min = std::max(0.0f, std::min(1.0f, zone.v_min));
            zone.v_max = std::max(0.0f, std::min(1.0f, zone.v_max));
        }
        
        // Notify that values changed
        if(on_value_changed)
        {
            on_value_changed();
        }
        
        update();
    }
    
    void mouseReleaseEvent(QMouseEvent* event) override
    {
        Q_UNUSED(event);
        if(dragging && selected_zone_index >= 0 && selected_zone_index < (int)capture_zones->size())
        {
            // Update drag_start_zone to current zone state so next click uses current state
            drag_start_zone = (*capture_zones)[selected_zone_index];
        }
        dragging = false;
        drag_handle = None;
        update();
    }
    
private:
    enum CornerHandle
    {
        None,
        TopLeft,
        TopRight,
        BottomRight,
        BottomLeft,
        MoveZone
    };
    
    int selected_zone_index;
    bool dragging;
    CornerHandle drag_handle;
    QPoint drag_start_pos;
    ScreenMirror3D::CaptureZone drag_start_zone;
    QRect preview_rect;  // Cached preview rectangle for mouse calculations
};

ScreenMirror3D::ScreenMirror3D(QWidget* parent)
    : SpatialEffect3D(parent)
    , global_scale_slider(nullptr)
    , global_scale_label(nullptr)
    , smoothing_time_slider(nullptr)
    , smoothing_time_label(nullptr)
    , brightness_slider(nullptr)
    , brightness_label(nullptr)
    , propagation_speed_slider(nullptr)
    , propagation_speed_label(nullptr)
    , wave_decay_slider(nullptr)
    , wave_decay_label(nullptr)
    , brightness_threshold_slider(nullptr)
    , brightness_threshold_label(nullptr)
    , global_scale_invert_check(nullptr)
    , monitor_status_label(nullptr)
    , monitor_help_label(nullptr)
    , monitors_container(nullptr)
    , monitors_layout(nullptr)
    
    , global_scale(1.0f) // Default 100% global scale
    , smoothing_time_ms(50.0f)
    , brightness_multiplier(1.0f)
    , brightness_threshold(0.0f) // Default: no threshold (capture everything)
    , propagation_speed_mm_per_ms(10.0f) // Slower default for more noticeable delay
    , wave_decay_ms(500.0f) // Longer decay for more noticeable wave
    , show_test_pattern(false)
    , reference_points(nullptr)
{
}

ScreenMirror3D::~ScreenMirror3D()
{
    StopCaptureIfNeeded();
}

/*---------------------------------------------------------*\
| Effect Info                                              |
\*---------------------------------------------------------*/
EffectInfo3D ScreenMirror3D::GetEffectInfo()
{
    EffectInfo3D info           = {};
    info.info_version           = 2;
    info.effect_name            = "Screen Mirror 3D";
    info.effect_description     = "Projects screen content onto LEDs using 3D spatial mapping";
    info.category               = "Ambilight";
    info.effect_type            = SPATIAL_EFFECT_WAVE;
    info.is_reversible          = false;
    info.supports_random        = false;
    info.max_speed              = 100;
    info.min_speed              = 1;
    info.user_colors            = 0;
    info.has_custom_settings    = true;
    info.needs_3d_origin        = false;
    info.needs_direction        = false;
    info.needs_thickness        = false;
    info.needs_arms             = false;
    info.needs_frequency        = false;
    info.use_size_parameter     = false;
    
    // Hide base class controls that don't apply to screen mirroring
    // Screen mirroring uses screen colors, not effect colors
    info.show_color_controls    = false;
    // Speed doesn't make sense - screen mirroring is real-time
    info.show_speed_control     = false;
    // Brightness is handled by custom "Intensity" control
    info.show_brightness_control = false;
    // Frequency doesn't apply to screen mirroring
    info.show_frequency_control = false;
    // Size doesn't apply - coverage is controlled by per-monitor scale
    info.show_size_control      = false;
    // Scale is handled by custom "Global Reach" controls
    info.show_scale_control     = false;
    // FPS is not user-configurable for screen mirroring
    info.show_fps_control       = false;
    // Axis control doesn't apply - each monitor has its own orientation
    info.show_axis_control      = false;

    return info;
}

/*---------------------------------------------------------*\
| Setup Custom UI                                          |
\*---------------------------------------------------------*/
void ScreenMirror3D::SetupCustomUI(QWidget* parent)
{
    // Hide rotation controls (not used for screen mirroring)
    if(rotation_yaw_slider)
    {
        QWidget* rotation_group = rotation_yaw_slider->parentWidget();
        while(rotation_group && !qobject_cast<QGroupBox*>(rotation_group))
        {
            rotation_group = rotation_group->parentWidget();
        }
        if(rotation_group && rotation_group != effect_controls_group)
        {
            rotation_group->setVisible(false);
        }
    }
    
    // Hide intensity and sharpness controls (not used for screen mirroring)
    if(intensity_slider)
    {
        QWidget* intensity_widget = intensity_slider->parentWidget();
        if(intensity_widget && intensity_widget != effect_controls_group)
        {
            intensity_widget->setVisible(false);
        }
    }
    if(sharpness_slider)
    {
        QWidget* sharpness_widget = sharpness_slider->parentWidget();
        if(sharpness_widget && sharpness_widget != effect_controls_group)
        {
            sharpness_widget->setVisible(false);
        }
    }
    
    QWidget* container = new QWidget();
    QVBoxLayout* main_layout = new QVBoxLayout(container);

    // Multi-Monitor Status
    QGroupBox* status_group = new QGroupBox("Multi-Monitor Status");
    QVBoxLayout* status_layout = new QVBoxLayout();

    QLabel* info_label = new QLabel("Uses every active display plane automatically.");
    info_label->setWordWrap(true);
    info_label->setStyleSheet("QLabel { color: #888; font-style: italic; }");
    status_layout->addWidget(info_label);

    // Show active planes count (will be updated by RefreshMonitorStatus)
    monitor_status_label = new QLabel("Calculating...");
    monitor_status_label->setStyleSheet("QLabel { font-weight: bold; font-size: 14pt; }");
    status_layout->addWidget(monitor_status_label);
    monitor_help_label = nullptr;

    status_group->setLayout(status_layout);
    main_layout->addWidget(status_group);

    // Per-Monitor Settings
    monitors_container = new QGroupBox("Per-Monitor Balance");
    monitors_layout = new QVBoxLayout();
    monitors_layout->setSpacing(6);

    // Get planes for monitor list creation
    std::vector<DisplayPlane3D*> planes = DisplayPlaneManager::instance()->GetDisplayPlanes();

    // Create expandable settings group for each monitor
    for(unsigned int plane_index = 0; plane_index < planes.size(); plane_index++)
    {
        DisplayPlane3D* plane = planes[plane_index];
        if(!plane) continue;

        std::string plane_name = plane->GetName();

        // Get or create settings for this monitor
        // Optimize: use emplace to avoid extra find() call
        std::map<std::string, MonitorSettings>::iterator settings_it = monitor_settings.find(plane_name);
        if(settings_it == monitor_settings.end())
        {
            MonitorSettings new_settings;
            int plane_ref_index = plane->GetReferencePointIndex();
            if(plane_ref_index >= 0)
            {
                new_settings.reference_point_index = plane_ref_index;
            }
            settings_it = monitor_settings.emplace(plane_name, new_settings).first;
        }
        MonitorSettings& settings = settings_it->second;
        
        // Update reference point index if not set
        if(settings.reference_point_index < 0)
        {
            int plane_ref_index = plane->GetReferencePointIndex();
            if(plane_ref_index >= 0)
            {
                settings.reference_point_index = plane_ref_index;
            }
        }

        if(!settings.group_box)
        {
            CreateMonitorSettingsUI(plane, settings);
        }
    }

    if(monitor_settings.empty())
    {
        QLabel* no_monitors_label = new QLabel("No monitors configured. Set up Display Planes first.");
        no_monitors_label->setStyleSheet("QLabel { color: #cc6600; font-style: italic; }");
        monitors_layout->addWidget(no_monitors_label);
    }

    monitors_container->setLayout(monitors_layout);
    main_layout->addWidget(monitors_container);

    // Initial status update (after monitors_layout is created)
    RefreshMonitorStatus();

    // Note: Test Pattern and Screen Preview are now per-monitor settings
    // They can be found in each monitor's settings group below

    // Note: All other settings (Global Reach, Calibration, Light & Motion) are now per-monitor
    // and can be found in each monitor's settings group below

    main_layout->addStretch();

    // Add container to parent's layout
    if(parent && parent->layout())
    {
        parent->layout()->addWidget(container);
    }

    // Start capturing from all configured monitors
    StartCaptureIfNeeded();

    // Emit initial screen preview state (delayed so viewport connection is ready)
    QTimer::singleShot(100, this, &ScreenMirror3D::OnScreenPreviewChanged);
}

/*---------------------------------------------------------*\
| Update Parameters                                        |
\*---------------------------------------------------------*/
void ScreenMirror3D::UpdateParams(SpatialEffectParams& /*params*/)
{
    // Screen mirror doesn't use standard parameters
}

/*---------------------------------------------------------*\
| Calculate Color (not used - we override CalculateColorGrid) |
\*---------------------------------------------------------*/
RGBColor ScreenMirror3D::CalculateColor(float /*x*/, float /*y*/, float /*z*/, float /*time*/)
{
    return ToRGBColor(0, 0, 0);
}

/*---------------------------------------------------------*\
| Calculate Color Grid - The Main Logic                    |
\*---------------------------------------------------------*/
namespace
{
    inline float Smoothstep(float edge0, float edge1, float x)
    {
        if(edge0 == edge1)
        {
            return (x >= edge1) ? 1.0f : 0.0f;
        }
        float t = (x - edge0) / (edge1 - edge0);
        t = std::clamp(t, 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }

    float ComputeMaxReferenceDistanceMm(const GridContext3D& grid, const Vector3D& reference, float grid_scale_mm)
    {
        std::array<float, 2> xs = {grid.min_x, grid.max_x};
        std::array<float, 2> ys = {grid.min_y, grid.max_y};
        std::array<float, 2> zs = {grid.min_z, grid.max_z};

        float max_distance_sq = 0.0f;
        for(size_t x_index = 0; x_index < xs.size(); x_index++)
        {
            float cx = xs[x_index];
            for(size_t y_index = 0; y_index < ys.size(); y_index++)
            {
                float cy = ys[y_index];
                for(size_t z_index = 0; z_index < zs.size(); z_index++)
                {
                    float cz = zs[z_index];
                    float dx = GridUnitsToMM(cx - reference.x, grid_scale_mm);
                    float dy = GridUnitsToMM(cy - reference.y, grid_scale_mm);
                    float dz = GridUnitsToMM(cz - reference.z, grid_scale_mm);
                    float dist_sq = dx * dx + dy * dy + dz * dz;
                    if(dist_sq > max_distance_sq)
                    {
                        max_distance_sq = dist_sq;
                    }
                }
            }
        }
        if(max_distance_sq <= 0.0f)
        {
            return 0.0f;
        }
        return sqrtf(max_distance_sq);
    }

    float ComputeInvertedShellFalloff(float distance_mm,
                                      float max_distance_mm,
                                      float coverage,
                                      float softness_percent)
    {
        coverage = std::max(0.0f, coverage);
        if(coverage <= 0.0001f || max_distance_mm <= 0.0f)
        {
            return 0.0f;
        }

        // Allow slight over-coverage to flood entire room when sliders exceed 100%
        if(coverage >= 0.999f)
        {
            return 1.0f;
        }

        float normalized_distance = std::clamp(distance_mm / std::max(max_distance_mm, 1.0f), 0.0f, 1.0f);
        float boundary = std::max(0.0f, 1.0f - std::min(coverage, 1.0f));
        if(boundary <= 0.0005f)
        {
            return 1.0f;
        }

        float softness_ratio = std::clamp(softness_percent / 100.0f, 0.0f, 0.95f);
        float feather_band = softness_ratio * 0.5f;
        float fade_start = std::max(0.0f, boundary - feather_band);
        float fade_end = boundary;

        if(normalized_distance <= fade_start)
        {
            return 0.0f;
        }
        if(normalized_distance >= fade_end)
        {
            return 1.0f;
        }
        return Smoothstep(fade_start, fade_end, normalized_distance);
    }
}

RGBColor ScreenMirror3D::CalculateColorGrid(float x, float y, float z, float /*time*/, const GridContext3D& grid)
{
    std::vector<DisplayPlane3D*> all_planes = DisplayPlaneManager::instance()->GetDisplayPlanes();

    if(all_planes.empty())
    {
        return ToRGBColor(0, 0, 0);
    }

    Vector3D led_pos = {x, y, z};
    ScreenCaptureManager& capture_mgr = ScreenCaptureManager::Instance();

    struct MonitorContribution
    {
        DisplayPlane3D* plane;
        Geometry3D::PlaneProjection proj;
        std::shared_ptr<CapturedFrame> frame;
        float weight; // How much this monitor contributes (0-1)
        float blend;  // Blend percentage for this monitor (0-100)
        float delay_ms;
        uint64_t sample_timestamp;
        float brightness_multiplier;  // Per-monitor brightness
        float brightness_threshold;   // Per-monitor threshold
        float smoothing_time_ms;     // Per-monitor smoothing
        bool use_test_pattern;       // Per-monitor test pattern
    };

    std::vector<MonitorContribution> contributions;
    contributions.reserve(all_planes.size());
    Vector3D grid_center_ref = {grid.center_x, grid.center_y, grid.center_z};

    const float scale_mm = (grid.grid_scale_mm > 0.001f) ? grid.grid_scale_mm : 10.0f;
    float base_max_distance_mm = ComputeMaxReferenceDistanceMm(grid, grid_center_ref, scale_mm);
    if(base_max_distance_mm <= 0.0f)
    {
        base_max_distance_mm = 3000.0f;
    }
    
    std::map<std::string, std::unordered_map<std::string, FrameHistory>::iterator> history_cache;

    for(size_t plane_index = 0; plane_index < all_planes.size(); plane_index++)
    {
        DisplayPlane3D* plane = all_planes[plane_index];
        if(!plane) continue;

        std::string plane_name = plane->GetName();
        std::map<std::string, MonitorSettings>::iterator settings_it = monitor_settings.find(plane_name);
        if(settings_it == monitor_settings.end())
        {
            settings_it = monitor_settings.emplace(plane_name, MonitorSettings()).first;
            if(settings_it->second.capture_zones.empty())
            {
                settings_it->second.capture_zones.push_back(CaptureZone(0.0f, 1.0f, 0.0f, 1.0f));
            }
        }

        MonitorSettings& mon_settings = settings_it->second;
        
        // Safety check: ensure at least one zone exists and is enabled
        if(mon_settings.capture_zones.empty())
        {
            mon_settings.capture_zones.push_back(CaptureZone(0.0f, 1.0f, 0.0f, 1.0f));
        }
        // If all zones are disabled, enable the first one (safety fallback)
        // Optimize: use early exit loop
        bool has_enabled_zone = false;
        for(size_t zone_idx = 0; zone_idx < mon_settings.capture_zones.size(); zone_idx++)
        {
            if(mon_settings.capture_zones[zone_idx].enabled)
            {
                has_enabled_zone = true;
                break;
            }
        }
        if(!has_enabled_zone && !mon_settings.capture_zones.empty())
        {
            mon_settings.capture_zones[0].enabled = true;
        }
        
        // Check UI state if available, otherwise use stored enabled state
        bool monitor_enabled = mon_settings.group_box ? mon_settings.group_box->isChecked() : mon_settings.enabled;
        if(!monitor_enabled)
        {
            continue;
        }

        // Check if this monitor has test pattern enabled
        bool monitor_test_pattern = mon_settings.show_test_pattern;
        
        // In test pattern mode, we don't need capture sources - skip those checks
        std::string capture_id = plane->GetCaptureSourceId();
        std::shared_ptr<CapturedFrame> frame = nullptr;

        if(!monitor_test_pattern)
        {
            // Normal mode: need valid capture source and frames
            if(capture_id.empty())
            {
                continue;
            }

            // Check if capture is actually running and has frames
            if(!capture_mgr.IsCapturing(capture_id))
            {
                capture_mgr.StartCapture(capture_id);
                if(!capture_mgr.IsCapturing(capture_id))
                {
                    continue;
                }
            }

            frame = capture_mgr.GetLatestFrame(capture_id);
            if(!frame || !frame->valid || frame->data.empty())
            {
                continue;
            }

            AddFrameToHistory(capture_id, frame);
        }

        const Vector3D* falloff_ref = &grid_center_ref;
        if(mon_settings.reference_point_index >= 0 && reference_points &&
           mon_settings.reference_point_index < (int)reference_points->size())
        {
            Vector3D custom_ref;
            if(ResolveReferencePoint(mon_settings.reference_point_index, custom_ref))
            {
                falloff_ref = &custom_ref;
            }
        }

        float reference_max_distance_mm = base_max_distance_mm;
        if(falloff_ref != &grid_center_ref)
        {
            reference_max_distance_mm = ComputeMaxReferenceDistanceMm(grid, *falloff_ref, scale_mm);
            if(reference_max_distance_mm <= 0.0f)
            {
                reference_max_distance_mm = base_max_distance_mm;
            }
        }

        // Get base projection (maps LED 3D position to screen UV coordinates)
        Geometry3D::PlaneProjection proj = Geometry3D::SpatialMapToScreen(led_pos, *plane, 0.0f, falloff_ref, scale_mm);

        if(!proj.is_valid) continue;

        float u = proj.u;
        float v = proj.v;
        
        if(mon_settings.capture_zones.empty())
        {
            mon_settings.capture_zones.push_back(CaptureZone(0.0f, 1.0f, 0.0f, 1.0f));
        }
        
        bool in_zone = false;
        for(size_t zone_idx = 0; zone_idx < mon_settings.capture_zones.size(); zone_idx++)
        {
            const CaptureZone& zone = mon_settings.capture_zones[zone_idx];
            if(zone.Contains(u, v))
            {
                in_zone = true;
                break;
            }
        }
        
        if(!in_zone)
        {
            // LED is outside all capture zones - don't contribute
            continue;
        }
        
        // Clamp UV to valid range (should already be 0-1, but be safe)
        u = std::clamp(u, 0.0f, 1.0f);
        v = std::clamp(v, 0.0f, 1.0f);
        
        // Update projection with UV
        proj.u = u;
        proj.v = v;

        // Use per-monitor scale and scale_inverted
        float monitor_scale = std::clamp(mon_settings.scale, 0.0f, 2.0f);
        float normalized_scale = std::clamp(monitor_scale / 2.0f, 0.0f, 1.0f);
        float coverage = normalized_scale;
        float distance_falloff = 0.0f;

        if(mon_settings.scale_inverted)
        {
            if(coverage > 0.0001f)
            {
                float effective_range = reference_max_distance_mm * coverage;
                effective_range = std::max(effective_range, 10.0f);
                distance_falloff = Geometry3D::ComputeFalloff(proj.distance, effective_range, mon_settings.edge_softness);
            }
        }
        else
        {
            distance_falloff = ComputeInvertedShellFalloff(proj.distance, reference_max_distance_mm, coverage, mon_settings.edge_softness);

            // Allow over-scaling ( >1 ) to fully illuminate room
            if(coverage >= 1.0f && distance_falloff < 1.0f)
            {
                distance_falloff = std::max(distance_falloff, std::min(coverage - 0.99f, 1.0f));
            }
        }

        // Use per-monitor propagation and wave settings
        // Frame-based propagation: LEDs closest to screen use current frame (real-time)
        // Further LEDs use progressively older frames, creating a pulse/wave effect
        // The same frame "travels" outward from the screen like ripples
        // 
        // Propagation Speed control:
        // - 0 = All LEDs instant (no wave/pulse, all use current frame)
        // - Low values (1-10 mm/ms) = Very noticeable wave, LEDs many frames behind
        // - High values (100-200 mm/ms) = Subtle wave, LEDs closer to real-time
        std::shared_ptr<CapturedFrame> sampling_frame = frame;
        int frame_offset = 0;  // How many frames behind (0 = current frame)
        float delay_ms = 0.0f;  // Delay in milliseconds (for wave envelope calculation)
        
        if(!monitor_test_pattern && !capture_id.empty() && mon_settings.propagation_speed_mm_per_ms > 0.001f)
        {
            float max_speed = 200.0f;
            float effective_speed = max_speed - mon_settings.propagation_speed_mm_per_ms + 1.0f;
            if(effective_speed < 0.1f) effective_speed = 0.1f;
            
            delay_ms = proj.distance / effective_speed;
            delay_ms = std::clamp(delay_ms, 0.0f, 5000.0f);
            std::unordered_map<std::string, FrameHistory>::iterator history_it;
            std::map<std::string, std::unordered_map<std::string, FrameHistory>::iterator>::iterator cache_it = history_cache.find(capture_id);
            if(cache_it != history_cache.end())
            {
                history_it = cache_it->second;
            }
            else
            {
                history_it = capture_history.find(capture_id);
                if(history_it != capture_history.end())
                {
                    history_cache[capture_id] = history_it;
                }
            }
            
            if(history_it != capture_history.end() && history_it->second.frames.size() >= 2)
            {
                FrameHistory& history = history_it->second;
                const std::deque<std::shared_ptr<CapturedFrame>>& frames = history.frames;
                
                // Use cached frame rate if available and recent, otherwise recalculate
                float avg_frame_time_ms = 16.67f; // Default 60fps
                uint64_t latest_timestamp = frames.back()->timestamp_ms;
                
                // Recalculate frame rate if cache is invalid or stale (older than 100ms)
                if(history.last_frame_rate_update == 0 || 
                   (latest_timestamp - history.last_frame_rate_update) > 100)
                {
                    if(frames.size() >= 2)
                    {
                        // Use last few frames to estimate frame rate
                        size_t check_frames = std::min(frames.size() - 1, (size_t)10);
                        uint64_t total_time = 0;
                        size_t valid_pairs = 0;
                        
                        // Calculate time differences between consecutive frames
                        for(size_t i = frames.size() - check_frames; i < frames.size(); i++)
                        {
                            if(i > 0 && i < frames.size())
                            {
                                uint64_t frame_time = frames[i]->timestamp_ms;
                                uint64_t prev_time = frames[i-1]->timestamp_ms;
                                if(frame_time > prev_time) // Sanity check
                                {
                                    total_time += frame_time - prev_time;
                                    valid_pairs++;
                                }
                            }
                        }
                        
                        if(valid_pairs > 0 && total_time > 0)
                        {
                            avg_frame_time_ms = (float)total_time / (float)valid_pairs;
                            // Clamp to reasonable range (10-100ms per frame = 10-100fps)
                            avg_frame_time_ms = std::clamp(avg_frame_time_ms, 10.0f, 100.0f);
                        }
                    }
                    
                    // Cache the calculated frame rate
                    history.cached_avg_frame_time_ms = avg_frame_time_ms;
                    history.last_frame_rate_update = latest_timestamp;
                }
                else
                {
                    // Use cached value
                    avg_frame_time_ms = history.cached_avg_frame_time_ms;
                }
                
                frame_offset = (int)std::round(delay_ms / avg_frame_time_ms);
                frame_offset = std::max(0, frame_offset);
                
                if(frame_offset < (int)frames.size())
                {
                    size_t frame_index = frames.size() - 1 - frame_offset;
                    if(frame_index < frames.size())
                    {
                        sampling_frame = frames[frame_index];
                    }
                }
            }
            else
            {
                frame_offset = 0;
            }
        }

        float wave_envelope = 1.0f;
        if(mon_settings.propagation_speed_mm_per_ms > 0.001f && mon_settings.wave_decay_ms > 0.1f)
        {
            if(delay_ms <= 0.0f && mon_settings.propagation_speed_mm_per_ms > 0.001f)
            {
                float max_speed = 200.0f;
                float effective_speed = max_speed - mon_settings.propagation_speed_mm_per_ms + 1.0f;
                if(effective_speed < 0.1f) effective_speed = 0.1f;
                delay_ms = proj.distance / effective_speed;
                delay_ms = std::clamp(delay_ms, 0.0f, 5000.0f);
            }
            wave_envelope = std::exp(-delay_ms / mon_settings.wave_decay_ms);
        }

        float weight = distance_falloff * wave_envelope;

        if(weight > 0.01f)
        {
            MonitorContribution contrib;
            contrib.plane = plane;
            contrib.proj = proj;
            contrib.frame = sampling_frame; // Can be null in test pattern mode
            contrib.weight = weight;
            contrib.blend = mon_settings.blend;
            contrib.delay_ms = delay_ms;
            contrib.sample_timestamp = sampling_frame ? sampling_frame->timestamp_ms :
                                       (frame ? frame->timestamp_ms : 0);
            contrib.brightness_multiplier = mon_settings.brightness_multiplier;
            contrib.brightness_threshold = mon_settings.brightness_threshold;
            contrib.smoothing_time_ms = mon_settings.smoothing_time_ms;
            contrib.use_test_pattern = mon_settings.show_test_pattern;
            contributions.push_back(contrib);
        }
    }

    if(contributions.empty())
    {
        if(show_test_pattern)
        {
            return ToRGBColor(0, 0, 0);
        }
        
        int capturing_count = 0;
        for(size_t plane_index = 0; plane_index < all_planes.size(); plane_index++)
        {
            DisplayPlane3D* plane = all_planes[plane_index];
            if(plane && !plane->GetCaptureSourceId().empty())
            {
                if(capture_mgr.IsCapturing(plane->GetCaptureSourceId()))
                {
                    capturing_count++;
                }
            }
        }

        if(capturing_count > 0)
        {
            return ToRGBColor(0, 0, 0);
        }
        else
        {
            return ToRGBColor(128, 0, 128);
        }
    }

    float avg_blend = 0.0f;
    if(!contributions.empty())
    {
        for(size_t contrib_index = 0; contrib_index < contributions.size(); contrib_index++)
        {
            avg_blend += contributions[contrib_index].blend;
        }
        avg_blend /= (float)contributions.size();
    }
    float blend_factor = avg_blend / 100.0f;

    if(blend_factor < 0.01f && contributions.size() > 1)
    {
        size_t strongest_idx = 0;
        float max_weight = contributions[0].weight;
        for(size_t i = 1; i < contributions.size(); i++)
        {
            if(contributions[i].weight > max_weight)
            {
                max_weight = contributions[i].weight;
                strongest_idx = i;
            }
        }
        if(strongest_idx != 0)
        {
            contributions[0] = contributions[strongest_idx];
        }
        contributions.resize(1);
    }

    float total_r = 0.0f, total_g = 0.0f, total_b = 0.0f;
    float total_weight = 0.0f;
    uint64_t latest_timestamp = 0;

    for(size_t contrib_index = 0; contrib_index < contributions.size(); contrib_index++)
    {
        MonitorContribution& contrib = contributions[contrib_index];
        float sample_u = contrib.proj.u;
        float sample_v = contrib.proj.v;

        float r, g, b;

        // Use per-monitor test pattern setting from contribution
        if(contrib.use_test_pattern)
        {
            float clamped_u = std::clamp(sample_u, 0.0f, 1.0f);
            float clamped_v = std::clamp(sample_v, 0.0f, 1.0f);

            bool left_half = (clamped_u < 0.5f);
            bool bottom_half = (clamped_v < 0.5f);

            if(bottom_half && left_half)
            {
                r = 255.0f;
                g = 0.0f;
                b = 0.0f;
            }
            else if(bottom_half && !left_half)
            {
                r = 0.0f;
                g = 255.0f;
                b = 0.0f;
            }
            else if(!bottom_half && !left_half)
            {
                r = 0.0f;
                g = 0.0f;
                b = 255.0f;
            }
            else
            {
                r = 255.0f;
                g = 255.0f;
                b = 0.0f;
            }
        }
        else
        {
            if(!contrib.frame || contrib.frame->data.empty())
            {
                continue;
            }

            float flipped_v = 1.0f - sample_v;

            RGBColor sampled_color = Geometry3D::SampleFrame(
                contrib.frame->data.data(),
                contrib.frame->width,
                contrib.frame->height,
                sample_u,
                flipped_v,
                true
            );

            r = (float)RGBGetRValue(sampled_color);
            g = (float)RGBGetGValue(sampled_color);
            b = (float)RGBGetBValue(sampled_color);

            // Apply per-monitor brightness threshold filter
            // Threshold filters out dim content - higher values = only bright content passes
            if(contrib.brightness_threshold > 0.0f)
            {
                // Calculate luminance using standard formula: 0.299*R + 0.587*G + 0.114*B
                float luminance = 0.299f * r + 0.587f * g + 0.114f * b;
                
                if(luminance < contrib.brightness_threshold)
                {
                    // At threshold 255, only pure white (255) should pass
                    // Use a steeper falloff curve for more aggressive filtering
                    float normalized_lum = std::clamp(luminance / 255.0f, 0.0f, 1.0f);
                    float normalized_threshold = std::clamp(contrib.brightness_threshold / 255.0f, 0.0f, 1.0f);
                    
                    if(normalized_lum < normalized_threshold)
                    {
                        float threshold_factor = std::max(0.0f, normalized_lum / std::max(normalized_threshold, 0.001f));
                        threshold_factor = threshold_factor * threshold_factor * threshold_factor;
                        contrib.weight *= threshold_factor;
                    }
                }
            }
            
            r *= contrib.brightness_multiplier;
            g *= contrib.brightness_multiplier;
            b *= contrib.brightness_multiplier;
        }

        float adjusted_weight = contrib.weight * (0.5f + 0.5f * blend_factor);

        total_r += r * adjusted_weight;
        total_g += g * adjusted_weight;
        total_b += b * adjusted_weight;
        total_weight += adjusted_weight;

        if(contrib.sample_timestamp > latest_timestamp)
        {
            latest_timestamp = contrib.sample_timestamp;
        }
    }

    // Normalize by total weight (prevents over-brightening when multiple monitors overlap)
    if(total_weight > 0.0f)
    {
        total_r /= total_weight;
        total_g /= total_weight;
        total_b /= total_weight;
    }


    // Clamp to valid range
    if(total_r > 255.0f) total_r = 255.0f;
    if(total_g > 255.0f) total_g = 255.0f;
    if(total_b > 255.0f) total_b = 255.0f;

    float max_smoothing_time = 0.0f;
    if(contributions.size() == 1)
    {
        max_smoothing_time = contributions[0].smoothing_time_ms;
    }
    else
    {
        for(size_t i = 0; i < contributions.size(); i++)
        {
            if(contributions[i].smoothing_time_ms > max_smoothing_time)
            {
                max_smoothing_time = contributions[i].smoothing_time_ms;
            }
        }
    }
    
    if(max_smoothing_time > 0.1f)
    {
        LEDKey key = MakeLEDKey(x, y, z);
        LEDState& state = led_states[key];

        std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
        // steady_clock doesn't have an epoch, use duration since a fixed point
        static std::chrono::steady_clock::time_point start_time = std::chrono::steady_clock::now();
        uint64_t now_ms = (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count();
        uint64_t sample_time_ms = latest_timestamp ? latest_timestamp : now_ms;

        if(state.last_update_ms == 0)
        {
            state.r = total_r;
            state.g = total_g;
            state.b = total_b;
            state.last_update_ms = sample_time_ms;
        }
        else
        {
            uint64_t dt_ms_u64 = (sample_time_ms > state.last_update_ms) ? (sample_time_ms - state.last_update_ms) : 0;
            if(dt_ms_u64 == 0)
            {
                dt_ms_u64 = 16; // assume ~60 FPS
            }
            float dt = (float)dt_ms_u64;
            float tau = max_smoothing_time;
            float alpha = dt / (tau + dt);

            state.r += alpha * (total_r - state.r);
            state.g += alpha * (total_g - state.g);
            state.b += alpha * (total_b - state.b);
            state.last_update_ms = sample_time_ms;

            total_r = state.r;
            total_g = state.g;
            total_b = state.b;
        }
    }
    else if(!led_states.empty())
    {
        led_states.clear();
    }

    return ToRGBColor((uint8_t)total_r, (uint8_t)total_g, (uint8_t)total_b);
}

/*---------------------------------------------------------*\
| Settings Persistence                                     |
\*---------------------------------------------------------*/
nlohmann::json ScreenMirror3D::SaveSettings() const
{
    nlohmann::json settings;

    // All settings are now per-monitor (no global settings to save)

    // Save per-monitor settings (all settings are now per-monitor)
    nlohmann::json monitors = nlohmann::json::object();
    for(std::map<std::string, MonitorSettings>::const_iterator it = monitor_settings.begin();
        it != monitor_settings.end();
        ++it)
    {
        const MonitorSettings& mon_settings = it->second;
        nlohmann::json mon;
        mon["enabled"] = mon_settings.enabled;
        
        // Global Reach / Scale
        mon["scale"] = mon_settings.scale;
        mon["scale_inverted"] = mon_settings.scale_inverted;
        
        // Calibration
        mon["smoothing_time_ms"] = mon_settings.smoothing_time_ms;
        mon["brightness_multiplier"] = mon_settings.brightness_multiplier;
        mon["brightness_threshold"] = mon_settings.brightness_threshold;
        
        // Light & Motion
        mon["edge_softness"] = mon_settings.edge_softness;
        mon["blend"] = mon_settings.blend;
        mon["propagation_speed_mm_per_ms"] = mon_settings.propagation_speed_mm_per_ms;
        mon["wave_decay_ms"] = mon_settings.wave_decay_ms;
        
        mon["reference_point_index"] = mon_settings.reference_point_index;
        mon["show_test_pattern"] = mon_settings.show_test_pattern;
        mon["show_screen_preview"] = mon_settings.show_screen_preview;
        
        // Save capture zones
        nlohmann::json zones_array = nlohmann::json::array();
        for(const CaptureZone& zone : mon_settings.capture_zones)
        {
            nlohmann::json zone_json;
            zone_json["u_min"] = zone.u_min;
            zone_json["u_max"] = zone.u_max;
            zone_json["v_min"] = zone.v_min;
            zone_json["v_max"] = zone.v_max;
            zone_json["enabled"] = zone.enabled;
            zone_json["name"] = zone.name;
            zones_array.push_back(zone_json);
        }
        mon["capture_zones"] = zones_array;
        monitors[it->first] = mon;
    }
    settings["monitor_settings"] = monitors;

    return settings;
}

void ScreenMirror3D::LoadSettings(const nlohmann::json& settings)
{
    // All settings are now per-monitor (no global settings to load)

    // Backward compatibility: Load old global settings to use as defaults for monitors
    float legacy_global_scale = 1.0f;
    bool legacy_scale_inverted = false;
    float legacy_smoothing_time_ms = 50.0f;
    float legacy_brightness_multiplier = 1.0f;
    float legacy_brightness_threshold = 0.0f;
    float legacy_propagation_speed_mm_per_ms = 10.0f;
    float legacy_wave_decay_ms = 500.0f;
    
    if(settings.contains("global_scale"))
    {
        legacy_global_scale = settings["global_scale"].get<float>();
        // Legacy safety: if value stored as 0-200 integer, normalise back to 0-2 range
        if(legacy_global_scale > 2.0f && legacy_global_scale <= 400.0f)
        {
            legacy_global_scale = legacy_global_scale / 100.0f;
        }
        legacy_global_scale = std::clamp(legacy_global_scale, 0.0f, 2.0f);
    }
    if(settings.contains("smoothing_time_ms"))
        legacy_smoothing_time_ms = settings["smoothing_time_ms"].get<float>();
    if(settings.contains("brightness_multiplier"))
        legacy_brightness_multiplier = settings["brightness_multiplier"].get<float>();
    if(settings.contains("brightness_threshold"))
        legacy_brightness_threshold = settings["brightness_threshold"].get<float>();
    if(settings.contains("propagation_speed_mm_per_ms"))
        legacy_propagation_speed_mm_per_ms = settings["propagation_speed_mm_per_ms"].get<float>();
    if(settings.contains("wave_decay_ms"))
        legacy_wave_decay_ms = settings["wave_decay_ms"].get<float>();
    if(settings.contains("scale_inverted"))
        legacy_scale_inverted = settings["scale_inverted"].get<bool>();

    // Load per-monitor settings
    if(settings.contains("monitor_settings"))
    {
        const nlohmann::json& monitors = settings["monitor_settings"];
        for(nlohmann::json::const_iterator it = monitors.begin(); it != monitors.end(); ++it)
        {
            const std::string& monitor_name = it.key();
            const nlohmann::json& mon = it.value();

            // Get or create monitor settings (preserve existing UI widgets)
            std::map<std::string, MonitorSettings>::iterator existing_it = monitor_settings.find(monitor_name);
            bool had_existing = (existing_it != monitor_settings.end());
            
            // Store UI widget pointers BEFORE any operations (they will be preserved)
            QGroupBox* existing_group_box = nullptr;
            QComboBox* existing_ref_point_combo = nullptr;
            QSlider* existing_scale_slider = nullptr;
            QLabel* existing_scale_label = nullptr;
            QCheckBox* existing_scale_invert_check = nullptr;
            QSlider* existing_smoothing_time_slider = nullptr;
            QLabel* existing_smoothing_time_label = nullptr;
            QSlider* existing_brightness_slider = nullptr;
            QLabel* existing_brightness_label = nullptr;
            QSlider* existing_brightness_threshold_slider = nullptr;
            QLabel* existing_brightness_threshold_label = nullptr;
            QSlider* existing_softness_slider = nullptr;
            QLabel* existing_softness_label = nullptr;
            QSlider* existing_blend_slider = nullptr;
            QLabel* existing_blend_label = nullptr;
            QSlider* existing_propagation_speed_slider = nullptr;
            QLabel* existing_propagation_speed_label = nullptr;
            QSlider* existing_wave_decay_slider = nullptr;
            QLabel* existing_wave_decay_label = nullptr;
            QCheckBox* existing_test_pattern_check = nullptr;
            QCheckBox* existing_screen_preview_check = nullptr;
            QWidget* existing_capture_area_preview = nullptr;
            QPushButton* existing_add_zone_button = nullptr;
            
            if(had_existing)
            {
                // Preserve UI widgets from existing settings
                existing_group_box = existing_it->second.group_box;
                existing_ref_point_combo = existing_it->second.ref_point_combo;
                existing_scale_slider = existing_it->second.scale_slider;
                existing_scale_label = existing_it->second.scale_label;
                existing_scale_invert_check = existing_it->second.scale_invert_check;
                existing_smoothing_time_slider = existing_it->second.smoothing_time_slider;
                existing_smoothing_time_label = existing_it->second.smoothing_time_label;
                existing_brightness_slider = existing_it->second.brightness_slider;
                existing_brightness_label = existing_it->second.brightness_label;
                existing_brightness_threshold_slider = existing_it->second.brightness_threshold_slider;
                existing_brightness_threshold_label = existing_it->second.brightness_threshold_label;
                existing_softness_slider = existing_it->second.softness_slider;
                existing_softness_label = existing_it->second.softness_label;
                existing_blend_slider = existing_it->second.blend_slider;
                existing_blend_label = existing_it->second.blend_label;
                existing_propagation_speed_slider = existing_it->second.propagation_speed_slider;
                existing_propagation_speed_label = existing_it->second.propagation_speed_label;
                existing_wave_decay_slider = existing_it->second.wave_decay_slider;
                existing_wave_decay_label = existing_it->second.wave_decay_label;
                existing_capture_area_preview = existing_it->second.capture_area_preview;
                existing_add_zone_button = existing_it->second.add_zone_button;
            }
            else
            {
                // Create new settings (no UI widgets to preserve)
                monitor_settings[monitor_name] = MonitorSettings();
                existing_it = monitor_settings.find(monitor_name);
            }
            MonitorSettings& msettings = existing_it->second;

            // Load values from JSON (with backward compatibility to global settings)
            if(mon.contains("enabled")) msettings.enabled = mon["enabled"].get<bool>();
            
            // Global Reach / Scale
            if(mon.contains("scale")) msettings.scale = mon["scale"].get<float>();
            else msettings.scale = legacy_global_scale; // Use legacy global as default
            if(mon.contains("scale_inverted")) msettings.scale_inverted = mon["scale_inverted"].get<bool>();
            else msettings.scale_inverted = legacy_scale_inverted;
            
            // Calibration
            if(mon.contains("smoothing_time_ms")) msettings.smoothing_time_ms = mon["smoothing_time_ms"].get<float>();
            else msettings.smoothing_time_ms = legacy_smoothing_time_ms;
            if(mon.contains("brightness_multiplier")) msettings.brightness_multiplier = mon["brightness_multiplier"].get<float>();
            else msettings.brightness_multiplier = legacy_brightness_multiplier;
            if(mon.contains("brightness_threshold")) msettings.brightness_threshold = mon["brightness_threshold"].get<float>();
            else msettings.brightness_threshold = legacy_brightness_threshold;
            
            // Light & Motion
            if(mon.contains("edge_softness")) msettings.edge_softness = mon["edge_softness"].get<float>();
            if(mon.contains("blend")) msettings.blend = mon["blend"].get<float>();
            if(mon.contains("propagation_speed_mm_per_ms")) msettings.propagation_speed_mm_per_ms = mon["propagation_speed_mm_per_ms"].get<float>();
            else msettings.propagation_speed_mm_per_ms = legacy_propagation_speed_mm_per_ms;
            if(mon.contains("wave_decay_ms")) msettings.wave_decay_ms = mon["wave_decay_ms"].get<float>();
            else msettings.wave_decay_ms = legacy_wave_decay_ms;
            
            // Preview settings
            if(mon.contains("show_test_pattern")) msettings.show_test_pattern = mon["show_test_pattern"].get<bool>();
            if(mon.contains("show_screen_preview")) msettings.show_screen_preview = mon["show_screen_preview"].get<bool>();
            
            // Load capture zones (new format) or convert old edge zone settings
            // First, try to load new format
            if(mon.contains("capture_zones") && mon["capture_zones"].is_array())
            {
                msettings.capture_zones.clear();
                for(const auto& zone_json : mon["capture_zones"])
                {
                    CaptureZone zone;
                    zone.u_min = zone_json.value("u_min", 0.0f);
                    zone.u_max = zone_json.value("u_max", 1.0f);
                    zone.v_min = zone_json.value("v_min", 0.0f);
                    zone.v_max = zone_json.value("v_max", 1.0f);
                    zone.enabled = zone_json.value("enabled", true);
                    zone.name = zone_json.value("name", "Zone");
                    
                    // Clamp and validate
                    zone.u_min = std::clamp(zone.u_min, 0.0f, 1.0f);
                    zone.u_max = std::clamp(zone.u_max, 0.0f, 1.0f);
                    zone.v_min = std::clamp(zone.v_min, 0.0f, 1.0f);
                    zone.v_max = std::clamp(zone.v_max, 0.0f, 1.0f);
                    if(zone.u_min > zone.u_max) { float temp = zone.u_min; zone.u_min = zone.u_max; zone.u_max = temp; }
                    if(zone.v_min > zone.v_max) { float temp = zone.v_min; zone.v_min = zone.v_max; zone.v_max = temp; }
                    
                    msettings.capture_zones.push_back(zone);
                }
                
                // Ensure at least one zone exists
                if(msettings.capture_zones.empty())
                {
                    msettings.capture_zones.push_back(CaptureZone(0.0f, 1.0f, 0.0f, 1.0f));
                }
            }
            else
            {
                // Backward compatibility: convert old edge zone settings to a capture zone
                float left_inner = 0.0f, left_outer = 0.5f;
                float right_inner = 0.0f, right_outer = 0.5f;
                float bottom_inner = 0.0f, bottom_outer = 0.5f;
                float top_inner = 0.0f, top_outer = 0.5f;
                
                // Try to load old edge zone values
                if(mon.contains("edge_zone_left_inner")) left_inner = mon["edge_zone_left_inner"].get<float>();
                else if(mon.contains("edge_zone_left")) left_inner = left_outer = mon["edge_zone_left"].get<float>();
                else if(mon.contains("edge_zone_depth")) left_inner = left_outer = mon["edge_zone_depth"].get<float>();
                
                if(mon.contains("edge_zone_left_outer")) left_outer = mon["edge_zone_left_outer"].get<float>();
                else if(mon.contains("edge_zone_left")) left_outer = mon["edge_zone_left"].get<float>();
                else if(mon.contains("edge_zone_depth")) left_outer = mon["edge_zone_depth"].get<float>();
                
                if(mon.contains("edge_zone_right_inner")) right_inner = mon["edge_zone_right_inner"].get<float>();
                else if(mon.contains("edge_zone_right")) right_inner = right_outer = mon["edge_zone_right"].get<float>();
                else if(mon.contains("edge_zone_depth")) right_inner = right_outer = mon["edge_zone_depth"].get<float>();
                
                if(mon.contains("edge_zone_right_outer")) right_outer = mon["edge_zone_right_outer"].get<float>();
                else if(mon.contains("edge_zone_right")) right_outer = mon["edge_zone_right"].get<float>();
                else if(mon.contains("edge_zone_depth")) right_outer = mon["edge_zone_depth"].get<float>();
                
                if(mon.contains("edge_zone_bottom_inner")) bottom_inner = mon["edge_zone_bottom_inner"].get<float>();
                else if(mon.contains("edge_zone_bottom")) bottom_inner = bottom_outer = mon["edge_zone_bottom"].get<float>();
                else if(mon.contains("edge_zone_depth")) bottom_inner = bottom_outer = mon["edge_zone_depth"].get<float>();
                
                if(mon.contains("edge_zone_bottom_outer")) bottom_outer = mon["edge_zone_bottom_outer"].get<float>();
                else if(mon.contains("edge_zone_bottom")) bottom_outer = mon["edge_zone_bottom"].get<float>();
                else if(mon.contains("edge_zone_depth")) bottom_outer = mon["edge_zone_depth"].get<float>();
                
                if(mon.contains("edge_zone_top_inner")) top_inner = mon["edge_zone_top_inner"].get<float>();
                else if(mon.contains("edge_zone_top")) top_inner = top_outer = mon["edge_zone_top"].get<float>();
                else if(mon.contains("edge_zone_depth")) top_inner = top_outer = mon["edge_zone_depth"].get<float>();
                
                if(mon.contains("edge_zone_top_outer")) top_outer = mon["edge_zone_top_outer"].get<float>();
                else if(mon.contains("edge_zone_top")) top_outer = mon["edge_zone_top"].get<float>();
                else if(mon.contains("edge_zone_depth")) top_outer = mon["edge_zone_depth"].get<float>();
                
                // Clamp values
                left_inner = std::clamp(left_inner, 0.0f, 0.5f);
                left_outer = std::clamp(left_outer, left_inner, 0.5f);
                right_inner = std::clamp(right_inner, 0.0f, 0.5f);
                right_outer = std::clamp(right_outer, right_inner, 0.5f);
                bottom_inner = std::clamp(bottom_inner, 0.0f, 0.5f);
                bottom_outer = std::clamp(bottom_outer, bottom_inner, 0.5f);
                top_inner = std::clamp(top_inner, 0.0f, 0.5f);
                top_outer = std::clamp(top_outer, top_inner, 0.5f);
                
                // Convert edge zones to capture zone
                // Edge zones define bands from edges, so we create a zone covering the area
                float u_min = left_inner;
                float u_max = 1.0f - right_inner;
                float v_min = bottom_inner;
                float v_max = 1.0f - top_inner;
                
                // Ensure valid zone
                if(u_min >= u_max) { u_min = 0.0f; u_max = 1.0f; }
                if(v_min >= v_max) { v_min = 0.0f; v_max = 1.0f; }
                
                msettings.capture_zones.clear();
                msettings.capture_zones.push_back(CaptureZone(u_min, u_max, v_min, v_max));
                msettings.capture_zones[0].name = "Converted Zone";
            }
            
            if(mon.contains("reference_point_index")) msettings.reference_point_index = mon["reference_point_index"].get<int>();

            // Clamp all per-monitor settings to valid ranges
            msettings.scale = std::clamp(msettings.scale, 0.0f, 2.0f);
            msettings.smoothing_time_ms = std::clamp(msettings.smoothing_time_ms, 0.0f, 500.0f);
            msettings.brightness_multiplier = std::clamp(msettings.brightness_multiplier, 0.0f, 2.0f);
            msettings.brightness_threshold = std::clamp(msettings.brightness_threshold, 0.0f, 255.0f);
            msettings.edge_softness = std::clamp(msettings.edge_softness, 0.0f, 100.0f);
            msettings.blend = std::clamp(msettings.blend, 0.0f, 100.0f);
            msettings.propagation_speed_mm_per_ms = std::clamp(msettings.propagation_speed_mm_per_ms, 0.0f, 100.0f);
            msettings.wave_decay_ms = std::clamp(msettings.wave_decay_ms, 0.0f, 2000.0f);
            
            // Restore UI widget pointers if they existed before
            if(had_existing)
            {
                msettings.group_box = existing_group_box;
                msettings.ref_point_combo = existing_ref_point_combo;
                msettings.scale_slider = existing_scale_slider;
                msettings.scale_label = existing_scale_label;
                msettings.scale_invert_check = existing_scale_invert_check;
                msettings.smoothing_time_slider = existing_smoothing_time_slider;
                msettings.smoothing_time_label = existing_smoothing_time_label;
                msettings.brightness_slider = existing_brightness_slider;
                msettings.brightness_label = existing_brightness_label;
                msettings.brightness_threshold_slider = existing_brightness_threshold_slider;
                msettings.brightness_threshold_label = existing_brightness_threshold_label;
                msettings.softness_slider = existing_softness_slider;
                msettings.softness_label = existing_softness_label;
                msettings.blend_slider = existing_blend_slider;
                msettings.blend_label = existing_blend_label;
                msettings.propagation_speed_slider = existing_propagation_speed_slider;
                msettings.propagation_speed_label = existing_propagation_speed_label;
                msettings.wave_decay_slider = existing_wave_decay_slider;
                msettings.wave_decay_label = existing_wave_decay_label;
                msettings.test_pattern_check = existing_test_pattern_check;
                msettings.screen_preview_check = existing_screen_preview_check;
                msettings.capture_area_preview = existing_capture_area_preview;
                msettings.add_zone_button = existing_add_zone_button;
                
                // Update widget if it exists
                if(msettings.capture_area_preview)
                {
                    CaptureAreaPreviewWidget* preview_widget = static_cast<CaptureAreaPreviewWidget*>(msettings.capture_area_preview);
                    preview_widget->capture_zones = &msettings.capture_zones;
                    
                    // Update display plane and callback
                    std::vector<DisplayPlane3D*> planes = DisplayPlaneManager::instance()->GetDisplayPlanes();
                    for(DisplayPlane3D* plane : planes)
                    {
                        if(plane && plane->GetName() == monitor_name)
                        {
                            preview_widget->SetDisplayPlane(plane);
                            break;
                        }
                    }
                    preview_widget->SetValueChangedCallback([this]() {
                        OnParameterChanged();
                    });
                }
            }
        }
    }

    // Emit initial preview states based on per-monitor settings
    OnScreenPreviewChanged();
    OnTestPatternChanged();

    // Update monitor UI widgets to match loaded state
    for(std::map<std::string, MonitorSettings>::iterator it = monitor_settings.begin();
        it != monitor_settings.end();
        ++it)
    {
        MonitorSettings& msettings = it->second;
        if(msettings.group_box)
        {
            QSignalBlocker blocker(msettings.group_box);
            msettings.group_box->setChecked(msettings.enabled);
        }
        
        // Global Reach / Scale
        if(msettings.scale_slider)
        {
            QSignalBlocker blocker(msettings.scale_slider);
            msettings.scale_slider->setValue((int)std::lround(msettings.scale * 100.0f));
        }
        if(msettings.scale_label)
        {
            msettings.scale_label->setText(QString::number((int)std::lround(msettings.scale * 100.0f)) + "%");
        }
        if(msettings.scale_invert_check)
        {
            QSignalBlocker blocker(msettings.scale_invert_check);
            msettings.scale_invert_check->setChecked(msettings.scale_inverted);
        }
        
        // Calibration
        if(msettings.smoothing_time_slider)
        {
            QSignalBlocker blocker(msettings.smoothing_time_slider);
            msettings.smoothing_time_slider->setValue((int)std::lround(msettings.smoothing_time_ms));
        }
        if(msettings.smoothing_time_label)
        {
            msettings.smoothing_time_label->setText(QString::number((int)msettings.smoothing_time_ms) + "ms");
        }
        if(msettings.brightness_slider)
        {
            QSignalBlocker blocker(msettings.brightness_slider);
            msettings.brightness_slider->setValue((int)std::lround(msettings.brightness_multiplier * 100.0f));
        }
        if(msettings.brightness_label)
        {
            msettings.brightness_label->setText(QString::number((int)std::lround(msettings.brightness_multiplier * 100.0f)) + "%");
        }
        if(msettings.brightness_threshold_slider)
        {
            QSignalBlocker blocker(msettings.brightness_threshold_slider);
            msettings.brightness_threshold_slider->setValue((int)msettings.brightness_threshold);
        }
        if(msettings.brightness_threshold_label)
        {
            msettings.brightness_threshold_label->setText(QString::number((int)msettings.brightness_threshold));
        }
        
        // Light & Motion
        if(msettings.softness_slider)
        {
            QSignalBlocker blocker(msettings.softness_slider);
            msettings.softness_slider->setValue((int)std::lround(msettings.edge_softness));
        }
        if(msettings.softness_label)
        {
            msettings.softness_label->setText(QString::number((int)msettings.edge_softness));
        }
        if(msettings.blend_slider)
        {
            QSignalBlocker blocker(msettings.blend_slider);
            msettings.blend_slider->setValue((int)std::lround(msettings.blend));
        }
        if(msettings.blend_label)
        {
            msettings.blend_label->setText(QString::number((int)msettings.blend));
        }
        if(msettings.propagation_speed_slider)
        {
            QSignalBlocker blocker(msettings.propagation_speed_slider);
            msettings.propagation_speed_slider->setValue((int)std::lround(msettings.propagation_speed_mm_per_ms * 10.0f));
        }
        if(msettings.propagation_speed_label)
        {
            msettings.propagation_speed_label->setText(QString::number(msettings.propagation_speed_mm_per_ms, 'f', 1) + " mm/ms");
        }
        if(msettings.wave_decay_slider)
        {
            QSignalBlocker blocker(msettings.wave_decay_slider);
            msettings.wave_decay_slider->setValue((int)std::lround(msettings.wave_decay_ms));
        }
        if(msettings.wave_decay_label)
        {
            msettings.wave_decay_label->setText(QString::number((int)msettings.wave_decay_ms) + "ms");
        }
        
        // Preview settings
        if(msettings.test_pattern_check)
        {
            QSignalBlocker blocker(msettings.test_pattern_check);
            msettings.test_pattern_check->setChecked(msettings.show_test_pattern);
        }
        if(msettings.screen_preview_check)
        {
            QSignalBlocker blocker(msettings.screen_preview_check);
            msettings.screen_preview_check->setChecked(msettings.show_screen_preview);
        }
        
        // Update preview widget to reflect loaded values
        if(msettings.capture_area_preview)
        {
            msettings.capture_area_preview->update();
        }
        if(msettings.ref_point_combo)
        {
            QSignalBlocker blocker(msettings.ref_point_combo);
            int desired = msettings.reference_point_index;
            int idx = msettings.ref_point_combo->findData(desired);
            if(idx < 0)
            {
                idx = msettings.ref_point_combo->findData(-1);
            }
            if(idx >= 0)
            {
                msettings.ref_point_combo->setCurrentIndex(idx);
            }
        }
    }

    // Ensure reference point menus reflect updated selections
    RefreshReferencePointDropdowns();
    
    // Refresh monitor status display
    RefreshMonitorStatus();
    
    // Emit preview signals based on per-monitor settings
    OnScreenPreviewChanged();
    OnTestPatternChanged();
    
    OnParameterChanged();
}

void ScreenMirror3D::OnParameterChanged()
{
    // Update per-monitor settings (convert slider values to float)
    for(std::map<std::string, MonitorSettings>::iterator it = monitor_settings.begin();
        it != monitor_settings.end();
        ++it)
    {
        MonitorSettings& settings = it->second;
        if(settings.group_box) settings.enabled = settings.group_box->isChecked();
        
        // Global Reach / Scale
        if(settings.scale_slider) settings.scale = std::clamp(settings.scale_slider->value() / 100.0f, 0.0f, 2.0f);
        if(settings.scale_invert_check) settings.scale_inverted = settings.scale_invert_check->isChecked();
        
        // Calibration
        if(settings.smoothing_time_slider) settings.smoothing_time_ms = (float)settings.smoothing_time_slider->value();
        if(settings.brightness_slider) settings.brightness_multiplier = std::clamp(settings.brightness_slider->value() / 100.0f, 0.0f, 2.0f);
        if(settings.brightness_threshold_slider) settings.brightness_threshold = (float)settings.brightness_threshold_slider->value();
        
        // Light & Motion
        if(settings.softness_slider) settings.edge_softness = (float)settings.softness_slider->value();
        if(settings.blend_slider) settings.blend = (float)settings.blend_slider->value();
        if(settings.propagation_speed_slider) settings.propagation_speed_mm_per_ms = std::clamp(settings.propagation_speed_slider->value() / 10.0f, 0.0f, 200.0f);
        if(settings.wave_decay_slider) settings.wave_decay_ms = (float)settings.wave_decay_slider->value();
        
        // Preview settings
        bool old_test_pattern = settings.show_test_pattern;
        bool old_screen_preview = settings.show_screen_preview;
        if(settings.test_pattern_check) settings.show_test_pattern = settings.test_pattern_check->isChecked();
        if(settings.screen_preview_check) settings.show_screen_preview = settings.screen_preview_check->isChecked();
        
        // Update preview widget if test pattern or screen preview changed
        if((old_test_pattern != settings.show_test_pattern || old_screen_preview != settings.show_screen_preview) 
           && settings.capture_area_preview)
        {
            settings.capture_area_preview->update();
        }
        
        // Capture zones are managed by the preview widget - no need to read from sliders
        if(settings.ref_point_combo)
        {
            int index = settings.ref_point_combo->currentIndex();
            if(index >= 0)
            {
                settings.reference_point_index = settings.ref_point_combo->itemData(index).toInt();
            }
        }
    }

    // Refresh monitor status when parameters change (in case display planes were updated)
    RefreshMonitorStatus();
    RefreshReferencePointDropdowns();
    
    emit ParametersChanged();
}

void ScreenMirror3D::OnScreenPreviewChanged()
{
    // Check if any monitor has screen preview enabled
    // Optimize: single pass with early exit
    bool any_enabled = false;
    for(std::map<std::string, MonitorSettings>::iterator it = monitor_settings.begin();
        it != monitor_settings.end() && !any_enabled;
        ++it)
    {
        if(it->second.show_screen_preview && it->second.enabled)
        {
            any_enabled = true;
        }
    }
    emit ScreenPreviewChanged(any_enabled);
    emit ParametersChanged();
}

void ScreenMirror3D::OnTestPatternChanged()
{
    // Check if any monitor has test pattern enabled
    // Optimize: single pass with early exit
    bool any_enabled = false;
    for(std::map<std::string, MonitorSettings>::iterator it = monitor_settings.begin();
        it != monitor_settings.end() && !any_enabled;
        ++it)
    {
        if(it->second.show_test_pattern && it->second.enabled)
        {
            any_enabled = true;
        }
    }
    emit TestPatternChanged(any_enabled);
    
    // Update all preview widgets when test pattern changes
    // Optimize: batch update calls
    for(std::map<std::string, MonitorSettings>::iterator it = monitor_settings.begin();
        it != monitor_settings.end();
        ++it)
    {
        MonitorSettings& settings = it->second;
        if(settings.capture_area_preview)
        {
            settings.capture_area_preview->update();
        }
    }
}

bool ScreenMirror3D::ShouldShowTestPattern(const std::string& plane_name) const
{
    std::map<std::string, MonitorSettings>::const_iterator it = monitor_settings.find(plane_name);
    if(it != monitor_settings.end())
    {
        return it->second.show_test_pattern && it->second.enabled;
    }
    return false;
}

bool ScreenMirror3D::ShouldShowScreenPreview(const std::string& plane_name) const
{
    std::map<std::string, MonitorSettings>::const_iterator it = monitor_settings.find(plane_name);
    if(it != monitor_settings.end())
    {
        return it->second.show_screen_preview && it->second.enabled;
    }
    return false;
}

/*---------------------------------------------------------*\
| Reference Points Management                              |
\*---------------------------------------------------------*/
void ScreenMirror3D::SetReferencePoints(std::vector<std::unique_ptr<VirtualReferencePoint3D>>* ref_points)
{
    reference_points = ref_points;
    
    // Only refresh dropdowns if this instance has UI (monitors_layout exists)
    // and we have monitor settings with combo boxes
    if(monitors_layout && monitor_settings.size() > 0)
    {
        // Check if any monitor settings have combo boxes
        bool has_ui_widgets = false;
        for(std::map<std::string, MonitorSettings>::iterator it = monitor_settings.begin(); it != monitor_settings.end(); ++it)
        {
            if(it->second.ref_point_combo)
            {
                has_ui_widgets = true;
                break;
            }
        }
        if(has_ui_widgets)
        {
            RefreshReferencePointDropdowns();
        }
    }
}

void ScreenMirror3D::RefreshReferencePointDropdowns()
{
    if(!reference_points || !monitors_layout)
    {
        return;
    }

    // Optimize: build reference point list once per call, reuse for all combos
    std::vector<QString> ref_point_names;
    std::vector<int> ref_point_indices;
    ref_point_names.reserve(reference_points->size() + 1);
    ref_point_indices.reserve(reference_points->size() + 1);
    
    ref_point_names.push_back("Room Center");
    ref_point_indices.push_back(-1);
    
    for(size_t i = 0; i < reference_points->size(); i++)
    {
        VirtualReferencePoint3D* ref_point = (*reference_points)[i].get();
        if(!ref_point) continue;
        
        QString name = QString::fromStdString(ref_point->GetName());
        QString type = QString(VirtualReferencePoint3D::GetTypeName(ref_point->GetType()));
        QString display = QString("%1 (%2)").arg(name).arg(type);
        ref_point_names.push_back(display);
        ref_point_indices.push_back((int)i);
    }

    // Update all combo boxes with pre-built list
    for(std::map<std::string, MonitorSettings>::iterator it = monitor_settings.begin();
        it != monitor_settings.end();
        ++it)
    {
        MonitorSettings& settings = it->second;
        if(!settings.ref_point_combo)
        {
            continue;
        }

        int current_index = settings.ref_point_combo->currentIndex();
        int current_data = -1;
        if(current_index >= 0)
        {
            current_data = settings.ref_point_combo->currentData().toInt();
        }
        if(current_data < 0 && settings.reference_point_index >= 0)
        {
            current_data = settings.reference_point_index;
        }

        settings.ref_point_combo->blockSignals(true);
        settings.ref_point_combo->clear();

        settings.ref_point_combo->addItem("Room Center", QVariant(-1));

        for(size_t i = 0; i < reference_points->size(); i++)
        {
            VirtualReferencePoint3D* ref_point = (*reference_points)[i].get();
            if(!ref_point) continue;

            QString name = QString::fromStdString(ref_point->GetName());
            QString type = QString(VirtualReferencePoint3D::GetTypeName(ref_point->GetType()));
            QString display = QString("%1 (%2)").arg(name).arg(type);
            settings.ref_point_combo->addItem(display, QVariant((int)i));
        }

        if(current_data >= -1)
        {
            int restore_index = settings.ref_point_combo->findData(QVariant(current_data));
            if(restore_index >= 0)
            {
                settings.ref_point_combo->setCurrentIndex(restore_index);
            }
        }

        settings.ref_point_combo->blockSignals(false);
    }
}

bool ScreenMirror3D::ResolveReferencePoint(int index, Vector3D& out) const
{
    if(!reference_points || index < 0 || index >= (int)reference_points->size())
    {
        return false;
    }

    VirtualReferencePoint3D* ref_point = (*reference_points)[index].get();
    if(!ref_point)
    {
        return false;
    }

    out = ref_point->GetPosition();
    return true;
}

bool ScreenMirror3D::GetEffectReferencePoint(Vector3D& out) const
{
    (void)out;
    return false;
}

void ScreenMirror3D::AddFrameToHistory(const std::string& capture_id, const std::shared_ptr<CapturedFrame>& frame)
{
    if(capture_id.empty() || !frame || !frame->valid)
    {
        return;
    }

    FrameHistory& history = capture_history[capture_id];
    if(!history.frames.empty() && history.frames.back()->frame_id == frame->frame_id)
    {
        return;
    }

    history.frames.push_back(frame);

    uint64_t retention_ms = (uint64_t)GetHistoryRetentionMs();
    uint64_t cutoff = (frame->timestamp_ms > retention_ms) ? frame->timestamp_ms - retention_ms : 0;

    while(history.frames.size() > 1 && history.frames.front()->timestamp_ms < cutoff)
    {
        history.frames.pop_front();
    }

    const size_t max_frames = 180; // ~3 seconds at 60fps
    if(history.frames.size() > max_frames)
    {
        history.frames.pop_front();
    }
    
    // Invalidate cached frame rate when new frame is added (will be recalculated on next use)
    history.last_frame_rate_update = 0;
}

std::shared_ptr<CapturedFrame> ScreenMirror3D::GetFrameForDelay(const std::string& capture_id, float delay_ms) const
{
    std::unordered_map<std::string, FrameHistory>::const_iterator history_it = capture_history.find(capture_id);
    if(history_it == capture_history.end() || history_it->second.frames.empty())
    {
        return nullptr;
    }

    const std::deque<std::shared_ptr<CapturedFrame>>& frames = history_it->second.frames;
    if(delay_ms <= 0.0f)
    {
        return frames.back();
    }

    uint64_t latest_timestamp = frames.back()->timestamp_ms;
    uint64_t delay_u64 = delay_ms >= (float)std::numeric_limits<uint64_t>::max() ? latest_timestamp : (uint64_t)delay_ms;
    uint64_t target_timestamp = (latest_timestamp > delay_u64) ? latest_timestamp - delay_u64 : 0;

    for(std::deque<std::shared_ptr<CapturedFrame>>::const_reverse_iterator frame_it = frames.rbegin();
        frame_it != frames.rend();
        ++frame_it)
    {
        if((*frame_it)->timestamp_ms <= target_timestamp)
        {
            return *frame_it;
        }
    }

    return frames.front();
}

float ScreenMirror3D::GetHistoryRetentionMs() const
{
    // Calculate maximum retention needed across all monitors
    float max_retention = 600.0f; // Minimum retention
    
    for(std::map<std::string, MonitorSettings>::const_iterator it = monitor_settings.begin();
        it != monitor_settings.end();
        ++it)
    {
        const MonitorSettings& mon_settings = it->second;
        if(!mon_settings.enabled) continue;
        
        float monitor_retention = std::max(mon_settings.wave_decay_ms * 3.0f, mon_settings.smoothing_time_ms * 3.0f);
        if(mon_settings.propagation_speed_mm_per_ms > 0.001f)
        {
            // ensure we can cover longer distances (up to ~5m with doubled range)
            float max_distance_mm = 5000.0f;
            monitor_retention = std::max(monitor_retention, max_distance_mm / mon_settings.propagation_speed_mm_per_ms);
            // Also account for wave decay time
            monitor_retention = std::max(monitor_retention, mon_settings.wave_decay_ms * 2.0f);
        }
        max_retention = std::max(max_retention, monitor_retention);
    }
    
    return std::max(max_retention, 600.0f);
}

ScreenMirror3D::LEDKey ScreenMirror3D::MakeLEDKey(float x, float y, float z) const
{
    const float quantize_scale = 1000.0f;
    LEDKey key;
    key.x = (int)std::lround(x * quantize_scale);
    key.y = (int)std::lround(y * quantize_scale);
    key.z = (int)std::lround(z * quantize_scale);
    return key;
}

void ScreenMirror3D::CreateMonitorSettingsUI(DisplayPlane3D* plane, MonitorSettings& settings)
{
    if(!plane || !monitors_layout)
    {
        return;
    }

    std::string plane_name = plane->GetName();
    bool has_capture_source = !plane->GetCaptureSourceId().empty();

    QString display_name = QString::fromStdString(plane_name);
    if(!has_capture_source)
    {
        display_name += " (No Capture Source)";
    }
    settings.group_box = new QGroupBox(display_name);
    settings.group_box->setCheckable(true);
    settings.group_box->setChecked(settings.enabled && has_capture_source);
    settings.group_box->setEnabled(has_capture_source);
    if(has_capture_source)
    {
        settings.group_box->setToolTip("Enable or disable this monitor's influence.");
    }
    else
    {
        settings.group_box->setToolTip("This monitor needs a capture source assigned in Display Plane settings.");
        settings.group_box->setStyleSheet("QGroupBox { color: #cc6600; }");
    }
    connect(settings.group_box, &QGroupBox::toggled, this, &ScreenMirror3D::OnParameterChanged);

    QFormLayout* monitor_form = new QFormLayout();
    monitor_form->setContentsMargins(8, 4, 8, 4);

    // Global Reach / Scale
    QWidget* scale_widget = new QWidget();
    QHBoxLayout* scale_layout = new QHBoxLayout(scale_widget);
    scale_layout->setContentsMargins(0, 0, 0, 0);
    settings.scale_slider = new QSlider(Qt::Horizontal);
    settings.scale_slider->setEnabled(has_capture_source);
    settings.scale_slider->setRange(0, 200);
    settings.scale_slider->setValue((int)(settings.scale * 100));
    settings.scale_slider->setTickPosition(QSlider::TicksBelow);
    settings.scale_slider->setTickInterval(25);
    settings.scale_slider->setToolTip("Per-monitor brightness reach (0% to 200%).");
    scale_layout->addWidget(settings.scale_slider);
    settings.scale_label = new QLabel(QString::number((int)(settings.scale * 100)) + "%");
    settings.scale_label->setMinimumWidth(50);
    scale_layout->addWidget(settings.scale_label);
    connect(settings.scale_slider, &QSlider::valueChanged, this, &ScreenMirror3D::OnParameterChanged);
    connect(settings.scale_slider, &QSlider::valueChanged, settings.scale_label, [&settings](int value) {
        settings.scale_label->setText(QString::number(value) + "%");
    });
    monitor_form->addRow("Global Reach:", scale_widget);
    
    settings.scale_invert_check = new QCheckBox("Invert Scale Falloff");
    settings.scale_invert_check->setEnabled(has_capture_source);
    settings.scale_invert_check->setChecked(settings.scale_inverted);
    settings.scale_invert_check->setToolTip("Invert the distance falloff (closer = dimmer, farther = brighter).");
    connect(settings.scale_invert_check, &QCheckBox::toggled, this, &ScreenMirror3D::OnParameterChanged);
    monitor_form->addRow("", settings.scale_invert_check);

    // Calibration
    QWidget* smoothing_widget = new QWidget();
    QHBoxLayout* smoothing_layout = new QHBoxLayout(smoothing_widget);
    smoothing_layout->setContentsMargins(0, 0, 0, 0);
    settings.smoothing_time_slider = new QSlider(Qt::Horizontal);
    settings.smoothing_time_slider->setRange(0, 500);
    settings.smoothing_time_slider->setValue((int)settings.smoothing_time_ms);
    settings.smoothing_time_slider->setEnabled(has_capture_source);
    settings.smoothing_time_slider->setTickPosition(QSlider::TicksBelow);
    settings.smoothing_time_slider->setTickInterval(50);
    settings.smoothing_time_slider->setToolTip("Temporal smoothing to reduce flicker (0-500ms).");
    smoothing_layout->addWidget(settings.smoothing_time_slider);
    settings.smoothing_time_label = new QLabel(QString::number((int)settings.smoothing_time_ms) + "ms");
    settings.smoothing_time_label->setMinimumWidth(50);
    smoothing_layout->addWidget(settings.smoothing_time_label);
    connect(settings.smoothing_time_slider, &QSlider::valueChanged, this, &ScreenMirror3D::OnParameterChanged);
    connect(settings.smoothing_time_slider, &QSlider::valueChanged, settings.smoothing_time_label, [&settings](int value) {
        settings.smoothing_time_label->setText(QString::number(value) + "ms");
    });
    monitor_form->addRow("Smoothing:", smoothing_widget);

    QWidget* brightness_widget = new QWidget();
    QHBoxLayout* brightness_layout = new QHBoxLayout(brightness_widget);
    brightness_layout->setContentsMargins(0, 0, 0, 0);
    settings.brightness_slider = new QSlider(Qt::Horizontal);
    settings.brightness_slider->setRange(0, 200);
    settings.brightness_slider->setValue((int)(settings.brightness_multiplier * 100));
    settings.brightness_slider->setEnabled(has_capture_source);
    settings.brightness_slider->setTickPosition(QSlider::TicksBelow);
    settings.brightness_slider->setTickInterval(25);
    settings.brightness_slider->setToolTip("Brightness multiplier (0-200%).");
    brightness_layout->addWidget(settings.brightness_slider);
    settings.brightness_label = new QLabel(QString::number((int)(settings.brightness_multiplier * 100)) + "%");
    settings.brightness_label->setMinimumWidth(50);
    brightness_layout->addWidget(settings.brightness_label);
    connect(settings.brightness_slider, &QSlider::valueChanged, this, &ScreenMirror3D::OnParameterChanged);
    connect(settings.brightness_slider, &QSlider::valueChanged, settings.brightness_label, [&settings](int value) {
        settings.brightness_label->setText(QString::number(value) + "%");
    });
    monitor_form->addRow("Brightness:", brightness_widget);

    QWidget* threshold_widget = new QWidget();
    QHBoxLayout* threshold_layout = new QHBoxLayout(threshold_widget);
    threshold_layout->setContentsMargins(0, 0, 0, 0);
    settings.brightness_threshold_slider = new QSlider(Qt::Horizontal);
    settings.brightness_threshold_slider->setRange(0, 255);
    settings.brightness_threshold_slider->setValue((int)settings.brightness_threshold);
    settings.brightness_threshold_slider->setEnabled(has_capture_source);
    settings.brightness_threshold_slider->setTickPosition(QSlider::TicksBelow);
    settings.brightness_threshold_slider->setTickInterval(25);
    settings.brightness_threshold_slider->setToolTip("Minimum brightness to trigger effect (0-255). Lower values capture more dim content.");
    threshold_layout->addWidget(settings.brightness_threshold_slider);
    settings.brightness_threshold_label = new QLabel(QString::number((int)settings.brightness_threshold));
    settings.brightness_threshold_label->setMinimumWidth(50);
    threshold_layout->addWidget(settings.brightness_threshold_label);
    connect(settings.brightness_threshold_slider, &QSlider::valueChanged, this, &ScreenMirror3D::OnParameterChanged);
    connect(settings.brightness_threshold_slider, &QSlider::valueChanged, settings.brightness_threshold_label, [&settings](int value) {
        settings.brightness_threshold_label->setText(QString::number(value));
    });
    monitor_form->addRow("Brightness Threshold:", threshold_widget);

    settings.ref_point_combo = new QComboBox();
    settings.ref_point_combo->addItem("Room Center", QVariant(-1));
    settings.ref_point_combo->setEnabled(has_capture_source);
    settings.ref_point_combo->setToolTip("Anchor for falloff distance. Defaults to the display plane's position for ambilight effects.");
    connect(settings.ref_point_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ScreenMirror3D::OnParameterChanged);
    
    int plane_ref_index = plane->GetReferencePointIndex();
    if(plane_ref_index >= 0 && settings.reference_point_index < 0)
    {
        settings.reference_point_index = plane_ref_index;
    }
    
    monitor_form->addRow("Reference:", settings.ref_point_combo);

    QWidget* softness_widget = new QWidget();
    QHBoxLayout* softness_layout = new QHBoxLayout(softness_widget);
    softness_layout->setContentsMargins(0, 0, 0, 0);
    settings.softness_slider = new QSlider(Qt::Horizontal);
    settings.softness_slider->setRange(0, 100);
    settings.softness_slider->setValue((int)settings.edge_softness);
    settings.softness_slider->setEnabled(has_capture_source);
    settings.softness_slider->setTickPosition(QSlider::TicksBelow);
    settings.softness_slider->setTickInterval(10);
    settings.softness_slider->setToolTip("Edge feathering (0 = hard, 100 = very soft).");
    softness_layout->addWidget(settings.softness_slider);
    settings.softness_label = new QLabel(QString::number((int)settings.edge_softness));
    settings.softness_label->setMinimumWidth(30);
    softness_layout->addWidget(settings.softness_label);
    connect(settings.softness_slider, &QSlider::valueChanged, this, &ScreenMirror3D::OnParameterChanged);
    connect(settings.softness_slider, &QSlider::valueChanged, settings.softness_label, [&settings](int value) {
        settings.softness_label->setText(QString::number(value));
    });
    monitor_form->addRow("Softness:", softness_widget);

    QWidget* blend_widget = new QWidget();
    QHBoxLayout* blend_layout = new QHBoxLayout(blend_widget);
    blend_layout->setContentsMargins(0, 0, 0, 0);
    settings.blend_slider = new QSlider(Qt::Horizontal);
    settings.blend_slider->setRange(0, 100);
    settings.blend_slider->setValue((int)settings.blend);
    settings.blend_slider->setEnabled(has_capture_source);
    settings.blend_slider->setTickPosition(QSlider::TicksBelow);
    settings.blend_slider->setTickInterval(10);
    settings.blend_slider->setToolTip("Blend with other monitors (0 = isolated, 100 = fully shared).");
    blend_layout->addWidget(settings.blend_slider);
    settings.blend_label = new QLabel(QString::number((int)settings.blend));
    settings.blend_label->setMinimumWidth(30);
    blend_layout->addWidget(settings.blend_label);
    connect(settings.blend_slider, &QSlider::valueChanged, this, &ScreenMirror3D::OnParameterChanged);
    connect(settings.blend_slider, &QSlider::valueChanged, settings.blend_label, [&settings](int value) {
        settings.blend_label->setText(QString::number(value));
    });
    monitor_form->addRow("Blend:", blend_widget);

    // Light & Motion - Propagation
    QWidget* propagation_widget = new QWidget();
    QHBoxLayout* propagation_layout = new QHBoxLayout(propagation_widget);
    propagation_layout->setContentsMargins(0, 0, 0, 0);
    settings.propagation_speed_slider = new QSlider(Qt::Horizontal);
    settings.propagation_speed_slider->setRange(0, 200);  // Doubled range: 0-200 mm/ms
    settings.propagation_speed_slider->setValue((int)(settings.propagation_speed_mm_per_ms * 10.0f));
    settings.propagation_speed_slider->setEnabled(has_capture_source);
    settings.propagation_speed_slider->setTickPosition(QSlider::TicksBelow);
    settings.propagation_speed_slider->setTickInterval(20);
    settings.propagation_speed_slider->setToolTip("Wave/Pulse intensity (0-200). 0 = All LEDs instant (no wave). Higher values = Stronger wave/pulse effect (LEDs more frames behind). Adjust to match the feel of the scene.");
    propagation_layout->addWidget(settings.propagation_speed_slider);
    settings.propagation_speed_label = new QLabel(QString::number(settings.propagation_speed_mm_per_ms, 'f', 1) + " mm/ms");
    settings.propagation_speed_label->setMinimumWidth(80);
    propagation_layout->addWidget(settings.propagation_speed_label);
    connect(settings.propagation_speed_slider, &QSlider::valueChanged, this, &ScreenMirror3D::OnParameterChanged);
    connect(settings.propagation_speed_slider, &QSlider::valueChanged, settings.propagation_speed_label, [&settings](int value) {
        settings.propagation_speed_label->setText(QString::number(value / 10.0f, 'f', 1) + " mm/ms");
    });
    monitor_form->addRow("Propagation Speed:", propagation_widget);

    QWidget* wave_decay_widget = new QWidget();
    QHBoxLayout* wave_decay_layout = new QHBoxLayout(wave_decay_widget);
    wave_decay_layout->setContentsMargins(0, 0, 0, 0);
    settings.wave_decay_slider = new QSlider(Qt::Horizontal);
    settings.wave_decay_slider->setRange(0, 4000);  // Doubled range: 0-4000ms
    settings.wave_decay_slider->setValue((int)settings.wave_decay_ms);
    settings.wave_decay_slider->setEnabled(has_capture_source);
    settings.wave_decay_slider->setTickPosition(QSlider::TicksBelow);
    settings.wave_decay_slider->setTickInterval(400);
    settings.wave_decay_slider->setToolTip("Wave decay time (0-4000ms). How long the wave effect lasts as it propagates outward from the screen.");
    wave_decay_layout->addWidget(settings.wave_decay_slider);
    settings.wave_decay_label = new QLabel(QString::number((int)settings.wave_decay_ms) + "ms");
    settings.wave_decay_label->setMinimumWidth(60);
    wave_decay_layout->addWidget(settings.wave_decay_label);
    connect(settings.wave_decay_slider, &QSlider::valueChanged, this, &ScreenMirror3D::OnParameterChanged);
    connect(settings.wave_decay_slider, &QSlider::valueChanged, settings.wave_decay_label, [&settings](int value) {
        settings.wave_decay_label->setText(QString::number(value) + "ms");
    });
    monitor_form->addRow("Wave Decay:", wave_decay_widget);

    // Preview Settings
    settings.test_pattern_check = new QCheckBox("Show Test Pattern");
    settings.test_pattern_check->setEnabled(has_capture_source);
    settings.test_pattern_check->setChecked(settings.show_test_pattern);
    settings.test_pattern_check->setToolTip("Display a fixed color quadrant pattern on this monitor for calibration.");
    connect(settings.test_pattern_check,
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
            &QCheckBox::checkStateChanged,
            this, [this]() { OnParameterChanged(); }
#else
            &QCheckBox::stateChanged,
            this, [this](int) { OnParameterChanged(); }
#endif
    );
    monitor_form->addRow("Test Pattern:", settings.test_pattern_check);
    
    settings.screen_preview_check = new QCheckBox("Show Screen Preview");
    settings.screen_preview_check->setEnabled(has_capture_source);
    settings.screen_preview_check->setChecked(settings.show_screen_preview);
    settings.screen_preview_check->setToolTip("Show captured screen content on display planes in the 3D viewport for this monitor. Turn off to save CPU/GPU bandwidth.");
    connect(settings.screen_preview_check,
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
            &QCheckBox::checkStateChanged,
            this, [this]() { OnParameterChanged(); OnScreenPreviewChanged(); }
#else
            &QCheckBox::stateChanged,
            this, [this](int) { OnParameterChanged(); OnScreenPreviewChanged(); }
#endif
    );
    monitor_form->addRow("Screen Preview:", settings.screen_preview_check);

    // Capture Zones Management
    QGroupBox* zones_group = new QGroupBox("Capture Zones");
    QVBoxLayout* zones_layout = new QVBoxLayout();
    
    // Add Zone button
    settings.add_zone_button = new QPushButton("Add Capture Zone");
    settings.add_zone_button->setEnabled(has_capture_source);
    settings.add_zone_button->setToolTip("Add a new capture zone that can be positioned anywhere on the screen.");
    connect(settings.add_zone_button, &QPushButton::clicked, this, [&settings, this]() {
        if(settings.capture_area_preview)
        {
            CaptureAreaPreviewWidget* preview_widget = static_cast<CaptureAreaPreviewWidget*>(settings.capture_area_preview);
            preview_widget->AddZone();
        }
    });
    zones_layout->addWidget(settings.add_zone_button);
    
    // Capture Area Preview
    CaptureAreaPreviewWidget* preview_widget = new CaptureAreaPreviewWidget(
        &settings.capture_zones,
        plane,
        &settings.show_test_pattern,
        &settings.show_screen_preview
    );
    preview_widget->SetValueChangedCallback([this]() {
        OnParameterChanged();
    });
    settings.capture_area_preview = preview_widget;
    settings.capture_area_preview->setEnabled(has_capture_source);
    zones_layout->addWidget(settings.capture_area_preview);
    
    zones_group->setLayout(zones_layout);
    monitor_form->addRow("Zones:", zones_group);

    settings.group_box->setLayout(monitor_form);
    monitors_layout->addWidget(settings.group_box);
    
    // Don't call RefreshReferencePointDropdowns here - it will be called after SetReferencePoints
    // The combo box will be populated when reference_points is set and RefreshReferencePointDropdowns is called
}

void ScreenMirror3D::StartCaptureIfNeeded()
{
    // Get ALL planes and start capture for each one with a capture source
    std::vector<DisplayPlane3D*> planes = DisplayPlaneManager::instance()->GetDisplayPlanes();

    ScreenCaptureManager& capture_mgr = ScreenCaptureManager::Instance();

    if(!capture_mgr.IsInitialized())
    {
        capture_mgr.Initialize();
    }

    for(size_t plane_index = 0; plane_index < planes.size(); plane_index++)
    {
        DisplayPlane3D* plane = planes[plane_index];
        if(!plane) continue;

        std::string capture_id = plane->GetCaptureSourceId();
        if(capture_id.empty()) continue;

        if(!capture_mgr.IsCapturing(capture_id))
        {
            capture_mgr.StartCapture(capture_id);
            LOG_INFO("[ScreenMirror3D] Started capture for '%s' (plane: %s)",
                       capture_id.c_str(), plane->GetName().c_str());
        }
    }
}

void ScreenMirror3D::StopCaptureIfNeeded()
{
    // Screen capture is managed globally by ScreenCaptureManager
    // We don't stop it here as other effects or instances may be using it
    // The manager handles cleanup when all references are gone
}

void ScreenMirror3D::RefreshMonitorStatus()
{
    if(!monitor_status_label) return;

    std::vector<DisplayPlane3D*> planes = DisplayPlaneManager::instance()->GetDisplayPlanes();
    int total_count = 0;
    int active_count = 0;
    
    // Update existing monitor group boxes to reflect current capture source status
    for(unsigned int plane_index = 0; plane_index < planes.size(); plane_index++)
    {
        DisplayPlane3D* plane = planes[plane_index];
        if(!plane) continue;
        
        total_count++;
        bool has_capture_source = !plane->GetCaptureSourceId().empty();
        if(has_capture_source)
        {
            active_count++;
        }
        
        std::string plane_name = plane->GetName();
        std::map<std::string, MonitorSettings>::iterator settings_it = monitor_settings.find(plane_name);
        if(settings_it == monitor_settings.end())
        {
            // Optimize: use emplace to avoid extra find() call
            MonitorSettings new_settings;
            int plane_ref_index = plane->GetReferencePointIndex();
            if(plane_ref_index >= 0)
            {
                new_settings.reference_point_index = plane_ref_index;
            }
            settings_it = monitor_settings.emplace(plane_name, new_settings).first;
        }
        MonitorSettings& settings = settings_it->second;
        
        if(settings.reference_point_index < 0)
        {
            int plane_ref_index = plane->GetReferencePointIndex();
            if(plane_ref_index >= 0)
            {
                settings.reference_point_index = plane_ref_index;
            }
        }

        if(!settings.group_box && monitors_container && monitors_layout)
        {
            CreateMonitorSettingsUI(plane, settings);
        }
        else if(settings.group_box)
        {
            QString display_name = QString::fromStdString(plane_name);
            if(!has_capture_source)
            {
                display_name += " (No Capture Source)";
            }
            settings.group_box->setTitle(display_name);
            settings.group_box->setEnabled(has_capture_source);
            
            if(has_capture_source)
            {
                settings.group_box->setToolTip("Enable or disable this monitor's influence.");
                settings.group_box->setStyleSheet("");
            }
            else
            {
                settings.group_box->setToolTip("This monitor needs a capture source assigned in Display Plane settings.");
                settings.group_box->setStyleSheet("QGroupBox { color: #cc6600; }");
            }
            
            if(settings.scale_slider) settings.scale_slider->setEnabled(has_capture_source);
            if(settings.ref_point_combo) settings.ref_point_combo->setEnabled(has_capture_source);
            if(settings.softness_slider) settings.softness_slider->setEnabled(has_capture_source);
            if(settings.blend_slider) settings.blend_slider->setEnabled(has_capture_source);
            // Enable/disable all per-monitor controls
            if(settings.scale_slider) settings.scale_slider->setEnabled(has_capture_source);
            if(settings.scale_invert_check) settings.scale_invert_check->setEnabled(has_capture_source);
            if(settings.smoothing_time_slider) settings.smoothing_time_slider->setEnabled(has_capture_source);
            if(settings.brightness_slider) settings.brightness_slider->setEnabled(has_capture_source);
            if(settings.brightness_threshold_slider) settings.brightness_threshold_slider->setEnabled(has_capture_source);
            if(settings.softness_slider) settings.softness_slider->setEnabled(has_capture_source);
            if(settings.blend_slider) settings.blend_slider->setEnabled(has_capture_source);
            if(settings.propagation_speed_slider) settings.propagation_speed_slider->setEnabled(has_capture_source);
            if(settings.wave_decay_slider) settings.wave_decay_slider->setEnabled(has_capture_source);
            if(settings.ref_point_combo) settings.ref_point_combo->setEnabled(has_capture_source);
            if(settings.test_pattern_check) settings.test_pattern_check->setEnabled(has_capture_source);
            if(settings.screen_preview_check) settings.screen_preview_check->setEnabled(has_capture_source);
            if(settings.capture_area_preview)
            {
                settings.capture_area_preview->setEnabled(has_capture_source);
                // Update display plane pointer and capture zones
                CaptureAreaPreviewWidget* preview_widget = static_cast<CaptureAreaPreviewWidget*>(settings.capture_area_preview);
                preview_widget->SetDisplayPlane(plane);
                preview_widget->capture_zones = &settings.capture_zones;
            }
            if(settings.add_zone_button)
            {
                settings.add_zone_button->setEnabled(has_capture_source);
            }
            
            if(!has_capture_source && settings.group_box->isChecked())
            {
                settings.group_box->setChecked(false);
            }
        }
    }

    QString status_text;
    if(total_count == 0)
    {
        status_text = "No Display Planes configured";
    }
    else if(active_count == 0)
    {
        status_text = QString("Display Planes: %1 (none have capture sources)").arg(total_count);
    }
    else
    {
        status_text = QString("Display Planes: %1 total, %2 active").arg(total_count).arg(active_count);
    }
    monitor_status_label->setText(status_text);

    // Update or create help label
    QWidget* parent = monitor_status_label->parentWidget();
    if(parent)
    {
        QGroupBox* status_group = qobject_cast<QGroupBox*>(parent);
        if(status_group && status_group->layout())
        {
            if(total_count > 0 && active_count == 0)
            {
                if(!monitor_help_label)
                {
                    monitor_help_label = new QLabel("Tip: Assign capture sources to Display Planes in the Object Creator tab.");
                    monitor_help_label->setWordWrap(true);
                    monitor_help_label->setStyleSheet("QLabel { color: #cc6600; font-style: italic; }");
                    status_group->layout()->addWidget(monitor_help_label);
                }
            }
            else
            {
                if(monitor_help_label)
                {
                    monitor_help_label->deleteLater();
                    monitor_help_label = nullptr;
                }
            }
        }
    }
}
