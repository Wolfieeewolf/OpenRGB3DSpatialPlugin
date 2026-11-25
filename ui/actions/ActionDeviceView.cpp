// SPDX-License-Identifier: GPL-2.0-or-later
#include "ActionDeviceView.h"
#include "ActionApplier.h"
#include "ActionApplierDetail.h"
#include "LEDPosition3D.h"
#include "ZoneManager3D.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QRadialGradient>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace
{
struct Dot
{
    float x = 0.0f;
    float y = 0.0f;
    float depth = 0.0f;
    QColor color;
    bool lit = false;
};

/** Typical gap between neighbouring lights (sampled nearest-neighbour median). */
float EstimateSpacing(const std::vector<Vector3D>& pts)
{
    const size_t n = pts.size();
    if(n < 2)
    {
        return 1.0f;
    }
    const size_t step = std::max<size_t>(1, n / 300);
    std::vector<float> nearest;
    nearest.reserve(n / step + 1);
    for(size_t i = 0; i < n; i += step)
    {
        float best = 1e30f;
        for(size_t j = 0; j < n; ++j)
        {
            if(i == j)
            {
                continue;
            }
            const float dx = pts[i].x - pts[j].x;
            const float dy = pts[i].y - pts[j].y;
            const float dz = pts[i].z - pts[j].z;
            const float d = dx * dx + dy * dy + dz * dz;
            if(d > 1e-10f && d < best)
            {
                best = d;
            }
        }
        if(best < 1e29f)
        {
            nearest.push_back(std::sqrt(best));
        }
    }
    if(nearest.empty())
    {
        return 1.0f;
    }
    std::nth_element(nearest.begin(), nearest.begin() + nearest.size() / 2, nearest.end());
    return nearest[nearest.size() / 2];
}
} // namespace

ActionDeviceViewCanvas::ActionDeviceViewCanvas(const Action::Target& target,
                                               std::vector<std::unique_ptr<ControllerTransform>>* transforms,
                                               ZoneManager3D* zone_manager,
                                               QWidget* parent)
    : QWidget(parent)
    , target_(target)
    , transforms_(transforms)
    , zone_manager_(zone_manager)
{
    setMinimumSize(320, 180);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setMouseTracking(false);
}

void ActionDeviceViewCanvas::setThreeD(bool on)
{
    three_d_ = on;
    setCursor(on ? Qt::OpenHandCursor : Qt::ArrowCursor);
    update();
}

void ActionDeviceViewCanvas::collect(std::vector<const LEDPosition3D*>* out) const
{
    if(!transforms_ || !out)
    {
        return;
    }
    for(int i = 0; i < (int)transforms_->size(); ++i)
    {
        ControllerTransform* t = (*transforms_)[(size_t)i].get();
        if(!t || t->hidden_by_virtual)
        {
            continue;
        }
        if(target_.kind == Action::TargetKind::SceneZone)
        {
            if(!Action::TransformInSceneZone(t, i, target_, zone_manager_))
            {
                continue;
            }
            for(const LEDPosition3D& led : t->led_positions)
            {
                out->push_back(&led);
            }
            continue;
        }
        for(const LEDPosition3D& led : t->led_positions)
        {
            if(Action::applier_detail::LedMatchesTarget(led, t->controller, target_))
            {
                out->push_back(&led);
            }
        }
    }
}

void ActionDeviceViewCanvas::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(14, 14, 16));
    p.setRenderHint(QPainter::Antialiasing, true);

    std::vector<const LEDPosition3D*> leds;
    collect(&leds);
    last_count_ = (int)leds.size();
    if(leds.empty())
    {
        p.setPen(QColor(170, 170, 170));
        p.drawText(rect(), Qt::AlignCenter, QStringLiteral("No lights for this row in the current layout"));
        return;
    }

    /* Flat: one device reads best in its own layout space (a keyboard is its key grid);
       several devices (a scene zone) use room space. 3D always uses room space. */
    bool same_controller = true;
    for(const LEDPosition3D* led : leds)
    {
        same_controller = same_controller && led->controller == leds.front()->controller;
    }
    const bool use_local = !three_d_ && same_controller && target_.kind != Action::TargetKind::SceneZone;
    std::vector<Vector3D> pts;
    pts.reserve(leds.size());
    for(const LEDPosition3D* led : leds)
    {
        pts.push_back(use_local ? led->local_position : led->world_position);
    }
    Vector3D lo = pts.front();
    Vector3D hi = pts.front();
    for(const Vector3D& v : pts)
    {
        lo.x = std::min(lo.x, v.x); lo.y = std::min(lo.y, v.y); lo.z = std::min(lo.z, v.z);
        hi.x = std::max(hi.x, v.x); hi.y = std::max(hi.y, v.y); hi.z = std::max(hi.z, v.z);
    }
    const Vector3D mid{(lo.x + hi.x) * 0.5f, (lo.y + hi.y) * 0.5f, (lo.z + hi.z) * 0.5f};
    if(cached_count_ != (int)pts.size() || cached_local_ != use_local)
    {
        cached_spacing_ = EstimateSpacing(pts);
        cached_count_ = (int)pts.size();
        cached_local_ = use_local;
    }
    const float spacing = cached_spacing_;

    std::vector<Dot> dots(leds.size());
    float min_x = 1e30f, max_x = -1e30f, min_y = 1e30f, max_y = -1e30f;
    if(three_d_)
    {
        const float yaw = yaw_deg_ * 3.14159265f / 180.0f;
        const float pitch = pitch_deg_ * 3.14159265f / 180.0f;
        const float cy = std::cos(yaw), sy = std::sin(yaw), cp = std::cos(pitch), sp = std::sin(pitch);
        for(size_t i = 0; i < pts.size(); ++i)
        {
            const float x = pts[i].x - mid.x, y = pts[i].y - mid.y, z = pts[i].z - mid.z;
            const float rx = cy * x + sy * z;
            const float rz = -sy * x + cy * z;
            const float ry = cp * y - sp * rz;
            const float rz2 = sp * y + cp * rz;
            dots[i].x = rx;
            dots[i].y = -ry;
            dots[i].depth = rz2;
        }
        /* Frame the whole box so orbiting never jumps the zoom. */
        const float half = 0.5f * std::sqrt((hi.x - lo.x) * (hi.x - lo.x) + (hi.y - lo.y) * (hi.y - lo.y)
                                            + (hi.z - lo.z) * (hi.z - lo.z));
        min_x = min_y = -std::max(half, spacing);
        max_x = max_y = std::max(half, spacing);
    }
    else
    {
        /* Face-on: the two widest axes; up is +Y, or +Z (toward the back) for flat layouts. */
        const float ext[3] = {hi.x - lo.x, hi.y - lo.y, hi.z - lo.z};
        int order[3] = {0, 1, 2};
        std::sort(order, order + 3, [&](int a, int b) { return ext[a] > ext[b]; });
        int h_axis = order[0];
        int v_axis = order[1];
        /* Keep X horizontal when it is one of the two, so devices read left to right. */
        if(v_axis == 0)
        {
            std::swap(h_axis, v_axis);
        }
        auto comp = [](const Vector3D& v, int a) { return a == 0 ? v.x : (a == 1 ? v.y : v.z); };
        for(size_t i = 0; i < pts.size(); ++i)
        {
            dots[i].x = comp(pts[i], h_axis);
            dots[i].y = -comp(pts[i], v_axis);
            dots[i].depth = 0.0f;
        }
        for(const Dot& d : dots)
        {
            min_x = std::min(min_x, d.x); max_x = std::max(max_x, d.x);
            min_y = std::min(min_y, d.y); max_y = std::max(max_y, d.y);
        }
        min_x -= spacing * 0.5f; max_x += spacing * 0.5f;
        min_y -= spacing * 0.5f; max_y += spacing * 0.5f;
    }

    const float margin = 14.0f;
    const float w = std::max(1.0f, (float)width() - 2.0f * margin);
    const float h = std::max(1.0f, (float)height() - 2.0f * margin);
    const float span_x = std::max(max_x - min_x, 1e-4f);
    const float span_y = std::max(max_y - min_y, 1e-4f);
    const float scale = std::min(w / span_x, h / span_y) * (three_d_ ? zoom_ : 1.0f);
    const float cx = width() * 0.5f - (min_x + max_x) * 0.5f * scale;
    const float cy = height() * 0.5f - (min_y + max_y) * 0.5f * scale;
    const float size = std::clamp(spacing * scale * (three_d_ ? 0.6f : 0.82f), 2.5f, 42.0f);

    for(size_t i = 0; i < leds.size(); ++i)
    {
        const RGBColor c = leds[i]->preview_color;
        const int r = RGBGetRValue(c), g = RGBGetGValue(c), b = RGBGetBValue(c);
        dots[i].color = QColor(r, g, b);
        dots[i].lit = (r + g + b) > 12;
        dots[i].x = cx + dots[i].x * scale;
        dots[i].y = cy + dots[i].y * scale;
    }
    std::vector<size_t> order(dots.size());
    for(size_t i = 0; i < order.size(); ++i)
    {
        order[i] = i;
    }
    if(three_d_)
    {
        std::sort(order.begin(), order.end(), [&](size_t a, size_t b) { return dots[a].depth > dots[b].depth; });
    }

    /* Glow pass first so cores sit on top of every halo. */
    p.setPen(Qt::NoPen);
    for(size_t k : order)
    {
        const Dot& d = dots[k];
        if(!d.lit)
        {
            continue;
        }
        const float glow = size * 1.6f;
        QRadialGradient grad(QPointF(d.x, d.y), glow);
        QColor inner = d.color;
        inner.setAlpha(110);
        QColor outer = d.color;
        outer.setAlpha(0);
        grad.setColorAt(0.0, inner);
        grad.setColorAt(1.0, outer);
        p.setBrush(grad);
        p.drawEllipse(QPointF(d.x, d.y), glow, glow);
    }
    for(size_t k : order)
    {
        const Dot& d = dots[k];
        const QRectF cell(d.x - size * 0.5f, d.y - size * 0.5f, size, size);
        if(d.lit)
        {
            p.setPen(Qt::NoPen);
            p.setBrush(d.color);
        }
        else
        {
            p.setPen(QPen(QColor(70, 70, 76), 1));
            p.setBrush(QColor(28, 28, 32));
        }
        if(three_d_)
        {
            p.drawEllipse(cell);
        }
        else
        {
            p.drawRoundedRect(cell, size * 0.22f, size * 0.22f);
        }
    }
}

void ActionDeviceViewCanvas::mousePressEvent(QMouseEvent* event)
{
    drag_last_ = event->position().toPoint();
    if(three_d_)
    {
        setCursor(Qt::ClosedHandCursor);
    }
}

void ActionDeviceViewCanvas::mouseMoveEvent(QMouseEvent* event)
{
    if(!three_d_ || !(event->buttons() & Qt::LeftButton))
    {
        return;
    }
    const QPoint pos = event->position().toPoint();
    const QPoint delta = pos - drag_last_;
    drag_last_ = pos;
    yaw_deg_ += delta.x() * 0.5f;
    pitch_deg_ = std::clamp(pitch_deg_ + delta.y() * 0.5f, -89.0f, 89.0f);
    update();
}

void ActionDeviceViewCanvas::wheelEvent(QWheelEvent* event)
{
    if(!three_d_)
    {
        return;
    }
    const float steps = event->angleDelta().y() / 120.0f;
    zoom_ = std::clamp(zoom_ * std::pow(1.15f, steps), 0.3f, 8.0f);
    update();
    event->accept();
}

void ActionDeviceViewCanvas::mouseDoubleClickEvent(QMouseEvent*)
{
    yaw_deg_ = -30.0f;
    pitch_deg_ = 20.0f;
    zoom_ = 1.0f;
    update();
}

ActionDeviceViewWindow::ActionDeviceViewWindow(const Action::Target& target,
                                               const QString& label,
                                               std::vector<std::unique_ptr<ControllerTransform>>* transforms,
                                               ZoneManager3D* zone_manager,
                                               QWidget* parent)
    : QWidget(parent, Qt::Tool | Qt::WindowTitleHint | Qt::WindowCloseButtonHint | Qt::WindowMinMaxButtonsHint)
{
    setAttribute(Qt::WA_DeleteOnClose, true);
    setWindowTitle(QStringLiteral("%1 — lights").arg(label));
    resize(560, 300);

    canvas_ = new ActionDeviceViewCanvas(target, transforms, zone_manager, this);
    flat_button_ = new QToolButton();
    flat_button_->setText(QStringLiteral("Flat"));
    flat_button_->setCheckable(true);
    flat_button_->setChecked(true);
    flat_button_->setToolTip(QStringLiteral("Face-on layout (a keyboard shows as its key grid)"));
    three_d_button_ = new QToolButton();
    three_d_button_->setText(QStringLiteral("3D"));
    three_d_button_->setCheckable(true);
    three_d_button_->setToolTip(QStringLiteral("Lights where they sit in the room: drag to orbit, wheel to zoom, double-click to reset"));
    info_ = new QLabel();
    info_->setForegroundRole(QPalette::PlaceholderText);

    auto* bar = new QHBoxLayout();
    bar->setContentsMargins(8, 6, 8, 4);
    bar->setSpacing(4);
    bar->addWidget(flat_button_);
    bar->addWidget(three_d_button_);
    bar->addSpacing(10);
    bar->addWidget(info_, 1);
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->addLayout(bar);
    root->addWidget(canvas_, 1);

    auto set_mode = [this](bool three) {
        flat_button_->setChecked(!three);
        three_d_button_->setChecked(three);
        canvas_->setThreeD(three);
    };
    connect(flat_button_, &QToolButton::clicked, this, [set_mode]() { set_mode(false); });
    connect(three_d_button_, &QToolButton::clicked, this, [set_mode]() { set_mode(true); });

    timer_ = new QTimer(this);
    timer_->setInterval(33);
    connect(timer_, &QTimer::timeout, this, [this]() {
        canvas_->update();
        const int n = canvas_->lightCount();
        info_->setText(canvas_->threeD()
                           ? QStringLiteral("%1 lights · drag to orbit · wheel to zoom").arg(n)
                           : QStringLiteral("%1 lights").arg(n));
    });
}

void ActionDeviceViewWindow::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    timer_->start();
}

void ActionDeviceViewWindow::hideEvent(QHideEvent* event)
{
    timer_->stop();
    QWidget::hideEvent(event);
}
