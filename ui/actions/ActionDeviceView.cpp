// SPDX-License-Identifier: GPL-2.0-or-later
#include "ActionDeviceView.h"
#include "ActionApplier.h"
#include "ActionApplierDetail.h"
#include "ControllerLayout3D.h"
#include "LEDViewport3D.h"
#include "VirtualController3D.h"
#include "ZoneManager3D.h"
#include "viewport/ViewportGLFormat.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QRadialGradient>
#include <QStackedWidget>
#include <QTimer>
#include <QToolButton>
#include <QToolTip>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <map>
#include <tuple>

namespace Action
{
namespace device_view
{

RowLights CollectRowLights(const Target& target,
                           std::vector<std::unique_ptr<ControllerTransform>>* transforms,
                           ZoneManager3D* zone_manager)
{
    RowLights out;
    if(!transforms)
    {
        return out;
    }
    for(int i = 0; i < (int)transforms->size(); ++i)
    {
        ControllerTransform* t = (*transforms)[(size_t)i].get();
        if(!t || t->hidden_by_virtual)
        {
            continue;
        }
        RowLights::Group group;
        group.transform = t;
        if(target.kind == TargetKind::SceneZone)
        {
            if(!TransformInSceneZone(t, i, target, zone_manager))
            {
                continue;
            }
            for(const LEDPosition3D& led : t->led_positions)
            {
                group.leds.push_back(&led);
            }
        }
        else if(target.kind == TargetKind::All)
        {
            for(const LEDPosition3D& led : t->led_positions)
            {
                group.leds.push_back(&led);
            }
        }
        else
        {
            if(!TransformMatchesDevice(t, target.device_name))
            {
                continue;
            }
            /* A custom controller that borrows a few of this device's LEDs only shows those. */
            const bool whole = target.device_name.empty()
                || (t->virtual_controller && NameMatches(t->virtual_controller->GetName(), target.device_name))
                || (t->controller && ControllerMatchesDevice(t->controller, target.device_name));
            for(const LEDPosition3D& led : t->led_positions)
            {
                if(!whole && !(led.controller && ControllerMatchesDevice(led.controller, target.device_name)))
                {
                    continue;
                }
                if(applier_detail::LedMatchesTarget(led, t->controller, target))
                {
                    group.leds.push_back(&led);
                }
            }
        }
        if(!group.leds.empty())
        {
            out.count += group.leds.size();
            out.groups.push_back(std::move(group));
        }
    }
    return out;
}

RGBColor ShownColor(const LEDPosition3D& led)
{
    if(led.preview_color != 0x00FFFFFF || !led.controller)
    {
        return led.preview_color;
    }
    unsigned int global = 0;
    if(applier_detail::TryGetGlobalLedIndex(led.controller, led.zone_idx, led.led_idx, &global))
    {
        return led.controller->GetColor(global);
    }
    return led.preview_color;
}

} // namespace device_view
} // namespace Action

namespace
{
using Action::device_view::RowLights;

inline size_t HashMix(size_t h, size_t v)
{
    return h ^ (v + (size_t)0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2));
}

size_t HashFloat(size_t h, float f)
{
    uint32_t bits = 0;
    std::memcpy(&bits, &f, sizeof(bits));
    return HashMix(h, bits);
}

size_t LightsSignature(const RowLights& lights, bool with_transforms)
{
    size_t h = lights.count;
    for(const RowLights::Group& g : lights.groups)
    {
        h = HashMix(h, (size_t)g.transform);
        h = HashMix(h, g.leds.size());
        if(!g.leds.empty())
        {
            h = HashMix(h, (size_t)g.leds.front());
            h = HashMix(h, (size_t)g.leds.back());
        }
        if(with_transforms && g.transform)
        {
            const Transform3D& x = g.transform->transform;
            const float v[9] = {x.position.x, x.position.y, x.position.z,
                                x.rotation.x, x.rotation.y, x.rotation.z,
                                x.scale.x, x.scale.y, x.scale.z};
            for(float f : v)
            {
                h = HashFloat(h, f);
            }
        }
    }
    return h;
}

QString LedLabel(RGBControllerInterface* c, unsigned int global)
{
    if(!c || global >= c->GetLEDCount())
    {
        return QString();
    }
    std::string name = c->GetLEDDisplayName(global);
    if(name.empty())
    {
        name = c->GetLEDName(global);
    }
    QString s = QString::fromStdString(name);
    if(s.startsWith(QStringLiteral("Key: ")))
    {
        s = s.mid(5);
    }
    return s;
}

QString ZoneTitle(RGBControllerInterface* c, unsigned int z)
{
    std::string name = c->GetZoneDisplayName(z);
    if(name.empty())
    {
        name = c->GetZoneName(z);
    }
    return name.empty() ? QStringLiteral("Zone %1").arg(z) : QString::fromStdString(name);
}

QString DeviceTitle(const ControllerTransform* t, RGBControllerInterface* c)
{
    if(t && t->virtual_controller)
    {
        return QString::fromStdString(t->virtual_controller->GetName());
    }
    if(c)
    {
        std::string name = c->GetDisplayName();
        return QString::fromStdString(name.empty() ? c->GetName() : name);
    }
    return QStringLiteral("Device");
}

float Luma(const QColor& c)
{
    return 0.2126f * (float)c.redF() + 0.7152f * (float)c.greenF() + 0.0722f * (float)c.blueF();
}

QColor ToQColor(RGBColor rgb)
{
    return QColor(RGBGetRValue(rgb), RGBGetGValue(rgb), RGBGetBValue(rgb));
}

constexpr float kSectionTitlePx = 16.0f;
constexpr float kDeviceTitlePx = 20.0f;
constexpr float kSectionGapPx = 10.0f;
constexpr float kMarginPx = 14.0f;
constexpr int kStripWrap = 32;
} // namespace

/* ------------------------------------------------------------------ flat -- */

ActionDeviceMatrixView::ActionDeviceMatrixView(const Action::Target& target,
                                               std::vector<std::unique_ptr<ControllerTransform>>* transforms,
                                               ZoneManager3D* zone_manager,
                                               QWidget* parent)
    : QOpenGLWidget(parent)
    , target_(target)
    , transforms_(transforms)
    , zone_manager_(zone_manager)
{
    ViewportGLFormat::ApplyToWidget(this);
    setMinimumSize(320, 160);
    setMouseTracking(true);
}

void ActionDeviceMatrixView::refresh()
{
    update();
}

void ActionDeviceMatrixView::rebuildLayout(const RowLights& lights)
{
    sections_.clear();

    /* Physical (controller, zone) -> the row's LEDs in it. */
    std::vector<std::pair<RGBControllerInterface*, const ControllerTransform*>> device_order;
    std::map<std::pair<RGBControllerInterface*, unsigned int>, int> zones; // value: LEDs seen
    std::vector<const RowLights::Group*> loose; // custom controllers: laid out from their own grid

    for(const RowLights::Group& g : lights.groups)
    {
        if(g.transform && g.transform->virtual_controller)
        {
            loose.push_back(&g);
            continue;
        }
        for(const LEDPosition3D* led : g.leds)
        {
            RGBControllerInterface* c = led->controller ? led->controller : g.transform->controller;
            if(!c || led->zone_idx >= c->GetZoneCount())
            {
                continue;
            }
            if(std::none_of(device_order.begin(), device_order.end(),
                            [c](const std::pair<RGBControllerInterface*, const ControllerTransform*>& d) {
                                return d.first == c;
                            }))
            {
                device_order.push_back({c, g.transform});
            }
            zones[{c, led->zone_idx}] += 1;
        }
    }

    auto has_matrix = [](RGBControllerInterface* c, unsigned int z) {
        return c->GetZoneMatrixMapData(z) && c->GetZoneMatrixMapWidth(z) > 0 && c->GetZoneMatrixMapHeight(z) > 0;
    };
    int wrap = 0;
    for(const auto& z : zones)
    {
        if(has_matrix(z.first.first, z.first.second))
        {
            wrap = std::max(wrap, (int)z.first.first->GetZoneMatrixMapWidth(z.first.second));
        }
    }
    if(wrap <= 0)
    {
        wrap = kStripWrap;
    }
    const bool several = device_order.size() + loose.size() > 1;

    for(const auto& dev : device_order)
    {
        RGBControllerInterface* c = dev.first;
        bool first = true;
        for(const auto& z : zones)
        {
            if(z.first.first != c)
            {
                continue;
            }
            const unsigned int zi = z.first.second;
            const unsigned int start = c->GetZoneStartIndex(zi);
            const unsigned int zone_leds = c->GetZoneLEDsCount(zi);
            Section s;
            if(first && several)
            {
                s.device_title = DeviceTitle(dev.second, c);
            }
            first = false;
            s.title = ZoneTitle(c, zi);
            if(has_matrix(c, zi))
            {
                const unsigned int* map = c->GetZoneMatrixMapData(zi);
                const unsigned int mw = c->GetZoneMatrixMapWidth(zi);
                const unsigned int mh = c->GetZoneMatrixMapHeight(zi);
                s.matrix = true;
                s.cols = (int)mw;
                s.rows = (int)mh;
                for(unsigned int y = 0; y < mh; ++y)
                {
                    for(unsigned int x = 0; x < mw; ++x)
                    {
                        const unsigned int v = map[y * mw + x];
                        if(v == 0xFFFFFFFF || v >= zone_leds)
                        {
                            continue;
                        }
                        Cell cell;
                        cell.col = (int)x;
                        cell.row = (int)y;
                        cell.controller = c;
                        cell.zone_idx = zi;
                        cell.led_idx = v;
                        cell.label = LedLabel(c, start + v);
                        s.cells.push_back(cell);
                    }
                }
            }
            else
            {
                const int n = (int)std::max(1u, zone_leds);
                s.cols = std::min(n, wrap);
                s.rows = (n + s.cols - 1) / s.cols;
                for(int i = 0; i < (int)zone_leds; ++i)
                {
                    Cell cell;
                    cell.col = i % s.cols;
                    cell.row = i / s.cols;
                    cell.controller = c;
                    cell.zone_idx = zi;
                    cell.led_idx = (unsigned int)i;
                    s.cells.push_back(cell);
                }
            }
            sections_.push_back(std::move(s));
        }
    }

    /* Custom controllers have no OpenRGB matrix: lay their lights on their own X/Y grid. */
    for(const RowLights::Group* g : loose)
    {
        std::vector<float> xs;
        std::vector<float> ys;
        for(const LEDPosition3D* led : g->leds)
        {
            xs.push_back(std::round(led->local_position.x * 100.0f) / 100.0f);
            ys.push_back(std::round(led->local_position.y * 100.0f) / 100.0f);
        }
        auto uniq = [](std::vector<float> v) {
            std::sort(v.begin(), v.end());
            v.erase(std::unique(v.begin(), v.end()), v.end());
            return v;
        };
        const std::vector<float> ux = uniq(xs);
        const std::vector<float> uy = uniq(ys);
        Section s;
        s.title = DeviceTitle(g->transform, nullptr);
        s.cols = std::max(1, (int)ux.size());
        s.rows = std::max(1, (int)uy.size());
        std::vector<char> used((size_t)s.cols * (size_t)s.rows, 0);
        for(size_t i = 0; i < g->leds.size(); ++i)
        {
            const int col = (int)(std::lower_bound(ux.begin(), ux.end(), xs[i]) - ux.begin());
            /* +Y is up in the room; rows count down from the top. */
            const int row = s.rows - 1 - (int)(std::lower_bound(uy.begin(), uy.end(), ys[i]) - uy.begin());
            char& slot = used[(size_t)row * (size_t)s.cols + (size_t)col];
            if(slot)
            {
                continue; // a deeper layer behind a front light
            }
            slot = 1;
            const LEDPosition3D* led = g->leds[i];
            Cell cell;
            cell.col = col;
            cell.row = row;
            cell.controller = led->controller ? led->controller : g->transform->controller;
            cell.zone_idx = led->zone_idx;
            cell.led_idx = led->led_idx;
            s.cells.push_back(cell);
        }
        sections_.push_back(std::move(s));
    }
}

QRectF ActionDeviceMatrixView::cellRect(const Section& s, const Cell& c, float top) const
{
    (void)s;
    const float gap = std::max(1.0f, cell_px_ * 0.10f);
    return QRectF(origin_x_ + c.col * cell_px_ + gap * 0.5f,
                  top + c.row * cell_px_ + gap * 0.5f,
                  cell_px_ - gap,
                  cell_px_ - gap);
}

void ActionDeviceMatrixView::paintGL()
{
    const RowLights lights = Action::device_view::CollectRowLights(target_, transforms_, zone_manager_);
    light_count_ = (int)lights.count;
    const size_t sig = LightsSignature(lights, false);
    if(sig != signature_)
    {
        signature_ = sig;
        rebuildLayout(lights);
        hover_section_ = hover_cell_ = -1;
    }

    /* Point every cell at this frame's LED (or nothing when the row does not drive it). */
    std::map<std::tuple<RGBControllerInterface*, unsigned int, unsigned int>, const LEDPosition3D*> by_key;
    for(const RowLights::Group& g : lights.groups)
    {
        for(const LEDPosition3D* led : g.leds)
        {
            by_key[std::make_tuple(led->controller ? led->controller : g.transform->controller,
                                   led->zone_idx, led->led_idx)] = led;
        }
    }
    for(Section& s : sections_)
    {
        for(Cell& c : s.cells)
        {
            auto it = by_key.find(std::make_tuple(c.controller, c.zone_idx, c.led_idx));
            c.led = it == by_key.end() ? nullptr : it->second;
        }
    }

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);
    const QRectF area = rect();
    {
        QLinearGradient bg(area.topLeft(), area.bottomLeft());
        bg.setColorAt(0.0, QColor(24, 25, 30));
        bg.setColorAt(1.0, QColor(12, 12, 15));
        p.fillRect(area, bg);
    }

    if(sections_.empty())
    {
        p.setPen(QColor(160, 160, 168));
        p.drawText(area, Qt::AlignCenter, tr("No lights for this row in the current layout"));
        return;
    }

    int max_cols = 1;
    int total_rows = 0;
    float fixed_px = 0.0f;
    for(const Section& s : sections_)
    {
        max_cols = std::max(max_cols, s.cols);
        total_rows += s.rows;
        fixed_px += kSectionTitlePx + (s.device_title.isEmpty() ? 0.0f : kDeviceTitlePx);
    }
    fixed_px += kSectionGapPx * (float)(sections_.size() - 1);
    const float avail_w = std::max(1.0f, (float)width() - 2.0f * kMarginPx);
    const float avail_h = std::max(1.0f, (float)height() - 2.0f * kMarginPx - fixed_px);
    cell_px_ = std::clamp(std::min(avail_w / (float)max_cols, avail_h / (float)std::max(1, total_rows)), 3.0f, 56.0f);
    const float content_w = cell_px_ * (float)max_cols;
    const float content_h = cell_px_ * (float)total_rows + fixed_px;
    origin_x_ = std::floor(((float)width() - content_w) * 0.5f);
    float y = std::max(kMarginPx, std::floor(((float)height() - content_h) * 0.5f));

    section_tops_.assign(sections_.size(), 0.0f);
    QFont title_font = font();
    title_font.setPixelSize(11);
    QFont device_font = font();
    device_font.setPixelSize(13);
    device_font.setBold(true);
    QFont key_font = font();
    key_font.setPixelSize((int)std::clamp(cell_px_ * 0.30f, 7.0f, 12.0f));
    const float radius = std::max(1.0f, cell_px_ * 0.18f);

    for(size_t si = 0; si < sections_.size(); ++si)
    {
        const Section& s = sections_[si];
        if(!s.device_title.isEmpty())
        {
            p.setFont(device_font);
            p.setPen(QColor(225, 225, 232));
            p.drawText(QRectF(origin_x_, y, content_w, kDeviceTitlePx), Qt::AlignLeft | Qt::AlignVCenter, s.device_title);
            y += kDeviceTitlePx;
        }
        p.setFont(title_font);
        p.setPen(QColor(140, 142, 152));
        p.drawText(QRectF(origin_x_, y, content_w, kSectionTitlePx), Qt::AlignLeft | Qt::AlignVCenter,
                   QStringLiteral("%1  ·  %2").arg(s.title).arg(s.cells.size()));
        y += kSectionTitlePx;
        section_tops_[si] = y;

        /* Halo pass with added light, so neighbours blend like real diffusers. */
        p.setPen(Qt::NoPen);
        p.setCompositionMode(QPainter::CompositionMode_Plus);
        for(const Cell& c : s.cells)
        {
            if(!c.led)
            {
                continue;
            }
            const QColor col = ToQColor(Action::device_view::ShownColor(*c.led));
            const float lum = (float)(col.red() + col.green() + col.blue()) / 765.0f;
            if(lum < 0.02f)
            {
                continue;
            }
            const QRectF r = cellRect(s, c, y);
            const float glow = cell_px_ * 1.05f;
            QRadialGradient grad(r.center(), glow);
            QColor inner = col;
            inner.setAlpha((int)std::clamp(60.0f + 80.0f * lum, 0.0f, 150.0f));
            QColor outer = col;
            outer.setAlpha(0);
            grad.setColorAt(0.0, inner);
            grad.setColorAt(1.0, outer);
            p.setBrush(grad);
            p.drawEllipse(r.center(), glow, glow);
        }
        p.setCompositionMode(QPainter::CompositionMode_SourceOver);

        p.setFont(key_font);
        const bool labels = s.matrix && cell_px_ >= 22.0f;
        for(size_t ci = 0; ci < s.cells.size(); ++ci)
        {
            const Cell& c = s.cells[ci];
            const QRectF r = cellRect(s, c, y);
            QColor fill(32, 33, 39);
            QColor edge(60, 62, 71);
            bool lit = false;
            if(c.led)
            {
                const QColor col = ToQColor(Action::device_view::ShownColor(*c.led));
                lit = (col.red() + col.green() + col.blue()) > 12;
                if(lit)
                {
                    fill = col;
                    edge = col.lighter(135);
                }
            }
            else
            {
                fill = QColor(22, 22, 26);
                edge = QColor(36, 37, 42);
            }
            if(lit)
            {
                QLinearGradient face(r.topLeft(), r.bottomLeft());
                face.setColorAt(0.0, fill.lighter(125));
                face.setColorAt(1.0, fill.darker(112));
                p.setBrush(face);
            }
            else
            {
                p.setBrush(fill);
            }
            const bool hovered = (int)si == hover_section_ && (int)ci == hover_cell_;
            p.setPen(QPen(hovered ? QColor(255, 255, 255) : edge, hovered ? 1.6 : 1.0));
            p.drawRoundedRect(r, radius, radius);
            if(labels && !c.label.isEmpty())
            {
                p.setPen(lit && Luma(fill) > 0.55f ? QColor(15, 15, 18) : QColor(200, 202, 210, lit ? 255 : 140));
                const QString text = p.fontMetrics().elidedText(c.label, Qt::ElideRight, (int)r.width() - 2);
                p.drawText(r, Qt::AlignCenter, text);
            }
        }
        y += cell_px_ * (float)s.rows + kSectionGapPx;
    }
}

void ActionDeviceMatrixView::mouseMoveEvent(QMouseEvent* event)
{
    const QPointF pos = event->position();
    int hs = -1;
    int hc = -1;
    for(size_t si = 0; si < sections_.size() && si < section_tops_.size() && hs < 0; ++si)
    {
        const Section& s = sections_[si];
        for(size_t ci = 0; ci < s.cells.size(); ++ci)
        {
            if(cellRect(s, s.cells[ci], section_tops_[si]).contains(pos))
            {
                hs = (int)si;
                hc = (int)ci;
                break;
            }
        }
    }
    if(hs == hover_section_ && hc == hover_cell_)
    {
        return;
    }
    hover_section_ = hs;
    hover_cell_ = hc;
    if(hs >= 0)
    {
        const Cell& c = sections_[(size_t)hs].cells[(size_t)hc];
        QString name = c.label;
        if(name.isEmpty() && c.controller && c.zone_idx < c.controller->GetZoneCount())
        {
            name = LedLabel(c.controller, c.controller->GetZoneStartIndex(c.zone_idx) + c.led_idx);
        }
        if(name.isEmpty())
        {
            name = QStringLiteral("LED %1").arg(c.led_idx + 1);
        }
        QString tip = name;
        if(c.led)
        {
            const RGBColor rgb = Action::device_view::ShownColor(*c.led);
            tip += QStringLiteral("  ·  #%1%2%3")
                       .arg(RGBGetRValue(rgb), 2, 16, QChar('0'))
                       .arg(RGBGetGValue(rgb), 2, 16, QChar('0'))
                       .arg(RGBGetBValue(rgb), 2, 16, QChar('0'))
                       .toUpper();
        }
        else
        {
            tip += tr("  ·  not driven by this row");
        }
        QToolTip::showText(event->globalPosition().toPoint(), tip, this);
    }
    else
    {
        QToolTip::hideText();
    }
    update();
}

void ActionDeviceMatrixView::leaveEvent(QEvent* event)
{
    hover_section_ = hover_cell_ = -1;
    QToolTip::hideText();
    update();
    QOpenGLWidget::leaveEvent(event);
}

/* -------------------------------------------------------------------- 3D -- */

ActionDevice3DView::ActionDevice3DView(const Action::Target& target,
                                       std::vector<std::unique_ptr<ControllerTransform>>* transforms,
                                       ZoneManager3D* zone_manager,
                                       QWidget* parent)
    : QWidget(parent)
    , target_(target)
    , source_(transforms)
    , zone_manager_(zone_manager)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    viewport_ = new LEDViewport3D(this);
    ViewportGLFormat::ApplyToWidget(viewport_);
    viewport_->setFocusPolicy(Qt::StrongFocus);
    viewport_->SetShowRoomGuideLabels(false);
    viewport_->SetShowRoomGridOverlay(false);
    viewport_->SetReferencePoints(nullptr);
    viewport_->SetDisplayPlanes(nullptr);
    viewport_->SetSceneProps(nullptr);
    viewport_->SetRoomDimensions(0.0f, 0.0f, 0.0f, false);
    viewport_->SetGridSnapEnabled(false);
    viewport_->SetControllerTransforms(&copies_);
    viewport_->SetViewportPaintingEnabled(false);
    layout->addWidget(viewport_);

    /* A look-only copy: picking a light must not leave a move gizmo behind. */
    connect(viewport_, &LEDViewport3D::ControllerSelected, this, [this](int index) {
        if(index >= 0)
        {
            QTimer::singleShot(0, viewport_, [this]() {
                viewport_->ClearSelection();
                viewport_->update();
            });
        }
    });
}

ActionDevice3DView::~ActionDevice3DView()
{
    if(viewport_)
    {
        viewport_->SetControllerTransforms(nullptr);
    }
}

void ActionDevice3DView::setActive(bool active)
{
    viewport_->SetViewportPaintingEnabled(active);
    if(active)
    {
        refresh();
    }
}

void ActionDevice3DView::rebuild(const RowLights& lights)
{
    viewport_->ClearSelection();
    copies_.clear();
    light_count_ = (int)lights.count;

    for(const RowLights::Group& g : lights.groups)
    {
        const ControllerTransform* src = g.transform;
        auto copy = std::make_unique<ControllerTransform>();
        const bool whole = g.leds.size() == src->led_positions.size();
        copy->controller = src->controller;
        /* Part of a custom controller is drawn as loose lights, not the whole shell. */
        copy->virtual_controller = whole ? src->virtual_controller : nullptr;
        if(!copy->controller && !copy->virtual_controller)
        {
            copy->controller = g.leds.front()->controller;
        }
        copy->transform = src->transform;
        copy->display_color = src->display_color;
        copy->hidden_by_virtual = false;
        copy->led_spacing_mm_x = src->led_spacing_mm_x;
        copy->led_spacing_mm_y = src->led_spacing_mm_y;
        copy->led_spacing_mm_z = src->led_spacing_mm_z;
        copy->granularity = src->granularity;
        copy->item_idx = src->item_idx;
        copy->linked_reference_point_index = -1;
        copy->world_positions_dirty = true;
        copy->led_positions.reserve(g.leds.size());
        for(const LEDPosition3D* led : g.leds)
        {
            copy->led_positions.push_back(*led);
            copy->led_positions.back().preview_color = Action::device_view::ShownColor(*led);
        }
        ControllerLayout3D::UpdateWorldPositions(copy.get());
        copies_.push_back(std::move(copy));
    }

    /* Sit the lights in the middle of a small grid, the way the Custom Controller preview does. */
    bool any = false;
    Vector3D lo{0, 0, 0};
    Vector3D hi{0, 0, 0};
    for(const auto& c : copies_)
    {
        for(const LEDPosition3D& led : c->led_positions)
        {
            const Vector3D& w = led.world_position;
            if(!any)
            {
                lo = hi = w;
                any = true;
                continue;
            }
            lo.x = std::min(lo.x, w.x); lo.y = std::min(lo.y, w.y); lo.z = std::min(lo.z, w.z);
            hi.x = std::max(hi.x, w.x); hi.y = std::max(hi.y, w.y); hi.z = std::max(hi.z, w.z);
        }
    }
    constexpr float pad = 3.0f;
    auto dim = [](float span) { return std::max(6, (int)std::ceil(span + 2.0f * pad)); };
    grid_x_ = any ? dim(hi.x - lo.x) : 6;
    grid_y_ = any ? dim(hi.y - lo.y) : 6;
    grid_z_ = any ? dim(hi.z - lo.z) : 6;
    if(any)
    {
        const float dx = (float)grid_x_ * 0.5f - (lo.x + hi.x) * 0.5f;
        const float dy = (float)grid_y_ * 0.5f - (lo.y + hi.y) * 0.5f;
        const float dz = (float)grid_z_ * 0.5f - (lo.z + hi.z) * 0.5f;
        for(auto& c : copies_)
        {
            c->transform.position.x += dx;
            c->transform.position.y += dy;
            c->transform.position.z += dz;
            ControllerLayout3D::MarkWorldPositionsDirty(c.get());
            ControllerLayout3D::UpdateWorldPositions(c.get());
        }
    }
    viewport_->SetGridDimensions(grid_x_, grid_y_, grid_z_);
    viewport_->SetControllerTransforms(&copies_);
    if(!framed_ && any)
    {
        frameCamera();
        framed_ = true;
    }
}

void ActionDevice3DView::frameCamera()
{
    const float extent = std::max(4.0f, (float)std::max({grid_x_, grid_y_, grid_z_, 1}) - 6.0f);
    viewport_->SetCamera(std::max(10.0f, extent * 1.9f), 45.0f, 24.0f,
                         (float)grid_x_ * 0.5f, (float)grid_y_ * 0.5f, (float)grid_z_ * 0.5f);
}

void ActionDevice3DView::refresh()
{
    const RowLights lights = Action::device_view::CollectRowLights(target_, source_, zone_manager_);
    const size_t sig = LightsSignature(lights, true);
    if(sig != signature_ || copies_.size() != lights.groups.size())
    {
        signature_ = sig;
        rebuild(lights);
    }
    else
    {
        /* Same lights in the same places: only the colours move. */
        for(size_t g = 0; g < lights.groups.size(); ++g)
        {
            const std::vector<const LEDPosition3D*>& src = lights.groups[g].leds;
            std::vector<LEDPosition3D>& dst = copies_[g]->led_positions;
            const size_t n = std::min(src.size(), dst.size());
            for(size_t i = 0; i < n; ++i)
            {
                dst[i].preview_color = Action::device_view::ShownColor(*src[i]);
            }
        }
    }
    viewport_->UpdateColors();
}

/* ---------------------------------------------------------------- window -- */

ActionDeviceViewWindow::ActionDeviceViewWindow(const Action::Target& target,
                                               const QString& label,
                                               std::vector<std::unique_ptr<ControllerTransform>>* transforms,
                                               ZoneManager3D* zone_manager,
                                               QWidget* parent)
    : QWidget(parent, Qt::Tool | Qt::WindowTitleHint | Qt::WindowCloseButtonHint | Qt::WindowMinMaxButtonsHint)
{
    setAttribute(Qt::WA_DeleteOnClose, true);
    setWindowTitle(QStringLiteral("%1 — lights").arg(label));
    resize(640, 360);

    flat_ = new ActionDeviceMatrixView(target, transforms, zone_manager);
    three_d_ = new ActionDevice3DView(target, transforms, zone_manager);
    stack_ = new QStackedWidget();
    stack_->addWidget(flat_);
    stack_->addWidget(three_d_);

    flat_button_ = new QToolButton();
    flat_button_->setText(tr("Flat"));
    flat_button_->setCheckable(true);
    flat_button_->setChecked(true);
    flat_button_->setToolTip(tr("The device's own LED layout, as OpenRGB shows it (keyboards as their key grid)"));
    three_d_button_ = new QToolButton();
    three_d_button_->setText(tr("3D"));
    three_d_button_->setCheckable(true);
    three_d_button_->setToolTip(tr("Just this row's lights in 3D: right-drag to orbit, left-drag to pan, wheel to zoom"));
    frame_button_ = new QToolButton();
    frame_button_->setText(tr("Frame"));
    frame_button_->setToolTip(tr("Point the 3D camera back at the lights"));
    frame_button_->setVisible(false);
    info_ = new QLabel();
    info_->setForegroundRole(QPalette::PlaceholderText);

    auto* bar = new QHBoxLayout();
    bar->setContentsMargins(8, 6, 8, 4);
    bar->setSpacing(4);
    bar->addWidget(flat_button_);
    bar->addWidget(three_d_button_);
    bar->addWidget(frame_button_);
    bar->addSpacing(10);
    bar->addWidget(info_, 1);
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->addLayout(bar);
    root->addWidget(stack_, 1);

    connect(flat_button_, &QToolButton::clicked, this, [this]() { setThreeD(false); });
    connect(three_d_button_, &QToolButton::clicked, this, [this]() { setThreeD(true); });
    connect(frame_button_, &QToolButton::clicked, this, [this]() { three_d_->frameCamera(); });

    timer_ = new QTimer(this);
    timer_->setTimerType(Qt::PreciseTimer);
    timer_->setInterval(33);
    connect(timer_, &QTimer::timeout, this, &ActionDeviceViewWindow::tick);
}

void ActionDeviceViewWindow::setThreeD(bool on)
{
    flat_button_->setChecked(!on);
    three_d_button_->setChecked(on);
    frame_button_->setVisible(on);
    stack_->setCurrentWidget(on ? static_cast<QWidget*>(three_d_) : static_cast<QWidget*>(flat_));
    three_d_->setActive(on && isVisible());
    tick();
}

void ActionDeviceViewWindow::tick()
{
    const bool three = stack_->currentWidget() == three_d_;
    int n = 0;
    if(three)
    {
        three_d_->refresh();
        n = three_d_->lightCount();
    }
    else
    {
        flat_->refresh();
        n = flat_->lightCount();
    }
    if(n == 0)
    {
        info_->setText(tr("No lights for this row in the current layout"));
    }
    else
    {
        info_->setText(three ? tr("%1 lights · right-drag orbit · left-drag pan · wheel zoom").arg(n)
                             : tr("%1 lights · hover a light for its name and colour").arg(n));
    }
}

void ActionDeviceViewWindow::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    three_d_->setActive(stack_->currentWidget() == three_d_);
    timer_->start();
}

void ActionDeviceViewWindow::hideEvent(QHideEvent* event)
{
    timer_->stop();
    three_d_->setActive(false);
    QWidget::hideEvent(event);
}
