// SPDX-License-Identifier: GPL-2.0-only

#include "EffectPackEditorDialog.h"
#include "EffectPackCatalog.h"
#include "EffectPackUserCurves.h"
#include "EffectPackGradientBar.h"
#include "EffectPackTimelineWidget.h"
#include "EffectPackToolBar.h"
#include "EffectPackUserGradients.h"
#include "PluginSettingsPaths.h"
#include "EffectPacks/EffectScript.h"
#include "EffectPacks/EffectPackApplier.h"
#include "LEDPosition3D.h"
#include "OpenRGB3DSpatialTab.h"
#include "PluginUiUtils.h"
#include "VirtualController3D.h"
#include "ZoneManager3D.h"
#include "EffectCollapsibleSection.h"

#include <QAbstractItemView>
#include <QAbstractSpinBox>
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
#include <QSizePolicy>
#include <QSlider>
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

    const filesystem::path effect_files_dir = PluginSettingsPaths::TimelineBlocksDir(tab_ ? tab_->resource_manager : nullptr);
    effect_toolbar_ = new EffectPackToolBar(
        PluginSettingsPaths::UserGradientsFile(tab_ ? tab_->resource_manager : nullptr),
        PluginSettingsPaths::UserColorsFile(tab_ ? tab_->resource_manager : nullptr),
        effect_files_dir,
        PluginSettingsPaths::UserCurvesFile(tab_ ? tab_->resource_manager : nullptr));
    root->addWidget(effect_toolbar_);

    auto* splitter = new QSplitter(Qt::Horizontal);

    auto* timeline_scroll = new QScrollArea();
    timeline_scroll->setWidgetResizable(true);
    timeline_scroll->setFrameShape(QFrame::NoFrame);
    timeline_ = new EffectPackTimelineWidget();
    timeline_->setEffectFilesDir(effect_files_dir);
    timeline_scroll->setWidget(timeline_);
    splitter->addWidget(timeline_scroll);

    auto* props_wrap = new QWidget();
    auto* props_layout = new QVBoxLayout(props_wrap);
    props_layout->setContentsMargins(4, 4, 4, 4);

    auto* palette_row = new QHBoxLayout();
    remove_block_button_ = new QPushButton(QStringLiteral("Remove effect"));
    remove_block_button_->setToolTip(QStringLiteral("Delete selected block (Delete / Backspace)"));
    props_hint_ = new QLabel(QStringLiteral("Select a block to edit"));
    props_hint_->setWordWrap(true);
    PluginUiApplyMutedSecondaryLabel(props_hint_);
    palette_row->addWidget(props_hint_, 1);
    palette_row->addWidget(remove_block_button_, 0, Qt::AlignTop);
    props_layout->addLayout(palette_row);

    auto* props_scroll = new QScrollArea();
    props_scroll->setWidgetResizable(true);
    props_scroll->setFrameShape(QFrame::NoFrame);
    auto* props_inner = new QWidget();
    auto* props_inner_layout = new QVBoxLayout(props_inner);
    props_inner_layout->setContentsMargins(0, 0, 0, 0);

    name_edit_ = new QLineEdit();
    duration_spin_ = new QSpinBox();
    duration_spin_->setRange(100, EffectPack::kMaxDurationMs);
    duration_spin_->setSingleStep(100);
    duration_spin_->setSuffix(QStringLiteral(" ms"));
    duration_spin_->setKeyboardTracking(false);
    duration_spin_->setValue(5000);
    loop_combo_ = new QComboBox();
    loop_combo_->addItem(QStringLiteral("Once"), QStringLiteral("once"));
    loop_combo_->addItem(QStringLiteral("Forever"), QStringLiteral("forever"));
    loop_combo_->addItem(QStringLiteral("While active"), QStringLiteral("while_active"));
    controllers_button_ = new QPushButton(QStringLiteral("Controllers…"));
    controllers_button_->setToolTip(QStringLiteral("Choose which scene controllers appear on this pack’s timeline"));
    map_controllers_button_ = new QPushButton(QStringLiteral("Map…"));
    map_controllers_button_->setToolTip(QStringLiteral("Point this pack’s controller names at controllers in your scene"));
    auto* timeline_buttons = new QWidget();
    auto* timeline_row = new QHBoxLayout(timeline_buttons);
    timeline_row->setContentsMargins(0, 0, 0, 0);
    timeline_row->setSpacing(6);
    timeline_row->addWidget(controllers_button_, 1);
    timeline_row->addWidget(map_controllers_button_);

    auto* pack_sec = new EffectCollapsibleSection(QStringLiteral("Pack"));
    pack_sec->setExpanded(true);
    pack_sec->bodyLayout()->setSpacing(6);
    pack_sec->bodyLayout()->setContentsMargins(2, 4, 2, 2);
    auto* pack_form = new QFormLayout();
    pack_form->setContentsMargins(0, 0, 0, 0);
    pack_form->setSpacing(6);
    pack_form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    pack_form->addRow(QStringLiteral("Name"), name_edit_);
    pack_form->addRow(QStringLiteral("Duration"), duration_spin_);
    pack_form->addRow(QStringLiteral("Loop"), loop_combo_);
    pack_form->addRow(QStringLiteral("Timeline"), timeline_buttons);
    pack_sec->bodyLayout()->addLayout(pack_form);
    props_inner_layout->addWidget(pack_sec);

    auto* effect_sec = new EffectCollapsibleSection(QStringLiteral("Effect"));
    effect_sec->setExpanded(true);
    effect_sec->bodyLayout()->setSpacing(6);
    effect_sec->bodyLayout()->setContentsMargins(2, 4, 2, 2);
    auto* effect_form = new QFormLayout();
    effect_form->setContentsMargins(0, 0, 0, 0);
    effect_form->setSpacing(6);
    effect_form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    type_combo_ = new QComboBox();
    for(const EffectPackCatalog::Entry& e : EffectPackCatalog::LoadEntries(effect_files_dir))
    {
        type_combo_->addItem(EffectPackCatalog::MakeEffectIcon(e), e.name, e.id);
    }
    start_spin_ = new QSpinBox();
    start_spin_->setRange(0, EffectPack::kMaxDurationMs);
    start_spin_->setSuffix(QStringLiteral(" ms"));
    start_spin_->setKeyboardTracking(false);
    end_spin_ = new QSpinBox();
    end_spin_->setRange(1, EffectPack::kMaxDurationMs);
    end_spin_->setSuffix(QStringLiteral(" ms"));
    end_spin_->setKeyboardTracking(false);
    auto attachTimeSlider = [](QSpinBox* spin, const QString& tip) {
        auto* slider = new QSlider(Qt::Horizontal);
        slider->setRange(spin->minimum(), spin->maximum());
        slider->setValue(spin->value());
        slider->setSingleStep(10);
        slider->setPageStep(100);
        slider->setFixedHeight(14);
        slider->setToolTip(tip);
        QObject::connect(slider, &QSlider::valueChanged, spin, &QSpinBox::setValue);
        QObject::connect(spin, QOverload<int>::of(&QSpinBox::valueChanged), slider, [slider](int value) {
            const bool blocked = slider->blockSignals(true);
            slider->setValue(value);
            slider->blockSignals(blocked);
        });
        auto* wrap = new QWidget();
        auto* lay = new QVBoxLayout(wrap);
        lay->setContentsMargins(0, 0, 0, 0);
        lay->setSpacing(0);
        lay->addWidget(spin);
        lay->addWidget(slider);
        return std::pair<QWidget*, QSlider*>{wrap, slider};
    };
    const auto start_field = attachTimeSlider(start_spin_, QStringLiteral("Drag to set the start time"));
    const auto end_field = attachTimeSlider(end_spin_, QStringLiteral("Drag to set the end time"));
    start_slider_ = start_field.second;
    end_slider_ = end_field.second;
    syncTimeSliderRanges();
    effect_form->addRow(QStringLiteral("Type"), type_combo_);
    effect_form->addRow(QStringLiteral("Start"), start_field.first);
    effect_form->addRow(QStringLiteral("End"), end_field.first);
    effect_sec->bodyLayout()->addLayout(effect_form);
    props_inner_layout->addWidget(effect_sec);

    auto* color_sec = new EffectCollapsibleSection(QStringLiteral("Color"));
    color_sec->setExpanded(true);
    color_sec->bodyLayout()->setSpacing(6);
    color_sec->bodyLayout()->setContentsMargins(2, 4, 2, 2);
    color_button_ = new QPushButton(QStringLiteral("Pick…"));
    color_button_->setMinimumHeight(28);
    color_button_->setMaximumHeight(28);
    color_button_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    color_to_button_ = new QPushButton(QStringLiteral("Pick…"));
    color_to_button_->setMinimumHeight(28);
    color_to_button_->setMaximumHeight(28);
    color_to_button_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    color_to_row_ = new QWidget();
    auto* color_to_layout = new QHBoxLayout(color_to_row_);
    color_to_layout->setContentsMargins(0, 0, 0, 0);
    color_to_layout->setSpacing(6);
    gradient_preset_ = new QComboBox();
    save_gradient_button_ = new QPushButton(QStringLiteral("Save"));
    save_gradient_button_->setToolTip(QStringLiteral("Save the current gradient as a preset"));
    delete_gradient_button_ = new QPushButton(QStringLiteral("Delete"));
    delete_gradient_button_->setToolTip(QStringLiteral("Remove the selected gradient preset"));
    delete_gradient_button_->setEnabled(false);
    gradient_bar_ = new EffectPackGradientBar();
    auto* color_form = new QFormLayout();
    color_form->setContentsMargins(0, 0, 0, 0);
    color_form->setSpacing(6);
    color_form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    color_form->addRow(QStringLiteral("Primary"), color_button_);
    color_sec->bodyLayout()->addLayout(color_form);
    color_to_layout->addWidget(new QLabel(QStringLiteral("End")));
    color_to_layout->addWidget(color_to_button_, 1);
    color_sec->bodyLayout()->addWidget(color_to_row_);
    auto* preset_form = new QFormLayout();
    preset_form->setContentsMargins(0, 0, 0, 0);
    preset_form->setSpacing(6);
    preset_form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    auto* preset_row = new QWidget();
    auto* preset_layout = new QHBoxLayout(preset_row);
    preset_layout->setContentsMargins(0, 0, 0, 0);
    preset_layout->setSpacing(6);
    preset_layout->addWidget(gradient_preset_, 1);
    preset_layout->addWidget(save_gradient_button_);
    preset_layout->addWidget(delete_gradient_button_);
    preset_form->addRow(QStringLiteral("Preset"), preset_row);
    refillGradientPresets();
    color_sec->bodyLayout()->addLayout(preset_form);
    color_sec->bodyLayout()->addWidget(new QLabel(QStringLiteral("Color gradient")));
    color_sec->bodyLayout()->addWidget(gradient_bar_);
    auto* grad_help = new QLabel(
        QStringLiteral("Drag stops · click bar to add · double-click recolour · right-click remove"));
    grad_help->setWordWrap(true);
    PluginUiApplyMutedSecondaryLabel(grad_help);
    color_sec->bodyLayout()->addWidget(grad_help);
    props_inner_layout->addWidget(color_sec);

    auto* bright_sec = new EffectCollapsibleSection(QStringLiteral("Brightness"));
    bright_sec->setExpanded(false);
    bright_sec->bodyLayout()->setSpacing(6);
    bright_sec->bodyLayout()->setContentsMargins(2, 4, 2, 2);
    intensity_spin_ = new QSpinBox();
    intensity_spin_->setRange(1, 100);
    intensity_spin_->setValue(100);
    min_intensity_spin_ = new QSpinBox();
    min_intensity_spin_->setRange(0, 100);
    min_intensity_spin_->setValue(15);
    auto* bright_form = new QFormLayout();
    bright_form->setContentsMargins(0, 0, 0, 0);
    bright_form->setSpacing(6);
    bright_form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
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
    static_cast<EffectCollapsibleSection*>(direction_section_)->setExpanded(false);
    static_cast<EffectCollapsibleSection*>(direction_section_)->bodyLayout()->setSpacing(6);
    static_cast<EffectCollapsibleSection*>(direction_section_)->bodyLayout()->setContentsMargins(2, 4, 2, 2);
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
            QStringLiteral("Toward Front wall (Z=0 / room −Z). Wipe: Back → Front."));
    add_dir(QStringLiteral("Back"), EffectPack::Direction::Back,
            QStringLiteral("Toward Back wall (+Z). Wipe: Front → Back."));
    direction_combo_->insertSeparator(direction_combo_->count());
    add_dir(QStringLiteral("+X"), EffectPack::Direction::PosX, QStringLiteral("Explicit room +X (toward Right wall)"));
    add_dir(QStringLiteral("−X"), EffectPack::Direction::NegX, QStringLiteral("Explicit room −X (toward Left wall)"));
    add_dir(QStringLiteral("+Y"), EffectPack::Direction::PosY, QStringLiteral("Explicit room +Y (toward Ceiling)"));
    add_dir(QStringLiteral("−Y"), EffectPack::Direction::NegY, QStringLiteral("Explicit room −Y (toward Floor)"));
    add_dir(QStringLiteral("+Z"), EffectPack::Direction::PosZ, QStringLiteral("Explicit room +Z (toward Back wall)"));
    add_dir(QStringLiteral("−Z"), EffectPack::Direction::NegZ, QStringLiteral("Explicit room −Z (toward Front wall)"));
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
    dir_form->setContentsMargins(0, 0, 0, 0);
    dir_form->setSpacing(6);
    dir_form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    dir_form->addRow(QStringLiteral("Space"), axis_space_combo_);
    dir_form->addRow(QStringLiteral("Axis"), axis_mode_combo_);
    dir_form->addRow(QStringLiteral("Direction"), direction_combo_);
    dir_form->addRow(QStringLiteral("Yaw"), axis_yaw_spin_);
    dir_form->addRow(QStringLiteral("Pitch"), axis_pitch_spin_);
    static_cast<EffectCollapsibleSection*>(direction_section_)->bodyLayout()->addLayout(dir_form);
    props_inner_layout->addWidget(direction_section_);

    curve_combo_ = new QComboBox();
    const filesystem::path curves_path = PluginSettingsPaths::UserCurvesFile(tab_ ? tab_->resource_manager : nullptr);
    for(const EffectPackUserCurves::Entry& c : EffectPackUserCurves::Load(curves_path))
    {
        curve_combo_->addItem(c.label.isEmpty() ? c.id : c.label, c.id);
    }
    curve_combo_->addItem(QStringLiteral("Custom"), QStringLiteral("custom"));
    auto* curve_sec = new EffectCollapsibleSection(QStringLiteral("Intensity Curve"));
    curve_sec->setExpanded(false);
    auto* curve_form = new QFormLayout();
    curve_form->addRow(QStringLiteral("Preset"), curve_combo_);
    curve_sec->bodyLayout()->addLayout(curve_form);
    props_inner_layout->addWidget(curve_sec);

    speed_section_ = new EffectCollapsibleSection(QStringLiteral("Speed"));
    static_cast<EffectCollapsibleSection*>(speed_section_)->setExpanded(false);
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
    static_cast<EffectCollapsibleSection*>(pulse_section_)->setExpanded(false);
    pulse_length_spin_ = new QSpinBox();
    pulse_length_spin_->setRange(2, 100);
    pulse_length_spin_->setValue(25);
    auto* pulse_form = new QFormLayout();
    pulse_form->addRow(QStringLiteral("Length / Duty %"), pulse_length_spin_);
    static_cast<EffectCollapsibleSection*>(pulse_section_)->bodyLayout()->addLayout(pulse_form);
    props_inner_layout->addWidget(pulse_section_);

    props_inner_layout->setSpacing(8);
    props_inner_layout->addStretch(1);
    props_scroll->setWidget(props_inner);
    props_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    props_layout->addWidget(props_scroll, 1);
    props_wrap->setMinimumWidth(300);
    props_wrap->setMaximumWidth(420);
    splitter->addWidget(props_wrap);

    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 0);
    splitter->setSizes({900, 340});
    root->addWidget(splitter, 1);

    status_label_ = new QLabel(QStringLiteral("Idle"));
    PluginUiApplyMutedSecondaryLabel(status_label_);
    root->addWidget(status_label_);

    auto* action_row = new QHBoxLayout();
    preview_button_ = new QPushButton(QStringLiteral("Preview"));
    stop_button_ = new QPushButton(QStringLiteral("Stop"));
    stop_button_->setEnabled(false);
    save_button_ = new QPushButton(QStringLiteral("Save"));
    PluginUiApplyPrimaryButton(save_button_);
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
    connect(map_controllers_button_, &QPushButton::clicked, this, &EffectPackEditorDialog::onMapControllers);
    connect(timeline_, &EffectPackTimelineWidget::playheadChanged, this, &EffectPackEditorDialog::onPlayheadChanged);
    connect(timeline_, &EffectPackTimelineWidget::blockSelected, this, &EffectPackEditorDialog::onBlockSelected);
    connect(timeline_, &EffectPackTimelineWidget::blockEdited, this, &EffectPackEditorDialog::onBlockSelected);
    connect(timeline_, &EffectPackTimelineWidget::blockDeleteRequested, this, &EffectPackEditorDialog::onBlockDeleteRequested);
    connect(timeline_, &EffectPackTimelineWidget::effectAddRequested, this, &EffectPackEditorDialog::onEffectAddRequested);
    connect(timeline_, &EffectPackTimelineWidget::gradientPresetApplied, this, &EffectPackEditorDialog::onGradientPresetApplied);
    connect(timeline_, &EffectPackTimelineWidget::curvePresetApplied, this, &EffectPackEditorDialog::onCurvePresetApplied);
    connect(timeline_, &EffectPackTimelineWidget::colorDropped, this, &EffectPackEditorDialog::onColorDropped);
    connect(timeline_, &EffectPackTimelineWidget::gradientDropped, this, &EffectPackEditorDialog::onGradientDropped);
    connect(timeline_, &EffectPackTimelineWidget::sceneZoneControllersReordered,
            this, &EffectPackEditorDialog::onSceneZoneControllersReordered);
    connect(effect_toolbar_, &EffectPackToolBar::effectClicked, this, &EffectPackEditorDialog::onToolbarEffectClicked);
    connect(effect_toolbar_, &EffectPackToolBar::colorClicked, this, &EffectPackEditorDialog::onToolbarColorClicked);
    connect(effect_toolbar_, &EffectPackToolBar::gradientPresetClicked, this, &EffectPackEditorDialog::onToolbarGradientClicked);
    connect(effect_toolbar_, &EffectPackToolBar::gradientPresetOverwriteRequested, this, &EffectPackEditorDialog::onOverwriteGradientPreset);
    connect(effect_toolbar_, &EffectPackToolBar::gradientPresetDeleteRequested, this, &EffectPackEditorDialog::onDeleteGradientPreset);
    connect(effect_toolbar_, &EffectPackToolBar::gradientPresetResetRequested, this, &EffectPackEditorDialog::onResetGradientPreset);
    connect(effect_toolbar_, &EffectPackToolBar::gradientPresetOverwriteRequested, this, &EffectPackEditorDialog::onOverwriteGradientPreset);
    connect(effect_toolbar_, &EffectPackToolBar::gradientPresetDeleteRequested, this, &EffectPackEditorDialog::onDeleteGradientPreset);
    connect(effect_toolbar_, &EffectPackToolBar::gradientPresetResetRequested, this, &EffectPackEditorDialog::onResetGradientPreset);
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
    connect(save_gradient_button_, &QPushButton::clicked, this, &EffectPackEditorDialog::onSaveUserGradient);
    connect(delete_gradient_button_, &QPushButton::clicked, this, &EffectPackEditorDialog::onDeleteUserGradient);
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
    bool needs_map = false;
    for(const std::string& name : packDeviceNames())
    {
        if(!deviceNameInScene(name))
        {
            needs_map = true;
            break;
        }
    }
    if(needs_map)
    {
        promptMapControllers();
    }
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

std::vector<std::string> EffectPackEditorDialog::packDeviceNames() const
{
    std::vector<std::string> names;
    auto add = [&](const std::string& name) {
        if(name.empty())
        {
            return;
        }
        if(std::find(names.begin(), names.end(), name) == names.end())
        {
            names.push_back(name);
        }
    };
    for(const std::string& device : pack_.devices)
    {
        add(device);
    }
    for(const EffectPack::Track& track : pack_.tracks)
    {
        add(track.target.device_name);
    }
    return names;
}

bool EffectPackEditorDialog::deviceNameInScene(const std::string& name) const
{
    if(!tab_ || name.empty())
    {
        return false;
    }
    const auto& transforms = tab_->GetControllerTransforms();
    for(int i = 0; i < (int)transforms.size(); ++i)
    {
        ControllerTransform* transform = transforms[(size_t)i].get();
        if(!transform || transform->hidden_by_virtual)
        {
            continue;
        }
        const std::string key = ControllerKeyName(transform, i);
        if(EffectPack::NameMatches(key, name) || EffectPack::NameMatches(name, key))
        {
            return true;
        }
    }
    return false;
}

bool EffectPackEditorDialog::promptMapControllers()
{
    const std::vector<std::string> names = packDeviceNames();
    if(names.empty())
    {
        QMessageBox::information(this, QStringLiteral("Effect Pack Editor"),
                                 QStringLiteral("This pack has no per-controller rows to map."));
        return false;
    }
    if(!tab_)
    {
        return false;
    }

    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("Map controllers"));
    auto* layout = new QVBoxLayout(&dlg);
    layout->addWidget(new QLabel(
        QStringLiteral("Point each controller from this pack at one of yours.\n"
                       "LED and zone rows copy in order. A 10-row mouse mapped onto a 2-row mouse keeps the first two and drops the rest.")));

    struct Row
    {
        std::string from;
        QComboBox* combo = nullptr;
    };
    std::vector<Row> rows;
    auto* form = new QFormLayout();
    const auto& transforms = tab_->GetControllerTransforms();
    for(const std::string& name : names)
    {
        auto* combo = new QComboBox();
        combo->addItem(QStringLiteral("Not mapped"), QString());
        int matched = 0;
        for(int i = 0; i < (int)transforms.size(); ++i)
        {
            ControllerTransform* transform = transforms[(size_t)i].get();
            if(!transform || transform->hidden_by_virtual)
            {
                continue;
            }
            const std::string key = ControllerKeyName(transform, i);
            combo->addItem(ControllerLabel(transform, i), QString::fromStdString(key));
            if(EffectPack::NameMatches(key, name) || EffectPack::NameMatches(name, key))
            {
                matched = combo->count() - 1;
            }
        }
        combo->setCurrentIndex(matched);
        form->addRow(QString::fromStdString(name), combo);
        rows.push_back({name, combo});
    }
    layout->addLayout(form);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    layout->addWidget(buttons);
    if(dlg.exec() != QDialog::Accepted)
    {
        return false;
    }

    int dropped = 0;
    for(const Row& row : rows)
    {
        const std::string to = row.combo->currentData().toString().toStdString();
        if(to.empty() || to == row.from)
        {
            continue;
        }

        ControllerTransform* dest = nullptr;
        int dest_index = -1;
        for(int i = 0; i < (int)transforms.size(); ++i)
        {
            ControllerTransform* transform = transforms[(size_t)i].get();
            if(!transform || transform->hidden_by_virtual)
            {
                continue;
            }
            if(ControllerKeyName(transform, i) == to)
            {
                dest = transform;
                dest_index = i;
                break;
            }
        }

        std::vector<EffectPack::Target> dest_zones;
        std::vector<EffectPack::Target> dest_leds;
        if(dest && dest_index >= 0)
        {
            const EffectPackTimelineWidget::Node node = buildControllerNode(dest, dest_index);
            for(const EffectPackTimelineWidget::Node& zone : node.children)
            {
                dest_zones.push_back(zone.target);
                for(const EffectPackTimelineWidget::Node& led : zone.children)
                {
                    dest_leds.push_back(led.target);
                }
            }
        }

        std::vector<size_t> led_tracks;
        std::vector<size_t> zone_tracks;
        for(size_t i = 0; i < pack_.tracks.size(); ++i)
        {
            const EffectPack::Target& target = pack_.tracks[i].target;
            if(target.device_name != row.from)
            {
                continue;
            }
            if(target.kind == EffectPack::TargetKind::Leds)
            {
                led_tracks.push_back(i);
            }
            else if(target.kind == EffectPack::TargetKind::Zone)
            {
                zone_tracks.push_back(i);
            }
            else
            {
                pack_.tracks[i].target.device_name = to;
            }
        }
        std::sort(led_tracks.begin(), led_tracks.end(), [&](size_t a, size_t b) {
            const int ia = pack_.tracks[a].target.led_indices.empty() ? 0 : pack_.tracks[a].target.led_indices.front();
            const int ib = pack_.tracks[b].target.led_indices.empty() ? 0 : pack_.tracks[b].target.led_indices.front();
            return ia < ib;
        });

        std::vector<char> drop(pack_.tracks.size(), 0);
        for(size_t i = 0; i < led_tracks.size(); ++i)
        {
            if(i < dest_leds.size())
            {
                EffectPack::Target& target = pack_.tracks[led_tracks[i]].target;
                target.device_name = to;
                target.zone_name = dest_leds[i].zone_name;
                target.led_indices = dest_leds[i].led_indices;
            }
            else
            {
                drop[led_tracks[i]] = 1;
                ++dropped;
            }
        }
        for(size_t i = 0; i < zone_tracks.size(); ++i)
        {
            if(i < dest_zones.size())
            {
                EffectPack::Target& target = pack_.tracks[zone_tracks[i]].target;
                target.device_name = to;
                target.zone_name = dest_zones[i].zone_name;
            }
            else
            {
                drop[zone_tracks[i]] = 1;
                ++dropped;
            }
        }

        std::vector<EffectPack::Track> kept;
        kept.reserve(pack_.tracks.size());
        for(size_t i = 0; i < pack_.tracks.size(); ++i)
        {
            if(!drop[i])
            {
                kept.push_back(std::move(pack_.tracks[i]));
            }
        }
        pack_.tracks = std::move(kept);

        bool replaced = false;
        for(std::string& device : pack_.devices)
        {
            if(device == row.from)
            {
                device = to;
                replaced = true;
            }
        }
        if(!replaced && std::find(pack_.devices.begin(), pack_.devices.end(), to) == pack_.devices.end())
        {
            pack_.devices.push_back(to);
        }
    }
    onRebuildTimelineModel();
    status_label_->setText(dropped > 0
                                ? QStringLiteral("Mapped. %1 extra row(s) did not fit and were dropped.").arg(dropped)
                                : QStringLiteral("Controller rows mapped to this scene"));
    return true;
}

void EffectPackEditorDialog::onMapControllers()
{
    promptMapControllers();
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

void EffectPackEditorDialog::syncTimeSliderRanges()
{
    const int dur = std::max(1, duration_spin_ ? duration_spin_->value() : 1);
    auto fit = [](QSlider* slider, int min_v, int max_v) {
        if(!slider)
        {
            return;
        }
        const bool blocked = slider->blockSignals(true);
        slider->setRange(min_v, std::max(min_v, max_v));
        slider->blockSignals(blocked);
    };
    fit(start_slider_, 0, std::max(0, dur - 1));
    fit(end_slider_, 1, dur);
}

void EffectPackEditorDialog::loadIntoUi(const EffectPack::Pack& pack)
{
    suppress_ui_ = true;
    name_edit_->setText(QString::fromStdString(pack.name));
    duration_spin_->setValue(pack.duration_ms);
    syncTimeSliderRanges();
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
    updateSelectionActions();
}

void EffectPackEditorDialog::onDurationChanged(int value)
{
    if(suppress_ui_)
    {
        return;
    }
    pack_.duration_ms = value;
    syncTimeSliderRanges();
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
    const EffectPack::Block* block = selectedBlock();
    const bool has = block != nullptr;
    if(remove_block_button_)
    {
        remove_block_button_->setEnabled(has && !player_.IsPlaying());
    }
    if(props_hint_)
    {
        QString title = QStringLiteral("Select a block to edit");
        if(has)
        {
            title = QString::fromStdString(EffectPack::BlockFileId(*block));
            const QList<EffectPackCatalog::Entry> entries = EffectPackCatalog::LoadEntries(
                PluginSettingsPaths::TimelineBlocksDir(tab_ ? tab_->resource_manager : nullptr));
            for(const EffectPackCatalog::Entry& entry : entries)
            {
                if(entry.id == title)
                {
                    title = entry.name;
                    break;
                }
            }
        }
        props_hint_->setText(title);
    }
    if(delete_gradient_button_ && gradient_preset_)
    {
        delete_gradient_button_->setEnabled(!gradient_preset_->currentData().toString().isEmpty());
    }
    if(effect_toolbar_)
    {
        bool any_block = false;
        for(const EffectPack::Track& track : pack_.tracks)
        {
            if(!track.blocks.empty())
            {
                any_block = true;
                break;
            }
        }
        effect_toolbar_->setCurvesEnabled(any_block);
    }
}

void EffectPackEditorDialog::onColorDropped(int row_index, int ms, unsigned int rgb)
{
    addBlockAt(row_index, ms);
    if(EffectPack::Block* block = selectedBlock())
    {
        const RGBColor color = (RGBColor)rgb;
        block->color = color;
        block->color_from = color;
        block->color_to = color;
        block->gradient = {{0.0f, color}};
        applyBlockToForm();
        if(timeline_)
        {
            timeline_->update();
        }
    }
    updateSelectionActions();
}

void EffectPackEditorDialog::onGradientDropped(int row_index, int ms, const QString& preset_id)
{
    addBlockAt(row_index, ms);
    applyGradientPresetToBlock(selectedBlock(), preset_id);
    applyBlockToForm();
    updateSelectionActions();
}

void EffectPackEditorDialog::onEffectAddRequested(int row_index, int ms, const QString& effect_id)
{
    addBlockAt(row_index, ms, effect_id);
}

void EffectPackEditorDialog::onBlockSelected(int track_index, int block_index)
{
    if(timeline_)
    {
        timeline_->cancelDrag();
    }
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
    if(!timeline_)
    {
        return -1;
    }
    const int selected = timeline_->selectedRowIndex();
    if(selected >= 0 && selected < timeline_->rows().size())
    {
        return selected;
    }
    return 0;
}

void EffectPackEditorDialog::onToolbarEffectClicked(const QString& effect_id)
{
    addBlockAt(currentTimelineRow(), timeline_ ? timeline_->playheadMs() : 0, effect_id);
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
    suppress_ui_ = true;
    blocks.erase(blocks.begin() + selected_block_);
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
