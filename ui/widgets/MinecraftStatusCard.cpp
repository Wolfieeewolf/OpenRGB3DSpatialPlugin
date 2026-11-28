// SPDX-License-Identifier: GPL-2.0-or-later

#include "MinecraftStatusCard.h"
#include "Game/GameTelemetryBridge.h"
#include "Game/RoomSampleConfigPublisher.h"
#include "Game/RoomSampleFrameProtocol.h"
#include "PluginUiUtils.h"

#include <QDateTime>
#include <QFrame>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QLabel>
#include <QListWidget>
#include <QPainter>
#include <QPainterPath>
#include <QProgressBar>
#include <QShowEvent>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>

namespace
{
unsigned long long SteadyMs()
{
    return (unsigned long long)std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

/** Label + thin coloured bar + value, on one line. */
QWidget* StatRow(QWidget* parent, const QString& name, const QColor& colour, QProgressBar** bar, QLabel** value)
{
    QWidget* row = new QWidget(parent);
    QHBoxLayout* h = new QHBoxLayout(row);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(8);
    QLabel* label = new QLabel(name, row);
    label->setMinimumWidth(64);
    h->addWidget(label);
    *bar = new QProgressBar(row);
    (*bar)->setTextVisible(false);
    (*bar)->setRange(0, 1000);
    (*bar)->setFixedHeight(8);
    (*bar)->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    /* Track from the OpenRGB theme (window colour moved toward the text colour), so it fits
       light and dark themes alike. */
    const QPalette pal = parent->palette();
    const QColor track = PluginUiBlendColors(pal.color(QPalette::Window), pal.color(QPalette::WindowText), 0.18f);
    (*bar)->setStyleSheet(QStringLiteral("QProgressBar { border: none; border-radius: 4px; background: %1; }"
                                         "QProgressBar::chunk { border-radius: 4px; background: %2; }")
                              .arg(track.name(), colour.name()));
    h->addWidget(*bar, 1);
    *value = new QLabel(row);
    (*value)->setMinimumWidth(56);
    (*value)->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    h->addWidget(*value);
    return row;
}

QString Dot(bool on, const QString& text, const QString& tip)
{
    return QStringLiteral("<span title=\"%3\" style=\"color:%1\">●</span> %2")
        .arg(on ? QStringLiteral("#3fbf5f") : QStringLiteral("#7a7a7a"), text, tip);
}

QString Num(float v)
{
    return std::fabs(v - std::round(v)) < 0.05f ? QString::number((int)std::lround(v)) : QString::number(v, 'f', 1);
}
} // namespace

/**
 * Where the data goes, as a small diagram:  Minecraft -> OpenRGB -> LEDs, with the vitals link
 * (UDP) and the Room Ambilight link (shared memory) as two lines, and the LED updates after the
 * plugin. Lines in the theme's highlight colour while data flows (dots travel along them), grey
 * when idle, red when messages fail. Rates sit on the lines; the full numbers are in the tooltip.
 */
class MinecraftLinkDiagram : public QWidget
{
public:
    struct Link
    {
        bool active = false;
        bool error = false;
        QString label;
        double rate = 0.0; /* per second: dot speed */
    };
    Link vitals;
    Link room;
    Link leds;

    explicit MinecraftLinkDiagram(QWidget* parent)
        : QWidget(parent)
    {
        setMinimumHeight(64);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        anim_ = new QTimer(this);
        anim_->setInterval(33);
        connect(anim_, &QTimer::timeout, this, [this]() {
            phase_ += 0.033;
            update();
        });
    }

    QSize sizeHint() const override
    {
        return QSize(320, 64);
    }

protected:
    void showEvent(QShowEvent* e) override
    {
        QWidget::showEvent(e);
        anim_->start();
    }
    void hideEvent(QHideEvent* e) override
    {
        anim_->stop();
        QWidget::hideEvent(e);
    }

    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        const QPalette pal = palette();
        const QColor text = pal.color(QPalette::WindowText);
        const QColor node_fill = PluginUiBlendColors(pal.color(QPalette::Window), pal.color(QPalette::Button), 0.7f);
        const QColor node_line = PluginUiBlendColors(pal.color(QPalette::Window), text, 0.30f);
        const QColor on = pal.color(QPalette::Highlight);
        const QColor off = PluginUiBlendColors(pal.color(QPalette::Window), text, 0.25f);
        const QColor bad(220, 80, 70);

        QFont small = font();
        small.setPointSizeF(std::max(6.5, small.pointSizeF() * 0.82));
        const QFontMetricsF fm(font());
        const QFontMetricsF sfm(small);

        const double W = width();
        const double H = height();
        const double node_h = 22.0;
        const auto node_w = [&](const QString& s) { return fm.horizontalAdvance(s) + 18.0; };
        const QString n1 = QStringLiteral("Minecraft");
        const QString n2 = QStringLiteral("OpenRGB");
        const QString n3 = QStringLiteral("LEDs");
        const double w1 = node_w(n1);
        const double w2 = node_w(n2);
        const double w3 = node_w(n3);
        const double cy = H / 2.0;
        /* Spread: the Minecraft -> OpenRGB stretch is longer (two links and their labels). */
        const double free_space = std::max(40.0, W - w1 - w2 - w3 - 2.0);
        const double gap1 = free_space * 0.62;
        const double x1 = 1.0;
        const double x2 = x1 + w1 + gap1;
        const double x3 = W - w3 - 1.0;

        const auto draw_node = [&](double x, double w, const QString& label, bool lit) {
            QRectF r(x, cy - node_h / 2.0, w, node_h);
            QPainterPath path;
            path.addRoundedRect(r, 6, 6);
            p.fillPath(path, node_fill);
            p.setPen(QPen(lit ? on : node_line, 1.2));
            p.drawPath(path);
            p.setPen(text);
            p.setFont(font());
            p.drawText(r, Qt::AlignCenter, label);
        };

        /* A link: line + arrow head + travelling dots + label above or below. */
        const auto draw_link = [&](double ax, double bx, double y, const Link& l, bool label_above, double offset) {
            const QColor c = l.error ? bad : (l.active ? on : off);
            p.setPen(QPen(c, l.active ? 2.0 : 1.2, l.active ? Qt::SolidLine : Qt::DashLine, Qt::RoundCap));
            p.drawLine(QPointF(ax, y), QPointF(bx - 5, y));
            QPainterPath head;
            head.moveTo(bx, y);
            head.lineTo(bx - 6, y - 3.5);
            head.lineTo(bx - 6, y + 3.5);
            head.closeSubpath();
            p.fillPath(head, c);
            if(l.active && bx - ax > 20)
            {
                /* Dots: faster for higher rates, capped so they stay readable. */
                const double speed = 0.35 + std::min(1.0, l.rate / 120.0) * 0.9; /* lengths per second */
                const int dots = 3;
                p.setPen(Qt::NoPen);
                p.setBrush(c);
                for(int i = 0; i < dots; i++)
                {
                    double t = std::fmod(phase_ * speed + offset + (double)i / dots, 1.0);
                    const double x = ax + (bx - 8 - ax) * t;
                    const double fade = std::sin(t * 3.14159);
                    QColor dc = c;
                    dc.setAlphaF(0.35 + 0.65 * fade);
                    p.setBrush(dc);
                    p.drawEllipse(QPointF(x, y), 2.4, 2.4);
                }
                p.setBrush(Qt::NoBrush);
            }
            p.setFont(small);
            p.setPen(l.error ? bad : (l.active ? text : off));
            const QString s = sfm.elidedText(l.label, Qt::ElideRight, std::max(10.0, bx - ax - 4));
            const double ty = label_above ? y - 4 : y + sfm.ascent() + 3;
            p.drawText(QPointF(ax + (bx - ax - sfm.horizontalAdvance(s)) / 2.0, ty), s);
        };

        const double a1 = x1 + w1;
        const double b1 = x2;
        draw_link(a1 + 2, b1 - 2, cy - 6, vitals, true, 0.0);
        draw_link(a1 + 2, b1 - 2, cy + 6, room, false, 0.5);
        draw_link(x2 + w2 + 2, x3 - 2, cy, leds, true, 0.25);

        draw_node(x1, w1, n1, vitals.active || room.active);
        draw_node(x2, w2, n2, room.active || vitals.active);
        draw_node(x3, w3, n3, leds.active);
    }

private:
    QTimer* anim_ = nullptr;
    double phase_ = 0.0;
};

MinecraftStatusCard::MinecraftStatusCard(QWidget* parent)
    : QWidget(parent)
{
    QVBoxLayout* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(6);

    QFrame* card = new QFrame(this);
    QVBoxLayout* v = new QVBoxLayout(card);
    v->setContentsMargins(10, 8, 10, 8);
    v->setSpacing(4);

    title_ = new QLabel(card);
    QFont tf = title_->font();
    tf.setBold(true);
    tf.setPointSizeF(tf.pointSizeF() * 1.15);
    title_->setFont(tf);
    v->addWidget(title_);

    lights_ = new QLabel(card);
    lights_->setTextFormat(Qt::RichText);
    lights_->setWordWrap(true);
    v->addWidget(lights_);

    v->addWidget(StatRow(card, tr("Health"), QColor(220, 60, 60), &health_, &health_text_));
    v->addWidget(StatRow(card, tr("Hunger"), QColor(220, 150, 60), &hunger_, &hunger_text_));
    air_row_ = StatRow(card, tr("Air"), QColor(80, 160, 230), &air_, &air_text_);
    v->addWidget(air_row_);
    durability_row_ = StatRow(card, tr("Tool"), QColor(120, 200, 120), &durability_, &durability_text_);
    v->addWidget(durability_row_);

    room_ = new QLabel(card);
    room_->setWordWrap(true);
    PluginUiApplyMutedSecondaryLabel(room_);
    v->addWidget(room_);
    diagram_ = new MinecraftLinkDiagram(card);
    v->addWidget(diagram_);

    outer->addWidget(card);
    PluginUiApplyControllerCardChrome(card);
    card->setStyleSheet(card->styleSheet() + QStringLiteral(" QLabel { border: none; background: transparent; }"));

    log_ = new QListWidget(this);
    log_->setFixedHeight(96);
    log_->setSelectionMode(QAbstractItemView::NoSelection);
    log_->setFocusPolicy(Qt::NoFocus);
    log_->setToolTip(tr("What happened: the mod connecting or dropping out, damage taken, Room Ambilight "
                        "setting changes. Newest first."));
    outer->addWidget(log_);

    timer_ = new QTimer(this);
    timer_->setInterval(250);
    connect(timer_, &QTimer::timeout, this, &MinecraftStatusCard::refresh);
    refresh();
}

void MinecraftStatusCard::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    refresh();
    timer_->start();
}

void MinecraftStatusCard::hideEvent(QHideEvent* event)
{
    timer_->stop();
    QWidget::hideEvent(event);
}

void MinecraftStatusCard::log(const QString& text, bool warn)
{
    auto* item = new QListWidgetItem(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")) + QStringLiteral("  ")
                                     + text);
    if(warn)
    {
        item->setForeground(QColor(235, 110, 90));
    }
    log_->insertItem(0, item);
    while(log_->count() > 100)
    {
        delete log_->takeItem(log_->count() - 1);
    }
}

void MinecraftStatusCard::refresh()
{
    const unsigned long long now = SteadyMs();
    const GameTelemetryBridge::TelemetrySnapshot t = GameTelemetryBridge::GetTelemetrySnapshot();
    const GameTelemetryBridge::LiveRates r = GameTelemetryBridge::GetLiveRates();

    /* ---- link ---- */
    const bool telemetry_fresh = t.last_event_ms > 0 && now - t.last_event_ms < 2000;
    const bool room_fresh = r.room_last_ms > 0 && now - r.room_last_ms < 1500;
    RoomSampleFrameProtocol::ConfigHeader cfg{};
    const bool have_cfg = RoomSampleConfigPublisher::GetLastPublishedConfig(cfg);
    std::uint32_t options = 0;
    std::uint32_t led_texels = 0;
    if(have_cfg)
    {
        std::memcpy(&options, cfg.reserved + RoomSampleFrameProtocol::kReservedOptionsOffset, sizeof(options));
        std::memcpy(&led_texels, cfg.reserved + RoomSampleFrameProtocol::kReservedImportantCountOffset, sizeof(led_texels));
    }
    const bool picture = (options & RoomSampleFrameProtocol::kOptScreenCapture) != 0;

    const int link = telemetry_fresh || room_fresh ? 1 : (seen_link_ == 1 || seen_link_ == 2 ? 2 : 0);
    if(link != seen_link_)
    {
        if(link == 1)
        {
            log(tr("Minecraft mod connected"));
        }
        else if(link == 2)
        {
            log(tr("Minecraft mod stopped sending (left the world, paused or closed)"), true);
        }
        seen_link_ = link;
    }
    title_->setText(link == 1 ? tr("Minecraft — in game") : link == 2 ? tr("Minecraft — not sending")
                                                                          : tr("Minecraft — waiting for the mod"));
    lights_->setText(Dot(telemetry_fresh, tr("Mod link"), tr("Vitals and position over UDP 127.0.0.1:9876")) +
                     QStringLiteral("&nbsp;&nbsp;&nbsp;") +
                     Dot(room_fresh, tr("Room Ambilight"), tr("Room Ambilight frames from the mod")) +
                     QStringLiteral("&nbsp;&nbsp;&nbsp;") +
                     Dot(picture && room_fresh, tr("Game picture"),
                         picture ? tr("Front LEDs from the game picture (needs first person)")
                                 : tr("Off: rays only (Room Ambilight settings)")));

    /* ---- vitals ---- */
    const bool vitals = t.has_health_state && telemetry_fresh;
    const auto set_bar = [](QProgressBar* bar, QLabel* text, float v, float max, bool on) {
        const float m = std::max(max, 1e-3f);
        bar->setValue(on ? (int)std::lround(std::clamp(v / m, 0.0f, 1.0f) * 1000.0f) : 0);
        text->setText(on ? QStringLiteral("%1 / %2").arg(Num(v), Num(max)) : QStringLiteral("—"));
    };
    set_bar(health_, health_text_, t.health, t.health_max, vitals);
    set_bar(hunger_, hunger_text_, t.hunger, t.hunger_max, vitals);
    /* Air and tool only when they matter (under water, holding something that wears). */
    const bool air_used = vitals && t.air < t.air_max - 0.5f;
    air_row_->setVisible(air_used);
    set_bar(air_, air_text_, t.air, t.air_max, air_used);
    const bool tool = vitals && t.has_item_durability;
    durability_row_->setVisible(tool);
    set_bar(durability_, durability_text_, t.item_durability, t.item_durability_max, tool);

    if(t.has_damage_event && t.damage_received_ms != 0 && t.damage_received_ms != seen_damage_ms_)
    {
        if(seen_damage_ms_ != 0)
        {
            log(tr("Took %1 damage").arg(Num(t.damage_amount)), true);
        }
        seen_damage_ms_ = t.damage_received_ms;
    }

    /* ---- Room Ambilight ---- */
    if(have_cfg && (cfg.config_id != seen_room_config_ || options != seen_options_))
    {
        if(seen_options_ != 0xFFFFFFFFu)
        {
            log(tr("Room Ambilight settings sent: %1, face %2, %3 LED directions")
                    .arg(picture ? tr("picture + rays") : tr("rays only"))
                    .arg(cfg.size_x)
                    .arg(led_texels));
        }
        seen_room_config_ = cfg.config_id;
        seen_options_ = options;
    }
    if(!have_cfg)
    {
        room_->setText(tr("Room Ambilight: not in the effect stack"));
    }
    else
    {
        QString line = tr("Room Ambilight: %1 · %2 LED directions · face %3")
                           .arg(picture ? tr("game picture + rays") : tr("rays"))
                           .arg(led_texels)
                           .arg(cfg.size_x);
        if(std::fabs(cfg.heading_offset_deg) > 0.05f)
        {
            line += tr(" · facing %1°").arg(cfg.heading_offset_deg, 0, 'f', 0);
        }
        room_->setText(line);
    }

    /* ---- link diagram: rates about once a second, details in its tooltip ---- */
    unsigned int total = 0;
    unsigned int valid = 0;
    unsigned int errors = 0;
    std::string source;
    std::string type;
    GameTelemetryBridge::GetStats(total, valid, errors, source, type);
    if(rates_ms_ == 0 || now - rates_ms_ >= 1000)
    {
        if(rates_ms_ > 0 && diagram_)
        {
            const double secs = (double)(now - rates_ms_) / 1000.0;
            const double led_fps = (double)(r.led_frames - rates_led_) / secs;
            const double room_fps = (double)(r.room_frames - rates_room_) / secs;
            const double msg_ps = (double)((unsigned long long)total - rates_msgs_) / secs;
            diagram_->vitals.active = telemetry_fresh;
            diagram_->vitals.error = errors > 0 && total > 0 && errors * 10u > total; /* more than 10% failing */
            diagram_->vitals.rate = msg_ps;
            diagram_->vitals.label = telemetry_fresh ? tr("vitals %1/s").arg(msg_ps, 0, 'f', 0) : tr("vitals");
            diagram_->room.active = room_fresh;
            diagram_->room.rate = room_fps;
            diagram_->room.label = room_fresh ? (r.room_delay_ms >= 0 ? tr("room %1/s \u00B7 %2 ms").arg(room_fps, 0, 'f', 0).arg(r.room_delay_ms)
                                                                     : tr("room %1/s").arg(room_fps, 0, 'f', 0))
                                              : tr("room");
            diagram_->leds.active = led_fps > 0.5;
            diagram_->leds.rate = led_fps;
            diagram_->leds.label = led_fps > 0.5 ? tr("%1/s").arg(led_fps, 0, 'f', 0) : tr("idle");
            diagram_->update();
        }
        rates_ms_ = now;
        rates_led_ = r.led_frames;
        rates_room_ = r.room_frames;
        rates_msgs_ = total;
    }

    if(diagram_)
    {
        QString tip = tr("Vitals and pose: UDP 127.0.0.1:9876 \u2014 %1 messages, %2 valid, %3 errors").arg(total).arg(valid).arg(errors);
        if(!source.empty() || !type.empty())
        {
            tip += tr("\nLast message: %1 / %2").arg(QString::fromStdString(source), QString::fromStdString(type));
        }
        if(t.last_event_ms > 0)
        {
            tip += tr(" (%1 s ago)").arg((now - t.last_event_ms) / 1000.0, 0, 'f', 1);
        }
        tip += t.room_sample.has_frame
                   ? tr("\nRoom Ambilight: shared memory, cubemap %1\u00D7%2\u00D7%3, %4 LED texels per frame")
                         .arg(t.room_sample.size_x)
                         .arg(t.room_sample.size_y)
                         .arg(t.room_sample.size_z)
                         .arg(t.room_sample.texel_flats ? (int)t.room_sample.texel_flats->size() : 0)
                   : tr("\nRoom Ambilight: no frames yet");
        tip += tr("\nLED updates: how often the effect stack sends colours to your devices; each device then "
                  "updates at its own speed.");
        if(r.room_delay_ms >= 0)
        {
            tip += tr("\nDelay: from the mod writing the newest frame to the plugin reading it.");
        }
        diagram_->setToolTip(tip);
    }
}
