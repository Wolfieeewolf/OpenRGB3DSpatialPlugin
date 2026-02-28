// SPDX-License-Identifier: GPL-2.0-only

#include "ScreenMirror3D.h"
#include "ScreenCaptureManager.h"
#include "DisplayPlane3D.h"
#include "DisplayPlaneManager.h"
#include "Geometry3DUtils.h"
#include "GridSpaceUtils.h"
#include "VirtualReferencePoint3D.h"
#include "OpenRGB3DSpatialTab.h"

REGISTER_EFFECT_3D(ScreenMirror3D);

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFormLayout>
#include <QSlider>
#include <QTimer>
#include <QDateTime>
#include <QSignalBlocker>
#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QPushButton>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <array>
#include <limits>
#include <functional>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <GL/gl.h>
#include <GL/glu.h>
#else
#include <GL/gl.h>
#include <GL/glu.h>
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static const int   PREVIEW_BUFFER_GRID_UNITS = 5;
static const float PREVIEW_CUBE_HALF_GRID_UNITS = 20.0f;
static const int   PREVIEW_GRID_RES = 11;
static const int   PREVIEW_LED_COUNT_VOLUME = PREVIEW_GRID_RES * PREVIEW_GRID_RES * PREVIEW_GRID_RES;
static const float DISPLAY_PREVIEW_SCALE = 0.5f;

static Vector3D TransformLocalToWorld(const Vector3D& local_pos, const Transform3D& transform)
{
    Vector3D scaled = {
        local_pos.x * transform.scale.x,
        local_pos.y * transform.scale.y,
        local_pos.z * transform.scale.z
    };
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
    float rx = transform.rotation.x * (float)(M_PI / 180.0);
    float ry = transform.rotation.y * (float)(M_PI / 180.0);
    float rz = transform.rotation.z * (float)(M_PI / 180.0);
    float temp_y = scaled.y * cosf(rx) - scaled.z * sinf(rx);
    float temp_z = scaled.y * sinf(rx) + scaled.z * cosf(rx);
    scaled.y = temp_y;
    scaled.z = temp_z;
    float temp_x = scaled.x * cosf(ry) + scaled.z * sinf(ry);
    temp_z = -scaled.x * sinf(ry) + scaled.z * cosf(ry);
    scaled.x = temp_x;
    scaled.z = temp_z;
    temp_x = scaled.x * cosf(rz) - scaled.y * sinf(rz);
    temp_y = scaled.x * sinf(rz) + scaled.y * cosf(rz);
    scaled.x = temp_x;
    scaled.y = temp_y;
    return {
        scaled.x + transform.position.x,
        scaled.y + transform.position.y,
        scaled.z + transform.position.z
    };
}

class AmbilightPreview3DWidget : public QOpenGLWidget, protected QOpenGLFunctions
{
public:
    static const int PREVIEW_LED_COUNT = PREVIEW_LED_COUNT_VOLUME;

    DisplayPlane3D* display_plane = nullptr;
    float grid_scale_mm_ = 10.0f;
    std::vector<Vector3D> positions;
    std::vector<Vector3D> box_corners_world;
    std::vector<RGBColor> colors;

    float cam_azimuth = 0.75f;
    float cam_elevation = 0.45f;
    float cam_zoom = 1.0f;
    QPoint last_mouse_pos;
    bool dragging = false;

    bool* show_screen_preview_ptr = nullptr;
    bool* show_test_pattern_ptr = nullptr;
    bool* show_test_pattern_pulse_ptr = nullptr;
    GLuint display_texture_id = 0;
    QTimer* pulse_animation_timer = nullptr;

    void SetPreviewOptions(bool* screen, bool* test, bool* test_pulse)
    {
        show_screen_preview_ptr = screen;
        show_test_pattern_ptr = test;
        show_test_pattern_pulse_ptr = test_pulse;
        update();
    }

    void tickPulseAnimation()
    {
        if(show_test_pattern_pulse_ptr && *show_test_pattern_pulse_ptr && display_plane)
        {
            update();
        }
    }
    void SetGridScaleMM(float mm)
    {
        float v = (mm > 0.001f) ? mm : 10.0f;
        if(grid_scale_mm_ != v) { grid_scale_mm_ = v; updatePositions(); update(); }
    }
    float GetGridScaleMM() const { return grid_scale_mm_; }

    explicit AmbilightPreview3DWidget(QWidget* parent = nullptr)
        : QOpenGLWidget(parent)
    {
        setMinimumSize(200, 160);
        setMaximumHeight(280);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        setMouseTracking(true);
        setCursor(Qt::OpenHandCursor);
        setToolTip("Live ambilight preview: LED grid around display plane. Drag to rotate, scroll to zoom.");
        pulse_animation_timer = new QTimer(this);
        connect(pulse_animation_timer, &QTimer::timeout, this, [this]() { tickPulseAnimation(); });
        pulse_animation_timer->start(33);
    }

    void SetDisplayPlane(DisplayPlane3D* plane)
    {
        display_plane = plane;
        updatePositions();
        update();
    }

    void updatePositions()
    {
        positions.clear();
        box_corners_world.clear();
        if(!display_plane) return;

        const Transform3D& t = display_plane->GetTransform();
        float half = PREVIEW_CUBE_HALF_GRID_UNITS;
        const int res = PREVIEW_GRID_RES;
        float step = (2.0f * half) / (float)(res - 1);

        Vector3D local_corners[8] = {
            {-half, -half, -half}, { half, -half, -half}, { half,  half, -half}, {-half,  half, -half},
            {-half, -half,  half}, { half, -half,  half}, { half,  half,  half}, {-half,  half,  half}
        };
        for(int i = 0; i < 8; i++)
            box_corners_world.push_back(TransformLocalToWorld(local_corners[i], t));

        auto addLocal = [&](float lx, float ly, float lz) {
            positions.push_back(TransformLocalToWorld(Vector3D{lx, ly, lz}, t));
        };

        for(int ix = 0; ix < res; ix++)
            for(int iy = 0; iy < res; iy++)
                for(int iz = 0; iz < res; iz++)
                {
                    float x = -half + (float)ix * step;
                    float y = -half + (float)iy * step;
                    float z = -half + (float)iz * step;
                    addLocal(x, y, z);
                }
    }

    const std::vector<Vector3D>& GetPreviewPositions() const { return positions; }

    void SetColors(const std::vector<RGBColor>& new_colors)
    {
        colors = new_colors;
        update();
    }

protected:
    void mousePressEvent(QMouseEvent* event) override
    {
        if(event->button() == Qt::LeftButton)
        {
            dragging = true;
            last_mouse_pos = event->pos();
            setCursor(Qt::ClosedHandCursor);
        }
            QOpenGLWidget::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent* event) override
    {
        if(dragging)
        {
            QPoint delta = event->pos() - last_mouse_pos;
            last_mouse_pos = event->pos();
            cam_azimuth -= (float)delta.x() * 0.01f;
            cam_elevation += (float)delta.y() * 0.01f;
            float max_el = 1.45f;
            if(cam_elevation > max_el) cam_elevation = max_el;
            if(cam_elevation < -max_el) cam_elevation = -max_el;
            update();
        }
        QOpenGLWidget::mouseMoveEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent* event) override
    {
        if(event->button() == Qt::LeftButton)
        {
            dragging = false;
            setCursor(Qt::OpenHandCursor);
        }
        QOpenGLWidget::mouseReleaseEvent(event);
    }

    void wheelEvent(QWheelEvent* event) override
    {
        int delta = event->angleDelta().y();
        if(delta > 0)
            cam_zoom *= 1.15f;
        else if(delta < 0)
            cam_zoom *= 0.87f;
        if(cam_zoom < 0.2f) cam_zoom = 0.2f;
        if(cam_zoom > 8.0f) cam_zoom = 8.0f;
        update();
        event->accept();
    }

    void initializeGL() override
    {
        initializeOpenGLFunctions();
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glClearColor(0.15f, 0.15f, 0.18f, 1.0f);
    }

    void resizeGL(int w, int h) override
    {
        if(h == 0) h = 1;
        glViewport(0, 0, w, h);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        float aspect = (float)w / (float)h;
        gluPerspective(45.0, aspect, 0.1, 1000.0);
        glMatrixMode(GL_MODELVIEW);
    }

    void paintGL() override
    {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glDisable(GL_LIGHTING);
        glDisable(GL_TEXTURE_2D);

        if(positions.empty() || box_corners_world.size() < 8)
        {
            glColor3f(0.4f, 0.4f, 0.45f);
            glBegin(GL_QUADS);
            glVertex2f(-0.5f, -0.5f);
            glVertex2f( 0.5f, -0.5f);
            glVertex2f( 0.5f,  0.5f);
            glVertex2f(-0.5f,  0.5f);
            glEnd();
            return;
        }

        float cx = 0.0f, cy = 0.0f, cz = 0.0f;
        for(const Vector3D& pos : positions)
        {
            cx += pos.x; cy += pos.y; cz += pos.z;
        }
        float n = (float)positions.size();
        cx /= n; cy /= n; cz /= n;

        float radius = 0.0f;
        for(const Vector3D& pos : positions)
        {
            float d = sqrtf((pos.x - cx) * (pos.x - cx) + (pos.y - cy) * (pos.y - cy) + (pos.z - cz) * (pos.z - cz));
            if(d > radius) radius = d;
        }
        for(const Vector3D& pos : box_corners_world)
        {
            float d = sqrtf((pos.x - cx) * (pos.x - cx) + (pos.y - cy) * (pos.y - cy) + (pos.z - cz) * (pos.z - cz));
            if(d > radius) radius = d;
        }
        if(radius < 1.0f) radius = 1.0f;

        float cam_dist = radius * 2.8f / cam_zoom;
        float camera_x = cx + cam_dist * cosf(cam_elevation) * cosf(cam_azimuth);
        float camera_y = cy + cam_dist * sinf(cam_elevation);
        float camera_z = cz + cam_dist * cosf(cam_elevation) * sinf(cam_azimuth);

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        gluLookAt(camera_x, camera_y, camera_z,
                  cx, cy, cz,
                  0.0f, 1.0f, 0.0f);

        glColor3f(0.35f, 0.38f, 0.45f);
        glLineWidth(1.5f);
        glBegin(GL_LINES);
        int cube_edges[12][2] = {{0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}};
        for(int e = 0; e < 12; e++)
        {
            const Vector3D& a = box_corners_world[cube_edges[e][0]];
            const Vector3D& b = box_corners_world[cube_edges[e][1]];
            glVertex3f(a.x, a.y, a.z);
            glVertex3f(b.x, b.y, b.z);
        }
        glEnd();

        if(display_plane)
        {
            const Transform3D& t = display_plane->GetTransform();
            float width_units = MMToGridUnits(display_plane->GetWidthMM(), grid_scale_mm_);
            float height_units = MMToGridUnits(display_plane->GetHeightMM(), grid_scale_mm_);
            float half_w = width_units * 0.5f * DISPLAY_PREVIEW_SCALE;
            float half_h = height_units * 0.5f * DISPLAY_PREVIEW_SCALE;
            Vector3D local[4] = { {-half_w,-half_h,0}, {half_w,-half_h,0}, {half_w,half_h,0}, {-half_w,half_h,0} };
            Vector3D world[4];
            for(int i = 0; i < 4; i++)
                world[i] = TransformLocalToWorld(local[i], t);

            bool show_test = show_test_pattern_ptr && *show_test_pattern_ptr;
            bool show_pulse = show_test_pattern_pulse_ptr && *show_test_pattern_pulse_ptr;
            bool show_screen = show_screen_preview_ptr && *show_screen_preview_ptr && !show_test && !show_pulse;

            if(!show_screen && display_texture_id != 0)
            {
                glDeleteTextures(1, &display_texture_id);
                display_texture_id = 0;
            }

            if(show_pulse)
            {
                float t_sec = (float)QDateTime::currentMSecsSinceEpoch() / 1000.0f;
                float pulse = 0.5f + 0.5f * sinf(t_sec * (float)(2.0 * M_PI));
                int color_cycle = ((int)floorf(t_sec)) % 3;
                float r = 0.0f, g = 0.0f, b = 0.0f;
                if(color_cycle == 0)      { r = 1.0f; }
                else if(color_cycle == 1) { g = 1.0f; }
                else                      { b = 1.0f; }
                r *= pulse; g *= pulse; b *= pulse;
                glColor4f(r, g, b, 0.9f);
                glBegin(GL_QUADS);
                for(int i = 0; i < 4; i++)
                    glVertex3f(world[i].x, world[i].y, world[i].z);
                glEnd();
            }
            else if(show_test)
            {
                Vector3D center, mid_bottom, mid_top, mid_left, mid_right;
                center.x = (world[0].x + world[2].x) * 0.5f;
                center.y = (world[0].y + world[2].y) * 0.5f;
                center.z = (world[0].z + world[2].z) * 0.5f;
                mid_bottom.x = (world[0].x + world[1].x) * 0.5f; mid_bottom.y = (world[0].y + world[1].y) * 0.5f; mid_bottom.z = (world[0].z + world[1].z) * 0.5f;
                mid_top.x    = (world[2].x + world[3].x) * 0.5f; mid_top.y    = (world[2].y + world[3].y) * 0.5f; mid_top.z    = (world[2].z + world[3].z) * 0.5f;
                mid_left.x   = (world[0].x + world[3].x) * 0.5f; mid_left.y   = (world[0].y + world[3].y) * 0.5f; mid_left.z   = (world[0].z + world[3].z) * 0.5f;
                mid_right.x  = (world[1].x + world[2].x) * 0.5f; mid_right.y  = (world[1].y + world[2].y) * 0.5f; mid_right.z  = (world[1].z + world[2].z) * 0.5f;
                glColor4f(1.0f, 0.0f, 0.0f, 0.85f);
                glBegin(GL_QUADS);
                glVertex3f(world[0].x, world[0].y, world[0].z); glVertex3f(mid_bottom.x, mid_bottom.y, mid_bottom.z);
                glVertex3f(center.x, center.y, center.z); glVertex3f(mid_left.x, mid_left.y, mid_left.z);
                glEnd();
                glColor4f(0.0f, 1.0f, 0.0f, 0.85f);
                glBegin(GL_QUADS);
                glVertex3f(mid_bottom.x, mid_bottom.y, mid_bottom.z); glVertex3f(world[1].x, world[1].y, world[1].z);
                glVertex3f(mid_right.x, mid_right.y, mid_right.z); glVertex3f(center.x, center.y, center.z);
                glEnd();
                glColor4f(0.0f, 0.0f, 1.0f, 0.85f);
                glBegin(GL_QUADS);
                glVertex3f(center.x, center.y, center.z); glVertex3f(mid_right.x, mid_right.y, mid_right.z);
                glVertex3f(world[2].x, world[2].y, world[2].z); glVertex3f(mid_top.x, mid_top.y, mid_top.z);
                glEnd();
                glColor4f(1.0f, 1.0f, 0.0f, 0.85f);
                glBegin(GL_QUADS);
                glVertex3f(mid_left.x, mid_left.y, mid_left.z); glVertex3f(center.x, center.y, center.z);
                glVertex3f(mid_top.x, mid_top.y, mid_top.z); glVertex3f(world[3].x, world[3].y, world[3].z);
                glEnd();
            }
            else if(show_screen)
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
                            if(display_texture_id == 0)
                                glGenTextures(1, &display_texture_id);
                            if(display_texture_id != 0)
                            {
                                glBindTexture(GL_TEXTURE_2D, display_texture_id);
                                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, frame->width, frame->height,
                                            0, GL_RGBA, GL_UNSIGNED_BYTE, frame->data.data());
                                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                                glBindTexture(GL_TEXTURE_2D, 0);

                                glEnable(GL_TEXTURE_2D);
                                glBindTexture(GL_TEXTURE_2D, display_texture_id);
                                glColor4f(1.0f, 1.0f, 1.0f, 0.85f);
                                glBegin(GL_QUADS);
                                glTexCoord2f(0.0f, 1.0f); glVertex3f(world[0].x, world[0].y, world[0].z);
                                glTexCoord2f(1.0f, 1.0f); glVertex3f(world[1].x, world[1].y, world[1].z);
                                glTexCoord2f(1.0f, 0.0f); glVertex3f(world[2].x, world[2].y, world[2].z);
                                glTexCoord2f(0.0f, 0.0f); glVertex3f(world[3].x, world[3].y, world[3].z);
                                glEnd();
                                glBindTexture(GL_TEXTURE_2D, 0);
                                glDisable(GL_TEXTURE_2D);
                            }
                        }
                        else
                        {
                            glColor4f(0.2f, 0.55f, 0.6f, 0.25f);
                            glBegin(GL_QUADS);
                            for(int i = 0; i < 4; i++)
                                glVertex3f(world[i].x, world[i].y, world[i].z);
                            glEnd();
                        }
                    }
                    else
                    {
                        glColor4f(0.2f, 0.55f, 0.6f, 0.25f);
                        glBegin(GL_QUADS);
                        for(int i = 0; i < 4; i++)
                            glVertex3f(world[i].x, world[i].y, world[i].z);
                        glEnd();
                    }
                }
                else
                {
                    glColor4f(0.2f, 0.55f, 0.6f, 0.25f);
                    glBegin(GL_QUADS);
                    for(int i = 0; i < 4; i++)
                        glVertex3f(world[i].x, world[i].y, world[i].z);
                    glEnd();
                }
            }
            else
            {
                glColor4f(0.2f, 0.55f, 0.6f, 0.25f);
                glBegin(GL_QUADS);
                for(int i = 0; i < 4; i++)
                    glVertex3f(world[i].x, world[i].y, world[i].z);
                glEnd();
            }
            glColor3f(0.35f, 0.7f, 0.75f);
            glBegin(GL_LINE_LOOP);
            for(int i = 0; i < 4; i++)
                glVertex3f(world[i].x, world[i].y, world[i].z);
            glEnd();
        }

        std::vector<int> order(positions.size());
        for(size_t i = 0; i < positions.size(); i++)
            order[i] = (int)i;
        std::sort(order.begin(), order.end(), [&](int a, int b) {
            const Vector3D& pa = positions[a];
            const Vector3D& pb = positions[b];
            float da = (pa.x - camera_x) * (pa.x - camera_x) + (pa.y - camera_y) * (pa.y - camera_y) + (pa.z - camera_z) * (pa.z - camera_z);
            float db = (pb.x - camera_x) * (pb.x - camera_x) + (pb.y - camera_y) * (pb.y - camera_y) + (pb.z - camera_z) * (pb.z - camera_z);
            return da > db;
        });
        glEnable(GL_POINT_SMOOTH);
        glHint(GL_POINT_SMOOTH_HINT, GL_NICEST);
        glPointSize(2.5f);
        glBegin(GL_POINTS);
        for(int idx : order)
        {
            const Vector3D& p = positions[idx];
            float r_ = 0.35f, g_ = 0.35f, b_ = 0.4f;
            if((size_t)idx < colors.size())
            {
                RGBColor c = colors[idx];
                r_ = RGBGetRValue(c) / 255.0f;
                g_ = RGBGetGValue(c) / 255.0f;
                b_ = RGBGetBValue(c) / 255.0f;
            }
            glColor3f(r_, g_, b_);
            glVertex3f(p.x, p.y, p.z);
        }
        glEnd();
        glDisable(GL_POINT_SMOOTH);
    }
};

class CaptureAreaPreviewWidget : public QWidget
{
public:
    DisplayPlane3D* display_plane;
    std::vector<ScreenMirror3D::CaptureZone>* capture_zones;
    std::function<void()> on_value_changed;
    bool* show_test_pattern_ptr;
    bool* show_screen_preview_ptr;
    float* black_bar_letterbox_percent_ptr;
    float* black_bar_pillarbox_percent_ptr;

    CaptureAreaPreviewWidget(std::vector<ScreenMirror3D::CaptureZone>* zones,
                             DisplayPlane3D* plane = nullptr,
                             bool* test_pattern = nullptr,
                             bool* screen_preview = nullptr,
                             float* black_bar_letterbox_percent = nullptr,
                             float* black_bar_pillarbox_percent = nullptr,
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
        if(capture_zones->size() <= 1) return;
        
        capture_zones->erase(capture_zones->begin() + selected_zone_index);
        if(selected_zone_index >= (int)capture_zones->size())
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
        {
            aspect_ratio = width_mm / height_mm;
        }
        
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
                    {
                        capture_mgr.StartCapture(source_id);
                    }
                    
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
            const ScreenMirror3D::CaptureZone& zone = (*capture_zones)[i];
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
                const ScreenMirror3D::CaptureZone& zone = (*capture_zones)[i];
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
        
        for(size_t i = 0; i < capture_zones->size(); i++)
        {
            const ScreenMirror3D::CaptureZone& zone = (*capture_zones)[i];
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
            const ScreenMirror3D::CaptureZone& zone = (*capture_zones)[i];
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
        preview_rect = calc_preview_rect;
        
        if(!dragging)
        {
            CornerHandle new_hover = None;
            for(size_t i = 0; i < capture_zones->size(); i++)
            {
                const ScreenMirror3D::CaptureZone& zone = (*capture_zones)[i];
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
        
        ScreenMirror3D::CaptureZone& zone = (*capture_zones)[selected_zone_index];
        
        if(calc_preview_rect.width() <= 0 || calc_preview_rect.height() <= 0)
            return;
        
        float normalized_x = (float)(pos.x() - calc_preview_rect.left()) / (float)calc_preview_rect.width();
        float normalized_y = (float)(pos.y() - calc_preview_rect.top()) / (float)calc_preview_rect.height();

        normalized_x = std::max(-0.01f, std::min(1.01f, normalized_x));
        normalized_y = std::max(-0.01f, std::min(1.01f, normalized_y));
        normalized_x = std::max(0.0f, std::min(1.0f, normalized_x));
        normalized_y = std::max(0.0f, std::min(1.0f, normalized_y));
        
        float v = 1.0f - normalized_y;
        
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
            
            if(zone.u_min > zone.u_max) { float temp = zone.u_min; zone.u_min = zone.u_max; zone.u_max = temp; }
            if(zone.v_min > zone.v_max) { float temp = zone.v_min; zone.v_min = zone.v_max; zone.v_max = temp; }
        }
        else
        {
            const float min_size = 0.01f;
            normalized_x = std::max(0.0f, std::min(1.0f, normalized_x));
            v            = std::max(0.0f, std::min(1.0f, v));

            switch(drag_handle)
            {
                case TopLeft:
                {
                    zone.u_max = drag_start_zone.u_max;
                    zone.v_min = drag_start_zone.v_min;
                    zone.u_min = normalized_x;
                    zone.v_max = v;
                    break;
                }
                case TopRight:
                {
                    zone.u_min = drag_start_zone.u_min;
                    zone.v_min = drag_start_zone.v_min;
                    zone.u_max = normalized_x;
                    zone.v_max = v;
                    break;
                }
                case BottomRight:
                {
                    zone.u_min = drag_start_zone.u_min;
                    zone.v_max = drag_start_zone.v_max;
                    zone.u_max = normalized_x;
                    zone.v_min = v;
                    break;
                }
                case BottomLeft:
                {
                    zone.u_max = drag_start_zone.u_max;
                    zone.v_max = drag_start_zone.v_max;
                    zone.u_min = normalized_x;
                    zone.v_min = v;
                    break;
                }
                default:
                    break;
            }

            zone.u_min = std::max(0.0f, std::min(1.0f, zone.u_min));
            zone.u_max = std::max(0.0f, std::min(1.0f, zone.u_max));
            zone.v_min = std::max(0.0f, std::min(1.0f, zone.v_min));
            zone.v_max = std::max(0.0f, std::min(1.0f, zone.v_max));
            switch(drag_handle)
            {
                case TopLeft:
                    if(zone.u_max - zone.u_min < min_size) zone.u_min = zone.u_max - min_size;
                    if(zone.v_max - zone.v_min < min_size) zone.v_max = zone.v_min + min_size;
                    break;
                case TopRight:
                    if(zone.u_max - zone.u_min < min_size) zone.u_max = zone.u_min + min_size;
                    if(zone.v_max - zone.v_min < min_size) zone.v_max = zone.v_min + min_size;
                    break;
                case BottomRight:
                    if(zone.u_max - zone.u_min < min_size) zone.u_max = zone.u_min + min_size;
                    if(zone.v_max - zone.v_min < min_size) zone.v_min = zone.v_max - min_size;
                    break;
                case BottomLeft:
                    if(zone.u_max - zone.u_min < min_size) zone.u_min = zone.u_max - min_size;
                    if(zone.v_max - zone.v_min < min_size) zone.v_min = zone.v_max - min_size;
                    break;
                default:
                    break;
            }
            if(zone.u_max <= zone.u_min) zone.u_max = std::min(1.0f, zone.u_min + min_size);
            if(zone.v_max <= zone.v_min) zone.v_min = std::max(0.0f, zone.v_max - min_size);
            zone.u_min = std::max(0.0f, std::min(1.0f, zone.u_min));
            zone.u_max = std::max(0.0f, std::min(1.0f, zone.u_max));
            zone.v_min = std::max(0.0f, std::min(1.0f, zone.v_min));
            zone.v_max = std::max(0.0f, std::min(1.0f, zone.v_max));
        }
        if(on_value_changed)
        {
            on_value_changed();
        }
        
        update();
    }
    
    void mouseReleaseEvent(QMouseEvent* event) override
    {
        (void)event;
        if(dragging && selected_zone_index >= 0 && selected_zone_index < (int)capture_zones->size())
        {
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
    QRect preview_rect;
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
    , ambilight_preview_timer(nullptr)
    , monitors_layout(nullptr)
    , grid_scale_mm_(10.0f)
    , source_effect_for_preview_(nullptr)
    , capture_quality(1)
    , capture_quality_combo(nullptr)
    , show_test_pattern(false)
    , reference_points(nullptr)
{
}

ScreenMirror3D::~ScreenMirror3D()
{
    StopCaptureIfNeeded();
}

EffectInfo3D ScreenMirror3D::GetEffectInfo()
{
    EffectInfo3D info           = {};
    info.info_version           = 2;
    info.effect_name            = "Screen Mirror 3D";
    info.effect_description     = "Maps screen content onto LEDs in 3D space";
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
    
    info.show_color_controls    = false;
    info.show_speed_control     = false;
    info.show_brightness_control = false;
    info.show_frequency_control = false;
    info.show_size_control      = false;
    info.show_scale_control     = false;
    info.show_fps_control       = false;
    info.show_axis_control      = false;

    return info;
}

void ScreenMirror3D::SetupCustomUI(QWidget* parent)
{
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
    QGroupBox* status_group = new QGroupBox("Multi-Monitor Status");
    QVBoxLayout* status_layout = new QVBoxLayout();

    QLabel* info_label = new QLabel("Uses every active display plane automatically.");
    info_label->setWordWrap(true);
    info_label->setStyleSheet("QLabel { color: #888; font-style: italic; }");
    status_layout->addWidget(info_label);
    monitor_status_label = new QLabel("Calculating...");
    monitor_status_label->setStyleSheet("QLabel { font-weight: bold; font-size: 14pt; }");
    status_layout->addWidget(monitor_status_label);
    monitor_help_label = nullptr;

    status_group->setLayout(status_layout);
    main_layout->addWidget(status_group);
    QGroupBox* capture_group = new QGroupBox("Capture Quality");
    QHBoxLayout* capture_layout = new QHBoxLayout();
    QLabel* capture_quality_label = new QLabel("Resolution:");
    capture_quality_combo = new QComboBox();
    capture_quality_combo->addItem("Low (320×180)", QVariant(0));
    capture_quality_combo->addItem("Medium (480×270)", QVariant(1));
    capture_quality_combo->addItem("High (640×360)", QVariant(2));
    capture_quality_combo->addItem("Ultra (960×540)", QVariant(3));
    capture_quality_combo->addItem("Maximum (1280×720)", QVariant(4));
    capture_quality_combo->addItem("1080p (1920×1080)", QVariant(5));
    capture_quality_combo->addItem("1440p (2560×1440)", QVariant(6));
    capture_quality_combo->addItem("4K (3840×2160)", QVariant(7));
    capture_quality_combo->setCurrentIndex(std::clamp(capture_quality, 0, 7));
    capture_quality_combo->setToolTip("Higher resolution = sharper ambilight, more CPU/GPU/RAM. 4K for high-end GPUs.");
    capture_layout->addWidget(capture_quality_label);
    capture_layout->addWidget(capture_quality_combo, 1);
    capture_group->setLayout(capture_layout);
    main_layout->addWidget(capture_group);
    connect(capture_quality_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        capture_quality = std::clamp(index, 0, 7);
        int w = 320, h = 180;
        if(capture_quality == 1) { w = 480; h = 270; }
        else if(capture_quality == 2) { w = 640; h = 360; }
        else if(capture_quality == 3) { w = 960; h = 540; }
        else if(capture_quality == 4) { w = 1280; h = 720; }
        else if(capture_quality == 5) { w = 1920; h = 1080; }
        else if(capture_quality == 6) { w = 2560; h = 1440; }
        else if(capture_quality == 7) { w = 3840; h = 2160; }
        ScreenCaptureManager::Instance().SetDownscaleResolution(w, h);
    });

    monitors_container = new QGroupBox("Per-Monitor Balance");
    monitors_layout = new QVBoxLayout();
    monitors_layout->setSpacing(6);

    std::vector<DisplayPlane3D*> planes = DisplayPlaneManager::instance()->GetDisplayPlanes();
    for(unsigned int plane_index = 0; plane_index < planes.size(); plane_index++)
    {
        DisplayPlane3D* plane = planes[plane_index];
        if(!plane) continue;

        std::string plane_name = plane->GetName();

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

    RefreshMonitorStatus();
    main_layout->addStretch();

    AddWidgetToParent(container, parent);

    StartCaptureIfNeeded();

    if(!ambilight_preview_timer)
    {
        ambilight_preview_timer = new QTimer(this);
        connect(ambilight_preview_timer, &QTimer::timeout, this, &ScreenMirror3D::UpdateAmbilightPreviews);
        ambilight_preview_timer->start(33);
        QTimer::singleShot(0, this, &ScreenMirror3D::UpdateAmbilightPreviews);
    }

    QTimer::singleShot(100, this, &ScreenMirror3D::OnScreenPreviewChanged);
}

void ScreenMirror3D::UpdateParams(SpatialEffectParams& /*params*/)
{
}

RGBColor ScreenMirror3D::CalculateColor(float /*x*/, float /*y*/, float /*z*/, float /*time*/)
{
    return ToRGBColor(0, 0, 0);
}

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
                                      float softness_percent,
                                      float curve_exponent = 1.0f)
    {
        coverage = std::max(0.0f, coverage);
        if(coverage <= 0.0001f || max_distance_mm <= 0.0f)
        {
            return 0.0f;
        }

        if(coverage >= 0.999f)
        {
            return 1.0f;
        }

        float normalized_distance = std::clamp(distance_mm / std::max(max_distance_mm, 1.0f), 0.0f, 1.0f);
        float exp_val = std::clamp(curve_exponent, 0.25f, 4.0f);
        normalized_distance = powf(normalized_distance, exp_val);
        float boundary = std::max(0.0f, 1.0f - coverage);
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

RGBColor ScreenMirror3D::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    return CalculateColorGridInternal(x, y, z, time, grid, nullptr, nullptr);
}

RGBColor ScreenMirror3D::CalculateColorGridInternal(float x, float y, float z, float time, const GridContext3D& grid,
                                                     const std::unordered_map<std::string, std::shared_ptr<CapturedFrame>>* frame_cache,
                                                     const std::vector<DisplayPlane3D*>* pre_fetched_planes)
{
    std::vector<DisplayPlane3D*> all_planes;
    if(pre_fetched_planes)
        all_planes = *pre_fetched_planes;
    else
        all_planes = DisplayPlaneManager::instance()->GetDisplayPlanes();

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
        std::shared_ptr<CapturedFrame> frame_blend;
        float blend_t;
        float weight;
        float blend;
        float delay_ms;
        uint64_t sample_timestamp;
        float brightness_multiplier;
        float brightness_threshold;
        float black_bar_letterbox_percent;
        float black_bar_pillarbox_percent;
        float smoothing_time_ms;
        bool use_test_pattern;
        bool use_test_pattern_pulse;
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
        
        if(mon_settings.capture_zones.empty())
        {
            mon_settings.capture_zones.push_back(CaptureZone(0.0f, 1.0f, 0.0f, 1.0f));
        }
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
        
        bool monitor_enabled = mon_settings.group_box ? mon_settings.group_box->isChecked() : mon_settings.enabled;
        if(!monitor_enabled)
        {
            continue;
        }

        bool monitor_test_pattern = mon_settings.show_test_pattern || mon_settings.show_test_pattern_pulse;
        
        std::string capture_id = plane->GetCaptureSourceId();
        std::shared_ptr<CapturedFrame> frame = nullptr;

        if(!monitor_test_pattern)
        {
            if(capture_id.empty())
            {
                continue;
            }
            if(frame_cache)
            {
                auto it = frame_cache->find(capture_id);
                frame = (it != frame_cache->end()) ? it->second : nullptr;
            }
            else
            {
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
            if(!frame || !frame->valid || frame->data.empty())
            {
                continue;
            }
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
            continue;
        }
        
        u = std::clamp(u, 0.0f, 1.0f);
        v = std::clamp(v, 0.0f, 1.0f);
        
        proj.u = u;
        proj.v = v;

        float monitor_scale = std::clamp(mon_settings.scale, 0.0f, 3.0f);
        float coverage = monitor_scale;
        float curve_exp = std::clamp(mon_settings.falloff_curve_exponent, 0.5f, 2.0f);
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
            distance_falloff = ComputeInvertedShellFalloff(proj.distance, reference_max_distance_mm, coverage, mon_settings.edge_softness, curve_exp);

            if(coverage >= 1.0f && distance_falloff < 1.0f)
            {
                distance_falloff = std::max(distance_falloff, std::min(coverage - 0.99f, 1.0f));
            }
        }

        std::shared_ptr<CapturedFrame> sampling_frame = frame;
        std::shared_ptr<CapturedFrame> sampling_frame_blend;
        float sampling_blend_t = 0.0f;
        int frame_offset = 0;
        float delay_ms = 0.0f;
        float speed_mm_per_ms = 0.0f;
        bool use_wave = !monitor_test_pattern && !capture_id.empty();
        if(use_wave && mon_settings.wave_time_to_edge_sec > 0.4f)
        {
            float t_sec = std::clamp(mon_settings.wave_time_to_edge_sec, 0.5f, 30.0f);
            speed_mm_per_ms = reference_max_distance_mm / (t_sec * 1000.0f);
            speed_mm_per_ms = std::max(speed_mm_per_ms, 0.1f);
            delay_ms = proj.distance / speed_mm_per_ms;
            delay_ms = std::clamp(delay_ms, 0.0f, 60000.0f);
        }
        else if(use_wave && mon_settings.propagation_speed_mm_per_ms > 0.001f)
        {
            float strength = std::clamp(mon_settings.propagation_speed_mm_per_ms, 1.0f, 800.0f);
            speed_mm_per_ms = 800.0f - strength + 1.0f;
            delay_ms = proj.distance / std::max(speed_mm_per_ms, 0.5f);
            delay_ms = std::clamp(delay_ms, 0.0f, 15000.0f);
        }

        if(use_wave && (mon_settings.wave_time_to_edge_sec > 0.4f || mon_settings.propagation_speed_mm_per_ms > 0.001f))
        {
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
                
                float avg_frame_time_ms = history.cached_avg_frame_time_ms > 0.0f
                    ? history.cached_avg_frame_time_ms : 16.67f;
                uint64_t latest_timestamp = frames.back()->timestamp_ms;
                const uint64_t frame_rate_stale_ms = 200;

                if(history.last_frame_rate_update == 0 ||
                   (latest_timestamp - history.last_frame_rate_update) > frame_rate_stale_ms)
                {
                    if(frames.size() >= 2)
                    {
                        size_t check_frames = std::min(frames.size() - 1, (size_t)10);
                        uint64_t total_time = 0;
                        size_t valid_pairs = 0;
                        const uint64_t min_delta_ms = 8;
                        const uint64_t max_delta_ms = 80;

                        for(size_t i = frames.size() - check_frames; i < frames.size(); i++)
                        {
                            if(i > 0 && i < frames.size())
                            {
                                uint64_t frame_time = frames[i]->timestamp_ms;
                                uint64_t prev_time = frames[i-1]->timestamp_ms;
                                uint64_t delta = (frame_time > prev_time) ? (frame_time - prev_time) : 0;
                                if(delta >= min_delta_ms && delta <= max_delta_ms)
                                {
                                    total_time += delta;
                                    valid_pairs++;
                                }
                            }
                        }

                        if(valid_pairs > 0 && total_time > 0)
                        {
                            float measured_ms = (float)total_time / (float)valid_pairs;
                            measured_ms = std::clamp(measured_ms, 12.0f, 50.0f);
                            if(history.cached_avg_frame_time_ms > 0.0f)
                                avg_frame_time_ms = 0.75f * history.cached_avg_frame_time_ms + 0.25f * measured_ms;
                            else
                                avg_frame_time_ms = measured_ms;
                        }
                    }
                    history.cached_avg_frame_time_ms = avg_frame_time_ms;
                    history.last_frame_rate_update = latest_timestamp;
                }
                else
                {
                    avg_frame_time_ms = history.cached_avg_frame_time_ms;
                }

                float frame_offset_f = delay_ms / std::max(avg_frame_time_ms, 1.0f);
                frame_offset_f = std::max(0.0f, frame_offset_f);
                int frame_offset_int = (int)(frame_offset_f + 0.5f);
                frame_offset_int = std::max(0, frame_offset_int);

                if(frame_offset_int < (int)frames.size())
                {
                    size_t frame_index_lo = frames.size() - 1 - (size_t)frame_offset_int;
                    float frac = frame_offset_f - std::floor(frame_offset_f);
                    if(frame_index_lo < frames.size())
                    {
                        sampling_frame = frames[frame_index_lo];
                        frame_offset = frame_offset_int;
                        if(frac > 0.01f && frame_index_lo + 1 < frames.size())
                        {
                            sampling_frame_blend = frames[frame_index_lo + 1];
                            sampling_blend_t = frac;
                        }
                    }
                }
            }
            else
            {
                frame_offset = 0;
            }
        }

        float wave_envelope = 1.0f;
        if((mon_settings.wave_time_to_edge_sec > 0.4f || mon_settings.propagation_speed_mm_per_ms > 0.001f) && mon_settings.wave_decay_ms > 0.1f)
        {
            if(delay_ms <= 0.0f && use_wave)
            {
                if(mon_settings.wave_time_to_edge_sec > 0.4f)
                {
                    float t_sec = std::clamp(mon_settings.wave_time_to_edge_sec, 0.5f, 30.0f);
                    speed_mm_per_ms = reference_max_distance_mm / (t_sec * 1000.0f);
                    delay_ms = proj.distance / std::max(speed_mm_per_ms, 0.1f);
                }
                else
                {
                    float strength = std::clamp(mon_settings.propagation_speed_mm_per_ms, 1.0f, 800.0f);
                    speed_mm_per_ms = 800.0f - strength + 1.0f;
                    delay_ms = proj.distance / std::max(speed_mm_per_ms, 0.5f);
                }
                delay_ms = std::clamp(delay_ms, 0.0f, 60000.0f);
            }
            wave_envelope = std::exp(-delay_ms / std::max(mon_settings.wave_decay_ms, 0.1f));
        }

        float weight = distance_falloff * wave_envelope;

        float ref_max_units = reference_max_distance_mm / std::max(scale_mm, 0.001f);
        if(ref_max_units > 0.001f && (std::fabs(mon_settings.front_back_balance) > 0.5f || std::fabs(mon_settings.left_right_balance) > 0.5f || std::fabs(mon_settings.top_bottom_balance) > 0.5f))
        {
            Vector3D ref_to_led = { led_pos.x - falloff_ref->x, led_pos.y - falloff_ref->y, led_pos.z - falloff_ref->z };
            const Transform3D& transform = plane->GetTransform();
            float rot[9];
            Geometry3D::ComputeRotationMatrix(transform.rotation, rot);
            Vector3D plane_right  = { rot[0], rot[3], rot[6] };
            Vector3D plane_up     = { rot[1], rot[4], rot[7] };
            Vector3D plane_normal = { rot[2], rot[5], rot[8] };
            float lateral = ref_to_led.x * plane_right.x + ref_to_led.y * plane_right.y + ref_to_led.z * plane_right.z;
            float vertical = ref_to_led.x * plane_up.x + ref_to_led.y * plane_up.y + ref_to_led.z * plane_up.z;
            float depth = ref_to_led.x * plane_normal.x + ref_to_led.y * plane_normal.y + ref_to_led.z * plane_normal.z;
            float lat_norm = std::clamp(lateral / ref_max_units, -1.0f, 1.0f);
            float vert_norm = std::clamp(vertical / ref_max_units, -1.0f, 1.0f);
            float depth_norm = std::clamp(depth / ref_max_units, -1.0f, 1.0f);
            float dir_fb = std::clamp(1.0f + (mon_settings.front_back_balance / 100.0f) * depth_norm, 0.0f, 2.0f);
            float dir_lr = std::clamp(1.0f + (mon_settings.left_right_balance / 100.0f) * lat_norm, 0.0f, 2.0f);
            float dir_tb = std::clamp(1.0f + (mon_settings.top_bottom_balance / 100.0f) * vert_norm, 0.0f, 2.0f);
            weight *= dir_fb * dir_lr * dir_tb;
        }

        if(weight > 0.01f)
        {
            MonitorContribution contrib;
            contrib.plane = plane;
            contrib.proj = proj;
            contrib.frame = sampling_frame;
            contrib.frame_blend = sampling_frame_blend;
            contrib.blend_t = sampling_blend_t;
            contrib.weight = weight;
            contrib.blend = mon_settings.blend;
            contrib.delay_ms = delay_ms;
            contrib.sample_timestamp = sampling_frame ? sampling_frame->timestamp_ms :
                                       (frame ? frame->timestamp_ms : 0);
            contrib.brightness_multiplier = mon_settings.brightness_multiplier;
            contrib.brightness_threshold = mon_settings.brightness_threshold;
            contrib.black_bar_letterbox_percent = mon_settings.black_bar_letterbox_percent;
            contrib.black_bar_pillarbox_percent = mon_settings.black_bar_pillarbox_percent;
            contrib.smoothing_time_ms = mon_settings.smoothing_time_ms;
            contrib.use_test_pattern = mon_settings.show_test_pattern;
            contrib.use_test_pattern_pulse = mon_settings.show_test_pattern_pulse;
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

        if(contrib.use_test_pattern_pulse)
        {
            float pulse = 0.5f + 0.5f * sinf(time * 2.0f);
            int color_cycle = ((int)floorf(time)) % 3;
            float rf = 0.0f, gf = 0.0f, bf = 0.0f;
            if(color_cycle == 0)      rf = 1.0f;
            else if(color_cycle == 1)  gf = 1.0f;
            else                       bf = 1.0f;
            r = rf * pulse * 255.0f;
            g = gf * pulse * 255.0f;
            b = bf * pulse * 255.0f;
        }
        else if(contrib.use_test_pattern)
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

            float lp = std::clamp(contrib.black_bar_letterbox_percent, 0.0f, 49.0f) / 100.0f;
            float pp = std::clamp(contrib.black_bar_pillarbox_percent, 0.0f, 49.0f) / 100.0f;
            float u_min = pp, u_max = 1.0f - pp;
            float v_min = lp, v_max = 1.0f - lp;
            float sample_u_clamped = std::clamp(sample_u, u_min, u_max);
            float flipped_v = 1.0f - sample_v;
            flipped_v = std::clamp(flipped_v, v_min, v_max);

            RGBColor sampled_color = Geometry3D::SampleFrame(
                contrib.frame->data.data(),
                contrib.frame->width,
                contrib.frame->height,
                sample_u_clamped,
                flipped_v,
                true
            );

            r = (float)RGBGetRValue(sampled_color);
            g = (float)RGBGetGValue(sampled_color);
            b = (float)RGBGetBValue(sampled_color);

            if(contrib.frame_blend && !contrib.frame_blend->data.empty() && contrib.blend_t > 0.01f)
            {
                float u_b = std::clamp(sample_u, u_min, u_max);
                float v_b = std::clamp(flipped_v, v_min, v_max);
                RGBColor sampled_blend = Geometry3D::SampleFrame(
                    contrib.frame_blend->data.data(),
                    contrib.frame_blend->width,
                    contrib.frame_blend->height,
                    u_b, v_b, true
                );
                float r2 = (float)RGBGetRValue(sampled_blend);
                float g2 = (float)RGBGetGValue(sampled_blend);
                float b2 = (float)RGBGetBValue(sampled_blend);
                float t = std::clamp(contrib.blend_t, 0.0f, 1.0f);
                r = (1.0f - t) * r + t * r2;
                g = (1.0f - t) * g + t * g2;
                b = (1.0f - t) * b + t * b2;
            }

            if(contrib.brightness_threshold > 0.0f)
            {
                float luminance = 0.299f * r + 0.587f * g + 0.114f * b;
                float thr = std::min(255.0f, contrib.brightness_threshold * (255.0f / 1020.0f));
                if(luminance <= thr)
                {
                    float t = (thr <= 0.0f) ? 1.0f : std::max(0.0f, luminance / thr);
                    t = t * t;
                    contrib.weight *= t;
                    r *= t;
                    g *= t;
                    b *= t;
                }
            }
        }

        r *= contrib.brightness_multiplier;
        g *= contrib.brightness_multiplier;
        b *= contrib.brightness_multiplier;

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

    if(total_weight > 0.0f)
    {
        total_r /= total_weight;
        total_g /= total_weight;
        total_b /= total_weight;
    }


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
                dt_ms_u64 = 16;
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

void ScreenMirror3D::BuildFrameCache(std::unordered_map<std::string, std::shared_ptr<CapturedFrame>>* out_cache)
{
    if(!out_cache) return;
    out_cache->clear();
    std::vector<DisplayPlane3D*> all_planes = DisplayPlaneManager::instance()->GetDisplayPlanes();
    ScreenCaptureManager& capture_mgr = ScreenCaptureManager::Instance();
    for(DisplayPlane3D* plane : all_planes)
    {
        if(!plane) continue;
        std::string plane_name = plane->GetName();
        auto it = monitor_settings.find(plane_name);
        if(it == monitor_settings.end()) continue;
        MonitorSettings& mon_settings = it->second;
        bool monitor_enabled = mon_settings.group_box ? mon_settings.group_box->isChecked() : mon_settings.enabled;
        if(!monitor_enabled) continue;
        if(mon_settings.show_test_pattern || mon_settings.show_test_pattern_pulse) continue;
        std::string capture_id = plane->GetCaptureSourceId();
        if(capture_id.empty()) continue;
        if(!capture_mgr.IsCapturing(capture_id))
        {
            capture_mgr.StartCapture(capture_id);
            if(!capture_mgr.IsCapturing(capture_id)) continue;
        }
        std::shared_ptr<CapturedFrame> frame = capture_mgr.GetLatestFrame(capture_id);
        if(!frame || !frame->valid || frame->data.empty()) continue;
        AddFrameToHistory(capture_id, frame);
        (*out_cache)[capture_id] = frame;
    }
}

void ScreenMirror3D::CalculateColorGridBatch(const std::vector<Vector3D>& positions, float time, const GridContext3D& grid,
                                             std::vector<RGBColor>& out)
{
    out.resize(positions.size());
    if(positions.empty()) return;
    std::vector<DisplayPlane3D*> planes = DisplayPlaneManager::instance()->GetDisplayPlanes();
    std::unordered_map<std::string, std::shared_ptr<CapturedFrame>> frame_cache;
    BuildFrameCache(&frame_cache);
    for(size_t i = 0; i < positions.size(); i++)
    {
        const Vector3D& p = positions[i];
        out[i] = CalculateColorGridInternal(p.x, p.y, p.z, time, grid, &frame_cache, &planes);
    }
}

nlohmann::json ScreenMirror3D::SaveSettings() const
{
    nlohmann::json settings;
    settings["capture_quality"] = std::clamp(capture_quality, 0, 7);
    nlohmann::json monitors = nlohmann::json::object();
    for(std::map<std::string, MonitorSettings>::const_iterator it = monitor_settings.begin();
        it != monitor_settings.end();
        ++it)
    {
        const MonitorSettings& mon_settings = it->second;
        nlohmann::json mon;
        mon["enabled"] = mon_settings.enabled;
        
        mon["scale"] = mon_settings.scale;
        mon["scale_inverted"] = mon_settings.scale_inverted;
        
        mon["smoothing_time_ms"] = mon_settings.smoothing_time_ms;
        mon["brightness_multiplier"] = mon_settings.brightness_multiplier;
        mon["brightness_threshold"] = mon_settings.brightness_threshold;
        mon["black_bar_letterbox_percent"] = mon_settings.black_bar_letterbox_percent;
        mon["black_bar_pillarbox_percent"] = mon_settings.black_bar_pillarbox_percent;
        
        mon["edge_softness"] = mon_settings.edge_softness;
        mon["blend"] = mon_settings.blend;
        mon["propagation_speed_mm_per_ms"] = mon_settings.propagation_speed_mm_per_ms;
        mon["wave_decay_ms"] = mon_settings.wave_decay_ms;
        mon["wave_time_to_edge_sec"] = mon_settings.wave_time_to_edge_sec;
        mon["falloff_curve_exponent"] = mon_settings.falloff_curve_exponent;
        mon["front_back_balance"] = mon_settings.front_back_balance;
        mon["left_right_balance"] = mon_settings.left_right_balance;
        mon["top_bottom_balance"] = mon_settings.top_bottom_balance;
        
        mon["reference_point_index"] = mon_settings.reference_point_index;
        mon["show_test_pattern"] = mon_settings.show_test_pattern;
        mon["show_test_pattern_pulse"] = mon_settings.show_test_pattern_pulse;
        mon["show_screen_preview"] = mon_settings.show_screen_preview;
        
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

void ScreenMirror3D::SyncMonitorSettingsToUI(MonitorSettings& msettings)
{
    if(msettings.group_box)
    {
        QSignalBlocker blocker(msettings.group_box);
        msettings.group_box->setChecked(msettings.enabled);
    }
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
    if(msettings.black_bar_letterbox_slider)
    {
        QSignalBlocker blocker(msettings.black_bar_letterbox_slider);
        msettings.black_bar_letterbox_slider->setValue((int)std::lround(msettings.black_bar_letterbox_percent));
    }
    if(msettings.black_bar_letterbox_label)
    {
        msettings.black_bar_letterbox_label->setText(QString::number((int)std::lround(msettings.black_bar_letterbox_percent)));
    }
    if(msettings.black_bar_pillarbox_slider)
    {
        QSignalBlocker blocker(msettings.black_bar_pillarbox_slider);
        msettings.black_bar_pillarbox_slider->setValue((int)std::lround(msettings.black_bar_pillarbox_percent));
    }
    if(msettings.black_bar_pillarbox_label)
    {
        msettings.black_bar_pillarbox_label->setText(QString::number((int)std::lround(msettings.black_bar_pillarbox_percent)));
    }
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
        msettings.propagation_speed_slider->setValue((int)std::lround(msettings.propagation_speed_mm_per_ms));
    }
    if(msettings.propagation_speed_label)
    {
        msettings.propagation_speed_label->setText(QString::number((int)std::lround(msettings.propagation_speed_mm_per_ms)));
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
    if(msettings.wave_time_to_edge_slider)
    {
        QSignalBlocker blocker(msettings.wave_time_to_edge_slider);
        msettings.wave_time_to_edge_slider->setValue((int)(msettings.wave_time_to_edge_sec * 10.0f));
    }
    if(msettings.wave_time_to_edge_label)
    {
        msettings.wave_time_to_edge_label->setText(msettings.wave_time_to_edge_sec <= 0.0f ? "Off" : QString::number(msettings.wave_time_to_edge_sec, 'f', 1) + "s");
    }
    if(msettings.falloff_curve_slider)
    {
        QSignalBlocker blocker(msettings.falloff_curve_slider);
        msettings.falloff_curve_slider->setValue((int)(msettings.falloff_curve_exponent * 100.0f));
    }
    if(msettings.falloff_curve_label)
    {
        msettings.falloff_curve_label->setText(QString::number((int)(msettings.falloff_curve_exponent * 100.0f)) + "%");
    }
    if(msettings.front_back_balance_slider)
    {
        QSignalBlocker blocker(msettings.front_back_balance_slider);
        msettings.front_back_balance_slider->setValue((int)std::lround(msettings.front_back_balance));
    }
    if(msettings.front_back_balance_label)
    {
        msettings.front_back_balance_label->setText(QString::number((int)std::lround(msettings.front_back_balance)));
    }
    if(msettings.left_right_balance_slider)
    {
        QSignalBlocker blocker(msettings.left_right_balance_slider);
        msettings.left_right_balance_slider->setValue((int)std::lround(msettings.left_right_balance));
    }
    if(msettings.left_right_balance_label)
    {
        msettings.left_right_balance_label->setText(QString::number((int)std::lround(msettings.left_right_balance)));
    }
    if(msettings.top_bottom_balance_slider)
    {
        QSignalBlocker blocker(msettings.top_bottom_balance_slider);
        msettings.top_bottom_balance_slider->setValue((int)std::lround(msettings.top_bottom_balance));
    }
    if(msettings.top_bottom_balance_label)
    {
        msettings.top_bottom_balance_label->setText(QString::number((int)std::lround(msettings.top_bottom_balance)));
    }
    if(msettings.test_pattern_check)
    {
        QSignalBlocker blocker(msettings.test_pattern_check);
        msettings.test_pattern_check->setChecked(msettings.show_test_pattern);
    }
    if(msettings.test_pattern_pulse_check)
    {
        QSignalBlocker blocker(msettings.test_pattern_pulse_check);
        msettings.test_pattern_pulse_check->setChecked(msettings.show_test_pattern_pulse);
    }
    if(msettings.screen_preview_check)
    {
        QSignalBlocker blocker(msettings.screen_preview_check);
        msettings.screen_preview_check->setChecked(msettings.show_screen_preview);
    }
    if(msettings.capture_area_preview)
    {
        msettings.capture_area_preview->update();
    }
    if(msettings.ref_point_combo)
    {
        QSignalBlocker blocker(msettings.ref_point_combo);
        int desired = msettings.reference_point_index;
        int idx = msettings.ref_point_combo->findData(desired);
        if(idx < 0) idx = msettings.ref_point_combo->findData(-1);
        if(idx >= 0) msettings.ref_point_combo->setCurrentIndex(idx);
    }
}

void ScreenMirror3D::LoadSettings(const nlohmann::json& settings)
{
    static const float default_scale = 1.0f;
    static const bool default_scale_inverted = true;
    static const float default_smoothing_time_ms = 50.0f;
    static const float default_brightness_multiplier = 1.0f;
    static const float default_brightness_threshold = 0.0f;
    static const float default_propagation_speed_mm_per_ms = 0.0f;
    static const float default_wave_decay_ms = 0.0f;
    static const float default_wave_time_to_edge_sec = 0.0f;
    static const float default_falloff_curve_exponent = 1.0f;
    static const float default_front_back_balance = 0.0f;
    static const float default_left_right_balance = 0.0f;
    static const float default_top_bottom_balance = 0.0f;

    if(settings.contains("capture_quality"))
    {
        capture_quality = std::clamp(settings["capture_quality"].get<int>(), 0, 7);
        if(capture_quality_combo)
        {
            capture_quality_combo->setCurrentIndex(capture_quality);
        }
    }

    if(!settings.contains("monitor_settings"))
    {
        return;
    }

    const nlohmann::json& monitors = settings["monitor_settings"];
    for(nlohmann::json::const_iterator it = monitors.begin(); it != monitors.end(); ++it)
    {
            const std::string& monitor_name = it.key();
            const nlohmann::json& mon = it.value();

            std::map<std::string, MonitorSettings>::iterator existing_it = monitor_settings.find(monitor_name);
            bool had_existing = (existing_it != monitor_settings.end());
            
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
            QSlider* existing_black_bar_letterbox_slider = nullptr;
            QLabel* existing_black_bar_letterbox_label = nullptr;
            QSlider* existing_black_bar_pillarbox_slider = nullptr;
            QLabel* existing_black_bar_pillarbox_label = nullptr;
            QSlider* existing_softness_slider = nullptr;
            QLabel* existing_softness_label = nullptr;
            QSlider* existing_blend_slider = nullptr;
            QLabel* existing_blend_label = nullptr;
            QSlider* existing_propagation_speed_slider = nullptr;
            QLabel* existing_propagation_speed_label = nullptr;
            QSlider* existing_wave_decay_slider = nullptr;
            QLabel* existing_wave_decay_label = nullptr;
            QSlider* existing_wave_time_to_edge_slider = nullptr;
            QLabel* existing_wave_time_to_edge_label = nullptr;
            QSlider* existing_falloff_curve_slider = nullptr;
            QLabel* existing_falloff_curve_label = nullptr;
            QSlider* existing_front_back_balance_slider = nullptr;
            QLabel* existing_front_back_balance_label = nullptr;
            QSlider* existing_left_right_balance_slider = nullptr;
            QLabel* existing_left_right_balance_label = nullptr;
            QSlider* existing_top_bottom_balance_slider = nullptr;
            QLabel* existing_top_bottom_balance_label = nullptr;
            QCheckBox* existing_test_pattern_check = nullptr;
            QCheckBox* existing_screen_preview_check = nullptr;
            QWidget* existing_capture_area_preview = nullptr;
            QPushButton* existing_add_zone_button = nullptr;
            QWidget* existing_ambilight_preview_3d = nullptr;

            if(had_existing)
            {
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
                existing_black_bar_letterbox_slider = existing_it->second.black_bar_letterbox_slider;
                existing_black_bar_letterbox_label = existing_it->second.black_bar_letterbox_label;
                existing_black_bar_pillarbox_slider = existing_it->second.black_bar_pillarbox_slider;
                existing_black_bar_pillarbox_label = existing_it->second.black_bar_pillarbox_label;
                existing_softness_slider = existing_it->second.softness_slider;
                existing_softness_label = existing_it->second.softness_label;
                existing_blend_slider = existing_it->second.blend_slider;
                existing_blend_label = existing_it->second.blend_label;
                existing_propagation_speed_slider = existing_it->second.propagation_speed_slider;
                existing_propagation_speed_label = existing_it->second.propagation_speed_label;
                existing_wave_decay_slider = existing_it->second.wave_decay_slider;
                existing_wave_decay_label = existing_it->second.wave_decay_label;
                existing_wave_time_to_edge_slider = existing_it->second.wave_time_to_edge_slider;
                existing_wave_time_to_edge_label = existing_it->second.wave_time_to_edge_label;
                existing_falloff_curve_slider = existing_it->second.falloff_curve_slider;
                existing_falloff_curve_label = existing_it->second.falloff_curve_label;
                existing_front_back_balance_slider = existing_it->second.front_back_balance_slider;
                existing_front_back_balance_label = existing_it->second.front_back_balance_label;
                existing_left_right_balance_slider = existing_it->second.left_right_balance_slider;
                existing_left_right_balance_label = existing_it->second.left_right_balance_label;
                existing_top_bottom_balance_slider = existing_it->second.top_bottom_balance_slider;
                existing_top_bottom_balance_label = existing_it->second.top_bottom_balance_label;
                existing_capture_area_preview = existing_it->second.capture_area_preview;
                existing_add_zone_button = existing_it->second.add_zone_button;
                existing_ambilight_preview_3d = existing_it->second.ambilight_preview_3d;
            }
            else
            {
                monitor_settings[monitor_name] = MonitorSettings();
                existing_it = monitor_settings.find(monitor_name);
            }
            MonitorSettings& msettings = existing_it->second;

            if(mon.contains("enabled")) msettings.enabled = mon["enabled"].get<bool>();
            
            if(mon.contains("scale")) msettings.scale = mon["scale"].get<float>();
            else msettings.scale = default_scale;
            if(mon.contains("scale_inverted")) msettings.scale_inverted = mon["scale_inverted"].get<bool>();
            else msettings.scale_inverted = default_scale_inverted;

            if(mon.contains("smoothing_time_ms")) msettings.smoothing_time_ms = mon["smoothing_time_ms"].get<float>();
            else msettings.smoothing_time_ms = default_smoothing_time_ms;
            if(mon.contains("brightness_multiplier")) msettings.brightness_multiplier = mon["brightness_multiplier"].get<float>();
            else msettings.brightness_multiplier = default_brightness_multiplier;
            if(mon.contains("brightness_threshold")) msettings.brightness_threshold = mon["brightness_threshold"].get<float>();
            else msettings.brightness_threshold = default_brightness_threshold;
            if(mon.contains("black_bar_letterbox_percent")) msettings.black_bar_letterbox_percent = mon["black_bar_letterbox_percent"].get<float>();
            else msettings.black_bar_letterbox_percent = 0.0f;
            if(mon.contains("black_bar_pillarbox_percent")) msettings.black_bar_pillarbox_percent = mon["black_bar_pillarbox_percent"].get<float>();
            else msettings.black_bar_pillarbox_percent = 0.0f;

            if(mon.contains("edge_softness")) msettings.edge_softness = mon["edge_softness"].get<float>();
            if(mon.contains("blend")) msettings.blend = mon["blend"].get<float>();
            if(mon.contains("propagation_speed_mm_per_ms")) msettings.propagation_speed_mm_per_ms = mon["propagation_speed_mm_per_ms"].get<float>();
            else msettings.propagation_speed_mm_per_ms = default_propagation_speed_mm_per_ms;
            if(mon.contains("wave_decay_ms")) msettings.wave_decay_ms = mon["wave_decay_ms"].get<float>();
            else msettings.wave_decay_ms = default_wave_decay_ms;
            if(mon.contains("wave_time_to_edge_sec")) msettings.wave_time_to_edge_sec = mon["wave_time_to_edge_sec"].get<float>();
            else msettings.wave_time_to_edge_sec = default_wave_time_to_edge_sec;
            if(mon.contains("falloff_curve_exponent")) msettings.falloff_curve_exponent = mon["falloff_curve_exponent"].get<float>();
            else msettings.falloff_curve_exponent = default_falloff_curve_exponent;
            if(mon.contains("front_back_balance")) msettings.front_back_balance = mon["front_back_balance"].get<float>();
            else msettings.front_back_balance = default_front_back_balance;
            if(mon.contains("left_right_balance")) msettings.left_right_balance = mon["left_right_balance"].get<float>();
            else msettings.left_right_balance = default_left_right_balance;
            if(mon.contains("top_bottom_balance")) msettings.top_bottom_balance = mon["top_bottom_balance"].get<float>();
            else msettings.top_bottom_balance = default_top_bottom_balance;
            
            if(mon.contains("show_test_pattern")) msettings.show_test_pattern = mon["show_test_pattern"].get<bool>();
            if(mon.contains("show_test_pattern_pulse")) msettings.show_test_pattern_pulse = mon["show_test_pattern_pulse"].get<bool>();
            if(mon.contains("show_screen_preview")) msettings.show_screen_preview = mon["show_screen_preview"].get<bool>();
            
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
                    
                    zone.u_min = std::clamp(zone.u_min, 0.0f, 1.0f);
                    zone.u_max = std::clamp(zone.u_max, 0.0f, 1.0f);
                    zone.v_min = std::clamp(zone.v_min, 0.0f, 1.0f);
                    zone.v_max = std::clamp(zone.v_max, 0.0f, 1.0f);
                    if(zone.u_min > zone.u_max) { float temp = zone.u_min; zone.u_min = zone.u_max; zone.u_max = temp; }
                    if(zone.v_min > zone.v_max) { float temp = zone.v_min; zone.v_min = zone.v_max; zone.v_max = temp; }
                    
                    msettings.capture_zones.push_back(zone);
                }
                
                if(msettings.capture_zones.empty())
                {
                    msettings.capture_zones.push_back(CaptureZone(0.0f, 1.0f, 0.0f, 1.0f));
                }
            }
            else
            {
                msettings.capture_zones.clear();
                msettings.capture_zones.push_back(CaptureZone(0.0f, 1.0f, 0.0f, 1.0f));
            }
            
            if(mon.contains("reference_point_index")) msettings.reference_point_index = mon["reference_point_index"].get<int>();

            msettings.scale = std::clamp(msettings.scale, 0.0f, 3.0f);
            msettings.smoothing_time_ms = std::clamp(msettings.smoothing_time_ms, 0.0f, 500.0f);
            msettings.brightness_multiplier = std::clamp(msettings.brightness_multiplier, 0.0f, 2.0f);
            msettings.brightness_threshold = std::clamp(msettings.brightness_threshold, 0.0f, 1020.0f);
            msettings.black_bar_letterbox_percent = std::clamp(msettings.black_bar_letterbox_percent, 0.0f, 50.0f);
            msettings.black_bar_pillarbox_percent = std::clamp(msettings.black_bar_pillarbox_percent, 0.0f, 50.0f);
            msettings.edge_softness = std::clamp(msettings.edge_softness, 0.0f, 100.0f);
            msettings.blend = std::clamp(msettings.blend, 0.0f, 100.0f);
            msettings.propagation_speed_mm_per_ms = std::clamp(msettings.propagation_speed_mm_per_ms, 0.0f, 800.0f);
            msettings.wave_decay_ms = std::clamp(msettings.wave_decay_ms, 0.0f, 20000.0f);
            msettings.wave_time_to_edge_sec = std::clamp(msettings.wave_time_to_edge_sec, 0.0f, 30.0f);
            msettings.falloff_curve_exponent = std::clamp(msettings.falloff_curve_exponent, 0.5f, 2.0f);
            msettings.front_back_balance = std::clamp(msettings.front_back_balance, -100.0f, 100.0f);
            msettings.left_right_balance = std::clamp(msettings.left_right_balance, -100.0f, 100.0f);
            msettings.top_bottom_balance = std::clamp(msettings.top_bottom_balance, -100.0f, 100.0f);
            
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
                msettings.black_bar_letterbox_slider = existing_black_bar_letterbox_slider;
                msettings.black_bar_letterbox_label = existing_black_bar_letterbox_label;
                msettings.black_bar_pillarbox_slider = existing_black_bar_pillarbox_slider;
                msettings.black_bar_pillarbox_label = existing_black_bar_pillarbox_label;
                msettings.softness_slider = existing_softness_slider;
                msettings.softness_label = existing_softness_label;
                msettings.blend_slider = existing_blend_slider;
                msettings.blend_label = existing_blend_label;
                msettings.propagation_speed_slider = existing_propagation_speed_slider;
                msettings.propagation_speed_label = existing_propagation_speed_label;
                msettings.wave_decay_slider = existing_wave_decay_slider;
                msettings.wave_decay_label = existing_wave_decay_label;
                msettings.wave_time_to_edge_slider = existing_wave_time_to_edge_slider;
                msettings.wave_time_to_edge_label = existing_wave_time_to_edge_label;
                msettings.falloff_curve_slider = existing_falloff_curve_slider;
                msettings.falloff_curve_label = existing_falloff_curve_label;
                msettings.front_back_balance_slider = existing_front_back_balance_slider;
                msettings.front_back_balance_label = existing_front_back_balance_label;
                msettings.left_right_balance_slider = existing_left_right_balance_slider;
                msettings.left_right_balance_label = existing_left_right_balance_label;
                msettings.top_bottom_balance_slider = existing_top_bottom_balance_slider;
                msettings.top_bottom_balance_label = existing_top_bottom_balance_label;
                msettings.test_pattern_check = existing_test_pattern_check;
                msettings.screen_preview_check = existing_screen_preview_check;
                msettings.capture_area_preview = existing_capture_area_preview;
                msettings.add_zone_button = existing_add_zone_button;
                msettings.ambilight_preview_3d = existing_ambilight_preview_3d;

                if(msettings.capture_area_preview)
                {
                    CaptureAreaPreviewWidget* preview_widget = static_cast<CaptureAreaPreviewWidget*>(msettings.capture_area_preview);
                    preview_widget->capture_zones = &msettings.capture_zones;
                    
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
                if(msettings.ambilight_preview_3d)
                {
                    AmbilightPreview3DWidget* ambi_w = dynamic_cast<AmbilightPreview3DWidget*>(msettings.ambilight_preview_3d);
                    if(ambi_w)
                    {
                        std::vector<DisplayPlane3D*> planes = DisplayPlaneManager::instance()->GetDisplayPlanes();
                        for(DisplayPlane3D* plane : planes)
                        {
                            if(plane && plane->GetName() == monitor_name)
                            {
                                ambi_w->SetDisplayPlane(plane);
                                break;
                            }
                        }
                    }
                }
            }
    }

    OnScreenPreviewChanged();
    OnTestPatternChanged();

    for(std::map<std::string, MonitorSettings>::iterator it = monitor_settings.begin();
        it != monitor_settings.end();
        ++it)
    {
        SyncMonitorSettingsToUI(it->second);
    }

    RefreshReferencePointDropdowns();
    
    RefreshMonitorStatus();
    
    OnScreenPreviewChanged();
    OnTestPatternChanged();
    
    OnParameterChanged();
}

void ScreenMirror3D::OnParameterChanged()
{
    for(std::map<std::string, MonitorSettings>::iterator it = monitor_settings.begin();
        it != monitor_settings.end();
        ++it)
    {
        MonitorSettings& settings = it->second;
        if(settings.group_box) settings.enabled = settings.group_box->isChecked();
        
        if(settings.scale_slider) settings.scale = std::clamp(settings.scale_slider->value() / 100.0f, 0.0f, 3.0f);
        if(settings.scale_invert_check) settings.scale_inverted = settings.scale_invert_check->isChecked();
        
        if(settings.smoothing_time_slider) settings.smoothing_time_ms = (float)settings.smoothing_time_slider->value();
        if(settings.brightness_slider) settings.brightness_multiplier = std::clamp(settings.brightness_slider->value() / 100.0f, 0.0f, 2.0f);
        if(settings.brightness_threshold_slider) settings.brightness_threshold = (float)settings.brightness_threshold_slider->value();
        if(settings.black_bar_letterbox_slider) settings.black_bar_letterbox_percent = (float)settings.black_bar_letterbox_slider->value();
        if(settings.black_bar_pillarbox_slider) settings.black_bar_pillarbox_percent = (float)settings.black_bar_pillarbox_slider->value();
        
        if(settings.softness_slider) settings.edge_softness = (float)settings.softness_slider->value();
        if(settings.blend_slider) settings.blend = (float)settings.blend_slider->value();
        if(settings.propagation_speed_slider) settings.propagation_speed_mm_per_ms = std::clamp((float)settings.propagation_speed_slider->value(), 0.0f, 800.0f);
        if(settings.wave_decay_slider) settings.wave_decay_ms = (float)settings.wave_decay_slider->value();
        if(settings.wave_time_to_edge_slider) settings.wave_time_to_edge_sec = (float)settings.wave_time_to_edge_slider->value() / 10.0f;
        if(settings.falloff_curve_slider) settings.falloff_curve_exponent = std::clamp((float)settings.falloff_curve_slider->value() / 100.0f, 0.5f, 2.0f);
        if(settings.front_back_balance_slider) settings.front_back_balance = std::clamp((float)settings.front_back_balance_slider->value(), -100.0f, 100.0f);
        if(settings.left_right_balance_slider) settings.left_right_balance = std::clamp((float)settings.left_right_balance_slider->value(), -100.0f, 100.0f);
        if(settings.top_bottom_balance_slider) settings.top_bottom_balance = std::clamp((float)settings.top_bottom_balance_slider->value(), -100.0f, 100.0f);
        
        bool old_test_pattern = settings.show_test_pattern;
        bool old_screen_preview = settings.show_screen_preview;
        if(settings.test_pattern_check) settings.show_test_pattern = settings.test_pattern_check->isChecked();
        if(settings.test_pattern_pulse_check) settings.show_test_pattern_pulse = settings.test_pattern_pulse_check->isChecked();
        if(settings.screen_preview_check) settings.show_screen_preview = settings.screen_preview_check->isChecked();
        if((settings.show_test_pattern || settings.show_test_pattern_pulse) && settings.group_box && !settings.group_box->isChecked())
        {
            QSignalBlocker block(settings.group_box);
            settings.group_box->setChecked(true);
            settings.enabled = true;
        }
        
        if((old_test_pattern != settings.show_test_pattern || old_screen_preview != settings.show_screen_preview) 
           && settings.capture_area_preview)
        {
            settings.capture_area_preview->update();
        }

        if(settings.ambilight_preview_3d)
        {
            AmbilightPreview3DWidget* ambi_w = dynamic_cast<AmbilightPreview3DWidget*>(settings.ambilight_preview_3d);
            bool has_capture = ambi_w && ambi_w->display_plane && !ambi_w->display_plane->GetCaptureSourceId().empty();
            settings.ambilight_preview_3d->setEnabled(has_capture || settings.show_test_pattern || settings.show_test_pattern_pulse);
        }

        if(settings.ref_point_combo)
        {
            int index = settings.ref_point_combo->currentIndex();
            if(index >= 0)
            {
                settings.reference_point_index = settings.ref_point_combo->itemData(index).toInt();
            }
        }
    }

    RefreshMonitorStatus();
    RefreshReferencePointDropdowns();

    UpdateAmbilightPreviews();
    
    emit ParametersChanged();
}

void ScreenMirror3D::OnScreenPreviewChanged()
{
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

void ScreenMirror3D::SetReferencePoints(std::vector<std::unique_ptr<VirtualReferencePoint3D>>* ref_points)
{
    reference_points = ref_points;
    if(monitors_layout && monitor_settings.size() > 0)
    {
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

    const size_t max_frames = 180;
    if(history.frames.size() > max_frames)
    {
        history.frames.pop_front();
    }
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
    float max_retention = 600.0f;
    
    for(std::map<std::string, MonitorSettings>::const_iterator it = monitor_settings.begin();
        it != monitor_settings.end();
        ++it)
    {
        const MonitorSettings& mon_settings = it->second;
        if(!mon_settings.enabled) continue;
        
        float monitor_retention = std::max(mon_settings.wave_decay_ms * 3.0f, mon_settings.smoothing_time_ms * 3.0f);
        if(mon_settings.propagation_speed_mm_per_ms > 0.001f || mon_settings.wave_time_to_edge_sec > 0.4f)
        {
            float max_distance_mm = 5000.0f;
            if(mon_settings.wave_time_to_edge_sec > 0.4f)
            {
                float t_sec = std::clamp(mon_settings.wave_time_to_edge_sec, 0.5f, 30.0f);
                monitor_retention = std::max(monitor_retention, t_sec * 1000.0f);
            }
            else
            {
                monitor_retention = std::max(monitor_retention, max_distance_mm / mon_settings.propagation_speed_mm_per_ms);
            }
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
    settings.group_box->setChecked(settings.enabled && (has_capture_source || settings.show_test_pattern || settings.show_test_pattern_pulse));
    settings.group_box->setEnabled(has_capture_source || settings.show_test_pattern || settings.show_test_pattern_pulse);
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

    QVBoxLayout* main_layout = new QVBoxLayout();
    main_layout->setContentsMargins(8, 4, 8, 4);
    main_layout->setSpacing(8);

    QGroupBox* reach_group = new QGroupBox("Reach & Falloff");
    QFormLayout* reach_form = new QFormLayout(reach_group);
    reach_form->setContentsMargins(8, 12, 8, 8);

    QWidget* scale_widget = new QWidget();
    QHBoxLayout* scale_layout = new QHBoxLayout(scale_widget);
    scale_layout->setContentsMargins(0, 0, 0, 0);
    settings.scale_slider = new QSlider(Qt::Horizontal);
    settings.scale_slider->setEnabled(has_capture_source);
    settings.scale_slider->setRange(0, 300);
    settings.scale_slider->setValue((int)(settings.scale * 100));
    settings.scale_slider->setTickPosition(QSlider::TicksBelow);
    settings.scale_slider->setTickInterval(25);
    settings.scale_slider->setToolTip("Global reach: 0-100% = fill room, 101-300% = beyond room (extreme).");
    scale_layout->addWidget(settings.scale_slider);
    settings.scale_label = new QLabel(QString::number((int)(settings.scale * 100)) + "%");
    settings.scale_label->setMinimumWidth(50);
    scale_layout->addWidget(settings.scale_label);
    connect(settings.scale_slider, &QSlider::valueChanged, this, &ScreenMirror3D::OnParameterChanged);
    connect(settings.scale_slider, &QSlider::valueChanged, settings.scale_label, [&settings](int value) {
        settings.scale_label->setText(QString::number(value) + "%");
    });
    reach_form->addRow("Global Reach:", scale_widget);

    settings.scale_invert_check = new QCheckBox("Invert Scale Falloff");
    settings.scale_invert_check->setEnabled(has_capture_source);
    settings.scale_invert_check->setChecked(settings.scale_inverted);
    settings.scale_invert_check->setToolTip("Invert the distance falloff (closer = dimmer, farther = brighter).");
    connect(settings.scale_invert_check, &QCheckBox::toggled, this, &ScreenMirror3D::OnParameterChanged);
    reach_form->addRow("", settings.scale_invert_check);

    QWidget* falloff_curve_widget = new QWidget();
    QHBoxLayout* falloff_curve_layout = new QHBoxLayout(falloff_curve_widget);
    falloff_curve_layout->setContentsMargins(0, 0, 0, 0);
    settings.falloff_curve_slider = new QSlider(Qt::Horizontal);
    settings.falloff_curve_slider->setRange(50, 200);
    settings.falloff_curve_slider->setValue((int)(settings.falloff_curve_exponent * 100.0f));
    settings.falloff_curve_slider->setEnabled(has_capture_source);
    settings.falloff_curve_slider->setTickPosition(QSlider::TicksBelow);
    settings.falloff_curve_slider->setTickInterval(25);
    settings.falloff_curve_slider->setToolTip("Falloff curve: 50% = softer (gradual), 100% = linear, 200% = sharper (sudden edge).");
    falloff_curve_layout->addWidget(settings.falloff_curve_slider);
    settings.falloff_curve_label = new QLabel(QString::number((int)(settings.falloff_curve_exponent * 100.0f)) + "%");
    settings.falloff_curve_label->setMinimumWidth(45);
    falloff_curve_layout->addWidget(settings.falloff_curve_label);
    connect(settings.falloff_curve_slider, &QSlider::valueChanged, this, &ScreenMirror3D::OnParameterChanged);
    connect(settings.falloff_curve_slider, &QSlider::valueChanged, settings.falloff_curve_label, [&settings](int value) {
        settings.falloff_curve_label->setText(QString::number(value) + "%");
    });
    reach_form->addRow("Falloff curve:", falloff_curve_widget);

    settings.ref_point_combo = new QComboBox();
    settings.ref_point_combo->addItem("Room Center", QVariant(-1));
    settings.ref_point_combo->setEnabled(has_capture_source);
    settings.ref_point_combo->setToolTip("Anchor for falloff distance. Defaults to the display plane's position for ambilight effects.");
    connect(settings.ref_point_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ScreenMirror3D::OnParameterChanged);
    int plane_ref_index = plane->GetReferencePointIndex();
    if(plane_ref_index >= 0 && settings.reference_point_index < 0)
        settings.reference_point_index = plane_ref_index;
    reach_form->addRow("Reference:", settings.ref_point_combo);

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
    reach_form->addRow("Softness:", softness_widget);

    main_layout->addWidget(reach_group);

    QGroupBox* direction_group = new QGroupBox("Wall / Direction Focus");
    QFormLayout* direction_form = new QFormLayout(direction_group);
    direction_form->setContentsMargins(8, 12, 8, 8);
    QWidget* front_back_widget = new QWidget();
    QHBoxLayout* front_back_layout = new QHBoxLayout(front_back_widget);
    front_back_layout->setContentsMargins(0, 0, 0, 0);
    settings.front_back_balance_slider = new QSlider(Qt::Horizontal);
    settings.front_back_balance_slider->setRange(-100, 100);
    settings.front_back_balance_slider->setValue((int)std::lround(settings.front_back_balance));
    settings.front_back_balance_slider->setEnabled(has_capture_source);
    settings.front_back_balance_slider->setToolTip("Favor front (+) or back (-) LEDs. 0 = even.");
    front_back_layout->addWidget(settings.front_back_balance_slider);
    settings.front_back_balance_label = new QLabel(QString::number((int)std::lround(settings.front_back_balance)));
    settings.front_back_balance_label->setMinimumWidth(40);
    front_back_layout->addWidget(settings.front_back_balance_label);
    connect(settings.front_back_balance_slider, &QSlider::valueChanged, this, &ScreenMirror3D::OnParameterChanged);
    connect(settings.front_back_balance_slider, &QSlider::valueChanged, settings.front_back_balance_label, [&settings](int v) { settings.front_back_balance_label->setText(QString::number(v)); });
    direction_form->addRow("Front/Back:", front_back_widget);
    QWidget* left_right_widget = new QWidget();
    QHBoxLayout* left_right_layout = new QHBoxLayout(left_right_widget);
    left_right_layout->setContentsMargins(0, 0, 0, 0);
    settings.left_right_balance_slider = new QSlider(Qt::Horizontal);
    settings.left_right_balance_slider->setRange(-100, 100);
    settings.left_right_balance_slider->setValue((int)std::lround(settings.left_right_balance));
    settings.left_right_balance_slider->setEnabled(has_capture_source);
    settings.left_right_balance_slider->setToolTip("Favor right (+) or left (-) LEDs. 0 = even.");
    left_right_layout->addWidget(settings.left_right_balance_slider);
    settings.left_right_balance_label = new QLabel(QString::number((int)std::lround(settings.left_right_balance)));
    settings.left_right_balance_label->setMinimumWidth(40);
    left_right_layout->addWidget(settings.left_right_balance_label);
    connect(settings.left_right_balance_slider, &QSlider::valueChanged, this, &ScreenMirror3D::OnParameterChanged);
    connect(settings.left_right_balance_slider, &QSlider::valueChanged, settings.left_right_balance_label, [&settings](int v) { settings.left_right_balance_label->setText(QString::number(v)); });
    direction_form->addRow("Left/Right:", left_right_widget);
    QWidget* top_bottom_widget = new QWidget();
    QHBoxLayout* top_bottom_layout = new QHBoxLayout(top_bottom_widget);
    top_bottom_layout->setContentsMargins(0, 0, 0, 0);
    settings.top_bottom_balance_slider = new QSlider(Qt::Horizontal);
    settings.top_bottom_balance_slider->setRange(-100, 100);
    settings.top_bottom_balance_slider->setValue((int)std::lround(settings.top_bottom_balance));
    settings.top_bottom_balance_slider->setEnabled(has_capture_source);
    settings.top_bottom_balance_slider->setToolTip("Favor top (+) or bottom (-) LEDs. 0 = even.");
    top_bottom_layout->addWidget(settings.top_bottom_balance_slider);
    settings.top_bottom_balance_label = new QLabel(QString::number((int)std::lround(settings.top_bottom_balance)));
    settings.top_bottom_balance_label->setMinimumWidth(40);
    top_bottom_layout->addWidget(settings.top_bottom_balance_label);
    connect(settings.top_bottom_balance_slider, &QSlider::valueChanged, this, &ScreenMirror3D::OnParameterChanged);
    connect(settings.top_bottom_balance_slider, &QSlider::valueChanged, settings.top_bottom_balance_label, [&settings](int v) { settings.top_bottom_balance_label->setText(QString::number(v)); });
    direction_form->addRow("Top/Bottom:", top_bottom_widget);
    main_layout->addWidget(direction_group);

    QGroupBox* brightness_group = new QGroupBox("Brightness");
    QFormLayout* brightness_form = new QFormLayout(brightness_group);
    brightness_form->setContentsMargins(8, 12, 8, 8);

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
    brightness_form->addRow("Brightness:", brightness_widget);

    QWidget* threshold_widget = new QWidget();
    QHBoxLayout* threshold_layout = new QHBoxLayout(threshold_widget);
    threshold_layout->setContentsMargins(0, 0, 0, 0);
    settings.brightness_threshold_slider = new QSlider(Qt::Horizontal);
    settings.brightness_threshold_slider->setRange(0, 1020);
    settings.brightness_threshold_slider->setValue((int)settings.brightness_threshold);
    settings.brightness_threshold_slider->setEnabled(has_capture_source);
    settings.brightness_threshold_slider->setTickPosition(QSlider::TicksBelow);
    settings.brightness_threshold_slider->setTickInterval(102);
    settings.brightness_threshold_slider->setToolTip("Minimum brightness to trigger (0-1020). At max, ambilight is nearly off. Lower values capture more dim content.");
    threshold_layout->addWidget(settings.brightness_threshold_slider);
    settings.brightness_threshold_label = new QLabel(QString::number((int)settings.brightness_threshold));
    settings.brightness_threshold_label->setMinimumWidth(50);
    threshold_layout->addWidget(settings.brightness_threshold_label);
    connect(settings.brightness_threshold_slider, &QSlider::valueChanged, this, &ScreenMirror3D::OnParameterChanged);
    connect(settings.brightness_threshold_slider, &QSlider::valueChanged, settings.brightness_threshold_label, [&settings](int value) {
        settings.brightness_threshold_label->setText(QString::number(value));
    });
    brightness_form->addRow("Brightness Threshold:", threshold_widget);

    main_layout->addWidget(brightness_group);

    QGroupBox* wave_group = new QGroupBox("Wave");
    QFormLayout* wave_form = new QFormLayout(wave_group);
    wave_form->setContentsMargins(8, 12, 8, 8);

    QWidget* propagation_widget = new QWidget();
    QHBoxLayout* propagation_layout = new QHBoxLayout(propagation_widget);
    propagation_layout->setContentsMargins(0, 0, 0, 0);
    settings.propagation_speed_slider = new QSlider(Qt::Horizontal);
    settings.propagation_speed_slider->setRange(0, 800);
    settings.propagation_speed_slider->setValue((int)std::lround(settings.propagation_speed_mm_per_ms));
    settings.propagation_speed_slider->setEnabled(has_capture_source);
    settings.propagation_speed_slider->setTickPosition(QSlider::TicksBelow);
    settings.propagation_speed_slider->setTickInterval(80);
    settings.propagation_speed_slider->setToolTip("Wave intensity (0-800). 0 = no wave (instant). Higher = stronger wave. Use 'Time to edge' for e.g. 5s to walls.");
    propagation_layout->addWidget(settings.propagation_speed_slider);
    settings.propagation_speed_label = new QLabel(QString::number((int)std::lround(settings.propagation_speed_mm_per_ms)));
    settings.propagation_speed_label->setMinimumWidth(80);
    propagation_layout->addWidget(settings.propagation_speed_label);
    connect(settings.propagation_speed_slider, &QSlider::valueChanged, this, &ScreenMirror3D::OnParameterChanged);
    connect(settings.propagation_speed_slider, &QSlider::valueChanged, settings.propagation_speed_label, [&settings](int value) {
        settings.propagation_speed_label->setText(QString::number(value));
    });
    wave_form->addRow("Wave Intensity:", propagation_widget);

    QWidget* wave_decay_widget = new QWidget();
    QHBoxLayout* wave_decay_layout = new QHBoxLayout(wave_decay_widget);
    wave_decay_layout->setContentsMargins(0, 0, 0, 0);
    settings.wave_decay_slider = new QSlider(Qt::Horizontal);
    settings.wave_decay_slider->setRange(0, 20000);
    settings.wave_decay_slider->setValue((int)settings.wave_decay_ms);
    settings.wave_decay_slider->setEnabled(has_capture_source);
    settings.wave_decay_slider->setTickPosition(QSlider::TicksBelow);
    settings.wave_decay_slider->setTickInterval(2000);
    settings.wave_decay_slider->setToolTip("Wave decay (0-20000ms). How long the wave tail lasts. Use test pulse to tune.");
    wave_decay_layout->addWidget(settings.wave_decay_slider);
    settings.wave_decay_label = new QLabel(QString::number((int)settings.wave_decay_ms) + "ms");
    settings.wave_decay_label->setMinimumWidth(60);
    wave_decay_layout->addWidget(settings.wave_decay_label);
    connect(settings.wave_decay_slider, &QSlider::valueChanged, this, &ScreenMirror3D::OnParameterChanged);
    connect(settings.wave_decay_slider, &QSlider::valueChanged, settings.wave_decay_label, [&settings](int value) {
        settings.wave_decay_label->setText(QString::number(value) + "ms");
    });
    wave_form->addRow("Wave Decay:", wave_decay_widget);

    QWidget* wave_time_to_edge_widget = new QWidget();
    QHBoxLayout* wave_time_layout = new QHBoxLayout(wave_time_to_edge_widget);
    wave_time_layout->setContentsMargins(0, 0, 0, 0);
    settings.wave_time_to_edge_slider = new QSlider(Qt::Horizontal);
    settings.wave_time_to_edge_slider->setRange(0, 300);
    settings.wave_time_to_edge_slider->setValue((int)(settings.wave_time_to_edge_sec * 10.0f));
    settings.wave_time_to_edge_slider->setEnabled(has_capture_source);
    settings.wave_time_to_edge_slider->setTickPosition(QSlider::TicksBelow);
    settings.wave_time_to_edge_slider->setTickInterval(50);
    settings.wave_time_to_edge_slider->setToolTip("Time (s) for wave to reach farthest LED. 0 = use Wave Intensity. 5 = 0.5s, 50 = 5s, 300 = 30s.");
    wave_time_layout->addWidget(settings.wave_time_to_edge_slider);
    settings.wave_time_to_edge_label = new QLabel(settings.wave_time_to_edge_sec <= 0.0f ? "Off" : QString::number(settings.wave_time_to_edge_sec, 'f', 1) + "s");
    settings.wave_time_to_edge_label->setMinimumWidth(45);
    wave_time_layout->addWidget(settings.wave_time_to_edge_label);
    connect(settings.wave_time_to_edge_slider, &QSlider::valueChanged, this, &ScreenMirror3D::OnParameterChanged);
    connect(settings.wave_time_to_edge_slider, &QSlider::valueChanged, settings.wave_time_to_edge_label, [&settings](int value) {
        settings.wave_time_to_edge_label->setText(value == 0 ? "Off" : QString::number(value / 10.0, 'f', 1) + "s");
    });
    wave_form->addRow("Time to edge (s):", wave_time_to_edge_widget);

    main_layout->addWidget(wave_group);

    QGroupBox* blackbars_group = new QGroupBox("Black Bars (Crop)");
    QFormLayout* blackbars_form = new QFormLayout(blackbars_group);
    blackbars_form->setContentsMargins(8, 12, 8, 8);

    QWidget* letterbox_widget = new QWidget();
    QHBoxLayout* letterbox_layout = new QHBoxLayout(letterbox_widget);
    letterbox_layout->setContentsMargins(0, 0, 0, 0);
    settings.black_bar_letterbox_slider = new QSlider(Qt::Horizontal);
    settings.black_bar_letterbox_slider->setRange(0, 50);
    settings.black_bar_letterbox_slider->setValue((int)std::lround(settings.black_bar_letterbox_percent));
    settings.black_bar_letterbox_slider->setEnabled(has_capture_source);
    settings.black_bar_letterbox_slider->setTickPosition(QSlider::TicksBelow);
    settings.black_bar_letterbox_slider->setTickInterval(5);
    settings.black_bar_letterbox_slider->setToolTip("Crop top and bottom (letterbox). 0 = no crop.");
    letterbox_layout->addWidget(settings.black_bar_letterbox_slider);
    settings.black_bar_letterbox_label = new QLabel(QString::number((int)std::lround(settings.black_bar_letterbox_percent)) + "%");
    settings.black_bar_letterbox_label->setMinimumWidth(40);
    letterbox_layout->addWidget(settings.black_bar_letterbox_label);
    connect(settings.black_bar_letterbox_slider, &QSlider::valueChanged, this, &ScreenMirror3D::OnParameterChanged);
    connect(settings.black_bar_letterbox_slider, &QSlider::valueChanged, settings.black_bar_letterbox_label, [&settings](int value) {
        settings.black_bar_letterbox_label->setText(QString::number(value) + "%");
    });
    blackbars_form->addRow("Letterbox (top/bottom):", letterbox_widget);

    QWidget* pillarbox_widget = new QWidget();
    QHBoxLayout* pillarbox_layout = new QHBoxLayout(pillarbox_widget);
    pillarbox_layout->setContentsMargins(0, 0, 0, 0);
    settings.black_bar_pillarbox_slider = new QSlider(Qt::Horizontal);
    settings.black_bar_pillarbox_slider->setRange(0, 50);
    settings.black_bar_pillarbox_slider->setValue((int)std::lround(settings.black_bar_pillarbox_percent));
    settings.black_bar_pillarbox_slider->setEnabled(has_capture_source);
    settings.black_bar_pillarbox_slider->setTickPosition(QSlider::TicksBelow);
    settings.black_bar_pillarbox_slider->setTickInterval(5);
    settings.black_bar_pillarbox_slider->setToolTip("Crop left and right (pillarbox). 0 = no crop.");
    pillarbox_layout->addWidget(settings.black_bar_pillarbox_slider);
    settings.black_bar_pillarbox_label = new QLabel(QString::number((int)std::lround(settings.black_bar_pillarbox_percent)) + "%");
    settings.black_bar_pillarbox_label->setMinimumWidth(40);
    pillarbox_layout->addWidget(settings.black_bar_pillarbox_label);
    connect(settings.black_bar_pillarbox_slider, &QSlider::valueChanged, this, &ScreenMirror3D::OnParameterChanged);
    connect(settings.black_bar_pillarbox_slider, &QSlider::valueChanged, settings.black_bar_pillarbox_label, [&settings](int value) {
        settings.black_bar_pillarbox_label->setText(QString::number(value) + "%");
    });
    blackbars_form->addRow("Pillarbox (left/right):", pillarbox_widget);

    main_layout->addWidget(blackbars_group);

    QGroupBox* blend_group = new QGroupBox("Blend & Smoothing");
    QFormLayout* blend_form = new QFormLayout(blend_group);
    blend_form->setContentsMargins(8, 12, 8, 8);

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
    blend_form->addRow("Smoothing:", smoothing_widget);

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
    blend_form->addRow("Blend:", blend_widget);

    main_layout->addWidget(blend_group);

    QGroupBox* preview_group = new QGroupBox("Preview & Test");
    QFormLayout* preview_form = new QFormLayout(preview_group);
    preview_form->setContentsMargins(8, 12, 8, 8);

    settings.test_pattern_check = new QCheckBox("Show Test Pattern");
    settings.test_pattern_check->setEnabled(true);
    settings.test_pattern_check->setChecked(settings.show_test_pattern);
    settings.test_pattern_check->setToolTip("Display a fixed color quadrant pattern for calibration.");
    connect(settings.test_pattern_check,
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
            &QCheckBox::checkStateChanged,
            this, [this]() { OnParameterChanged(); }
#else
            &QCheckBox::stateChanged,
            this, [this](int) { OnParameterChanged(); }
#endif
    );
    preview_form->addRow("Test Pattern:", settings.test_pattern_check);

    settings.test_pattern_pulse_check = new QCheckBox("Pulse (for tuning wave)");
    settings.test_pattern_pulse_check->setEnabled(true);
    settings.test_pattern_pulse_check->setChecked(settings.show_test_pattern_pulse);
    settings.test_pattern_pulse_check->setToolTip("Pulsing color cycle to tune wave intensity and decay.");
    connect(settings.test_pattern_pulse_check,
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
            &QCheckBox::checkStateChanged,
#else
            &QCheckBox::stateChanged,
#endif
            this, [this, &settings]() {
                OnParameterChanged();
                if(settings.ambilight_preview_3d)
                    settings.ambilight_preview_3d->update();
            });
    preview_form->addRow("", settings.test_pattern_pulse_check);

    settings.screen_preview_check = new QCheckBox("Show Screen Preview");
    settings.screen_preview_check->setEnabled(has_capture_source);
    settings.screen_preview_check->setChecked(settings.show_screen_preview);
    settings.screen_preview_check->setToolTip("Show captured screen on display planes in the 3D viewport. Turn off to save CPU/GPU.");
    connect(settings.screen_preview_check,
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
            &QCheckBox::checkStateChanged,
            this, [this]() { OnParameterChanged(); OnScreenPreviewChanged(); }
#else
            &QCheckBox::stateChanged,
            this, [this](int) { OnParameterChanged(); OnScreenPreviewChanged(); }
#endif
    );
    preview_form->addRow("Screen Preview:", settings.screen_preview_check);

    main_layout->addWidget(preview_group);

    QGroupBox* zones_group = new QGroupBox("Capture Zones");
    QVBoxLayout* zones_layout = new QVBoxLayout();

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

    QHBoxLayout* previews_row = new QHBoxLayout();
    previews_row->setSpacing(12);

    QVBoxLayout* capture_col = new QVBoxLayout();
    capture_col->setContentsMargins(0, 0, 0, 0);
    capture_col->addWidget(new QLabel("Capture area (zones):"));
    CaptureAreaPreviewWidget* preview_widget = new CaptureAreaPreviewWidget(
        &settings.capture_zones,
        plane,
        &settings.show_test_pattern,
        &settings.show_screen_preview,
        &settings.black_bar_letterbox_percent,
        &settings.black_bar_pillarbox_percent
    );
    preview_widget->SetValueChangedCallback([this]() {
        OnParameterChanged();
    });
    settings.capture_area_preview = preview_widget;
    settings.capture_area_preview->setEnabled(has_capture_source);
    capture_col->addWidget(settings.capture_area_preview);
    previews_row->addLayout(capture_col, 1);

    QVBoxLayout* ambi_col = new QVBoxLayout();
    ambi_col->setContentsMargins(0, 0, 0, 0);
    ambi_col->addWidget(new QLabel("Ambilight preview (LED frame):"));
    AmbilightPreview3DWidget* ambilight_preview = new AmbilightPreview3DWidget(this);
    ambilight_preview->SetDisplayPlane(plane);
    ambilight_preview->SetPreviewOptions(&settings.show_screen_preview, &settings.show_test_pattern, &settings.show_test_pattern_pulse);
    for(QWidget* p = this->parentWidget(); p; p = p->parentWidget())
    {
        OpenRGB3DSpatialTab* tab = qobject_cast<OpenRGB3DSpatialTab*>(p);
        if(tab)
        {
            grid_scale_mm_ = tab->GetGridScaleMM();
            ambilight_preview->SetGridScaleMM(grid_scale_mm_);
            break;
        }
    }
    ambilight_preview->setEnabled(has_capture_source || settings.show_test_pattern || settings.show_test_pattern_pulse);
    settings.ambilight_preview_3d = ambilight_preview;
    ambi_col->addWidget(settings.ambilight_preview_3d);
    previews_row->addLayout(ambi_col, 1);

    zones_layout->addLayout(previews_row);

    zones_group->setLayout(zones_layout);
    main_layout->addWidget(zones_group);

    settings.group_box->setLayout(main_layout);
    monitors_layout->addWidget(settings.group_box);
    
}

void ScreenMirror3D::StartCaptureIfNeeded()
{
    ScreenCaptureManager& capture_mgr = ScreenCaptureManager::Instance();

    if(!capture_mgr.IsInitialized())
    {
        capture_mgr.Initialize();
    }

    int w = 320, h = 180;
    int q = std::clamp(capture_quality, 0, 7);
    if(q == 1) { w = 480; h = 270; }
    else if(q == 2) { w = 640; h = 360; }
    else if(q == 3) { w = 960; h = 540; }
    else if(q == 4) { w = 1280; h = 720; }
    else if(q == 5) { w = 1920; h = 1080; }
    else if(q == 6) { w = 2560; h = 1440; }
    else if(q == 7) { w = 3840; h = 2160; }
    capture_mgr.SetDownscaleResolution(w, h);

    std::vector<DisplayPlane3D*> planes = DisplayPlaneManager::instance()->GetDisplayPlanes();

    for(size_t plane_index = 0; plane_index < planes.size(); plane_index++)
    {
        DisplayPlane3D* plane = planes[plane_index];
        if(!plane) continue;

        std::string capture_id = plane->GetCaptureSourceId();
        if(capture_id.empty()) continue;

        if(!capture_mgr.IsCapturing(capture_id))
        {
            capture_mgr.StartCapture(capture_id);
        }
    }
}

void ScreenMirror3D::StopCaptureIfNeeded()
{
}

void ScreenMirror3D::UpdateAmbilightPreviews()
{
    std::vector<DisplayPlane3D*> planes = DisplayPlaneManager::instance()->GetDisplayPlanes();
    std::unordered_map<std::string, DisplayPlane3D*> plane_by_name;
    for(DisplayPlane3D* plane : planes)
    {
        if(plane && !plane->GetName().empty())
            plane_by_name[plane->GetName()] = plane;
    }
    ScreenMirror3D* color_source = source_effect_for_preview_ ? source_effect_for_preview_ : this;

    for(std::map<std::string, MonitorSettings>::iterator it = monitor_settings.begin(); it != monitor_settings.end(); ++it)
    {
        MonitorSettings& m = it->second;
        if(!m.ambilight_preview_3d)
            continue;
        AmbilightPreview3DWidget* w = dynamic_cast<AmbilightPreview3DWidget*>(m.ambilight_preview_3d);
        if(!w)
            continue;
        if(!w->display_plane)
        {
            auto pit = plane_by_name.find(it->first);
            if(pit != plane_by_name.end())
                w->SetDisplayPlane(pit->second);
            if(!w->display_plane && !planes.empty() && planes[0])
                w->SetDisplayPlane(planes[0]);
            if(!w->display_plane)
                continue;
        }
        w->updatePositions();
        const std::vector<Vector3D>& pos = w->GetPreviewPositions();
        if(pos.size() != (size_t)AmbilightPreview3DWidget::PREVIEW_LED_COUNT && !planes.empty() && planes[0])
        {
            w->SetDisplayPlane(planes[0]);
            w->updatePositions();
        }
        const std::vector<Vector3D>& pos_final = w->GetPreviewPositions();
        if(pos_final.size() != (size_t)AmbilightPreview3DWidget::PREVIEW_LED_COUNT)
            continue;
        float min_x = 1e9f, max_x = -1e9f, min_y = 1e9f, max_y = -1e9f, min_z = 1e9f, max_z = -1e9f;
        for(const Vector3D& p : pos_final)
        {
            if(p.x < min_x) min_x = p.x; if(p.x > max_x) max_x = p.x;
            if(p.y < min_y) min_y = p.y; if(p.y > max_y) max_y = p.y;
            if(p.z < min_z) min_z = p.z; if(p.z > max_z) max_z = p.z;
        }
        float pad = 20.0f;
        GridContext3D grid(min_x - pad, max_x + pad, min_y - pad, max_y + pad, min_z - pad, max_z + pad, grid_scale_mm_);
        float time = (float)QDateTime::currentMSecsSinceEpoch() / 1000.0f;
        std::vector<RGBColor> colors;
        if(color_source == this)
            CalculateColorGridBatch(pos_final, time, grid, colors);
        else
        {
            colors.reserve(pos_final.size());
            for(const Vector3D& p : pos_final)
                colors.push_back(color_source->CalculateColorGrid(p.x, p.y, p.z, time, grid));
        }
        w->SetColors(colors);
    }
}

void ScreenMirror3D::SetGridScaleMM(float mm)
{
    float v = (mm > 0.001f) ? mm : 10.0f;
    if(grid_scale_mm_ == v) return;
    grid_scale_mm_ = v;
    for(auto& it : monitor_settings)
    {
        AmbilightPreview3DWidget* w = dynamic_cast<AmbilightPreview3DWidget*>(it.second.ambilight_preview_3d);
        if(w)
            w->SetGridScaleMM(grid_scale_mm_);
    }
}

void ScreenMirror3D::SetRunningEffectForPreview(ScreenMirror3D* source)
{
    source_effect_for_preview_ = source;
}

void ScreenMirror3D::RefreshMonitorStatus()
{
    if(!monitor_status_label) return;

    std::vector<DisplayPlane3D*> planes = DisplayPlaneManager::instance()->GetDisplayPlanes();
    int total_count = 0;
    int active_count = 0;
    
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
            bool can_use_test_or_pulse = settings.show_test_pattern || settings.show_test_pattern_pulse;
            settings.group_box->setEnabled(has_capture_source || can_use_test_or_pulse);
            
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
            if(settings.scale_slider) settings.scale_slider->setEnabled(has_capture_source);
            if(settings.scale_invert_check) settings.scale_invert_check->setEnabled(has_capture_source);
            if(settings.smoothing_time_slider) settings.smoothing_time_slider->setEnabled(has_capture_source);
            if(settings.brightness_slider) settings.brightness_slider->setEnabled(has_capture_source);
            if(settings.brightness_threshold_slider) settings.brightness_threshold_slider->setEnabled(has_capture_source);
            if(settings.black_bar_letterbox_slider) settings.black_bar_letterbox_slider->setEnabled(has_capture_source);
            if(settings.black_bar_pillarbox_slider) settings.black_bar_pillarbox_slider->setEnabled(has_capture_source);
            if(settings.softness_slider) settings.softness_slider->setEnabled(has_capture_source);
            if(settings.blend_slider) settings.blend_slider->setEnabled(has_capture_source);
            if(settings.propagation_speed_slider) settings.propagation_speed_slider->setEnabled(has_capture_source);
            if(settings.wave_decay_slider) settings.wave_decay_slider->setEnabled(has_capture_source);
            if(settings.wave_time_to_edge_slider) settings.wave_time_to_edge_slider->setEnabled(has_capture_source);
            if(settings.falloff_curve_slider) settings.falloff_curve_slider->setEnabled(has_capture_source);
            if(settings.front_back_balance_slider) settings.front_back_balance_slider->setEnabled(has_capture_source);
            if(settings.left_right_balance_slider) settings.left_right_balance_slider->setEnabled(has_capture_source);
            if(settings.top_bottom_balance_slider) settings.top_bottom_balance_slider->setEnabled(has_capture_source);
            if(settings.ref_point_combo) settings.ref_point_combo->setEnabled(has_capture_source);
            if(settings.test_pattern_check) settings.test_pattern_check->setEnabled(true);
            if(settings.test_pattern_pulse_check) settings.test_pattern_pulse_check->setEnabled(true);
            if(settings.screen_preview_check) settings.screen_preview_check->setEnabled(has_capture_source);
            if(settings.capture_area_preview)
            {
                settings.capture_area_preview->setEnabled(has_capture_source);
                CaptureAreaPreviewWidget* preview_widget = static_cast<CaptureAreaPreviewWidget*>(settings.capture_area_preview);
                preview_widget->SetDisplayPlane(plane);
                preview_widget->capture_zones = &settings.capture_zones;
            }
            if(settings.ambilight_preview_3d)
            {
                settings.ambilight_preview_3d->setEnabled(has_capture_source || settings.show_test_pattern || settings.show_test_pattern_pulse);
                AmbilightPreview3DWidget* ambi_w = dynamic_cast<AmbilightPreview3DWidget*>(settings.ambilight_preview_3d);
                if(ambi_w)
                    ambi_w->SetDisplayPlane(plane);
            }
            if(settings.add_zone_button)
            {
                settings.add_zone_button->setEnabled(has_capture_source);
            }
            
            if(!has_capture_source && !can_use_test_or_pulse && settings.group_box->isChecked())
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
