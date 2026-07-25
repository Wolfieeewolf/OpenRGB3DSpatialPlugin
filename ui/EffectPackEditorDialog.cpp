// SPDX-License-Identifier: GPL-2.0-only

#include "EffectPackEditorDialog.h"
#include "EffectPackCatalog.h"
#include "EffectPackGradientBar.h"
#include "EffectPackTimelineWidget.h"
#include "EffectPackToolBar.h"
#include "EffectPacks/EffectPackApplier.h"
#include "LEDPosition3D.h"
#include "OpenRGB3DSpatialTab.h"
#include "PluginUiUtils.h"
#include "VirtualController3D.h"
#include "EffectCollapsibleSection.h"

#include <QAbstractItemView>
#include <QAbstractSpinBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QSplitter>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <map>
#include <system_error>
#include <utility>
#include <vector>

namespace
{

QColor RgbToQColor(RGBColor c)
{
    return QColor(RGBGetRValue(c), RGBGetGValue(c), RGBGetBValue(c));
}

RGBColor QColorToRgb(const QColor& c)
{
    return ToRGBColor(c.red(), c.green(), c.blue());
}

QString ControllerLabel(const ControllerTransform* transform, int index)
{
    if(!transform)
    {
        return QStringLiteral("Controller %1").arg(index);
    }
    if(transform->virtual_controller)
    {
        return QString::fromStdString(transform->virtual_controller->GetName());
    }
    if(transform->controller)
    {
        const std::string display = transform->controller->GetDisplayName();
        if(!display.empty())
        {
            return QString::fromStdString(display);
        }
        return QString::fromStdString(transform->controller->GetName());
    }
    return QStringLiteral("Controller %1").arg(index);
}

std::string ControllerKeyName(const ControllerTransform* transform, int index)
{
    if(transform && transform->virtual_controller)
    {
        return transform->virtual_controller->GetName();
    }
    if(transform && transform->controller)
    {
        const std::string display = transform->controller->GetDisplayName();
        if(!display.empty())
        {
            return display;
        }
        return transform->controller->GetName();
    }
    return std::string("controller_") + std::to_string(index);
}

QString ZoneLabelForLed(RGBControllerInterface* rgb, unsigned int zone_idx)
{
    if(rgb && zone_idx < rgb->GetZoneCount())
    {
        QString name = QString::fromStdString(rgb->GetZoneDisplayName(zone_idx));
        if(name.isEmpty())
        {
            name = QString::fromStdString(rgb->GetZoneName(zone_idx));
        }
        if(!name.isEmpty())
        {
            return name;
        }
    }
    return QStringLiteral("Zone %1").arg(zone_idx);
}

bool TryGlobalLedIndex(RGBControllerInterface* rgb, unsigned int zone_idx, unsigned int led_idx, int* out)
{
    if(!rgb || !out || zone_idx >= rgb->GetZoneCount())
    {
        return false;
    }
    if(led_idx >= rgb->GetZoneLEDsCount(zone_idx))
    {
        return false;
    }
    const unsigned int global = rgb->GetZoneStartIndex(zone_idx) + led_idx;
    if(global >= rgb->GetLEDCount())
    {
        return false;
    }
    *out = (int)global;
    return true;
}

} // namespace

EffectPackEditorDialog::EffectPackEditorDialog(OpenRGB3DSpatialTab* tab, QWidget* parent)
    : QDialog(parent)
    , tab_(tab)
{
    setWindowTitle(QStringLiteral("Effect Pack Editor"));
    setWindowFlags(windowFlags() | Qt::Window);
    setAttribute(Qt::WA_DeleteOnClose, false);
    resize(1100, 640);
    buildUi();

    timer_ = new QTimer(this);
    timer_->setInterval(33);
    connect(timer_, &QTimer::timeout, this, &EffectPackEditorDialog::onTick);
}

EffectPackEditorDialog::~EffectPackEditorDialog()
{
    stopPreview();
}

void EffectPackEditorDialog::buildUi()
{
    auto* root = new QVBoxLayout(this);

    auto* meta_row = new QHBoxLayout();
    name_edit_ = new QLineEdit();
    duration_spin_ = new QSpinBox();
    duration_spin_->setRange(100, EffectPack::kMaxDurationMs);
    duration_spin_->setSingleStep(100);
    duration_spin_->setValue(5000);
    loop_combo_ = new QComboBox();
    loop_combo_->addItem(QStringLiteral("Once"), QStringLiteral("once"));
    loop_combo_->addItem(QStringLiteral("Forever"), QStringLiteral("forever"));
    loop_combo_->addItem(QStringLiteral("While active"), QStringLiteral("while_active"));
    meta_row->addWidget(new QLabel(QStringLiteral("Name")));
    meta_row->addWidget(name_edit_, 2);
    meta_row->addWidget(new QLabel(QStringLiteral("Duration ms")));
    meta_row->addWidget(duration_spin_);
    meta_row->addWidget(new QLabel(QStringLiteral("Loop")));
    meta_row->addWidget(loop_combo_);
    controllers_button_ = new QPushButton(QStringLiteral("Controllers…"));
    controllers_button_->setToolTip(QStringLiteral("Choose which scene controllers appear on this pack’s timeline"));
    meta_row->addWidget(controllers_button_);
    root->addLayout(meta_row);

    effect_toolbar_ = new EffectPackToolBar();
    root->addWidget(effect_toolbar_);

    auto* splitter = new QSplitter(Qt::Horizontal);

    auto* timeline_scroll = new QScrollArea();
    timeline_scroll->setWidgetResizable(true);
    timeline_scroll->setFrameShape(QFrame::NoFrame);
    timeline_ = new EffectPackTimelineWidget();
    timeline_scroll->setWidget(timeline_);
    splitter->addWidget(timeline_scroll);

    auto* props_wrap = new QWidget();
    auto* props_layout = new QVBoxLayout(props_wrap);
    props_layout->setContentsMargins(4, 4, 4, 4);

    auto* palette_row = new QHBoxLayout();
    remove_block_button_ = new QPushButton(QStringLiteral("Remove effect"));
    remove_block_button_->setToolTip(QStringLiteral("Delete selected block (Delete / Backspace)"));
    palette_row->addWidget(new QLabel(QStringLiteral("Drag effects from the toolbar · right-click timeline to add")));
    palette_row->addStretch(1);
    palette_row->addWidget(remove_block_button_);
    props_layout->addLayout(palette_row);

    auto* props_scroll = new QScrollArea();
    props_scroll->setWidgetResizable(true);
    props_scroll->setFrameShape(QFrame::NoFrame);
    auto* props_inner = new QWidget();
    auto* props_inner_layout = new QVBoxLayout(props_inner);
    props_inner_layout->setContentsMargins(0, 0, 0, 0);

    auto* effect_sec = new EffectCollapsibleSection(QStringLiteral("Effect"));
    effect_sec->setExpanded(true);
    auto* effect_form = new QFormLayout();
    type_combo_ = new QComboBox();
    for(const EffectPackCatalog::Entry& e : EffectPackCatalog::AllEntries())
    {
        type_combo_->addItem(EffectPackCatalog::MakeEffectIcon(e), QString::fromUtf8(e.name), (int)e.type);
    }
    start_spin_ = new QSpinBox();
    start_spin_->setRange(0, EffectPack::kMaxDurationMs);
    end_spin_ = new QSpinBox();
    end_spin_->setRange(1, EffectPack::kMaxDurationMs);
    effect_form->addRow(QStringLiteral("Type"), type_combo_);
    effect_form->addRow(QStringLiteral("Start ms"), start_spin_);
    effect_form->addRow(QStringLiteral("End ms"), end_spin_);
    effect_sec->bodyLayout()->addLayout(effect_form);
    props_inner_layout->addWidget(effect_sec);

    auto* color_sec = new EffectCollapsibleSection(QStringLiteral("Color"));
    color_sec->setExpanded(true);
    color_button_ = new QPushButton(QStringLiteral("Pick…"));
    color_to_button_ = new QPushButton(QStringLiteral("Pick…"));
    color_to_row_ = new QWidget();
    auto* color_to_layout = new QHBoxLayout(color_to_row_);
    color_to_layout->setContentsMargins(0, 0, 0, 0);
    color_to_layout->addWidget(new QLabel(QStringLiteral("End")));
    color_to_layout->addWidget(color_to_button_, 1);
    gradient_preset_ = new QComboBox();
    gradient_preset_->addItem(QStringLiteral("Preset…"), QString());
    gradient_preset_->addItem(QStringLiteral("Rainbow"), QStringLiteral("rainbow"));
    gradient_preset_->addItem(QStringLiteral("Red → Blue"), QStringLiteral("red_blue"));
    gradient_preset_->addItem(QStringLiteral("White → Color"), QStringLiteral("white_color"));
    gradient_bar_ = new EffectPackGradientBar();
    color_sec->bodyLayout()->addWidget(new QLabel(QStringLiteral("Primary")));
    color_sec->bodyLayout()->addWidget(color_button_);
    color_sec->bodyLayout()->addWidget(color_to_row_);
    color_sec->bodyLayout()->addWidget(gradient_preset_);
    color_sec->bodyLayout()->addWidget(new QLabel(QStringLiteral("Color gradient")));
    color_sec->bodyLayout()->addWidget(gradient_bar_);
    color_sec->bodyLayout()->addWidget(new QLabel(
        QStringLiteral("Drag stops · click bar to add · double-click recolour · right-click remove")));
    props_inner_layout->addWidget(color_sec);

    auto* bright_sec = new EffectCollapsibleSection(QStringLiteral("Brightness"));
    bright_sec->setExpanded(true);
    intensity_spin_ = new QSpinBox();
    intensity_spin_->setRange(1, 100);
    intensity_spin_->setValue(100);
    min_intensity_spin_ = new QSpinBox();
    min_intensity_spin_->setRange(0, 100);
    min_intensity_spin_->setValue(15);
    auto* bright_form = new QFormLayout();
    bright_form->addRow(QStringLiteral("Intensity %"), intensity_spin_);
    min_intensity_row_ = new QWidget();
    auto* min_row_layout = new QHBoxLayout(min_intensity_row_);
    min_row_layout->setContentsMargins(0, 0, 0, 0);
    min_row_layout->addWidget(new QLabel(QStringLiteral("Min % (floor)")));
    min_row_layout->addWidget(min_intensity_spin_, 1);
    bright_sec->bodyLayout()->addLayout(bright_form);
    bright_sec->bodyLayout()->addWidget(min_intensity_row_);
    props_inner_layout->addWidget(bright_sec);

    direction_section_ = new EffectCollapsibleSection(QStringLiteral("Direction"));
    static_cast<EffectCollapsibleSection*>(direction_section_)->setExpanded(true);
    direction_combo_ = new QComboBox();
    auto add_dir = [&](const QString& label, EffectPack::Direction d, const QString& tip) {
        direction_combo_->addItem(label, (int)d);
        direction_combo_->setItemData(direction_combo_->count() - 1, tip, Qt::ToolTipRole);
    };
    add_dir(QStringLiteral("Left"), EffectPack::Direction::Left,
            QStringLiteral("Room −X. Along the key row on a typical keyboard."));
    add_dir(QStringLiteral("Right"), EffectPack::Direction::Right,
            QStringLiteral("Room +X."));
    add_dir(QStringLiteral("Up"), EffectPack::Direction::Up,
            QStringLiteral("Room +Y (vertical). On a flat keyboard Y is thin — uses depth (Z) as fallback."));
    add_dir(QStringLiteral("Down"), EffectPack::Direction::Down,
            QStringLiteral("Room −Y. Flat boards fall back to depth (Z), not Left/Right."));
    add_dir(QStringLiteral("Forward"), EffectPack::Direction::Forward,
            QStringLiteral("Room +Z (floor depth). Spacebar ↔ F-keys on a desk keyboard."));
    add_dir(QStringLiteral("Back"), EffectPack::Direction::Back,
            QStringLiteral("Room −Z."));
    direction_combo_->insertSeparator(direction_combo_->count());
    add_dir(QStringLiteral("+X"), EffectPack::Direction::PosX, QStringLiteral("Explicit room +X"));
    add_dir(QStringLiteral("−X"), EffectPack::Direction::NegX, QStringLiteral("Explicit room −X"));
    add_dir(QStringLiteral("+Y"), EffectPack::Direction::PosY, QStringLiteral("Explicit room +Y (up)"));
    add_dir(QStringLiteral("−Y"), EffectPack::Direction::NegY, QStringLiteral("Explicit room −Y (down)"));
    add_dir(QStringLiteral("+Z"), EffectPack::Direction::PosZ, QStringLiteral("Explicit room +Z"));
    add_dir(QStringLiteral("−Z"), EffectPack::Direction::NegZ, QStringLiteral("Explicit room −Z"));
    direction_combo_->setToolTip(
        QStringLiteral("Travel / spin axis. With Space=Device this follows the controller orientation from the viewport."));
    axis_space_combo_ = new QComboBox();
    axis_space_combo_->addItem(QStringLiteral("Device (layout)"), (int)EffectPack::AxisSpace::Device);
    axis_space_combo_->addItem(QStringLiteral("Room"), (int)EffectPack::AxisSpace::Room);
    axis_space_combo_->setToolTip(
        QStringLiteral("Device uses this controller's rotation from the 3D layout. Room uses fixed room ±X/Y/Z."));
    axis_mode_combo_ = new QComboBox();
    axis_mode_combo_->addItem(QStringLiteral("Preset direction"), (int)EffectPack::AxisMode::Preset);
    axis_mode_combo_->addItem(QStringLiteral("Custom yaw/pitch"), (int)EffectPack::AxisMode::Custom);
    axis_yaw_spin_ = new QDoubleSpinBox();
    axis_yaw_spin_->setRange(-180.0, 180.0);
    axis_yaw_spin_->setSuffix(QStringLiteral("°"));
    axis_pitch_spin_ = new QDoubleSpinBox();
    axis_pitch_spin_->setRange(-90.0, 90.0);
    axis_pitch_spin_->setSuffix(QStringLiteral("°"));
    auto* dir_form = new QFormLayout();
    dir_form->addRow(QStringLiteral("Space"), axis_space_combo_);
    dir_form->addRow(QStringLiteral("Axis"), axis_mode_combo_);
    dir_form->addRow(QStringLiteral("Direction"), direction_combo_);
    dir_form->addRow(QStringLiteral("Yaw"), axis_yaw_spin_);
    dir_form->addRow(QStringLiteral("Pitch"), axis_pitch_spin_);
    static_cast<EffectCollapsibleSection*>(direction_section_)->bodyLayout()->addLayout(dir_form);
    props_inner_layout->addWidget(direction_section_);

    curve_combo_ = new QComboBox();
    curve_combo_->addItem(QStringLiteral("Flat (none)"), QStringLiteral("flat"));
    curve_combo_->addItem(QStringLiteral("Triangle"), QStringLiteral("triangle"));
    curve_combo_->addItem(QStringLiteral("Ease In"), QStringLiteral("ease_in"));
    curve_combo_->addItem(QStringLiteral("Ease Out"), QStringLiteral("ease_out"));
    curve_combo_->addItem(QStringLiteral("Pulse"), QStringLiteral("pulse_curve"));
    curve_combo_->addItem(QStringLiteral("Custom"), QStringLiteral("custom"));
    auto* curve_sec = new EffectCollapsibleSection(QStringLiteral("Intensity Curve"));
    curve_sec->setExpanded(false);
    auto* curve_form = new QFormLayout();
    curve_form->addRow(QStringLiteral("Preset"), curve_combo_);
    curve_sec->bodyLayout()->addLayout(curve_form);
    props_inner_layout->addWidget(curve_sec);

    speed_section_ = new EffectCollapsibleSection(QStringLiteral("Speed"));
    static_cast<EffectCollapsibleSection*>(speed_section_)->setExpanded(true);
    speed_spin_ = new QDoubleSpinBox();
    speed_spin_->setRange(0.05, 8.0);
    speed_spin_->setSingleStep(0.1);
    speed_spin_->setValue(1.0);
    period_spin_ = new QSpinBox();
    period_spin_->setRange(50, EffectPack::kMaxDurationMs);
    period_spin_->setValue(800);
    auto* speed_form = new QFormLayout();
    speed_form->addRow(QStringLiteral("Speed ×"), speed_spin_);
    period_row_ = new QWidget();
    auto* period_layout = new QHBoxLayout(period_row_);
    period_layout->setContentsMargins(0, 0, 0, 0);
    period_layout->addWidget(new QLabel(QStringLiteral("Period ms")));
    period_layout->addWidget(period_spin_, 1);
    static_cast<EffectCollapsibleSection*>(speed_section_)->bodyLayout()->addLayout(speed_form);
    static_cast<EffectCollapsibleSection*>(speed_section_)->bodyLayout()->addWidget(period_row_);
    props_inner_layout->addWidget(speed_section_);

    pulse_section_ = new EffectCollapsibleSection(QStringLiteral("Pulse / Chase"));
    static_cast<EffectCollapsibleSection*>(pulse_section_)->setExpanded(true);
    pulse_length_spin_ = new QSpinBox();
    pulse_length_spin_->setRange(2, 100);
    pulse_length_spin_->setValue(25);
    auto* pulse_form = new QFormLayout();
    pulse_form->addRow(QStringLiteral("Length / Duty %"), pulse_length_spin_);
    static_cast<EffectCollapsibleSection*>(pulse_section_)->bodyLayout()->addLayout(pulse_form);
    props_inner_layout->addWidget(pulse_section_);

    props_inner_layout->addStretch(1);
    props_scroll->setWidget(props_inner);
    props_layout->addWidget(props_scroll, 1);
    props_wrap->setMinimumWidth(280);
    splitter->addWidget(props_wrap);

    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 0);
    root->addWidget(splitter, 1);

    status_label_ = new QLabel(QStringLiteral("Idle"));
    PluginUiApplyMutedSecondaryLabel(status_label_);
    root->addWidget(status_label_);

    auto* action_row = new QHBoxLayout();
    preview_button_ = new QPushButton(QStringLiteral("Preview"));
    stop_button_ = new QPushButton(QStringLiteral("Stop"));
    stop_button_->setEnabled(false);
    save_button_ = new QPushButton(QStringLiteral("Save"));
    auto* close_button = new QPushButton(QStringLiteral("Close"));
    action_row->addWidget(preview_button_);
    action_row->addWidget(stop_button_);
    action_row->addStretch(1);
    action_row->addWidget(save_button_);
    action_row->addWidget(close_button);
    root->addLayout(action_row);

    setColorButton(color_button_, ToRGBColor(255, 0, 0));
    setColorButton(color_to_button_, ToRGBColor(0, 128, 255));

    connect(duration_spin_, QOverload<int>::of(&QSpinBox::valueChanged), this, &EffectPackEditorDialog::onDurationChanged);
    connect(controllers_button_, &QPushButton::clicked, this, &EffectPackEditorDialog::onPickControllers);
    connect(timeline_, &EffectPackTimelineWidget::playheadChanged, this, &EffectPackEditorDialog::onPlayheadChanged);
    connect(timeline_, &EffectPackTimelineWidget::blockSelected, this, &EffectPackEditorDialog::onBlockSelected);
    connect(timeline_, &EffectPackTimelineWidget::blockEdited, this, &EffectPackEditorDialog::onBlockSelected);
    connect(timeline_, &EffectPackTimelineWidget::blockDeleteRequested, this, &EffectPackEditorDialog::onBlockDeleteRequested);
    connect(timeline_, &EffectPackTimelineWidget::effectAddRequested, this, &EffectPackEditorDialog::onEffectAddRequested);
    connect(timeline_, &EffectPackTimelineWidget::gradientPresetApplied, this, &EffectPackEditorDialog::onGradientPresetApplied);
    connect(timeline_, &EffectPackTimelineWidget::curvePresetApplied, this, &EffectPackEditorDialog::onCurvePresetApplied);
    connect(effect_toolbar_, &EffectPackToolBar::effectClicked, this, &EffectPackEditorDialog::onToolbarEffectClicked);
    connect(effect_toolbar_, &EffectPackToolBar::colorClicked, this, &EffectPackEditorDialog::onToolbarColorClicked);
    connect(effect_toolbar_, &EffectPackToolBar::gradientPresetClicked, this, &EffectPackEditorDialog::onToolbarGradientClicked);
    connect(effect_toolbar_, &EffectPackToolBar::curvePresetClicked, this, &EffectPackEditorDialog::onToolbarCurveClicked);
    connect(remove_block_button_, &QPushButton::clicked, this, &EffectPackEditorDialog::onRemoveBlock);
    connect(type_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &EffectPackEditorDialog::onTypeChanged);
    connect(start_spin_, QOverload<int>::of(&QSpinBox::valueChanged), this, &EffectPackEditorDialog::onBlockFieldChanged);
    connect(end_spin_, QOverload<int>::of(&QSpinBox::valueChanged), this, &EffectPackEditorDialog::onBlockFieldChanged);
    connect(period_spin_, QOverload<int>::of(&QSpinBox::valueChanged), this, &EffectPackEditorDialog::onBlockFieldChanged);
    connect(intensity_spin_, QOverload<int>::of(&QSpinBox::valueChanged), this, &EffectPackEditorDialog::onBlockFieldChanged);
    connect(min_intensity_spin_, QOverload<int>::of(&QSpinBox::valueChanged), this, &EffectPackEditorDialog::onBlockFieldChanged);
    connect(speed_spin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &EffectPackEditorDialog::onBlockFieldChanged);
    connect(pulse_length_spin_, QOverload<int>::of(&QSpinBox::valueChanged), this, &EffectPackEditorDialog::onBlockFieldChanged);
    connect(direction_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &EffectPackEditorDialog::onBlockFieldChanged);
    connect(axis_space_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &EffectPackEditorDialog::onBlockFieldChanged);
    connect(axis_mode_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &EffectPackEditorDialog::onBlockFieldChanged);
    connect(axis_yaw_spin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &EffectPackEditorDialog::onBlockFieldChanged);
    connect(axis_pitch_spin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &EffectPackEditorDialog::onBlockFieldChanged);
    connect(curve_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &EffectPackEditorDialog::onBlockFieldChanged);
    connect(color_button_, &QPushButton::clicked, this, &EffectPackEditorDialog::onPickColor);
    connect(color_to_button_, &QPushButton::clicked, this, &EffectPackEditorDialog::onPickColorTo);
    connect(gradient_preset_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &EffectPackEditorDialog::onGradientPreset);
    connect(gradient_bar_, &EffectPackGradientBar::stopsChanged, this, &EffectPackEditorDialog::onGradientStopsChanged);
    connect(preview_button_, &QPushButton::clicked, this, &EffectPackEditorDialog::onPreview);
    connect(stop_button_, &QPushButton::clicked, this, &EffectPackEditorDialog::stopPreview);
    connect(save_button_, &QPushButton::clicked, this, &EffectPackEditorDialog::onSave);
    connect(close_button, &QPushButton::clicked, this, &QDialog::close);

    updatePropVisibility();
}

void EffectPackEditorDialog::NewPack(const filesystem::path& packs_dir)
{
    std::vector<std::string> devices;
    if(!promptSelectControllers(&devices, true))
    {
        // Cancel: leave any currently open pack untouched.
        return;
    }

    packs_dir_ = packs_dir;
    pack_path_.clear();
    pack_ = EffectPack::Pack();
    pack_.id = "new_pack";
    pack_.name = "New pack";
    pack_.duration_ms = 5000;
    pack_.loop = EffectPack::LoopMode::Once;
    pack_.priority = 10;
    pack_.devices = std::move(devices);

    loadIntoUi(pack_);
    setWindowTitle(QStringLiteral("Effect Pack Editor — New"));
    status_label_->setText(QStringLiteral(
        "Drag effects onto a row · right-click to add · drag colors/gradients onto blocks · Delete removes"));
    show();
    raise();
    activateWindow();
}

void EffectPackEditorDialog::EditPack(const filesystem::path& path)
{
    EffectPack::Pack pack;
    std::string err;
    if(!EffectPack::LoadFromFile(path, &pack, &err))
    {
        QMessageBox::warning(this, QStringLiteral("Effect Pack Editor"),
                             QStringLiteral("Failed to load:\n%1").arg(QString::fromStdString(err)));
        return;
    }
    packs_dir_ = path.parent_path();
    pack_path_ = path;
    pack_ = std::move(pack);
    loadIntoUi(pack_);
    setWindowTitle(QStringLiteral("Effect Pack Editor — %1").arg(QString::fromStdString(pack_.name)));
    status_label_->setText(QStringLiteral("Editing %1 — All (this pack) shows pack-wide blocks").arg(
        QString::fromStdString(path.filename().string())));
    show();
    raise();
    activateWindow();
}

bool EffectPackEditorDialog::promptSelectControllers(std::vector<std::string>* devices, bool require_selection)
{
    if(!devices || !tab_)
    {
        return false;
    }

    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("Select controllers for this effect"));
    auto* layout = new QVBoxLayout(&dlg);
    layout->addWidget(new QLabel(
        QStringLiteral("Pick which scene controllers this pack can use.\n"
                       "Pack-wide blocks go on “All (this pack)”; per-device rows are underneath.")));

    auto* list = new QListWidget();
    list->setSelectionMode(QAbstractItemView::NoSelection);
    const auto& transforms = tab_->GetControllerTransforms();
    int visible = 0;
    for(int i = 0; i < (int)transforms.size(); ++i)
    {
        ControllerTransform* transform = transforms[(size_t)i].get();
        if(!transform || transform->hidden_by_virtual)
        {
            continue;
        }
        const std::string key = ControllerKeyName(transform, i);
        auto* item = new QListWidgetItem(ControllerLabel(transform, i), list);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setData(Qt::UserRole, QString::fromStdString(key));
        const bool checked = devices->empty()
            || std::any_of(devices->begin(), devices->end(),
                           [&](const std::string& d) {
                               return EffectPack::NameMatches(key, d) || EffectPack::NameMatches(d, key);
                           });
        item->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
        ++visible;
    }
    layout->addWidget(list, 1);

    if(visible == 0)
    {
        QMessageBox::warning(this, QStringLiteral("Effect Pack Editor"),
                             QStringLiteral("Add at least one controller to the 3D scene first."));
        return false;
    }

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    auto* select_all = buttons->addButton(QStringLiteral("Select all"), QDialogButtonBox::ActionRole);
    auto* clear_all = buttons->addButton(QStringLiteral("Clear"), QDialogButtonBox::ActionRole);
    connect(select_all, &QPushButton::clicked, list, [list]() {
        for(int i = 0; i < list->count(); ++i)
        {
            list->item(i)->setCheckState(Qt::Checked);
        }
    });
    connect(clear_all, &QPushButton::clicked, list, [list]() {
        for(int i = 0; i < list->count(); ++i)
        {
            list->item(i)->setCheckState(Qt::Unchecked);
        }
    });
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    layout->addWidget(buttons);

    if(dlg.exec() != QDialog::Accepted)
    {
        return false;
    }

    std::vector<std::string> picked;
    for(int i = 0; i < list->count(); ++i)
    {
        QListWidgetItem* item = list->item(i);
        if(item->checkState() == Qt::Checked)
        {
            picked.push_back(item->data(Qt::UserRole).toString().toStdString());
        }
    }
    if(require_selection && picked.empty())
    {
        QMessageBox::warning(this, QStringLiteral("Effect Pack Editor"),
                             QStringLiteral("Select at least one controller."));
        return false;
    }
    *devices = std::move(picked);
    return true;
}

void EffectPackEditorDialog::onPickControllers()
{
    std::vector<std::string> devices = pack_.devices;
    if(!promptSelectControllers(&devices, true))
    {
        return;
    }
    pack_.devices = std::move(devices);
    onRebuildTimelineModel();
    status_label_->setText(QStringLiteral("%1 controller(s) on this pack").arg((int)pack_.devices.size()));
}

bool EffectPackEditorDialog::deviceSelectedForPack(const std::string& key) const
{
    if(pack_.devices.empty())
    {
        return true; // empty devices list = whole scene
    }
    for(const std::string& d : pack_.devices)
    {
        if(EffectPack::NameMatches(key, d) || EffectPack::NameMatches(d, key))
        {
            return true;
        }
    }
    return false;
}

EffectPackTimelineWidget::Node EffectPackEditorDialog::buildControllerNode(ControllerTransform* transform, int index) const
{
    EffectPackTimelineWidget::Node ctrl;
    ctrl.label = ControllerLabel(transform, index);
    const std::string key = ControllerKeyName(transform, index);
    ctrl.target.kind = EffectPack::TargetKind::Device;
    ctrl.target.device_name = key;

    struct ZoneBucket
    {
        QString label;
        QString zone_name;
        std::vector<int> led_globals;
        std::vector<unsigned int> led_in_zone;
    };
    std::map<std::pair<RGBControllerInterface*, unsigned int>, ZoneBucket> zones;

    for(const LEDPosition3D& led : transform->led_positions)
    {
        RGBControllerInterface* rgb = led.controller ? led.controller : transform->controller;
        if(!rgb)
        {
            continue;
        }
        const auto zone_key = std::make_pair(rgb, led.zone_idx);
        ZoneBucket& bucket = zones[zone_key];
        if(bucket.zone_name.isEmpty())
        {
            bucket.zone_name = ZoneLabelForLed(rgb, led.zone_idx);
            bucket.label = bucket.zone_name;
        }
        int global = -1;
        if(TryGlobalLedIndex(rgb, led.zone_idx, led.led_idx, &global))
        {
            if(std::find(bucket.led_globals.begin(), bucket.led_globals.end(), global) == bucket.led_globals.end())
            {
                bucket.led_globals.push_back(global);
                bucket.led_in_zone.push_back(led.led_idx);
            }
        }
    }

    for(auto& entry : zones)
    {
        ZoneBucket& bucket = entry.second;
        int same_label = 0;
        for(const auto& other : zones)
        {
            if(other.second.zone_name == bucket.zone_name)
            {
                ++same_label;
            }
        }
        if(same_label > 1 && entry.first.first)
        {
            QString device = QString::fromStdString(entry.first.first->GetDisplayName());
            if(device.isEmpty())
            {
                device = QString::fromStdString(entry.first.first->GetName());
            }
            if(!device.isEmpty())
            {
                bucket.label = bucket.zone_name + QStringLiteral(" · ") + device;
            }
        }

        EffectPackTimelineWidget::Node zone;
        zone.label = bucket.label;
        zone.target.kind = EffectPack::TargetKind::Zone;
        zone.target.device_name = key;
        zone.target.zone_name = bucket.zone_name.toStdString();

        const size_t led_count = bucket.led_globals.size();
        zone.led_count = (int)std::max<size_t>(1, led_count);
        const size_t led_cap = std::min(led_count, size_t{256});
        for(size_t li = 0; li < led_cap; ++li)
        {
            EffectPackTimelineWidget::Node led_node;
            led_node.label = QStringLiteral("LED %1").arg(bucket.led_in_zone[li]);
            led_node.target.kind = EffectPack::TargetKind::Leds;
            led_node.target.device_name = key;
            led_node.target.zone_name = bucket.zone_name.toStdString();
            led_node.target.led_indices = {bucket.led_globals[li]};
            led_node.led_count = zone.led_count;
            zone.children.push_back(std::move(led_node));
        }
        ctrl.children.push_back(std::move(zone));
    }
    int total_leds = 0;
    for(const EffectPackTimelineWidget::Node& z : ctrl.children)
    {
        total_leds += z.led_count;
    }
    ctrl.led_count = std::max(1, total_leds);
    return ctrl;
}

void EffectPackEditorDialog::onRebuildTimelineModel()
{
    QVector<EffectPackTimelineWidget::Node> roots;

    // Pack-wide row — rainbow wash and other “all selected” blocks live here.
    EffectPackTimelineWidget::Node all;
    all.label = QStringLiteral("All (this pack)");
    all.target.kind = EffectPack::TargetKind::All;
    int pack_leds = 0;

    if(tab_)
    {
        timeline_->setControllerTransforms(tab_->GetControllerTransformsMutable());
        const auto& transforms = tab_->GetControllerTransforms();
        for(int i = 0; i < (int)transforms.size(); ++i)
        {
            ControllerTransform* transform = transforms[(size_t)i].get();
            if(!transform || transform->hidden_by_virtual)
            {
                continue;
            }
            const std::string key = ControllerKeyName(transform, i);
            if(!deviceSelectedForPack(key))
            {
                continue;
            }
            EffectPackTimelineWidget::Node ctrl = buildControllerNode(transform, i);
            pack_leds += ctrl.led_count;
            roots.push_back(std::move(ctrl));
        }
    }
    all.led_count = std::max(1, pack_leds);
    roots.prepend(std::move(all));

    timeline_->setModel(std::move(roots));
}

void EffectPackEditorDialog::loadIntoUi(const EffectPack::Pack& pack)
{
    suppress_ui_ = true;
    name_edit_->setText(QString::fromStdString(pack.name));
    duration_spin_->setValue(pack.duration_ms);
    const QString loop = (pack.loop == EffectPack::LoopMode::Forever) ? QStringLiteral("forever")
        : (pack.loop == EffectPack::LoopMode::WhileActive) ? QStringLiteral("while_active")
        : QStringLiteral("once");
    const int loop_idx = loop_combo_->findData(loop);
    loop_combo_->setCurrentIndex(loop_idx >= 0 ? loop_idx : 0);
    selected_track_ = -1;
    selected_block_ = -1;
    suppress_ui_ = false;
    timeline_->setPack(&pack_);
    timeline_->setDurationMs(pack_.duration_ms);
    timeline_->setPlayheadMs(0);
    onRebuildTimelineModel();
    applyBlockToForm();
}

void EffectPackEditorDialog::onDurationChanged(int value)
{
    if(suppress_ui_)
    {
        return;
    }
    pack_.duration_ms = value;
    for(EffectPack::Track& track : pack_.tracks)
    {
        for(EffectPack::Block& block : track.blocks)
        {
            block.start_ms = std::clamp(block.start_ms, 0, std::max(0, value - 1));
            block.end_ms = std::clamp(block.end_ms, block.start_ms + 1, value);
        }
    }
    timeline_->setDurationMs(value);
    applyBlockToForm();
    timeline_->update();
}

void EffectPackEditorDialog::onPlayheadChanged(int ms)
{
    timeline_->setPlayheadMs(ms);
    if(player_.IsPlaying())
    {
        player_.UpdatePack(pack_);
        player_.SeekToLocalMs(ms);
        wall_.restart();
        last_elapsed_ms_ = 0;
        if(tab_)
        {
            tab_->ApplyEffectPackPreviewFrame(pack_, ms);
        }
    }
}

void EffectPackEditorDialog::keyPressEvent(QKeyEvent* event)
{
    if(event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace)
    {
        // Allow Delete from anywhere in the dialog (not only when timeline has focus),
        // unless the user is editing a line edit / spin box text.
        const QWidget* focus = focusWidget();
        const bool editing_text = focus
            && (qobject_cast<const QLineEdit*>(focus)
                || qobject_cast<const QAbstractSpinBox*>(focus));
        if(!editing_text && selectedBlock())
        {
            onRemoveBlock();
            event->accept();
            return;
        }
    }
    QDialog::keyPressEvent(event);
}

void EffectPackEditorDialog::updateSelectionActions()
{
    const bool has = selectedBlock() != nullptr;
    if(remove_block_button_)
    {
        remove_block_button_->setEnabled(has && !player_.IsPlaying());
    }
}

int EffectPackEditorDialog::ensureTrackForTarget(const EffectPack::Target& target, const QString& label)
{
    for(int i = 0; i < (int)pack_.tracks.size(); ++i)
    {
        const EffectPack::Target& existing = pack_.tracks[(size_t)i].target;
        if(existing.kind == target.kind
           && existing.device_name == target.device_name
           && existing.zone_name == target.zone_name
           && existing.led_indices == target.led_indices)
        {
            return i;
        }
    }
    EffectPack::Track track;
    track.name = label.toStdString();
    track.target = target;
    pack_.tracks.push_back(std::move(track));
    return (int)pack_.tracks.size() - 1;
}

void EffectPackEditorDialog::addBlockAt(int row_index, int ms, EffectPack::BlockType type)
{
    if(timeline_)
    {
        timeline_->cancelDrag();
    }
    // Flush props into the previously selected block before creating another.
    applyFormToSelectedBlock();

    const QVector<EffectPackTimelineWidget::Row>& built = timeline_->rows();
    if(row_index < 0 || row_index >= built.size())
    {
        if(built.isEmpty())
        {
            return;
        }
        row_index = 0;
    }

    const auto& row = built[row_index];
    const int track = ensureTrackForTarget(row.target, row.label);
    EffectPack::Block block;
    block.type = type;
    block.start_ms = std::clamp(ms, 0, std::max(0, pack_.duration_ms - 1));
    const int default_len = (type == EffectPack::BlockType::Fade || type == EffectPack::BlockType::ColorWash) ? 2000 : 1000;
    block.end_ms = std::min(pack_.duration_ms, block.start_ms + default_len);
    if(block.end_ms <= block.start_ms)
    {
        block.end_ms = block.start_ms + 1;
    }
    block.color = ToRGBColor(255, 0, 0);
    block.color_from = ToRGBColor(255, 0, 0);
    block.color_to = ToRGBColor(0, 128, 255);
    block.period_ms = 800;
    block.min_intensity = (type == EffectPack::BlockType::Twinkle) ? 0.0f : 0.15f;
    block.max_intensity = 1.0f;
    block.intensity = 1.0f;
    block.direction = EffectPack::Direction::Right;
    block.axis_space = (row.target.kind == EffectPack::TargetKind::All)
        ? EffectPack::AxisSpace::Room
        : EffectPack::AxisSpace::Device;
    block.axis_mode = EffectPack::AxisMode::Preset;
    block.axis_yaw_deg = 0.0f;
    block.axis_pitch_deg = 0.0f;
    block.speed = 1.0f;
    block.pulse_length = 0.25f;
    if(type == EffectPack::BlockType::Twinkle)
    {
        block.period_ms = 700;
        block.gradient = {
            {0.0f, ToRGBColor(255, 220, 120)},
            {0.5f, ToRGBColor(255, 255, 255)},
            {1.0f, ToRGBColor(120, 180, 255)},
        };
        block.color = block.gradient.front().color;
        block.color_from = block.color;
        block.color_to = block.gradient.back().color;
    }
    else if(type == EffectPack::BlockType::Candle)
    {
        block.period_ms = 120;
        block.min_intensity = 0.35f;
        block.gradient = {
            {0.0f, ToRGBColor(180, 40, 0)},
            {0.45f, ToRGBColor(255, 120, 20)},
            {1.0f, ToRGBColor(255, 220, 80)},
        };
        block.color = block.gradient[1].color;
    }
    else if(type == EffectPack::BlockType::Strobe)
    {
        block.period_ms = 120;
        block.pulse_length = 0.35f;
        block.gradient = {
            {0.0f, ToRGBColor(255, 255, 255)},
            {1.0f, ToRGBColor(255, 255, 255)},
        };
        block.color = block.gradient.front().color;
    }
    else if(type == EffectPack::BlockType::Alternating)
    {
        block.period_ms = 400;
        block.gradient = {
            {0.0f, ToRGBColor(255, 40, 40)},
            {1.0f, ToRGBColor(40, 80, 255)},
        };
        block.color = block.gradient.front().color;
        block.color_to = block.gradient.back().color;
    }
    else if(type == EffectPack::BlockType::Spin)
    {
        block.direction = EffectPack::Direction::Up; // spin around room vertical
        block.pulse_length = 0.18f;
        block.speed = 1.5f;
        block.gradient = {
            {0.0f, ToRGBColor(40, 200, 255)},
            {0.5f, ToRGBColor(255, 255, 255)},
            {1.0f, ToRGBColor(40, 80, 255)},
        };
        block.color = block.gradient.front().color;
    }
    else if(type == EffectPack::BlockType::Dissolve)
    {
        const int default_dissolve = 2500;
        block.end_ms = std::min(pack_.duration_ms, block.start_ms + default_dissolve);
        block.gradient = {
            {0.0f, ToRGBColor(255, 0, 0)},
            {0.5f, ToRGBColor(255, 128, 0)},
            {1.0f, ToRGBColor(255, 255, 120)},
        };
        block.color = block.gradient.front().color;
    }
    else if(type == EffectPack::BlockType::Fire)
    {
        block.gradient = {
            {0.0f, ToRGBColor(20, 0, 0)},
            {0.35f, ToRGBColor(255, 40, 0)},
            {0.7f, ToRGBColor(255, 160, 0)},
            {1.0f, ToRGBColor(255, 255, 180)},
        };
        block.color = block.gradient[2].color;
    }
    else if(type == EffectPack::BlockType::Snow)
    {
        block.gradient = {
            {0.0f, ToRGBColor(200, 220, 255)},
            {1.0f, ToRGBColor(255, 255, 255)},
        };
        block.color = block.gradient.back().color;
    }
    else if(type == EffectPack::BlockType::SphereWipe || type == EffectPack::BlockType::Ripple
            || type == EffectPack::BlockType::Orbit || type == EffectPack::BlockType::Meteor
            || type == EffectPack::BlockType::Noise3D || type == EffectPack::BlockType::Plasma
            || type == EffectPack::BlockType::Balls || type == EffectPack::BlockType::Bars
            || type == EffectPack::BlockType::ColorWash || type == EffectPack::BlockType::Wipe
            || type == EffectPack::BlockType::Chase)
    {
        if(type == EffectPack::BlockType::SphereWipe || type == EffectPack::BlockType::Ripple)
        {
            block.end_ms = std::min(pack_.duration_ms, block.start_ms + 2500);
        }
        if(type == EffectPack::BlockType::Meteor)
        {
            block.direction = EffectPack::Direction::Down;
            block.pulse_length = 0.2f;
        }
        block.gradient = {
            {0.0f, ToRGBColor(255, 0, 0)},
            {0.2f, ToRGBColor(255, 128, 0)},
            {0.4f, ToRGBColor(255, 255, 0)},
            {0.6f, ToRGBColor(0, 255, 0)},
            {0.8f, ToRGBColor(0, 128, 255)},
            {1.0f, ToRGBColor(128, 0, 255)},
        };
        block.color = block.gradient.front().color;
    }
    else
    {
        EffectPack::EnsureBlockGradient(&block);
    }
    pack_.tracks[(size_t)track].blocks.push_back(block);
    selected_track_ = track;
    selected_block_ = (int)pack_.tracks[(size_t)track].blocks.size() - 1;
    timeline_->setPack(&pack_);
    timeline_->setSelectedBlock(selected_track_, selected_block_);
    if(player_.IsPlaying())
    {
        player_.UpdatePack(pack_);
    }
    applyBlockToForm();
    updateSelectionActions();
    timeline_->update();
}

void EffectPackEditorDialog::onEffectAddRequested(int row_index, int ms, int block_type)
{
    addBlockAt(row_index, ms, (EffectPack::BlockType)block_type);
}

void EffectPackEditorDialog::onBlockSelected(int track_index, int block_index)
{
    if(timeline_)
    {
        timeline_->cancelDrag();
    }
    // Commit the previously selected block before switching the form target.
    if(selected_track_ != track_index || selected_block_ != block_index)
    {
        applyFormToSelectedBlock();
    }
    selected_track_ = track_index;
    selected_block_ = block_index;
    applyBlockToForm();
    updateSelectionActions();
}

int EffectPackEditorDialog::currentTimelineRow() const
{
    const int selected = timeline_->selectedRowIndex();
    if(selected >= 0 && selected < timeline_->rows().size())
    {
        return selected;
    }
    return 0;
}

void EffectPackEditorDialog::onToolbarEffectClicked(int block_type)
{
    addBlockAt(currentTimelineRow(), timeline_ ? timeline_->playheadMs() : 0,
               (EffectPack::BlockType)block_type);
}

void EffectPackEditorDialog::onToolbarColorClicked(unsigned int rgb)
{
    EffectPack::Block* b = selectedBlock();
    if(!b)
    {
        return;
    }
    b->color = (RGBColor)rgb;
    b->color_from = (RGBColor)rgb;
    if(!b->gradient.empty())
    {
        b->gradient.front().color = (RGBColor)rgb;
    }
    EffectPack::EnsureBlockGradient(b);
    setColorButton(color_button_, b->color);
    syncGradientBar();
    timeline_->update();
}

void EffectPackEditorDialog::onToolbarGradientClicked(const QString& preset_id)
{
    applyGradientPresetToBlock(selectedBlock(), preset_id);
}

void EffectPackEditorDialog::onToolbarCurveClicked(const QString& preset_id)
{
    EffectPack::Block* b = selectedBlock();
    if(!b)
    {
        return;
    }
    if(preset_id.isEmpty() || preset_id == QStringLiteral("flat"))
    {
        b->intensity_curve.clear();
    }
    else
    {
        EffectPack::ApplyBuiltinIntensityCurve(b, preset_id.toUtf8().constData());
    }
    if(curve_combo_)
    {
        const int idx = curve_combo_->findData(preset_id);
        if(idx >= 0)
        {
            curve_combo_->setCurrentIndex(idx);
        }
    }
    timeline_->update();
}

void EffectPackEditorDialog::onCurvePresetApplied(int track_index, int block_index, const QString& preset_id)
{
    if(track_index < 0 || track_index >= (int)pack_.tracks.size())
    {
        return;
    }
    auto& blocks = pack_.tracks[(size_t)track_index].blocks;
    if(block_index < 0 || block_index >= (int)blocks.size())
    {
        return;
    }
    selected_track_ = track_index;
    selected_block_ = block_index;
    timeline_->setSelectedBlock(selected_track_, selected_block_);
    EffectPack::Block* b = &blocks[(size_t)block_index];
    if(preset_id.isEmpty() || preset_id == QStringLiteral("flat"))
    {
        b->intensity_curve.clear();
    }
    else
    {
        EffectPack::ApplyBuiltinIntensityCurve(b, preset_id.toUtf8().constData());
    }
    applyBlockToForm();
    timeline_->update();
}

void EffectPackEditorDialog::onGradientPresetApplied(int track_index, int block_index, const QString& preset_id)
{
    if(track_index < 0 || track_index >= (int)pack_.tracks.size())
    {
        return;
    }
    auto& blocks = pack_.tracks[(size_t)track_index].blocks;
    if(block_index < 0 || block_index >= (int)blocks.size())
    {
        return;
    }
    selected_track_ = track_index;
    selected_block_ = block_index;
    timeline_->setSelectedBlock(selected_track_, selected_block_);
    applyGradientPresetToBlock(&blocks[(size_t)block_index], preset_id);
    applyBlockToForm();
}

void EffectPackEditorDialog::applyGradientPresetToBlock(EffectPack::Block* b, const QString& preset)
{
    if(!b || preset.isEmpty())
    {
        return;
    }
    if(preset == QStringLiteral("rainbow"))
    {
        b->gradient = {
            {0.0f, ToRGBColor(255, 0, 0)},
            {0.2f, ToRGBColor(255, 128, 0)},
            {0.4f, ToRGBColor(255, 255, 0)},
            {0.6f, ToRGBColor(0, 255, 0)},
            {0.8f, ToRGBColor(0, 128, 255)},
            {1.0f, ToRGBColor(128, 0, 255)},
        };
    }
    else if(preset == QStringLiteral("red_blue"))
    {
        b->gradient = {{0.0f, ToRGBColor(255, 0, 0)}, {1.0f, ToRGBColor(0, 80, 255)}};
    }
    else if(preset == QStringLiteral("white_color"))
    {
        b->gradient = {{0.0f, ToRGBColor(255, 255, 255)}, {1.0f, colorFromButton(color_button_)}};
    }
    else
    {
        return;
    }
    if(!b->gradient.empty())
    {
        b->color = b->gradient.front().color;
        b->color_from = b->gradient.front().color;
        b->color_to = b->gradient.back().color;
        setColorButton(color_button_, b->color);
        setColorButton(color_to_button_, b->color_to);
    }
    syncGradientBar();
    if(timeline_)
    {
        timeline_->update();
    }
}

void EffectPackEditorDialog::onBlockDeleteRequested(int track_index, int block_index)
{
    selected_track_ = track_index;
    selected_block_ = block_index;
    onRemoveBlock();
}

void EffectPackEditorDialog::onRemoveBlock()
{
    if(selected_track_ < 0 || selected_track_ >= (int)pack_.tracks.size())
    {
        return;
    }
    auto& blocks = pack_.tracks[(size_t)selected_track_].blocks;
    if(selected_block_ < 0 || selected_block_ >= (int)blocks.size())
    {
        return;
    }
    if(timeline_)
    {
        timeline_->cancelDrag();
    }
    // Stop writing form values into a block that is about to vanish.
    suppress_ui_ = true;
    blocks.erase(blocks.begin() + selected_block_);
    // Drop empty tracks so stale target rows cannot keep painting blacks forever.
    if(blocks.empty())
    {
        pack_.tracks.erase(pack_.tracks.begin() + selected_track_);
        selected_track_ = -1;
        selected_block_ = -1;
    }
    else
    {
        selected_block_ = std::min(selected_block_, (int)blocks.size() - 1);
    }
    if(timeline_)
    {
        timeline_->setPack(&pack_);
        timeline_->setSelectedBlock(selected_track_, selected_block_);
        timeline_->update();
    }
    if(player_.IsPlaying())
    {
        player_.UpdatePack(pack_);
    }
    suppress_ui_ = false;
    applyBlockToForm();
    updateSelectionActions();
}

EffectPack::Block* EffectPackEditorDialog::selectedBlock()
{
    if(selected_track_ < 0 || selected_track_ >= (int)pack_.tracks.size())
    {
        return nullptr;
    }
    auto& blocks = pack_.tracks[(size_t)selected_track_].blocks;
    if(selected_block_ < 0 || selected_block_ >= (int)blocks.size())
    {
        return nullptr;
    }
    return &blocks[(size_t)selected_block_];
}

void EffectPackEditorDialog::updatePropVisibility()
{
    EffectPack::Block* b = selectedBlock();
    const bool ok = b != nullptr;
    const EffectPack::BlockType type = b
        ? b->type
        : (EffectPack::BlockType)type_combo_->currentData().toInt();
    const bool fade = type == EffectPack::BlockType::Fade;
    const bool wipe_chase = type == EffectPack::BlockType::Wipe || type == EffectPack::BlockType::Chase;
    const bool pulse_twinkle = type == EffectPack::BlockType::Pulse || type == EffectPack::BlockType::Twinkle;
    const bool alternating = type == EffectPack::BlockType::Alternating;
    const bool strobe = type == EffectPack::BlockType::Strobe;
    const bool spin = type == EffectPack::BlockType::Spin;
    const bool candle = type == EffectPack::BlockType::Candle;
    const bool dissolve = type == EffectPack::BlockType::Dissolve;
    const bool chase = type == EffectPack::BlockType::Chase;
    const bool colorwash = type == EffectPack::BlockType::ColorWash;
    const bool worldish = EffectPack::BlockNeedsWorldEval(type);
    const bool needs_period = pulse_twinkle || alternating || strobe;
    const bool needs_speed = wipe_chase || colorwash || pulse_twinkle || alternating || strobe || spin
        || dissolve || worldish;
    const bool needs_direction = EffectPack::BlockNeedsDirection(type);
    const bool needs_pulse_length = chase || spin || strobe
        || type == EffectPack::BlockType::Orbit
        || type == EffectPack::BlockType::Ripple
        || type == EffectPack::BlockType::Meteor
        || type == EffectPack::BlockType::Balls;
    const bool needs_min_intensity = pulse_twinkle || candle;
    const bool custom_axis = axis_mode_combo_
        && (EffectPack::AxisMode)axis_mode_combo_->currentData().toInt() == EffectPack::AxisMode::Custom;

    if(color_to_row_)
    {
        color_to_row_->setVisible(fade);
    }
    if(color_to_button_)
    {
        color_to_button_->setEnabled(ok && fade);
    }
    if(direction_section_)
    {
        direction_section_->setVisible(needs_direction);
    }
    if(direction_combo_)
    {
        direction_combo_->setEnabled(ok && needs_direction && !custom_axis);
        direction_combo_->setVisible(!custom_axis);
    }
    if(axis_yaw_spin_)
    {
        axis_yaw_spin_->setVisible(custom_axis && needs_direction);
        axis_yaw_spin_->setEnabled(ok && custom_axis && needs_direction);
    }
    if(axis_pitch_spin_)
    {
        axis_pitch_spin_->setVisible(custom_axis && needs_direction);
        axis_pitch_spin_->setEnabled(ok && custom_axis && needs_direction);
    }
    if(speed_section_)
    {
        speed_section_->setVisible(needs_speed);
    }
    if(period_row_)
    {
        period_row_->setVisible(needs_period);
    }
    if(period_spin_)
    {
        period_spin_->setEnabled(ok && needs_period);
    }
    if(pulse_section_)
    {
        pulse_section_->setVisible(needs_pulse_length);
    }
    if(min_intensity_row_)
    {
        min_intensity_row_->setVisible(needs_min_intensity);
    }
    if(min_intensity_spin_)
    {
        min_intensity_spin_->setEnabled(ok && needs_min_intensity);
    }
    updateSelectionActions();
}
void EffectPackEditorDialog::syncGradientBar()
{
    if(!gradient_bar_)
    {
        return;
    }
    EffectPack::Block* b = selectedBlock();
    if(!b)
    {
        gradient_bar_->setEnabled(false);
        return;
    }
    EffectPack::EnsureBlockGradient(b);
    gradient_bar_->setEnabled(true);
    suppress_ui_ = true;
    gradient_bar_->setStops(b->gradient);
    suppress_ui_ = false;
}

void EffectPackEditorDialog::onGradientStopsChanged()
{
    if(suppress_ui_)
    {
        return;
    }
    EffectPack::Block* b = selectedBlock();
    if(!b || !gradient_bar_)
    {
        return;
    }
    b->gradient = gradient_bar_->stops();
    if(!b->gradient.empty() && !gradient_bar_->isDragging())
    {
        b->color = b->gradient.front().color;
        b->color_from = b->gradient.front().color;
        b->color_to = b->gradient.back().color;
        setColorButton(color_button_, b->color);
        setColorButton(color_to_button_, b->color_to);
    }
    timeline_->update();
}

void EffectPackEditorDialog::applyBlockToForm()
{
    suppress_ui_ = true;
    EffectPack::Block* b = selectedBlock();
    const bool ok = b != nullptr;
    type_combo_->setEnabled(ok);
    start_spin_->setEnabled(ok);
    end_spin_->setEnabled(ok);
    color_button_->setEnabled(ok);
    color_to_button_->setEnabled(ok);
    intensity_spin_->setEnabled(ok);
    min_intensity_spin_->setEnabled(ok);
    speed_spin_->setEnabled(ok);
    period_spin_->setEnabled(ok);
    direction_combo_->setEnabled(ok);
    pulse_length_spin_->setEnabled(ok);
    if(gradient_bar_)
    {
        gradient_bar_->setEnabled(ok);
    }
    gradient_preset_->setEnabled(ok);
    if(!ok)
    {
        suppress_ui_ = false;
        updatePropVisibility();
        return;
    }
    EffectPack::EnsureBlockGradient(b);
    const int type_idx = type_combo_->findData((int)b->type);
    type_combo_->setCurrentIndex(type_idx >= 0 ? type_idx : 0);
    start_spin_->setValue(b->start_ms);
    end_spin_->setValue(b->end_ms);
    period_spin_->setValue(std::max(50, b->period_ms));
    intensity_spin_->setValue((int)std::lround(std::clamp(b->intensity, 0.0f, 1.0f) * 100.0f));
    min_intensity_spin_->setValue((int)std::lround(std::clamp(b->min_intensity, 0.0f, 1.0f) * 100.0f));
    speed_spin_->setValue(b->speed);
    pulse_length_spin_->setValue((int)std::lround(std::clamp(b->pulse_length, 0.02f, 1.0f) * 100.0f));
    const int dir_idx = direction_combo_->findData((int)b->direction);
    direction_combo_->setCurrentIndex(dir_idx >= 0 ? dir_idx : 1);
    if(axis_space_combo_)
    {
        const int sidx = axis_space_combo_->findData((int)b->axis_space);
        axis_space_combo_->setCurrentIndex(sidx >= 0 ? sidx : 0);
    }
    if(axis_mode_combo_)
    {
        const int midx = axis_mode_combo_->findData((int)b->axis_mode);
        axis_mode_combo_->setCurrentIndex(midx >= 0 ? midx : 0);
    }
    if(axis_yaw_spin_)
    {
        axis_yaw_spin_->setValue(b->axis_yaw_deg);
    }
    if(axis_pitch_spin_)
    {
        axis_pitch_spin_->setValue(b->axis_pitch_deg);
    }
    if(curve_combo_)
    {
        const char* matched = EffectPack::MatchBuiltinIntensityCurve(b->intensity_curve);
        const QString id = matched ? QString::fromUtf8(matched) : QStringLiteral("custom");
        const int cidx = curve_combo_->findData(id);
        curve_combo_->setCurrentIndex(cidx >= 0 ? cidx : curve_combo_->findData(QStringLiteral("custom")));
    }
    setColorButton(color_button_, b->type == EffectPack::BlockType::Fade ? b->color_from : b->color);
    setColorButton(color_to_button_, b->color_to);
    suppress_ui_ = false;
    syncGradientBar();
    updatePropVisibility();
}

void EffectPackEditorDialog::applyFormToSelectedBlock()
{
    if(suppress_ui_)
    {
        return;
    }
    EffectPack::Block* b = selectedBlock();
    if(!b)
    {
        return;
    }
    b->type = (EffectPack::BlockType)type_combo_->currentData().toInt();
    b->start_ms = start_spin_->value();
    b->end_ms = std::max(b->start_ms + 1, end_spin_->value());
    if(end_spin_->value() != b->end_ms)
    {
        const bool prev = suppress_ui_;
        suppress_ui_ = true;
        end_spin_->setValue(b->end_ms);
        suppress_ui_ = prev;
    }
    b->period_ms = period_spin_->value();
    b->intensity = intensity_spin_->value() / 100.0f;
    b->min_intensity = min_intensity_spin_->value() / 100.0f;
    b->speed = (float)speed_spin_->value();
    b->pulse_length = pulse_length_spin_->value() / 100.0f;
    b->direction = (EffectPack::Direction)direction_combo_->currentData().toInt();
    if(axis_space_combo_)
    {
        b->axis_space = (EffectPack::AxisSpace)axis_space_combo_->currentData().toInt();
    }
    if(axis_mode_combo_)
    {
        b->axis_mode = (EffectPack::AxisMode)axis_mode_combo_->currentData().toInt();
    }
    if(axis_yaw_spin_)
    {
        b->axis_yaw_deg = (float)axis_yaw_spin_->value();
    }
    if(axis_pitch_spin_)
    {
        b->axis_pitch_deg = (float)axis_pitch_spin_->value();
    }
    if(curve_combo_)
    {
        const QString cid = curve_combo_->currentData().toString();
        if(cid == QStringLiteral("custom"))
        {
        }
        else if(cid.isEmpty() || cid == QStringLiteral("flat"))
        {
            b->intensity_curve.clear();
        }
        else
        {
            EffectPack::ApplyBuiltinIntensityCurve(b, cid.toUtf8().constData());
        }
    }
    const RGBColor c = colorFromButton(color_button_);
    const RGBColor c2 = colorFromButton(color_to_button_);
    b->color = c;
    b->color_from = c;
    b->color_to = c2;
    if(b->type == EffectPack::BlockType::Fade)
    {
        if(b->gradient.size() < 2)
        {
            b->gradient = {{0.0f, c}, {1.0f, c2}};
        }
        else
        {
            b->gradient.front().color = c;
            b->gradient.back().color = c2;
        }
    }
    else if(b->type == EffectPack::BlockType::Solid || b->gradient.empty() || b->gradient.size() == 1)
    {
        b->gradient = {{0.0f, c}, {1.0f, c}};
    }
    else
    {
        // Multi-stop: keep shape, sync primary colour to the first stop.
        b->gradient.front().color = c;
    }
    timeline_->update();
}

void EffectPackEditorDialog::onTypeChanged()
{
    if(suppress_ui_)
    {
        return;
    }
    applyFormToSelectedBlock();
    EffectPack::Block* b = selectedBlock();
    if(b)
    {
        const bool spatial = b->type == EffectPack::BlockType::Wipe
            || b->type == EffectPack::BlockType::Chase
            || b->type == EffectPack::BlockType::ColorWash
            || b->type == EffectPack::BlockType::Spin
            || b->type == EffectPack::BlockType::Alternating
            || b->type == EffectPack::BlockType::Dissolve
            || EffectPack::BlockNeedsWorldEval(b->type);
        if(b->type == EffectPack::BlockType::Twinkle)
        {
            // Floor between flashes — keep sparky by default when switching type.
            if(b->min_intensity > 0.25f)
            {
                b->min_intensity = 0.0f;
                min_intensity_spin_->blockSignals(true);
                min_intensity_spin_->setValue(0);
                min_intensity_spin_->blockSignals(false);
            }
            if(b->period_ms < 80)
            {
                b->period_ms = 700;
            }
        }
        if(spatial && b->gradient.size() >= 2)
        {
            const bool flat = b->gradient.front().color == b->gradient.back().color
                && b->gradient.size() <= 2;
            if(flat)
            {
                b->gradient = {
                    {0.0f, ToRGBColor(255, 0, 0)},
                    {0.2f, ToRGBColor(255, 128, 0)},
                    {0.4f, ToRGBColor(255, 255, 0)},
                    {0.6f, ToRGBColor(0, 255, 0)},
                    {0.8f, ToRGBColor(0, 128, 255)},
                    {1.0f, ToRGBColor(128, 0, 255)},
                };
                b->color = b->gradient.front().color;
                b->color_from = b->gradient.front().color;
                b->color_to = b->gradient.back().color;
                setColorButton(color_button_, b->color);
                setColorButton(color_to_button_, b->color_to);
            }
        }
        EffectPack::EnsureBlockGradient(b);
    }
    updatePropVisibility();
    syncGradientBar();
    timeline_->update();
}

void EffectPackEditorDialog::onGradientPreset()
{
    if(suppress_ui_)
    {
        return;
    }
    const QString preset = gradient_preset_->currentData().toString();
    gradient_preset_->blockSignals(true);
    gradient_preset_->setCurrentIndex(0);
    gradient_preset_->blockSignals(false);
    applyGradientPresetToBlock(selectedBlock(), preset);
}

void EffectPackEditorDialog::onBlockFieldChanged()
{
    applyFormToSelectedBlock();
    // Keep gradient bar + timeline preview live for both new and existing blocks.
    if(!suppress_ui_)
    {
        syncGradientBar();
        timeline_->update();
    }
}

void EffectPackEditorDialog::setColorButton(QPushButton* button, RGBColor color)
{
    if(!button)
    {
        return;
    }
    const QColor qc = RgbToQColor(color);
    button->setProperty("rgbColor", (uint)color);
    button->setStyleSheet(
        QStringLiteral("background-color: %1; color: %2;")
            .arg(qc.name())
            .arg(qc.lightness() < 128 ? QStringLiteral("white") : QStringLiteral("black")));
    button->setText(qc.name().toUpper());
}

RGBColor EffectPackEditorDialog::colorFromButton(QPushButton* button) const
{
    if(!button)
    {
        return ToRGBColor(255, 0, 0);
    }
    return (RGBColor)button->property("rgbColor").toUInt();
}

void EffectPackEditorDialog::onPickColor()
{
    const QColor picked = QColorDialog::getColor(RgbToQColor(colorFromButton(color_button_)), this, QStringLiteral("Block color"));
    if(!picked.isValid())
    {
        return;
    }
    setColorButton(color_button_, QColorToRgb(picked));
    applyFormToSelectedBlock();
    syncGradientBar();
}

void EffectPackEditorDialog::onPickColorTo()
{
    const QColor picked = QColorDialog::getColor(RgbToQColor(colorFromButton(color_to_button_)), this, QStringLiteral("Fade end color"));
    if(!picked.isValid())
    {
        return;
    }
    setColorButton(color_to_button_, QColorToRgb(picked));
    applyFormToSelectedBlock();
    syncGradientBar();
}

QString EffectPackEditorDialog::sanitizeId(const QString& name) const
{
    QString out;
    for(QChar ch : name.toLower())
    {
        if(ch.isLetterOrNumber())
        {
            out.append(ch);
        }
        else if(ch.isSpace() || ch == '-' || ch == '_')
        {
            if(!out.isEmpty() && out.back() != '_')
            {
                out.append('_');
            }
        }
    }
    while(out.endsWith('_'))
    {
        out.chop(1);
    }
    return out.isEmpty() ? QStringLiteral("pack") : out;
}

void EffectPackEditorDialog::applyMetaToPack()
{
    pack_.name = name_edit_->text().trimmed().toStdString();
    if(pack_.name.empty())
    {
        pack_.name = "Untitled";
    }
    pack_.duration_ms = duration_spin_->value();
    const QString loop = loop_combo_->currentData().toString();
    if(loop == QStringLiteral("forever"))
    {
        pack_.loop = EffectPack::LoopMode::Forever;
    }
    else if(loop == QStringLiteral("while_active"))
    {
        pack_.loop = EffectPack::LoopMode::WhileActive;
    }
    else
    {
        pack_.loop = EffectPack::LoopMode::Once;
    }
    if(pack_path_.empty())
    {
        pack_.id = sanitizeId(QString::fromStdString(pack_.name)).toStdString();
    }
}

void EffectPackEditorDialog::onSave()
{
    stopPreview();
    applyFormToSelectedBlock();
    applyMetaToPack();
    if(pack_.tracks.empty())
    {
        QMessageBox::warning(this, QStringLiteral("Effect Pack Editor"),
                             QStringLiteral("Add at least one block on the timeline before saving."));
        return;
    }
    bool any_blocks = false;
    for(const auto& t : pack_.tracks)
    {
        if(!t.blocks.empty())
        {
            any_blocks = true;
            break;
        }
    }
    if(!any_blocks)
    {
        QMessageBox::warning(this, QStringLiteral("Effect Pack Editor"),
                             QStringLiteral("Add at least one block on the timeline before saving."));
        return;
    }
    if(packs_dir_.empty())
    {
        return;
    }
    std::error_code ec;
    filesystem::create_directories(packs_dir_, ec);
    if(pack_path_.empty())
    {
        pack_path_ = packs_dir_ / (pack_.id + EffectPack::kFileSuffix);
    }
    std::string err;
    if(!EffectPack::SaveToFile(pack_path_, pack_, &err))
    {
        QMessageBox::warning(this, QStringLiteral("Effect Pack Editor"),
                             QStringLiteral("Save failed:\n%1").arg(QString::fromStdString(err)));
        return;
    }
    setWindowTitle(QStringLiteral("Effect Pack Editor — %1").arg(QString::fromStdString(pack_.name)));
    status_label_->setText(QStringLiteral("Saved %1").arg(QString::fromStdString(pack_path_.filename().string())));
#ifdef _WIN32
    emit packSaved(QString::fromStdWString(pack_path_.wstring()));
#else
    emit packSaved(QString::fromStdString(pack_path_.string()));
#endif
}

void EffectPackEditorDialog::setPlayingUi(bool playing)
{
    preview_button_->setEnabled(!playing);
    stop_button_->setEnabled(playing);
    save_button_->setEnabled(!playing);
    updateSelectionActions();
}

void EffectPackEditorDialog::stopPreview()
{
    const bool was_playing = player_.IsPlaying();
    if(timer_ && timer_->isActive())
    {
        timer_->stop();
    }
    if(was_playing && tab_)
    {
        // Push buffered colours so hardware matches the last viewport frame.
        tab_->ApplyEffectPackPreviewFrame(pack_, player_.LocalMs(), true);
    }
    player_.Stop();
    setPlayingUi(false);
    emit previewStopped();
    if(status_label_)
    {
        status_label_->setText(QStringLiteral("Preview stopped"));
    }
}

void EffectPackEditorDialog::onPreview()
{
    if(!tab_)
    {
        return;
    }
    applyFormToSelectedBlock();
    applyMetaToPack();
    if(tab_)
    {
        auto* transforms = tab_->GetControllerTransformsMutable();
        if(transforms)
        {
            for(std::unique_ptr<ControllerTransform>& t : *transforms)
            {
                if(t)
                {
                    t->world_positions_dirty = true;
                }
            }
        }
    }
    tab_->PrepareEffectPackPreview();
    if(tab_->resource_manager && tab_->resource_manager->GetRGBControllers().empty())
    {
        QMessageBox::warning(this, QStringLiteral("Effect Pack Editor"),
                             QStringLiteral("No OpenRGB controllers available."));
        return;
    }
    emit previewStarted();
    player_.SetPack(pack_);
    player_.Play();
    wall_.restart();
    last_elapsed_ms_ = 0;
    setPlayingUi(true);
    timer_->start();
    status_label_->setText(QStringLiteral("Previewing…"));
}

void EffectPackEditorDialog::onTick()
{
    if(!tab_ || !player_.IsPlaying())
    {
        stopPreview();
        return;
    }
    const int elapsed = (int)wall_.elapsed();
    const int dt = std::max(0, elapsed - last_elapsed_ms_);
    last_elapsed_ms_ = elapsed;
    // Live editor edits already mutate pack_; keep the player copy current.
    player_.UpdatePack(pack_);
    if(!player_.Tick(dt, true))
    {
        stopPreview();
        status_label_->setText(QStringLiteral("Preview finished"));
        return;
    }
    tab_->ApplyEffectPackPreviewFrame(pack_, player_.LocalMs());
    timeline_->setPlayheadMs(player_.LocalMs());
    status_label_->setText(
        QStringLiteral("Preview %1 / %2 ms")
            .arg(player_.LocalMs())
            .arg(std::max(1, pack_.duration_ms)));
}
