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
#include "ZoneManager3D.h"
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
    for(const EffectPackCatalog::GradientEntry& g : EffectPackCatalog::GradientEntries())
    {
        gradient_preset_->addItem(QString::fromUtf8(g.label ? g.label : g.id),
                                  QString::fromUtf8(g.id ? g.id : ""));
    }
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
    axis_space_combo_->addItem(QStringLiteral("Sequence (order)"), (int)EffectPack::AxisSpace::Sequence);
    axis_space_combo_->setToolTip(
        QStringLiteral("Device: per-controller local space.\n"
                       "Room: one shared 3D box across the target (All / scene zone).\n"
                       "Sequence: wipe/chase along timeline controller order (drag to reorder)."));
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
    for(const EffectPackCatalog::CurveEntry& c : EffectPackCatalog::CurveEntries())
    {
        curve_combo_->addItem(QString::fromUtf8(c.label ? c.label : c.id),
                              QString::fromUtf8(c.id ? c.id : ""));
    }
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
    connect(timeline_, &EffectPackTimelineWidget::sceneZoneControllersReordered,
            this, &EffectPackEditorDialog::onSceneZoneControllersReordered);
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
