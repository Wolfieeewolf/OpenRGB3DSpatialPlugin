// SPDX-License-Identifier: GPL-2.0-only

#include "GameWrapperCaptureEffect3D.h"
#include "Game/GameLightingBridge.h"
#include "Game/GameLightingFormat.h"
#include "EffectUiRows.h"

#include <QAction>
#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

REGISTER_EFFECT_3D(GameWrapperCaptureEffect3D);

/** Live Chroma channel matrix preview (not a full OpenRGB device view). */
class ChromaGridPreview : public QWidget
{
public:
    explicit ChromaGridPreview(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setToolTip(QStringLiteral("Live Chroma LED grid for this game channel (mouse / keyboard / menu)."));
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setMinimumHeight(28);
        setMaximumHeight(48);
        setFixedHeight(36);
    }

    void setFromSample(const GameLightingBridge::DeviceSample& sample)
    {
        const int rows = sample.active && sample.has_grid ? (int)sample.grid_rows : (sample.active ? 1 : 0);
        const int cols = sample.active && sample.has_grid ? (int)sample.grid_cols : (sample.active ? 1 : 0);
        const int cells = std::max(0, rows * cols);
        bool changed = (rows != rows_) || (cols != cols_) || (sample.active != active_);
        if((int)rgb_.size() != cells * 3)
        {
            rgb_.assign((size_t)std::max(0, cells * 3), 0);
            changed = true;
        }
        if(cells > 0)
        {
            if(sample.has_grid)
            {
                const size_t nbytes = (size_t)cells * 3u;
                if(std::memcmp(rgb_.data(), sample.grid_rgb.data(), nbytes) != 0)
                {
                    std::memcpy(rgb_.data(), sample.grid_rgb.data(), nbytes);
                    changed = true;
                }
            }
            else
            {
                const std::uint8_t flat[3] = {(std::uint8_t)std::lround(std::clamp(sample.r, 0.0f, 1.0f) * 255.0f),
                                             (std::uint8_t)std::lround(std::clamp(sample.g, 0.0f, 1.0f) * 255.0f),
                                             (std::uint8_t)std::lround(std::clamp(sample.b, 0.0f, 1.0f) * 255.0f)};
                if(rgb_[0] != flat[0] || rgb_[1] != flat[1] || rgb_[2] != flat[2])
                {
                    rgb_[0] = flat[0];
                    rgb_[1] = flat[1];
                    rgb_[2] = flat[2];
                    changed = true;
                }
            }
        }
        rows_ = rows;
        cols_ = cols;
        active_ = sample.active;
        if(changed)
        {
            update();
        }
    }

    QSize sizeHint() const override
    {
        return QSize(160, 36);
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.fillRect(rect(), QColor(18, 18, 18));
        if(!active_ || rows_ <= 0 || cols_ <= 0 || rgb_.empty())
        {
            p.setPen(QColor(70, 70, 70));
            p.drawText(rect(), Qt::AlignCenter, QStringLiteral("—"));
            return;
        }

        const int gap = 1;
        const float cell_w = std::max(1.0f, (float)(width() - gap * (cols_ + 1)) / (float)cols_);
        const float cell_h = std::max(1.0f, (float)(height() - gap * (rows_ + 1)) / (float)rows_);
        const float cell = std::min(cell_w, cell_h);
        const float grid_w = cell * (float)cols_ + (float)gap * (float)(cols_ + 1);
        const float grid_h = cell * (float)rows_ + (float)gap * (float)(rows_ + 1);
        const float ox = std::max(0.0f, ((float)width() - grid_w) * 0.5f);
        const float oy = std::max(0.0f, ((float)height() - grid_h) * 0.5f);

        for(int row = 0; row < rows_; ++row)
        {
            for(int col = 0; col < cols_; ++col)
            {
                const int idx = (row * cols_ + col) * 3;
                if(idx + 2 >= (int)rgb_.size())
                {
                    continue;
                }
                const QColor c(rgb_[(size_t)idx], rgb_[(size_t)idx + 1], rgb_[(size_t)idx + 2]);
                const QRectF r(ox + (float)gap + (float)col * (cell + (float)gap),
                               oy + (float)gap + (float)row * (cell + (float)gap),
                               cell,
                               cell);
                p.fillRect(r, c);
            }
        }
    }

private:
    int rows_ = 0;
    int cols_ = 0;
    bool active_ = false;
    std::vector<std::uint8_t> rgb_;
};

namespace
{
struct DestInfo
{
    GameWrapperCapture::TargetDevice target;
    const char* label;
    const char* short_name;
};

const DestInfo kDestinations[] = {
    {GameWrapperCapture::TargetDevice::Keyboard, "Keyboard", "keyboard"},
    {GameWrapperCapture::TargetDevice::Mouse, "Mouse", "mouse"},
    {GameWrapperCapture::TargetDevice::Mousepad, "Mousepad", "mousepad"},
    {GameWrapperCapture::TargetDevice::Headset, "Headset", "headset"},
    {GameWrapperCapture::TargetDevice::Keypad, "Keypad", "keypad"},
    {GameWrapperCapture::TargetDevice::RoomLights, "Room lights (all strips)", "room lights"},
    {GameWrapperCapture::TargetDevice::WholeRoom, "Whole room (any LED)", "whole room"},
    {GameWrapperCapture::TargetDevice::Desk, "Desk / mid band", "desk"},
    {GameWrapperCapture::TargetDevice::Floor, "Floor", "floor"},
    {GameWrapperCapture::TargetDevice::Ceiling, "Ceiling", "ceiling"},
    {GameWrapperCapture::TargetDevice::Left, "Left side", "left"},
    {GameWrapperCapture::TargetDevice::Right, "Right side", "right"},
};

QString TargetShortName(GameWrapperCapture::TargetDevice target)
{
    for(const DestInfo& d : kDestinations)
    {
        if(d.target == target)
        {
            return QString::fromUtf8(d.short_name);
        }
    }
    return QStringLiteral("?");
}

QString TargetsSummary(const GameWrapperCapture::DeviceMapping& map)
{
    if(!map.enabled || !map.HasAnyTarget())
    {
        return QStringLiteral("off");
    }
    QStringList parts;
    for(GameWrapperCapture::TargetDevice t : map.ListedTargets())
    {
        parts << TargetShortName(t);
    }
    return parts.join(QStringLiteral("+"));
}

nlohmann::json DeviceMapToJson(const GameWrapperCapture::DeviceMapArray& devices)
{
    nlohmann::json arr = nlohmann::json::array();
    for(std::uint8_t i = 0; i < GameLightingFormat::kDeviceCount; ++i)
    {
        nlohmann::json targets = nlohmann::json::array();
        for(GameWrapperCapture::TargetDevice t : devices[i].ListedTargets())
        {
            targets.push_back(static_cast<int>(t));
        }
        arr.push_back({
            {"enabled", devices[i].enabled},
            {"targets", targets},
        });
    }
    return arr;
}

bool IsValidDestination(int value)
{
    return value >= static_cast<int>(GameWrapperCapture::TargetDevice::Keyboard) &&
           value <= static_cast<int>(GameWrapperCapture::TargetDevice::Right);
}

void DeviceMapFromJson(const nlohmann::json& arr, GameWrapperCapture::DeviceMapArray& devices)
{
    if(!arr.is_array())
    {
        return;
    }
    for(std::uint8_t i = 0; i < GameLightingFormat::kDeviceCount && i < arr.size(); ++i)
    {
        const auto& d = arr[i];
        if(!d.is_object())
        {
            continue;
        }
        devices[i].enabled = d.value("enabled", devices[i].enabled);
        devices[i].ClearTargets();
        if(!d.contains("targets") || !d["targets"].is_array())
        {
            continue;
        }
        for(const auto& t : d["targets"])
        {
            if(!t.is_number_integer())
            {
                continue;
            }
            const int v = t.get<int>();
            if(IsValidDestination(v))
            {
                devices[i].SetTarget(static_cast<GameWrapperCapture::TargetDevice>(v), true);
            }
        }
    }
}

QString DescribeHue(float r, float g, float b)
{
    const float mx = std::max({r, g, b});
    if(mx < 0.04f)
    {
        return QStringLiteral("black");
    }
    const float mn = std::min({r, g, b});
    const float sat = (mx > 1e-4f) ? ((mx - mn) / mx) : 0.0f;
    if(sat < 0.18f)
    {
        if(mx > 0.85f)
        {
            return QStringLiteral("white");
        }
        if(mx > 0.45f)
        {
            return QStringLiteral("grey");
        }
        return QStringLiteral("dim");
    }
    if(r >= g && r >= b)
    {
        if(g > 0.45f * r && b < 0.35f * r)
        {
            return QStringLiteral("orange");
        }
        if(g > 0.55f * r)
        {
            return QStringLiteral("yellow");
        }
        return QStringLiteral("red");
    }
    if(g >= r && g >= b)
    {
        if(b > 0.45f * g && r < 0.35f * g)
        {
            return QStringLiteral("cyan");
        }
        return QStringLiteral("green");
    }
    if(r > 0.45f * b && g < 0.35f * b)
    {
        return QStringLiteral("magenta");
    }
    return QStringLiteral("blue");
}

struct ChannelLook
{
    QString label;
    QColor swatch;
    int lit = 0;
    float mean_r = 0;
    float mean_g = 0;
    float mean_b = 0;
    float variance = 0;
};

ChannelLook AnalyzeChannel(const GameLightingBridge::DeviceSample& sample)
{
    ChannelLook look;
    if(!sample.active)
    {
        look.label = QStringLiteral("idle");
        look.swatch = QColor(20, 20, 20);
        return look;
    }

    double ar = 0, ag = 0, ab = 0;
    int lit = 0;
    const int rows = sample.has_grid ? (int)sample.grid_rows : 1;
    const int cols = sample.has_grid ? (int)sample.grid_cols : 1;
    const int cells = std::max(1, rows * cols);

    auto accumulate = [&](float r, float g, float b) {
        if(r + g + b < 0.02f)
        {
            return;
        }
        ar += r;
        ag += g;
        ab += b;
        ++lit;
    };

    if(sample.has_grid && rows > 0 && cols > 0)
    {
        for(int i = 0; i < cells; ++i)
        {
            const float r = sample.grid_rgb[(size_t)i * 3u + 0] / 255.0f;
            const float g = sample.grid_rgb[(size_t)i * 3u + 1] / 255.0f;
            const float b = sample.grid_rgb[(size_t)i * 3u + 2] / 255.0f;
            accumulate(r, g, b);
        }
    }
    else
    {
        accumulate(sample.r, sample.g, sample.b);
    }

    look.lit = lit;
    if(lit <= 0)
    {
        look.label = QStringLiteral("off / black");
        look.swatch = QColor(12, 12, 12);
        return look;
    }

    look.mean_r = (float)(ar / lit);
    look.mean_g = (float)(ag / lit);
    look.mean_b = (float)(ab / lit);
    look.swatch = QColor((int)std::lround(look.mean_r * 255.0f),
                         (int)std::lround(look.mean_g * 255.0f),
                         (int)std::lround(look.mean_b * 255.0f));

    if(sample.has_grid && lit > 1)
    {
        double vr = 0, vg = 0, vb = 0;
        for(int i = 0; i < cells; ++i)
        {
            const float r = sample.grid_rgb[(size_t)i * 3u + 0] / 255.0f;
            const float g = sample.grid_rgb[(size_t)i * 3u + 1] / 255.0f;
            const float b = sample.grid_rgb[(size_t)i * 3u + 2] / 255.0f;
            if(r + g + b < 0.02f)
            {
                continue;
            }
            vr += (r - look.mean_r) * (r - look.mean_r);
            vg += (g - look.mean_g) * (g - look.mean_g);
            vb += (b - look.mean_b) * (b - look.mean_b);
        }
        look.variance = (float)std::sqrt((vr + vg + vb) / (double)lit);
    }

    const QString hue = DescribeHue(look.mean_r, look.mean_g, look.mean_b);
    const bool big_grid = sample.has_grid && cells >= 20;
    if(big_grid && look.variance > 0.12f)
    {
        look.label = QStringLiteral("anim/%1").arg(hue);
    }
    else if(big_grid)
    {
        look.label = QStringLiteral("wash/%1").arg(hue);
    }
    else if(sample.has_grid && cells > 1)
    {
        look.label = QStringLiteral("strip/%1").arg(hue);
    }
    else
    {
        look.label = QStringLiteral("solid/%1").arg(hue);
    }
    return look;
}

} // namespace

GameWrapperCaptureEffect3D::GameWrapperCaptureEffect3D(QWidget* parent) : SpatialEffect3D(parent)
{
    effect_intensity = 100;
    effect_sharpness = 100;
}

EffectInfo3D GameWrapperCaptureEffect3D::GetEffectInfo() const
{
    EffectInfo3D info{};
    info.effect_name = "Game wrapper capture";
    info.effect_description =
        "Chroma SDK divert into OpenRGB (UDP :9877). Per-game profiles; multi-destination remap.";
    info.category = "Game";
    info.effect_type = SPATIAL_EFFECT_FOLDER_VOLUME;
    info.is_reversible = false;
    info.supports_random = false;
    info.max_speed = 200;
    info.min_speed = 0;
    info.user_colors = 0;
    info.has_custom_settings = true;
    info.needs_3d_origin = true;
    info.needs_direction = false;
    info.needs_frequency = false;
    info.use_size_parameter = false;
    info.show_speed_control = false;
    info.show_brightness_control = true;
    info.show_frequency_control = false;
    info.show_detail_control = false;
    info.show_size_control = false;
    info.show_scale_control = false;
    info.show_fps_control = false;
    info.show_color_controls = false;
    info.show_surface_control = false;
    info.show_position_offset_control = false;
    return info;
}

void GameWrapperCaptureEffect3D::ApplyControlVisibility()
{
    SpatialEffect3D::ApplyControlVisibility();
    if(speed_slider)
    {
        speed_slider->hide();
    }
    if(speed_label)
    {
        speed_label->hide();
    }
}

void GameWrapperCaptureEffect3D::SetupCustomUI(QWidget* parent)
{
    auto* root = new QWidget(parent);
    auto* layout = new QVBoxLayout(root);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    status_label_ = new QLabel(root);
    status_label_->setWordWrap(false);
    status_label_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    status_label_->setFixedHeight(status_label_->fontMetrics().height() + 2);
    status_label_->setToolTip(QStringLiteral("UDP listener status (updates live)."));
    layout->addWidget(status_label_);

    auto* hint = new QLabel(
        QStringLiteral("Six Chroma channels → map each to one or more OpenRGB destinations. "
                       "Profiles save per game exe."),
        root);
    hint->setWordWrap(true);
    hint->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    layout->addWidget(hint);

    GameWrapperCapture::Settings& s = capture_settings_;

    if(QVBoxLayout* live_body = EffectUiRows::AppendCollapsibleSectionBody(
           layout,
           QStringLiteral("Live Chroma feed"),
           QStringLiteral("Per-channel LED matrix + guessed look. Idle channels dim when unused."),
           true))
    {
        EnsureChannelHud(root, live_body);
        activity_log_ = new QPlainTextEdit(root);
        activity_log_->setReadOnly(true);
        activity_log_->setMaximumBlockCount(40);
        activity_log_->setPlaceholderText(QStringLiteral("Activity appears here when channels change…"));
        activity_log_->setFixedHeight(90);
        activity_log_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        activity_log_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
        activity_log_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        activity_log_->setLineWrapMode(QPlainTextEdit::WidgetWidth);
        activity_log_->setToolTip(
            QStringLiteral("Visual log of Chroma channel look changes (mouse / keyboard / menu)."));
        live_body->addWidget(activity_log_);
    }

    if(QVBoxLayout* profile_body = EffectUiRows::AppendCollapsibleSectionBody(
           layout,
           QStringLiteral("Game profile"),
           QStringLiteral("Per-game destination maps. Auto-switches with the live shim source."),
           true))
    {
        active_game_label_ = new QLabel(profile_body->parentWidget());
        active_game_label_->setWordWrap(false);
        active_game_label_->setText(QStringLiteral("Active: —"));
        active_game_label_->setToolTip(QStringLiteral("Live game exe driving the current map."));
        profile_body->addWidget(active_game_label_);

        auto* copy_row = new QHBoxLayout();
        copy_row->setContentsMargins(0, 0, 0, 0);
        auto* copy_caption = new QLabel(QStringLiteral("Saved games"), profile_body->parentWidget());
        copy_caption->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        profile_games_combo_ = new QComboBox(profile_body->parentWidget());
        profile_games_combo_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        profile_games_combo_->setToolTip(
            QStringLiteral("Saved per-game maps. Select one then Copy to load it onto the active game."));
        profile_games_combo_->setAccessibleName(QStringLiteral("Saved game profiles"));
        copy_row->addWidget(copy_caption);
        copy_row->addWidget(profile_games_combo_, 1);
        profile_body->addLayout(copy_row);

        auto* btn_row = new QHBoxLayout();
        btn_row->setContentsMargins(0, 0, 0, 0);
        auto* save_btn = new QPushButton(QStringLiteral("Save"), profile_body->parentWidget());
        save_btn->setToolTip(QStringLiteral("Save current map under the active game (also Ctrl+S focus)."));
        save_btn->setAccessibleName(QStringLiteral("Save game profile"));
        auto* copy_btn = new QPushButton(QStringLiteral("Copy"), profile_body->parentWidget());
        copy_btn->setToolTip(QStringLiteral("Copy the selected saved game map onto the active game."));
        copy_btn->setAccessibleName(QStringLiteral("Copy profile onto active game"));
        auto* reset_btn = new QPushButton(QStringLiteral("Reset"), profile_body->parentWidget());
        reset_btn->setToolTip(QStringLiteral("Reset active game to Keyboard → whole room."));
        reset_btn->setAccessibleName(QStringLiteral("Reset game profile"));
        auto* template_btn = new QToolButton(profile_body->parentWidget());
        template_btn->setText(QStringLiteral("Template"));
        template_btn->setPopupMode(QToolButton::InstantPopup);
        template_btn->setToolTip(QStringLiteral("Apply a starter template to the active game map."));
        template_btn->setAccessibleName(QStringLiteral("Apply profile template"));
        auto* template_menu = new QMenu(template_btn);
        template_menu->addAction(QStringLiteral("Keyboard → whole room"), this, [this]() {
            GameWrapperCapture::ApplyPresetDefault(capture_settings_);
            RefreshProfileGamesCombo();
            RefreshMapRowWidgets();
            emit ParametersChanged();
        });
        template_menu->addAction(QStringLiteral("All channels → whole room"), this, [this]() {
            GameWrapperCapture::ApplyPresetAllWholeRoom(capture_settings_);
            RefreshProfileGamesCombo();
            RefreshMapRowWidgets();
            emit ParametersChanged();
        });
        template_menu->addAction(QStringLiteral("Peripheral identity"), this, [this]() {
            GameWrapperCapture::ApplyPresetPeripheralIdentity(capture_settings_);
            RefreshProfileGamesCombo();
            RefreshMapRowWidgets();
            emit ParametersChanged();
        });
        template_btn->setMenu(template_menu);
        btn_row->addWidget(save_btn);
        btn_row->addWidget(copy_btn);
        btn_row->addWidget(reset_btn);
        btn_row->addWidget(template_btn);
        profile_body->addLayout(btn_row);

        auto persist = [this]() {
            if(!capture_settings_.active_game.empty())
            {
                capture_settings_.game_profiles[capture_settings_.active_game] = capture_settings_.devices;
            }
            RefreshProfileGamesCombo();
            emit ParametersChanged();
        };
        QObject::connect(save_btn, &QPushButton::clicked, root, persist);
        auto* save_shortcut = new QShortcut(QKeySequence::Save, root);
        save_shortcut->setContext(Qt::WidgetWithChildrenShortcut);
        QObject::connect(save_shortcut, &QShortcut::activated, root, persist);
        QObject::connect(copy_btn, &QPushButton::clicked, root, [this, persist]() {
            if(!profile_games_combo_)
            {
                return;
            }
            const QString key = profile_games_combo_->currentData().toString();
            if(key.isEmpty())
            {
                return;
            }
            const auto it = capture_settings_.game_profiles.find(key.toStdString());
            if(it == capture_settings_.game_profiles.end())
            {
                return;
            }
            capture_settings_.devices = it->second;
            RefreshMapRowWidgets();
            persist();
        });
        QObject::connect(reset_btn, &QPushButton::clicked, root, [this, persist]() {
            GameWrapperCapture::ApplyPresetDefault(capture_settings_);
            RefreshMapRowWidgets();
            persist();
        });

        auto* profile_ctx = new QMenu(root);
        profile_ctx->addAction(QStringLiteral("Save"), root, persist);
        profile_ctx->addAction(QStringLiteral("Copy from selected"), copy_btn, &QPushButton::click);
        profile_ctx->addAction(QStringLiteral("Reset"), reset_btn, &QPushButton::click);
        root->setContextMenuPolicy(Qt::CustomContextMenu);
        QObject::connect(root, &QWidget::customContextMenuRequested, root, [root, profile_ctx](const QPoint& p) {
            profile_ctx->popup(root->mapToGlobal(p));
        });

        RefreshProfileGamesCombo();
    }

    if(QVBoxLayout* map_body = EffectUiRows::AppendCollapsibleSectionBody(
           layout,
           QStringLiteral("Game device → destinations"),
           QStringLiteral("Multi-select: one Chroma channel can light headset + ceiling + …"),
           true))
    {
        auto* max_blend = new QCheckBox(QStringLiteral("Max-blend overlapping destinations"), root);
        max_blend->setChecked(s.max_blend);
        max_blend->setToolTip(QStringLiteral("When two channels hit the same LED, keep the brighter colour."));
        map_body->addWidget(max_blend);
        QObject::connect(max_blend, &QCheckBox::toggled, this, [this, &s](bool on) {
            s.max_blend = on;
            if(!s.active_game.empty())
            {
                s.game_profiles[s.active_game] = s.devices;
            }
            emit ParametersChanged();
        });

        for(std::uint8_t i = 0; i < GameLightingFormat::kDeviceCount; ++i)
        {
            const auto id = static_cast<GameLightingFormat::DeviceId>(i);
            GameWrapperCapture::DeviceMapping& m = s.devices[i];
            ChannelMapRow& row = channel_map_[i];

            auto* row_w = new QWidget(root);
            auto* row_l = new QVBoxLayout(row_w);
            row_l->setContentsMargins(0, 2, 0, 2);
            row_l->setSpacing(2);

            row.enable = new QCheckBox(QString::fromUtf8(GameLightingFormat::DeviceName(id)), row_w);
            row.enable->setChecked(m.enabled);
            row.enable->setAccessibleName(
                QStringLiteral("Enable game %1 channel").arg(QString::fromUtf8(GameLightingFormat::DeviceName(id))));
            row.enable->setToolTip(QStringLiteral("Include this Chroma channel from the game."));
            row_l->addWidget(row.enable);
            QObject::connect(row.enable, &QCheckBox::toggled, root, [this, &s, &m, i](bool on) {
                m.enabled = on;
                if(!s.active_game.empty())
                {
                    s.game_profiles[s.active_game] = s.devices;
                }
                RefreshDestButtonFor(i);
                emit ParametersChanged();
            });

            auto* dest_row = new QHBoxLayout();
            dest_row->setContentsMargins(0, 0, 0, 0);
            auto* dest_caption = new QLabel(QStringLiteral("  → play on"), row_w);
            dest_caption->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            row.dest_btn = new QToolButton(row_w);
            row.dest_btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            row.dest_btn->setPopupMode(QToolButton::InstantPopup);
            row.dest_btn->setToolButtonStyle(Qt::ToolButtonTextOnly);
            row.dest_btn->setAccessibleName(
                QStringLiteral("%1 destinations").arg(QString::fromUtf8(GameLightingFormat::DeviceName(id))));
            row.dest_btn->setMenu(new QMenu(row.dest_btn));
            dest_row->addWidget(dest_caption);
            dest_row->addWidget(row.dest_btn, 1);
            row_l->addLayout(dest_row);

            auto* map_ctx = new QMenu(row_w);
            map_ctx->addAction(QStringLiteral("Toggle enable"), row.enable, &QCheckBox::click);
            map_ctx->addAction(QStringLiteral("Edit destinations"), this, [btn = row.dest_btn]() {
                if(btn)
                {
                    btn->showMenu();
                }
            });
            map_ctx->addAction(QStringLiteral("Clear destinations"), this, [this, i]() {
                capture_settings_.devices[i].ClearTargets();
                if(!capture_settings_.active_game.empty())
                {
                    capture_settings_.game_profiles[capture_settings_.active_game] = capture_settings_.devices;
                }
                RebuildDestMenu(i);
                emit ParametersChanged();
            });
            row_w->setContextMenuPolicy(Qt::CustomContextMenu);
            QObject::connect(row_w, &QWidget::customContextMenuRequested, row_w, [row_w, map_ctx](const QPoint& p) {
                map_ctx->popup(row_w->mapToGlobal(p));
            });

            map_body->addWidget(row_w);
            RebuildDestMenu(i);
        }
    }

    status_timer_ = new QTimer(root);
    status_timer_->setInterval(100);
    connect(status_timer_, &QTimer::timeout, this, &GameWrapperCaptureEffect3D::RefreshStatusLabel);
    status_timer_->start();
    RefreshStatusLabel();

    AddWidgetToParent(root, parent);
}

void GameWrapperCaptureEffect3D::RefreshDestButtonFor(std::uint8_t channel)
{
    if(channel >= GameLightingFormat::kDeviceCount)
    {
        return;
    }
    QToolButton* btn = channel_map_[channel].dest_btn;
    if(!btn)
    {
        return;
    }
    const GameWrapperCapture::DeviceMapping& map = capture_settings_.devices[channel];
    const QString summary = TargetsSummary(map);
    btn->setText(summary);
    btn->setToolTip(
        QStringLiteral("Play this Chroma channel on one or more destinations (check all that apply).\n"
                       "Current: %1\nMouse: open menu · Keyboard: Space/Enter · Menu: right-click")
            .arg(summary));
}

void GameWrapperCaptureEffect3D::RebuildDestMenu(std::uint8_t channel)
{
    if(channel >= GameLightingFormat::kDeviceCount)
    {
        return;
    }
    ChannelMapRow& row = channel_map_[channel];
    if(!row.dest_btn)
    {
        return;
    }
    GameWrapperCapture::DeviceMapping& m = capture_settings_.devices[channel];
    QMenu* menu = row.dest_btn->menu();
    if(!menu)
    {
        menu = new QMenu(row.dest_btn);
        row.dest_btn->setMenu(menu);
    }
    menu->clear();
    auto* clear_act = menu->addAction(QStringLiteral("Clear all"));
    QObject::connect(clear_act, &QAction::triggered, this, [this, channel]() {
        capture_settings_.devices[channel].ClearTargets();
        if(!capture_settings_.active_game.empty())
        {
            capture_settings_.game_profiles[capture_settings_.active_game] = capture_settings_.devices;
        }
        RebuildDestMenu(channel);
        emit ParametersChanged();
    });
    menu->addSeparator();
    for(const DestInfo& d : kDestinations)
    {
        QAction* act = menu->addAction(QString::fromUtf8(d.label));
        act->setCheckable(true);
        act->setChecked(m.HasTarget(d.target));
        const GameWrapperCapture::TargetDevice dest = d.target;
        QObject::connect(act, &QAction::toggled, this, [this, channel, dest](bool on) {
            capture_settings_.devices[channel].SetTarget(dest, on);
            if(!capture_settings_.active_game.empty())
            {
                capture_settings_.game_profiles[capture_settings_.active_game] = capture_settings_.devices;
            }
            RefreshDestButtonFor(channel);
            emit ParametersChanged();
        });
    }
    RefreshDestButtonFor(channel);
}

void GameWrapperCaptureEffect3D::RefreshProfileGamesCombo()
{
    if(active_game_label_)
    {
        const QString game = capture_settings_.active_game.empty()
                                 ? QStringLiteral("—")
                                 : QString::fromStdString(capture_settings_.active_game);
        active_game_label_->setText(QStringLiteral("Active: %1").arg(game));
    }
    if(!profile_games_combo_)
    {
        return;
    }
    const QString previous = profile_games_combo_->currentData().toString();
    QSignalBlocker block(profile_games_combo_);
    profile_games_combo_->clear();
    for(const auto& kv : capture_settings_.game_profiles)
    {
        profile_games_combo_->addItem(QString::fromStdString(kv.first), QString::fromStdString(kv.first));
    }
    int idx = profile_games_combo_->findData(previous);
    if(idx < 0 && !capture_settings_.active_game.empty())
    {
        idx = profile_games_combo_->findData(QString::fromStdString(capture_settings_.active_game));
    }
    if(idx >= 0)
    {
        profile_games_combo_->setCurrentIndex(idx);
    }
}

void GameWrapperCaptureEffect3D::RefreshMapRowWidgets()
{
    for(std::uint8_t i = 0; i < GameLightingFormat::kDeviceCount; ++i)
    {
        ChannelMapRow& row = channel_map_[i];
        if(row.enable)
        {
            QSignalBlocker block(row.enable);
            row.enable->setChecked(capture_settings_.devices[i].enabled);
        }
        RebuildDestMenu(i);
    }
}

void GameWrapperCaptureEffect3D::EnsureChannelHud(QWidget* root, QVBoxLayout* body)
{
    if(!root || !body)
    {
        return;
    }
    for(std::uint8_t i = 0; i < GameLightingFormat::kDeviceCount; ++i)
    {
        ChannelHud& hud = channel_hud_[i];
        if(hud.row)
        {
            continue;
        }
        hud.row = new QWidget(root);
        hud.row->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        /* Fixed height so the Live feed does not jump when labels change. */
        constexpr int kHeadH = 18;
        constexpr int kMatrixH = 36;
        constexpr int kRowH = kHeadH + kMatrixH + 8;
        hud.row->setFixedHeight(kRowH);
        auto* col = new QVBoxLayout(hud.row);
        col->setContentsMargins(0, 2, 0, 2);
        col->setSpacing(2);

        auto* head = new QWidget(hud.row);
        head->setFixedHeight(kHeadH);
        head->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        auto* head_layout = new QHBoxLayout(head);
        head_layout->setContentsMargins(0, 0, 0, 0);
        head_layout->setSpacing(6);

        hud.swatch = new QLabel(head);
        hud.swatch->setFixedSize(14, 14);
        hud.swatch->setToolTip(QStringLiteral("Average colour for this Chroma channel"));
        hud.swatch->setStyleSheet(QStringLiteral("background:#111; border:1px solid #666;"));
        head_layout->addWidget(hud.swatch, 0, Qt::AlignVCenter);

        hud.text = new QLabel(head);
        hud.text->setWordWrap(false);
        hud.text->setFixedHeight(kHeadH);
        hud.text->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        hud.text->setTextInteractionFlags(Qt::TextSelectableByMouse);
        hud.text->setToolTip(QStringLiteral("Channel look + destination (single line; full text selectable)."));
        head_layout->addWidget(hud.text, 1);
        col->addWidget(head);

        hud.matrix = new ChromaGridPreview(hud.row);
        hud.matrix->setFixedHeight(kMatrixH);
        col->addWidget(hud.matrix);

        body->addWidget(hud.row);
    }
}

void GameWrapperCaptureEffect3D::RefreshStatusLabel()
{
    if(!status_label_)
    {
        return;
    }

    unsigned int total = 0, valid = 0, err = 0;
    std::string source;
    GameLightingBridge::GetStats(total, valid, err, source);

    const bool connected = GameLightingBridge::IsConnected();
    const auto snap = GameLightingBridge::GetSnapshot();
    const std::string prev_game = capture_settings_.active_game;
    GameWrapperCapture::SyncActiveGameProfile(capture_settings_,
                                              snap.source.empty() ? source : snap.source,
                                              &snap);
    if(capture_settings_.active_game != prev_game)
    {
        RefreshProfileGamesCombo();
        RefreshMapRowWidgets();
    }

    const QString game = capture_settings_.active_game.empty()
                             ? QStringLiteral("—")
                             : QString::fromStdString(capture_settings_.active_game);

    {
        const QString status = QStringLiteral("UDP :9877 | %1 | %2 | %3 ok / %4 err")
                                   .arg(connected ? QStringLiteral("live") : QStringLiteral("wait"))
                                   .arg(game)
                                   .arg(valid)
                                   .arg(err);
        if(status_label_->text() != status)
        {
            status_label_->setText(status);
            status_label_->setToolTip(status);
        }
    }

    if(activity_log_ && capture_settings_.active_game != last_activity_game_)
    {
        last_activity_game_ = capture_settings_.active_game;
        activity_log_->appendPlainText(QStringLiteral("— game %1 —").arg(game));
        for(auto& hud : channel_hud_)
        {
            hud.last_sig.clear();
            hud.last_line.clear();
            hud.anim_latch = false;
        }
    }

    for(std::uint8_t i = 0; i < GameLightingFormat::kDeviceCount; ++i)
    {
        ChannelHud& hud = channel_hud_[i];
        if(!hud.text || !hud.swatch)
        {
            continue;
        }

        const auto id = static_cast<GameLightingFormat::DeviceId>(i);
        const QString name = QString::fromUtf8(GameLightingFormat::DeviceName(id));
        const GameWrapperCapture::DeviceMapping& map = capture_settings_.devices[i];
        const GameLightingBridge::DeviceSample& sample = snap.devices[i];
        ChannelLook look = AnalyzeChannel(sample);

        if(hud.row)
        {
            hud.row->setEnabled(sample.active);
        }
        if(channel_map_[i].enable)
        {
            const QString base = QString::fromUtf8(GameLightingFormat::DeviceName(id));
            channel_map_[i].enable->setText(sample.active ? base
                                                          : QStringLiteral("%1 (idle)").arg(base));
        }

        /* Hysteresis so fire flicker does not flip anim/wash every tick. */
        const int cells = sample.has_grid ? (int)sample.grid_rows * (int)sample.grid_cols : 0;
        if(sample.active && sample.has_grid && cells >= 20)
        {
            if(look.variance > 0.16f)
            {
                hud.anim_latch = true;
            }
            else if(look.variance < 0.08f)
            {
                hud.anim_latch = false;
            }
            const QString hue = DescribeHue(look.mean_r, look.mean_g, look.mean_b);
            look.label = hud.anim_latch ? QStringLiteral("anim/%1").arg(hue)
                                        : QStringLiteral("wash/%1").arg(hue);
        }

        QString grid;
        if(sample.active && sample.has_grid)
        {
            grid = QStringLiteral("%1×%2").arg(sample.grid_rows).arg(sample.grid_cols);
        }
        else if(sample.active)
        {
            grid = QStringLiteral("1×1");
        }
        else
        {
            grid = QStringLiteral("—");
        }

        const QString dest = TargetsSummary(map);
        const QString line = sample.active ? QStringLiteral("%1 %2 %3 → %4").arg(name, grid, look.label, dest)
                                           : QStringLiteral("%1 idle → %2").arg(name, dest);
        if(line != hud.last_line)
        {
            hud.last_line = line;
            hud.text->setText(line);
            hud.text->setToolTip(
                QStringLiteral("%1\nlit %2  var %3")
                    .arg(line)
                    .arg(look.lit)
                    .arg(look.variance, 0, 'f', 3));
        }

        const QString css =
            QStringLiteral("background:%1; border:1px solid #888;").arg(look.swatch.name());
        if(css != hud.last_swatch_css)
        {
            hud.last_swatch_css = css;
            hud.swatch->setStyleSheet(css);
        }
        if(hud.matrix)
        {
            hud.matrix->setFromSample(sample);
        }

        /* Bucket RGB so fire flicker does not spam the activity log. */
        const QString sig =
            QStringLiteral("%1|%2|%3/%4/%5")
                .arg(look.label, grid)
                .arg(look.swatch.red() / 40)
                .arg(look.swatch.green() / 40)
                .arg(look.swatch.blue() / 40);
        if(activity_log_ && sig != hud.last_sig)
        {
            const bool first = hud.last_sig.isEmpty();
            hud.last_sig = sig;
            if(!first || sample.active)
            {
                activity_log_->appendPlainText(
                    QStringLiteral("%1: %2 → %3").arg(name, look.label, dest));
            }
        }
    }
}

nlohmann::json GameWrapperCaptureEffect3D::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    auto& g = j["game_wrapper_capture"];
    g["max_blend"] = capture_settings_.max_blend;
    g["active_game"] = capture_settings_.active_game;
    g["devices"] = DeviceMapToJson(capture_settings_.devices);

    nlohmann::json profiles = nlohmann::json::object();
    for(const auto& kv : capture_settings_.game_profiles)
    {
        profiles[kv.first] = DeviceMapToJson(kv.second);
    }
    if(!capture_settings_.active_game.empty())
    {
        profiles[capture_settings_.active_game] = DeviceMapToJson(capture_settings_.devices);
    }
    g["game_profiles"] = profiles;
    return j;
}

void GameWrapperCaptureEffect3D::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(!settings.contains("game_wrapper_capture") || !settings["game_wrapper_capture"].is_object())
    {
        return;
    }
    const nlohmann::json& g = settings["game_wrapper_capture"];
    capture_settings_.max_blend = g.value("max_blend", capture_settings_.max_blend);
    capture_settings_.active_game =
        GameWrapperCapture::NormalizeGameKey(g.value("active_game", capture_settings_.active_game));

    if(g.contains("devices"))
    {
        DeviceMapFromJson(g["devices"], capture_settings_.devices);
    }

    capture_settings_.game_profiles.clear();
    if(g.contains("game_profiles") && g["game_profiles"].is_object())
    {
        constexpr std::size_t kMaxProfiles = 64;
        for(auto it = g["game_profiles"].begin();
            it != g["game_profiles"].end() && capture_settings_.game_profiles.size() < kMaxProfiles;
            ++it)
        {
            const std::string key = GameWrapperCapture::NormalizeGameKey(it.key());
            if(key.empty() || key.size() > 96)
            {
                continue;
            }
            GameWrapperCapture::DeviceMapArray map{};
            GameWrapperCapture::Settings::ApplyIdentityMap(map);
            DeviceMapFromJson(it.value(), map);
            capture_settings_.game_profiles[key] = map;
        }
    }
    if(!capture_settings_.active_game.empty())
    {
        const auto it = capture_settings_.game_profiles.find(capture_settings_.active_game);
        if(it != capture_settings_.game_profiles.end())
        {
            capture_settings_.devices = it->second;
        }
        else
        {
            capture_settings_.game_profiles[capture_settings_.active_game] = capture_settings_.devices;
        }
    }
}

RGBColor GameWrapperCaptureEffect3D::CalculateColorGrid(float gx, float gy, float gz, float time, const GridContext3D& grid)
{
    (void)time;
    const GameLightingBridge::LightingSnapshot snap = GameLightingBridge::GetSnapshot();
    GameWrapperCapture::SyncActiveGameProfile(capture_settings_, snap.source, &snap);

    const Vector3D effect_origin = GetEffectOriginGrid(grid);
    return GameWrapperCapture::RenderColor(snap,
                                           gx,
                                           gy,
                                           gz,
                                           effect_origin.x,
                                           effect_origin.y,
                                           effect_origin.z,
                                           grid,
                                           capture_settings_);
}

bool GameWrapperCaptureEffect3D::IsPointOnActiveSurface(float, float, float, const GridContext3D&) const
{
    return true;
}
