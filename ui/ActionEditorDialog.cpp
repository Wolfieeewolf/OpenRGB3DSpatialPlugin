// SPDX-License-Identifier: GPL-2.0-only

#include "ActionEditorDialog.h"
#include "ActionCatalog.h"
#include "ActionCurveBar.h"
#include "ActionUserCurves.h"
#include "ActionGradientBar.h"
#include "ActionTimelineWidget.h"
#include "ActionToolBar.h"
#include "ActionUserGradients.h"
#include "PluginSettingsPaths.h"
#include "Actions/ActionScript.h"
#include "Actions/ActionApplier.h"
#include "LEDPosition3D.h"
#include "OpenRGB3DSpatialTab.h"
#include "PluginUiUtils.h"
#include "VirtualController3D.h"
#include "ZoneManager3D.h"
#include "EffectCollapsibleSection.h"

#include <QAbstractItemView>
#include <QAbstractSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QInputDialog>
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
#include <QToolButton>
#include <QMenu>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <string>
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

ActionEditorDialog::ActionEditorDialog(OpenRGB3DSpatialTab* tab, QWidget* parent)
    : QDialog(parent)
    , tab_(tab)
{
    setWindowTitle(QStringLiteral("Action Editor"));
    setWindowFlags(windowFlags() | Qt::Window);
    setAttribute(Qt::WA_DeleteOnClose, false);
    resize(1100, 640);
    buildUi();

    timer_ = new QTimer(this);
    timer_->setInterval(33);
    connect(timer_, &QTimer::timeout, this, &ActionEditorDialog::onTick);

    timeline_refresh_timer_ = new QTimer(this);
    timeline_refresh_timer_->setSingleShot(true);
    timeline_refresh_timer_->setInterval(64);
    connect(timeline_refresh_timer_, &QTimer::timeout, this, [this]() {
        if(timeline_)
        {
            timeline_->requestContentRepaint();
        }
    });
}

ActionEditorDialog::~ActionEditorDialog()
{
    stopPreview();
}

void ActionEditorDialog::buildUi()
{
    auto* root = new QVBoxLayout(this);

    const filesystem::path effect_files_dir = PluginSettingsPaths::ActionToolboxDir(tab_ ? tab_->resource_manager : nullptr);
    Action::SetActionScriptDirectory(effect_files_dir);
    Action::ReloadActionScripts();
    effect_toolbar_ = new ActionToolBar(
        PluginSettingsPaths::ActionEditorGradientsFile(tab_ ? tab_->resource_manager : nullptr),
        PluginSettingsPaths::ActionEditorColorsFile(tab_ ? tab_->resource_manager : nullptr),
        effect_files_dir,
        PluginSettingsPaths::ActionEditorCurvesFile(tab_ ? tab_->resource_manager : nullptr));
    root->addWidget(effect_toolbar_);

    auto* splitter = new QSplitter(Qt::Horizontal);

    auto* timeline_scroll = new QScrollArea();
    timeline_scroll->setWidgetResizable(true);
    timeline_scroll->setFrameShape(QFrame::NoFrame);
    timeline_ = new ActionTimelineWidget();
    timeline_->setEffectFilesDir(effect_files_dir);
    timeline_scroll->setWidget(timeline_);
    splitter->addWidget(timeline_scroll);

    auto* props_wrap = new QWidget();
    auto* props_layout = new QVBoxLayout(props_wrap);
    props_layout->setContentsMargins(4, 4, 4, 4);

    auto* palette_row = new QHBoxLayout();
    remove_clip_button_ = new QPushButton(QStringLiteral("Remove effect"));
    remove_clip_button_->setToolTip(QStringLiteral("Delete selected clip (Delete / Backspace)"));
    props_hint_ = new QLabel(QStringLiteral("Select a clip to edit"));
    props_hint_->setWordWrap(true);
    PluginUiApplyMutedSecondaryLabel(props_hint_);
    palette_row->addWidget(props_hint_, 1);
    palette_row->addWidget(remove_clip_button_, 0, Qt::AlignTop);
    props_layout->addLayout(palette_row);

    auto* props_scroll = new QScrollArea();
    props_scroll->setWidgetResizable(true);
    props_scroll->setFrameShape(QFrame::NoFrame);
    auto* props_inner = new QWidget();
    auto* props_inner_layout = new QVBoxLayout(props_inner);
    props_inner_layout->setContentsMargins(0, 0, 0, 0);

    name_edit_ = new QLineEdit();
    duration_spin_ = new QSpinBox();
    duration_spin_->setRange(100, Action::kMaxDurationMs);
    duration_spin_->setSingleStep(100);
    duration_spin_->setSuffix(QStringLiteral(" ms"));
    duration_spin_->setKeyboardTracking(false);
    duration_spin_->setValue(5000);
    loop_combo_ = new QComboBox();
    loop_combo_->addItem(QStringLiteral("Once"), QStringLiteral("once"));
    loop_combo_->addItem(QStringLiteral("Forever"), QStringLiteral("forever"));
    loop_combo_->addItem(QStringLiteral("While active"), QStringLiteral("while_active"));
    controllers_button_ = new QPushButton(QStringLiteral("Controllers..."));
    controllers_button_->setToolTip(QStringLiteral("Choose which scene controllers appear on this pack's timeline"));
    map_controllers_button_ = new QPushButton(QStringLiteral("Map..."));
    map_controllers_button_->setToolTip(QStringLiteral("Point this pack's controller names at controllers in your scene"));
    auto* timeline_buttons = new QWidget();
    auto* timeline_row = new QHBoxLayout(timeline_buttons);
    timeline_row->setContentsMargins(0, 0, 0, 0);
    timeline_row->setSpacing(6);
    timeline_row->addWidget(controllers_button_, 1);
    timeline_row->addWidget(map_controllers_button_);

    auto attachIntSpinSlider = [](QSpinBox* spin, const QString& tip, int single_step = 1, int page_step = 10) {
        auto* slider = new QSlider(Qt::Horizontal);
        slider->setRange(spin->minimum(), spin->maximum());
        slider->setValue(spin->value());
        slider->setSingleStep(single_step);
        slider->setPageStep(page_step);
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
    auto attachDoubleSpinSlider = [](QDoubleSpinBox* spin, const QString& tip) {
        int scale = 1;
        for(int i = 0; i < spin->decimals(); ++i)
        {
            scale *= 10;
        }
        auto* slider = new QSlider(Qt::Horizontal);
        const int min_i = (int)std::lround(spin->minimum() * (double)scale);
        const int max_i = (int)std::lround(spin->maximum() * (double)scale);
        slider->setRange(min_i, std::max(min_i, max_i));
        slider->setValue((int)std::lround(spin->value() * (double)scale));
        slider->setSingleStep(std::max(1, (int)std::lround(spin->singleStep() * (double)scale)));
        slider->setPageStep(std::max(slider->singleStep() * 5, slider->singleStep()));
        slider->setFixedHeight(14);
        slider->setToolTip(tip);
        QObject::connect(slider, &QSlider::valueChanged, spin, [spin, scale](int value) {
            spin->setValue((double)value / (double)scale);
        });
        QObject::connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), slider,
                         [slider, scale](double value) {
                             const bool blocked = slider->blockSignals(true);
                             slider->setValue((int)std::lround(value * (double)scale));
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

    const auto duration_field = attachIntSpinSlider(duration_spin_,
                                                    QStringLiteral("Drag to set pack duration"),
                                                    100,
                                                    500);

    auto* pack_sec = new EffectCollapsibleSection(QStringLiteral("Pack"));
    pack_sec->setExpanded(true);
    pack_sec->bodyLayout()->setSpacing(6);
    pack_sec->bodyLayout()->setContentsMargins(2, 4, 2, 2);
    auto* pack_form = new QFormLayout();
    pack_form->setContentsMargins(0, 0, 0, 0);
    pack_form->setSpacing(6);
    pack_form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    save_dest_combo_ = new QComboBox();
    save_dest_combo_->addItem(QStringLiteral("Triggers"), QStringLiteral("triggers"));
    save_dest_combo_->addItem(QStringLiteral("Movie Sync"), QStringLiteral("movie-sync"));
    save_dest_combo_->setToolTip(QStringLiteral(
        "Triggers -> triggers/effects/\n"
        "Movie Sync -> movie-sync/effects/<category>/"));
    movie_category_combo_ = new QComboBox();
    for(const char* cat : {"explosions", "pulses", "washes", "wraps"})
    {
        movie_category_combo_->addItem(QString::fromUtf8(cat), QString::fromUtf8(cat));
    }
    movie_category_combo_->setToolTip(QStringLiteral("Category folder under movie-sync/effects/"));

    pack_form->addRow(QStringLiteral("Name"), name_edit_);
    pack_form->addRow(QStringLiteral("Duration"), duration_field.first);
    pack_form->addRow(QStringLiteral("Loop"), loop_combo_);
    pack_form->addRow(QStringLiteral("Save for"), save_dest_combo_);
    const int category_row = pack_form->rowCount();
    pack_form->addRow(QStringLiteral("Category"), movie_category_combo_);
    pack_form->setRowVisible(category_row, false);
    connect(save_dest_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this, pack_form, category_row](int) {
                const bool film = save_dest_combo_->currentData().toString() == QStringLiteral("movie-sync");
                pack_form->setRowVisible(category_row, film);
            });
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
    for(const ActionCatalog::Entry& e : ActionCatalog::LoadEntries(effect_files_dir))
    {
        type_combo_->addItem(ActionCatalog::MakeEffectIcon(e), e.name, e.id);
    }
    start_spin_ = new QSpinBox();
    start_spin_->setRange(0, Action::kMaxDurationMs);
    start_spin_->setSuffix(QStringLiteral(" ms"));
    start_spin_->setKeyboardTracking(false);
    end_spin_ = new QSpinBox();
    end_spin_->setRange(1, Action::kMaxDurationMs);
    end_spin_->setSuffix(QStringLiteral(" ms"));
    end_spin_->setKeyboardTracking(false);
    const auto start_field = attachIntSpinSlider(start_spin_, QStringLiteral("Drag to set the start time"), 10, 100);
    const auto end_field = attachIntSpinSlider(end_spin_, QStringLiteral("Drag to set the end time"), 10, 100);
    start_slider_ = start_field.second;
    end_slider_ = end_field.second;
    syncTimeSliderRanges();
    effect_form->addRow(QStringLiteral("Type"), type_combo_);
    effect_form->addRow(QStringLiteral("Start"), start_field.first);
    effect_form->addRow(QStringLiteral("End"), end_field.first);
    effect_sec->bodyLayout()->addLayout(effect_form);
    props_inner_layout->addWidget(effect_sec);

    media_section_ = new EffectCollapsibleSection(QStringLiteral("Media / Matrix"));
    static_cast<EffectCollapsibleSection*>(media_section_)->setExpanded(true);
    media_path_edit_ = new QLineEdit();
    media_path_edit_->setPlaceholderText(QStringLiteral("Path to image or GIF..."));
    media_browse_button_ = new QPushButton(QStringLiteral("Browse..."));
    media_text_edit_ = new QLineEdit();
    media_text_edit_->setPlaceholderText(QStringLiteral("Marquee text"));
    media_scroll_check_ = new QCheckBox(QStringLiteral("Scroll / marquee"));
    media_scroll_check_->setToolTip(
        QStringLiteral("Slide image/text across LEDs using Direction, Speed, and Period."));
    auto* media_path_row = new QWidget();
    auto* media_path_layout = new QHBoxLayout(media_path_row);
    media_path_layout->setContentsMargins(0, 0, 0, 0);
    media_path_layout->setSpacing(6);
    media_path_layout->addWidget(media_path_edit_, 1);
    media_path_layout->addWidget(media_browse_button_);
    auto* media_form = new QFormLayout();
    media_form->setContentsMargins(0, 0, 0, 0);
    media_form->setSpacing(6);
    media_form->addRow(QStringLiteral("File"), media_path_row);
    media_form->addRow(QStringLiteral("Text"), media_text_edit_);
    media_form->addRow(QString(), media_scroll_check_);
    static_cast<EffectCollapsibleSection*>(media_section_)->bodyLayout()->addLayout(media_form);
    props_inner_layout->addWidget(media_section_);

    auto* color_sec = new EffectCollapsibleSection(QStringLiteral("Color"));
    color_sec->setExpanded(true);
    color_sec->bodyLayout()->setSpacing(6);
    color_sec->bodyLayout()->setContentsMargins(2, 4, 2, 2);
    color_button_ = new QPushButton(QStringLiteral("Pick..."));
    color_button_->setMinimumHeight(28);
    color_button_->setMaximumHeight(28);
    color_button_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    color_to_button_ = new QPushButton(QStringLiteral("Pick..."));
    color_to_button_->setMinimumHeight(28);
    color_to_button_->setMaximumHeight(28);
    color_to_button_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    color_to_row_ = new QWidget();
    auto* color_to_layout = new QHBoxLayout(color_to_row_);
    color_to_layout->setContentsMargins(0, 0, 0, 0);
    color_to_layout->setSpacing(6);
    gradient_preset_ = new QComboBox();
    save_gradient_button_ = new QPushButton(QStringLiteral("Save"));
    save_gradient_button_->setToolTip(QStringLiteral("Overwrite the selected preset (including defaults) or save as a new one"));
    delete_gradient_button_ = new QPushButton(QStringLiteral("Delete"));
    delete_gradient_button_->setToolTip(QStringLiteral("Remove the selected gradient preset"));
    delete_gradient_button_->setEnabled(false);
    gradient_bar_ = new ActionGradientBar();
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
    color_sec->bodyLayout()->addWidget(new QLabel(QStringLiteral("Colors / gradient")));
    color_sec->bodyLayout()->addWidget(gradient_bar_);
    auto* swatch_row = new QWidget();
    auto* swatch_layout = new QHBoxLayout(swatch_row);
    swatch_layout->setContentsMargins(0, 0, 0, 0);
    swatch_layout->setSpacing(6);
    add_color_button_ = new QPushButton(QStringLiteral("Add colour"));
    add_color_button_->setToolTip(QStringLiteral("Add another colour stop (blink/alternating use these as swatches)"));
    remove_color_button_ = new QPushButton(QStringLiteral("Remove colour"));
    remove_color_button_->setToolTip(QStringLiteral("Remove the last colour stop (one colour left = on/off for blink & alternating)"));
    swatch_layout->addWidget(add_color_button_);
    swatch_layout->addWidget(remove_color_button_);
    swatch_layout->addStretch(1);
    color_sec->bodyLayout()->addWidget(swatch_row);
    auto* grad_help = new QLabel(
        QStringLiteral("Gradient: drag stops - click bar to add - right-click remove.\n"
                       "Swatches: Add/Remove for blink (on/off) and alternating (colour<->off or colour<->colour)."));
    grad_help->setWordWrap(true);
    PluginUiApplyMutedSecondaryLabel(grad_help);
    color_sec->bodyLayout()->addWidget(grad_help);
    props_inner_layout->addWidget(color_sec);

    auto* bright_sec = new EffectCollapsibleSection(QStringLiteral("Brightness"));
    bright_sec->setExpanded(true);
    bright_sec->bodyLayout()->setSpacing(6);
    bright_sec->bodyLayout()->setContentsMargins(2, 4, 2, 2);
    intensity_spin_ = new QSpinBox();
    intensity_spin_->setRange(1, 100);
    intensity_spin_->setValue(100);
    intensity_spin_->setToolTip(QStringLiteral("Overall brightness scale for this effect"));
    min_intensity_spin_ = new QSpinBox();
    min_intensity_spin_->setRange(0, 100);
    min_intensity_spin_->setValue(15);
    min_intensity_spin_->setToolTip(QStringLiteral("Floor used by blink/pulse scripts and as the low end of the intensity range"));
    max_intensity_spin_ = new QSpinBox();
    max_intensity_spin_->setRange(1, 100);
    max_intensity_spin_->setValue(100);
    max_intensity_spin_->setToolTip(QStringLiteral("Ceiling used by scripts (soft max for twinkle/candle/etc.)"));
    const auto intensity_field = attachIntSpinSlider(intensity_spin_,
                                                     QStringLiteral("Drag to set intensity"),
                                                     1,
                                                     5);
    const auto min_intensity_field = attachIntSpinSlider(min_intensity_spin_,
                                                         QStringLiteral("Drag to set minimum intensity floor"),
                                                         1,
                                                         5);
    const auto max_intensity_field = attachIntSpinSlider(max_intensity_spin_,
                                                         QStringLiteral("Drag to set maximum intensity ceiling"),
                                                         1,
                                                         5);
    auto* bright_form = new QFormLayout();
    bright_form->setContentsMargins(0, 0, 0, 0);
    bright_form->setSpacing(6);
    bright_form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    bright_form->addRow(QStringLiteral("Intensity %"), intensity_field.first);
    min_intensity_row_ = new QWidget();
    auto* min_row_layout = new QVBoxLayout(min_intensity_row_);
    min_row_layout->setContentsMargins(0, 0, 0, 0);
    min_row_layout->setSpacing(2);
    min_row_layout->addWidget(new QLabel(QStringLiteral("Min % (floor)")));
    min_row_layout->addWidget(min_intensity_field.first);
    max_intensity_row_ = new QWidget();
    auto* max_row_layout = new QVBoxLayout(max_intensity_row_);
    max_row_layout->setContentsMargins(0, 0, 0, 0);
    max_row_layout->setSpacing(2);
    max_row_layout->addWidget(new QLabel(QStringLiteral("Max % (ceiling)")));
    max_row_layout->addWidget(max_intensity_field.first);
    bright_sec->bodyLayout()->addLayout(bright_form);
    bright_sec->bodyLayout()->addWidget(min_intensity_row_);
    bright_sec->bodyLayout()->addWidget(max_intensity_row_);
    props_inner_layout->addWidget(bright_sec);

    direction_section_ = new EffectCollapsibleSection(QStringLiteral("Direction"));
    static_cast<EffectCollapsibleSection*>(direction_section_)->setExpanded(true);
    static_cast<EffectCollapsibleSection*>(direction_section_)->bodyLayout()->setSpacing(6);
    static_cast<EffectCollapsibleSection*>(direction_section_)->bodyLayout()->setContentsMargins(2, 4, 2, 2);
    direction_combo_ = new QComboBox();
    auto add_dir = [&](const QString& label, Action::Direction d, const QString& tip) {
        direction_combo_->addItem(label, (int)d);
        direction_combo_->setItemData(direction_combo_->count() - 1, tip, Qt::ToolTipRole);
    };
    add_dir(QStringLiteral("Left"), Action::Direction::Left,
            QStringLiteral("Room -X. Along the key row on a typical keyboard."));
    add_dir(QStringLiteral("Right"), Action::Direction::Right,
            QStringLiteral("Room +X."));
    add_dir(QStringLiteral("Up"), Action::Direction::Up,
            QStringLiteral("Room +Y (vertical). On a flat keyboard Y is thin - uses depth (Z) as fallback."));
    add_dir(QStringLiteral("Down"), Action::Direction::Down,
            QStringLiteral("Room -Y. Flat boards fall back to depth (Z), not Left/Right."));
    add_dir(QStringLiteral("Forward"), Action::Direction::Forward,
            QStringLiteral("Toward Front wall (Z=0 / room -Z). Wipe: Back -> Front."));
    add_dir(QStringLiteral("Back"), Action::Direction::Back,
            QStringLiteral("Toward Back wall (+Z). Wipe: Front -> Back."));
    direction_combo_->insertSeparator(direction_combo_->count());
    add_dir(QStringLiteral("+X"), Action::Direction::PosX, QStringLiteral("Explicit room +X (toward Right wall)"));
    add_dir(QStringLiteral("-X"), Action::Direction::NegX, QStringLiteral("Explicit room -X (toward Left wall)"));
    add_dir(QStringLiteral("+Y"), Action::Direction::PosY, QStringLiteral("Explicit room +Y (toward Ceiling)"));
    add_dir(QStringLiteral("-Y"), Action::Direction::NegY, QStringLiteral("Explicit room -Y (toward Floor)"));
    add_dir(QStringLiteral("+Z"), Action::Direction::PosZ, QStringLiteral("Explicit room +Z (toward Back wall)"));
    add_dir(QStringLiteral("-Z"), Action::Direction::NegZ, QStringLiteral("Explicit room -Z (toward Front wall)"));
    direction_combo_->setToolTip(
        QStringLiteral("Travel / spin axis. With Space=Device this follows the controller orientation from the viewport."));
    axis_space_combo_ = new QComboBox();
    axis_space_combo_->addItem(QStringLiteral("Device (layout)"), (int)Action::AxisSpace::Device);
    axis_space_combo_->addItem(QStringLiteral("Room"), (int)Action::AxisSpace::Room);
    axis_space_combo_->addItem(QStringLiteral("Sequence (order)"), (int)Action::AxisSpace::Sequence);
    axis_space_combo_->setToolTip(
        QStringLiteral("Travel frame for direction / wipes.\n"
                       "Device: per-controller local axes.\n"
                       "Room: world XYZ.\n"
                       "Sequence: march along timeline controller order."));
    play_bounds_combo_ = new QComboBox();
    play_bounds_combo_->addItem(QStringLiteral("This track's lights"), (int)Action::PlayBounds::TrackLights);
    play_bounds_combo_->addItem(QStringLiteral("Whole room"), (int)Action::PlayBounds::RoomBox);
    play_bounds_combo_->setToolTip(
        QStringLiteral("Where the pattern is stretched (UV / world normalize).\n"
                       "This track's lights: keyboard marquee stays on the keyboard.\n"
                       "Whole room: room washes and Roomscape across the full box.\n"
                       "Origin still uses layout reference points (me, screen, rear, …)."));
    axis_mode_combo_ = new QComboBox();
    axis_mode_combo_->addItem(QStringLiteral("Preset direction"), (int)Action::AxisMode::Preset);
    axis_mode_combo_->addItem(QStringLiteral("Custom yaw/pitch"), (int)Action::AxisMode::Custom);
    axis_yaw_spin_ = new QDoubleSpinBox();
    axis_yaw_spin_->setRange(-180.0, 180.0);
    axis_yaw_spin_->setDecimals(1);
    axis_yaw_spin_->setSingleStep(1.0);
    axis_yaw_spin_->setSuffix(QStringLiteral(" deg"));
    axis_pitch_spin_ = new QDoubleSpinBox();
    axis_pitch_spin_->setRange(-90.0, 90.0);
    axis_pitch_spin_->setDecimals(1);
    axis_pitch_spin_->setSingleStep(1.0);
    axis_pitch_spin_->setSuffix(QStringLiteral(" deg"));
    const auto yaw_field = attachDoubleSpinSlider(axis_yaw_spin_, QStringLiteral("Drag to set yaw"));
    const auto pitch_field = attachDoubleSpinSlider(axis_pitch_spin_, QStringLiteral("Drag to set pitch"));
    auto* dir_form = new QFormLayout();
    dir_form->setContentsMargins(0, 0, 0, 0);
    dir_form->setSpacing(6);
    dir_form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    dir_form->addRow(QStringLiteral("Play on"), play_bounds_combo_);
    dir_form->addRow(QStringLiteral("Travel"), axis_space_combo_);
    dir_form->addRow(QStringLiteral("Axis"), axis_mode_combo_);
    dir_form->addRow(QStringLiteral("Direction"), direction_combo_);
    flip_direction_button_ = new QPushButton(QStringLiteral("Flip direction"));
    flip_direction_button_->setToolTip(QStringLiteral("Swap to the opposite direction (Left<->Right, Forward<->Back, ...)"));
    reverse_check_ = new QCheckBox(QStringLiteral("Reverse time"));
    reverse_check_->setToolTip(QStringLiteral("Play this clip's progress backwards (dissolve/fade/wipe reverse)"));
    flip_h_check_ = new QCheckBox(QStringLiteral("Flip H"));
    flip_h_check_->setToolTip(QStringLiteral("Mirror left<->right (media UV / wipe axis)"));
    flip_v_check_ = new QCheckBox(QStringLiteral("Flip V"));
    flip_v_check_->setToolTip(QStringLiteral("Mirror top<->bottom (media UV / height)"));
    rotate_combo_ = new QComboBox();
    rotate_combo_->addItem(QStringLiteral("0 deg"), 0);
    rotate_combo_->addItem(QStringLiteral("90 deg CW"), 1);
    rotate_combo_->addItem(QStringLiteral("180 deg"), 2);
    rotate_combo_->addItem(QStringLiteral("270 deg CW"), 3);
    rotate_combo_->setToolTip(QStringLiteral("Rotate media / UV mapping in 90 deg steps"));
    auto* flip_row = new QWidget();
    auto* flip_layout = new QHBoxLayout(flip_row);
    flip_layout->setContentsMargins(0, 0, 0, 0);
    flip_layout->setSpacing(8);
    flip_layout->addWidget(flip_h_check_);
    flip_layout->addWidget(flip_v_check_);
    flip_layout->addStretch(1);
    dir_form->addRow(QString(), flip_direction_button_);
    dir_form->addRow(QString(), reverse_check_);
    dir_form->addRow(QStringLiteral("Mirror"), flip_row);
    dir_form->addRow(QStringLiteral("Rotate"), rotate_combo_);
    dir_form->addRow(QStringLiteral("Yaw"), yaw_field.first);
    dir_form->addRow(QStringLiteral("Pitch"), pitch_field.first);
    static_cast<EffectCollapsibleSection*>(direction_section_)->bodyLayout()->addLayout(dir_form);
    props_inner_layout->addWidget(direction_section_);

    curve_combo_ = new QComboBox();
    save_curve_button_ = new QPushButton(QStringLiteral("Save"));
    save_curve_button_->setToolTip(QStringLiteral("Save the current envelope curve as a named preset"));
    curve_bar_ = new ActionCurveBar();
    period_curve_combo_ = new QComboBox();
    period_curve_bar_ = new ActionCurveBar();
    auto* curve_sec = new EffectCollapsibleSection(QStringLiteral("Curves"));
    curve_sec->setExpanded(true);
    auto* curve_form = new QFormLayout();
    auto* curve_preset_row = new QWidget();
    auto* curve_preset_layout = new QHBoxLayout(curve_preset_row);
    curve_preset_layout->setContentsMargins(0, 0, 0, 0);
    curve_preset_layout->setSpacing(6);
    curve_preset_layout->addWidget(curve_combo_, 1);
    curve_preset_layout->addWidget(save_curve_button_);
    curve_form->addRow(QStringLiteral("Envelope"), curve_preset_row);
    curve_sec->bodyLayout()->addLayout(curve_form);
    curve_sec->bodyLayout()->addWidget(curve_bar_);
    auto* period_form = new QFormLayout();
    period_form->addRow(QStringLiteral("Waveform"), period_curve_combo_);
    curve_sec->bodyLayout()->addLayout(period_form);
    curve_sec->bodyLayout()->addWidget(period_curve_bar_);
    refillCurvePresets();
    refillPeriodCurvePresets();
    props_inner_layout->addWidget(curve_sec);

    speed_section_ = new EffectCollapsibleSection(QStringLiteral("Timing"));
    static_cast<EffectCollapsibleSection*>(speed_section_)->setExpanded(true);
    speed_spin_ = new QDoubleSpinBox();
    speed_spin_->setRange(0.05, 8.0);
    speed_spin_->setDecimals(2);
    speed_spin_->setSingleStep(0.1);
    speed_spin_->setValue(1.0);
    speed_spin_->setToolTip(QStringLiteral("Scales clip progress and shortens Period cycles"));
    period_spin_ = new QSpinBox();
    period_spin_->setRange(50, Action::kMaxDurationMs);
    period_spin_->setValue(800);
    period_spin_->setToolTip(QStringLiteral("Length of one blink/strobe/pulse cycle (ms). Waveform curve repeats each period."));
    const auto speed_field = attachDoubleSpinSlider(speed_spin_, QStringLiteral("Drag to set speed"));
    const auto period_field = attachIntSpinSlider(period_spin_,
                                                  QStringLiteral("Drag to set period"),
                                                  10,
                                                  100);
    speed_row_ = new QWidget();
    auto* speed_row_layout = new QVBoxLayout(speed_row_);
    speed_row_layout->setContentsMargins(0, 0, 0, 0);
    speed_row_layout->setSpacing(2);
    speed_row_layout->addWidget(new QLabel(QStringLiteral("Speed x")));
    speed_row_layout->addWidget(speed_field.first);
    period_row_ = new QWidget();
    auto* period_layout = new QVBoxLayout(period_row_);
    period_layout->setContentsMargins(0, 0, 0, 0);
    period_layout->setSpacing(2);
    period_layout->addWidget(new QLabel(QStringLiteral("Period ms")));
    period_layout->addWidget(period_field.first);
    static_cast<EffectCollapsibleSection*>(speed_section_)->bodyLayout()->addWidget(speed_row_);
    static_cast<EffectCollapsibleSection*>(speed_section_)->bodyLayout()->addWidget(period_row_);
    props_inner_layout->addWidget(speed_section_);

    pulse_section_ = new EffectCollapsibleSection(QStringLiteral("Pulse / Chase"));
    static_cast<EffectCollapsibleSection*>(pulse_section_)->setExpanded(true);
    pulse_length_spin_ = new QSpinBox();
    pulse_length_spin_->setRange(2, 100);
    pulse_length_spin_->setValue(25);
    const auto pulse_field = attachIntSpinSlider(pulse_length_spin_,
                                                 QStringLiteral("Drag to set pulse / chase length"),
                                                 1,
                                                 5);
    auto* pulse_form = new QFormLayout();
    pulse_form->addRow(QStringLiteral("Length / Duty %"), pulse_field.first);
    static_cast<EffectCollapsibleSection*>(pulse_section_)->bodyLayout()->addLayout(pulse_form);
    props_inner_layout->addWidget(pulse_section_);

    placement_section_ = new EffectCollapsibleSection(QStringLiteral("Placement"));
    static_cast<EffectCollapsibleSection*>(placement_section_)->setExpanded(true);
    origin_ref_combo_ = new QComboBox();
    origin_ref_combo_->setToolTip(
        QStringLiteral("Layout reference point for the cue centre (same names as viewport refs).\n"
                       "Prefer shared role names: me, screen, tv, front, rear, left, right, ceiling, floor…\n"
                       "Missing names show as (missing) — add a matching ref on your layout.\n"
                       "Empty = room center. Independent of stack Spatial Anchor."));
    origin_offset_x_spin_ = new QSpinBox();
    origin_offset_x_spin_->setRange(-100, 100);
    origin_offset_x_spin_->setSuffix(QStringLiteral("%"));
    origin_offset_y_spin_ = new QSpinBox();
    origin_offset_y_spin_->setRange(-100, 100);
    origin_offset_y_spin_->setSuffix(QStringLiteral("%"));
    origin_offset_z_spin_ = new QSpinBox();
    origin_offset_z_spin_->setRange(-100, 100);
    origin_offset_z_spin_->setSuffix(QStringLiteral("%"));
    effect_size_spin_ = new QSpinBox();
    effect_size_spin_->setRange(5, 200);
    effect_size_spin_->setValue(100);
    effect_size_spin_->setSuffix(QStringLiteral("%"));
    effect_size_spin_->setToolTip(QStringLiteral("Spatial footprint of the effect (100% = default room scale)"));
    const auto ox_field = attachIntSpinSlider(origin_offset_x_spin_, QStringLiteral("Offset X (% of half room)"), 1, 5);
    const auto oy_field = attachIntSpinSlider(origin_offset_y_spin_, QStringLiteral("Offset Y (% of half room)"), 1, 5);
    const auto oz_field = attachIntSpinSlider(origin_offset_z_spin_, QStringLiteral("Offset Z (% of half room)"), 1, 5);
    const auto size_field = attachIntSpinSlider(effect_size_spin_, QStringLiteral("Effect size"), 1, 5);
    auto* place_form = new QFormLayout();
    place_form->addRow(QStringLiteral("Origin"), origin_ref_combo_);
    place_form->addRow(QStringLiteral("Offset X"), ox_field.first);
    place_form->addRow(QStringLiteral("Offset Y"), oy_field.first);
    place_form->addRow(QStringLiteral("Offset Z"), oz_field.first);
    place_form->addRow(QStringLiteral("Size"), size_field.first);
    static_cast<EffectCollapsibleSection*>(placement_section_)->bodyLayout()->addLayout(place_form);
    props_inner_layout->addWidget(placement_section_);

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
    preview_button_->setToolTip(QStringLiteral("Preview pack (Space)"));
    preview_menu_button_ = new QToolButton();
    preview_menu_button_->setText(QString());
    preview_menu_button_->setArrowType(Qt::DownArrow);
    preview_menu_button_->setToolTip(QStringLiteral("Preview options"));
    preview_menu_button_->setPopupMode(QToolButton::InstantPopup);
    preview_menu_button_->setFixedWidth(22);
    auto* preview_menu = new QMenu(preview_menu_button_);
    preview_menu->addAction(QStringLiteral("Preview pack\tSpace"), this, [this]() { onPreviewPack(0); });
    preview_menu->addAction(QStringLiteral("Preview selected track\tShift+Space"), this, [this]() {
        const int row = currentTimelineRow();
        if(selected_track_ >= 0)
        {
            onPreviewTrack(selected_track_, 0);
        }
        else
        {
            onPreviewRow(row >= 0 ? row : 0, 0);
        }
    });
    preview_menu->addAction(QStringLiteral("Preview from playhead"), this, [this]() {
        const int ms = timeline_ ? timeline_->playheadMs() : 0;
        onPreviewPack(ms);
    });
    preview_menu_button_->setMenu(preview_menu);
    stop_button_ = new QPushButton(QStringLiteral("Stop"));
    stop_button_->setEnabled(false);
    stop_button_->setToolTip(QStringLiteral("Stop preview (Esc)"));
    save_button_ = new QPushButton(QStringLiteral("Save"));
    PluginUiApplyPrimaryButton(save_button_);
    auto* close_button = new QPushButton(QStringLiteral("Close"));
    action_row->addWidget(preview_button_);
    action_row->addWidget(preview_menu_button_);
    action_row->addWidget(stop_button_);
    action_row->addStretch(1);
    action_row->addWidget(save_button_);
    action_row->addWidget(close_button);
    root->addLayout(action_row);

    setColorButton(color_button_, ToRGBColor(255, 0, 0));
    setColorButton(color_to_button_, ToRGBColor(0, 128, 255));

    connect(duration_spin_, QOverload<int>::of(&QSpinBox::valueChanged), this, &ActionEditorDialog::onDurationChanged);
    connect(loop_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        if(suppress_ui_)
        {
            return;
        }
        applyMetaToPack();
        markPreviewDirtyIfPlaying();
    });
    connect(controllers_button_, &QPushButton::clicked, this, &ActionEditorDialog::onPickControllers);
    connect(map_controllers_button_, &QPushButton::clicked, this, &ActionEditorDialog::onMapControllers);
    connect(timeline_, &ActionTimelineWidget::playheadChanged, this, &ActionEditorDialog::onPlayheadChanged);
    connect(timeline_, &ActionTimelineWidget::clipSelected, this, &ActionEditorDialog::onClipSelected);
    connect(timeline_, &ActionTimelineWidget::clipEdited, this, [this](int track, int clip) {
        onClipSelected(track, clip);
        markPreviewDirtyIfPlaying();
    });
    connect(timeline_, &ActionTimelineWidget::clipCreated, this, [this](int track, int clip) {
        onClipSelected(track, clip);
        markPreviewDirtyIfPlaying();
    });
    connect(timeline_, &ActionTimelineWidget::clipDeleteRequested, this, &ActionEditorDialog::onClipDeleteRequested);
    connect(timeline_, &ActionTimelineWidget::effectAddRequested, this, &ActionEditorDialog::onEffectAddRequested);
    connect(timeline_, &ActionTimelineWidget::gradientPresetApplied, this, &ActionEditorDialog::onGradientPresetApplied);
    connect(timeline_, &ActionTimelineWidget::curvePresetApplied, this, &ActionEditorDialog::onCurvePresetApplied);
    connect(timeline_, &ActionTimelineWidget::colorDropped, this, &ActionEditorDialog::onColorDropped);
    connect(timeline_, &ActionTimelineWidget::gradientDropped, this, &ActionEditorDialog::onGradientDropped);
    connect(timeline_, &ActionTimelineWidget::sceneZoneControllersReordered,
            this, &ActionEditorDialog::onSceneZoneControllersReordered);
    connect(timeline_, &ActionTimelineWidget::previewPackRequested,
            this, &ActionEditorDialog::onPreviewPack);
    connect(timeline_, &ActionTimelineWidget::previewRowRequested,
            this, &ActionEditorDialog::onPreviewRow);
    connect(timeline_, &ActionTimelineWidget::previewTrackRequested,
            this, &ActionEditorDialog::onPreviewTrack);
    connect(timeline_, &ActionTimelineWidget::stopPreviewRequested,
            this, &ActionEditorDialog::stopPreview);
    connect(effect_toolbar_, &ActionToolBar::effectClicked, this, &ActionEditorDialog::onToolbarEffectClicked);
    connect(effect_toolbar_, &ActionToolBar::colorClicked, this, &ActionEditorDialog::onToolbarColorClicked);
    connect(effect_toolbar_, &ActionToolBar::gradientPresetClicked, this, &ActionEditorDialog::onToolbarGradientClicked);
    connect(effect_toolbar_, &ActionToolBar::gradientPresetOverwriteRequested, this, &ActionEditorDialog::onOverwriteGradientPreset);
    connect(effect_toolbar_, &ActionToolBar::gradientPresetDeleteRequested, this, &ActionEditorDialog::onDeleteGradientPreset);
    connect(effect_toolbar_, &ActionToolBar::gradientPresetResetRequested, this, &ActionEditorDialog::onResetGradientPreset);
    connect(effect_toolbar_, &ActionToolBar::curvePresetClicked, this, &ActionEditorDialog::onToolbarCurveClicked);
    connect(remove_clip_button_, &QPushButton::clicked, this, &ActionEditorDialog::onRemoveClip);
    connect(type_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ActionEditorDialog::onTypeChanged);
    connect(start_spin_, QOverload<int>::of(&QSpinBox::valueChanged), this, &ActionEditorDialog::onClipFieldChanged);
    connect(end_spin_, QOverload<int>::of(&QSpinBox::valueChanged), this, &ActionEditorDialog::onClipFieldChanged);
    connect(period_spin_, QOverload<int>::of(&QSpinBox::valueChanged), this, &ActionEditorDialog::onClipFieldChanged);
    connect(intensity_spin_, QOverload<int>::of(&QSpinBox::valueChanged), this, &ActionEditorDialog::onClipFieldChanged);
    connect(min_intensity_spin_, QOverload<int>::of(&QSpinBox::valueChanged), this, &ActionEditorDialog::onClipFieldChanged);
    connect(max_intensity_spin_, QOverload<int>::of(&QSpinBox::valueChanged), this, &ActionEditorDialog::onClipFieldChanged);
    connect(speed_spin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &ActionEditorDialog::onClipFieldChanged);
    connect(pulse_length_spin_, QOverload<int>::of(&QSpinBox::valueChanged), this, &ActionEditorDialog::onClipFieldChanged);
    if(origin_ref_combo_)
    {
        connect(origin_ref_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ActionEditorDialog::onClipFieldChanged);
    }
    if(origin_offset_x_spin_)
    {
        connect(origin_offset_x_spin_, QOverload<int>::of(&QSpinBox::valueChanged), this, &ActionEditorDialog::onClipFieldChanged);
    }
    if(origin_offset_y_spin_)
    {
        connect(origin_offset_y_spin_, QOverload<int>::of(&QSpinBox::valueChanged), this, &ActionEditorDialog::onClipFieldChanged);
    }
    if(origin_offset_z_spin_)
    {
        connect(origin_offset_z_spin_, QOverload<int>::of(&QSpinBox::valueChanged), this, &ActionEditorDialog::onClipFieldChanged);
    }
    if(effect_size_spin_)
    {
        connect(effect_size_spin_, QOverload<int>::of(&QSpinBox::valueChanged), this, &ActionEditorDialog::onClipFieldChanged);
    }
    connect(direction_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ActionEditorDialog::onClipFieldChanged);
    connect(flip_direction_button_, &QPushButton::clicked, this, &ActionEditorDialog::onFlipDirection);
    connect(reverse_check_, &QCheckBox::toggled, this, &ActionEditorDialog::onClipFieldChanged);
    connect(flip_h_check_, &QCheckBox::toggled, this, &ActionEditorDialog::onClipFieldChanged);
    connect(flip_v_check_, &QCheckBox::toggled, this, &ActionEditorDialog::onClipFieldChanged);
    connect(rotate_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ActionEditorDialog::onClipFieldChanged);
    connect(axis_space_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ActionEditorDialog::onClipFieldChanged);
    if(play_bounds_combo_)
    {
        connect(play_bounds_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ActionEditorDialog::onClipFieldChanged);
    }
    connect(axis_mode_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ActionEditorDialog::onClipFieldChanged);
    connect(axis_yaw_spin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &ActionEditorDialog::onClipFieldChanged);
    connect(axis_pitch_spin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &ActionEditorDialog::onClipFieldChanged);
    connect(curve_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ActionEditorDialog::onCurvePresetChanged);
    connect(save_curve_button_, &QPushButton::clicked, this, &ActionEditorDialog::onSaveUserCurve);
    connect(curve_bar_, &ActionCurveBar::pointsChanged, this, &ActionEditorDialog::onCurvePointsChanged);
    connect(period_curve_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ActionEditorDialog::onPeriodCurvePresetChanged);
    connect(period_curve_bar_, &ActionCurveBar::pointsChanged, this, &ActionEditorDialog::onPeriodCurvePointsChanged);
    connect(media_browse_button_, &QPushButton::clicked, this, &ActionEditorDialog::onBrowseMediaPath);
    connect(media_path_edit_, &QLineEdit::editingFinished, this, &ActionEditorDialog::onClipFieldChanged);
    // editingFinished only - textChanged re-rasterized the whole timeline on every keystroke.
    connect(media_text_edit_, &QLineEdit::editingFinished, this, &ActionEditorDialog::onClipFieldChanged);
    connect(media_scroll_check_, &QCheckBox::toggled, this, &ActionEditorDialog::onClipFieldChanged);
    connect(color_button_, &QPushButton::clicked, this, &ActionEditorDialog::onPickColor);
    connect(color_to_button_, &QPushButton::clicked, this, &ActionEditorDialog::onPickColorTo);
    connect(gradient_preset_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ActionEditorDialog::onGradientPreset);
    connect(save_gradient_button_, &QPushButton::clicked, this, &ActionEditorDialog::onSaveUserGradient);
    connect(delete_gradient_button_, &QPushButton::clicked, this, &ActionEditorDialog::onDeleteUserGradient);
    connect(gradient_bar_, &ActionGradientBar::stopsChanged, this, &ActionEditorDialog::onGradientStopsChanged);
    connect(add_color_button_, &QPushButton::clicked, this, &ActionEditorDialog::onAddColorStop);
    connect(remove_color_button_, &QPushButton::clicked, this, &ActionEditorDialog::onRemoveColorStop);
    connect(preview_button_, &QPushButton::clicked, this, &ActionEditorDialog::onPreview);
    connect(stop_button_, &QPushButton::clicked, this, &ActionEditorDialog::stopPreview);
    connect(save_button_, &QPushButton::clicked, this, &ActionEditorDialog::onSave);
    connect(close_button, &QPushButton::clicked, this, &QDialog::close);

    updatePropVisibility();
}

void ActionEditorDialog::NewPack(const filesystem::path& save_dir)
{
    std::vector<std::string> devices;
    if(!promptSelectControllers(&devices, true))
    {
        return;
    }

    save_dir_ = save_dir;
    pack_path_.clear();
    pack_ = Action::Pack();
    pack_.id = "new_pack";
    pack_.name = "New pack";
    pack_.duration_ms = 5000;
    pack_.loop = Action::LoopMode::Once;
    pack_.priority = 10;
    pack_.devices = std::move(devices);

    const std::string dir_str = save_dir.generic_string();
    const bool film = dir_str.find("movie-sync") != std::string::npos;
    if(save_dest_combo_)
    {
        save_dest_combo_->setEnabled(true);
        const int idx = save_dest_combo_->findData(film ? QStringLiteral("movie-sync")
                                                        : QStringLiteral("triggers"));
        if(idx >= 0)
        {
            save_dest_combo_->setCurrentIndex(idx);
        }
    }
    if(movie_category_combo_)
    {
        movie_category_combo_->setEnabled(true);
        if(film && save_dir.has_filename())
        {
            const int cidx = movie_category_combo_->findData(
                QString::fromStdString(save_dir.filename().string()));
            if(cidx >= 0)
            {
                movie_category_combo_->setCurrentIndex(cidx);
            }
        }
    }

    loadIntoUi(pack_);
    setWindowTitle(QStringLiteral("Action Editor  -  New"));
    status_label_->setText(QStringLiteral("Drag tools onto a row · right-click to add · Delete removes"));
    show();
    raise();
    activateWindow();
}

void ActionEditorDialog::EditPack(const filesystem::path& path)
{
    Action::Pack pack;
    std::string err;
    if(!Action::LoadFromFile(path, &pack, &err))
    {
        QMessageBox::warning(this, QStringLiteral("Action Editor"),
                             QStringLiteral("Failed to load:\n%1").arg(QString::fromStdString(err)));
        return;
    }
    save_dir_ = path.parent_path();
    pack_path_ = path;
    pack_ = std::move(pack);
    {
        const std::string dir_str = save_dir_.generic_string();
        const bool film = dir_str.find("movie-sync") != std::string::npos;
        if(save_dest_combo_)
        {
            save_dest_combo_->setEnabled(false);
            const int idx = save_dest_combo_->findData(film ? QStringLiteral("movie-sync")
                                                            : QStringLiteral("triggers"));
            if(idx >= 0)
            {
                save_dest_combo_->setCurrentIndex(idx);
            }
        }
        if(movie_category_combo_)
        {
            movie_category_combo_->setEnabled(false);
            if(film && save_dir_.has_filename())
            {
                const int cidx = movie_category_combo_->findData(
                    QString::fromStdString(save_dir_.filename().string()));
                if(cidx >= 0)
                {
                    movie_category_combo_->setCurrentIndex(cidx);
                }
            }
        }
    }
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
    setWindowTitle(QStringLiteral("Action Editor  -  %1").arg(QString::fromStdString(pack_.name)));
    {
        QString status = QStringLiteral("Editing %1")
                             .arg(QString::fromStdString(path.filename().string()));
        const std::vector<std::string> refs = Action::CollectPackOriginRefs(pack_);
        if(!refs.empty())
        {
            QStringList missing;
            Action::ApplySpatialContext room;
            if(tab_)
            {
                tab_->GetActionRoomWorldBounds(&room);
            }
            for(const std::string& name : refs)
            {
                bool found = false;
                for(const Action::NamedWorldPoint& p : room.reference_points)
                {
                    if(p.name == name)
                    {
                        found = true;
                        break;
                    }
                }
                if(!found)
                {
                    missing << QString::fromStdString(name);
                }
            }
            QStringList all;
            for(const std::string& name : refs)
            {
                all << QString::fromStdString(name);
            }
            status += QStringLiteral(" · refs: %1").arg(all.join(QStringLiteral(", ")));
            if(!missing.isEmpty())
            {
                status += QStringLiteral(" · missing: %1 (add matching layout refs)")
                              .arg(missing.join(QStringLiteral(", ")));
            }
        }
        status_label_->setText(status);
    }
    show();
    raise();
    activateWindow();
}

bool ActionEditorDialog::promptSelectControllers(std::vector<std::string>* devices, bool require_selection)
{
    if(!devices || !tab_)
    {
        return false;
    }

    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("Select controllers for this effect"));
    auto* layout = new QVBoxLayout(&dlg);
    layout->addWidget(new QLabel(
        QStringLiteral("Controllers this action can use. Pack-wide clips go on All (this pack).")));

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
                               return Action::NameMatches(key, d) || Action::NameMatches(d, key);
                           });
        item->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
        ++visible;
    }
    layout->addWidget(list, 1);

    if(visible == 0)
    {
        QMessageBox::warning(this, QStringLiteral("Action Editor"),
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
        QMessageBox::warning(this, QStringLiteral("Action Editor"),
                             QStringLiteral("Select at least one controller."));
        return false;
    }
    *devices = std::move(picked);
    return true;
}

std::vector<std::string> ActionEditorDialog::packDeviceNames() const
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
    for(const Action::Track& track : pack_.tracks)
    {
        add(track.target.device_name);
    }
    return names;
}

bool ActionEditorDialog::deviceNameInScene(const std::string& name) const
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
        if(Action::NameMatches(key, name) || Action::NameMatches(name, key))
        {
            return true;
        }
    }
    return false;
}

bool ActionEditorDialog::promptMapControllers()
{
    const std::vector<std::string> names = packDeviceNames();
    if(names.empty())
    {
        QMessageBox::information(this, QStringLiteral("Action Editor"),
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
            if(Action::NameMatches(key, name) || Action::NameMatches(name, key))
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

        std::vector<Action::Target> dest_zones;
        std::vector<Action::Target> dest_leds;
        if(dest && dest_index >= 0)
        {
            const ActionTimelineWidget::Node node = buildControllerNode(dest, dest_index);
            for(const ActionTimelineWidget::Node& zone : node.children)
            {
                dest_zones.push_back(zone.target);
                for(const ActionTimelineWidget::Node& led : zone.children)
                {
                    dest_leds.push_back(led.target);
                }
            }
        }

        std::vector<size_t> led_tracks;
        std::vector<size_t> zone_tracks;
        for(size_t i = 0; i < pack_.tracks.size(); ++i)
        {
            const Action::Target& target = pack_.tracks[i].target;
            if(target.device_name != row.from)
            {
                continue;
            }
            if(target.kind == Action::TargetKind::Leds)
            {
                led_tracks.push_back(i);
            }
            else if(target.kind == Action::TargetKind::Zone)
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
                Action::Target& target = pack_.tracks[led_tracks[i]].target;
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
                Action::Target& target = pack_.tracks[zone_tracks[i]].target;
                target.device_name = to;
                target.zone_name = dest_zones[i].zone_name;
            }
            else
            {
                drop[zone_tracks[i]] = 1;
                ++dropped;
            }
        }

        std::vector<Action::Track> kept;
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

void ActionEditorDialog::onMapControllers()
{
    promptMapControllers();
}

void ActionEditorDialog::onPickControllers()
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

bool ActionEditorDialog::deviceSelectedForPack(const std::string& key) const
{
    if(pack_.devices.empty())
    {
        return true; // empty devices list = whole scene
    }
    for(const std::string& d : pack_.devices)
    {
        if(Action::NameMatches(key, d) || Action::NameMatches(d, key))
        {
            return true;
        }
    }
    return false;
}

void ActionEditorDialog::syncTimeSliderRanges()
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

void ActionEditorDialog::loadIntoUi(const Action::Pack& pack)
{
    suppress_ui_ = true;
    name_edit_->setText(QString::fromStdString(pack.name));
    duration_spin_->setValue(pack.duration_ms);
    syncTimeSliderRanges();
    const QString loop = (pack.loop == Action::LoopMode::Forever) ? QStringLiteral("forever")
        : (pack.loop == Action::LoopMode::WhileActive) ? QStringLiteral("while_active")
        : QStringLiteral("once");
    const int loop_idx = loop_combo_->findData(loop);
    loop_combo_->setCurrentIndex(loop_idx >= 0 ? loop_idx : 0);
    selected_track_ = -1;
    selected_clip_ = -1;
    suppress_ui_ = false;
    if(timeline_)
    {
        timeline_->clearGpuThumbCache();
    }
    timeline_->setPack(&pack_);
    timeline_->setDurationMs(pack_.duration_ms);
    timeline_->setPlayheadMs(0);
    onRebuildTimelineModel();
    applyClipToForm();
    updateSelectionActions();
}

void ActionEditorDialog::onDurationChanged(int value)
{
    if(suppress_ui_)
    {
        return;
    }
    pack_.duration_ms = value;
    syncTimeSliderRanges();
    for(Action::Track& track : pack_.tracks)
    {
        for(Action::Clip& clip : track.clips)
        {
            clip.start_ms = std::clamp(clip.start_ms, 0, std::max(0, value - 1));
            clip.end_ms = std::clamp(clip.end_ms, clip.start_ms + 1, value);
        }
    }
    timeline_->setDurationMs(value);
    markPreviewDirtyIfPlaying();
    applyClipToForm();
    timeline_->requestContentRepaint();
}

void ActionEditorDialog::onPlayheadChanged(int ms)
{
    timeline_->setPlayheadMs(ms);
    if(player_.IsPlaying())
    {
        player_.UpdatePack(preview_pack_);
        player_.SeekToLocalMs(ms);
        wall_.restart();
        last_elapsed_ms_ = 0;
        if(tab_)
        {
            Action::SetGpuAnimMs(player_.ElapsedMs());
            tab_->ApplyActionPreviewFrame(preview_pack_, ms);
            Action::SetGpuAnimMs(-1);
        }
    }
}

void ActionEditorDialog::keyPressEvent(QKeyEvent* event)
{
    if(event->key() == Qt::Key_Escape && player_.IsPlaying())
    {
        stopPreview();
        event->accept();
        return;
    }
    if(event->key() == Qt::Key_Space)
    {
        const QWidget* focus = focusWidget();
        const bool editing_text = focus
            && (qobject_cast<const QLineEdit*>(focus)
                || qobject_cast<const QAbstractSpinBox*>(focus));
        if(!editing_text)
        {
            if(player_.IsPlaying())
            {
                stopPreview();
            }
            else if(event->modifiers() & Qt::ShiftModifier)
            {
                const int row = currentTimelineRow();
                if(selected_track_ >= 0)
                {
                    onPreviewTrack(selected_track_, 0);
                }
                else
                {
                    onPreviewRow(row >= 0 ? row : 0, 0);
                }
            }
            else
            {
                onPreviewPack(0);
            }
            event->accept();
            return;
        }
    }
    if(event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace)
    {
        const QWidget* focus = focusWidget();
        const bool editing_text = focus
            && (qobject_cast<const QLineEdit*>(focus)
                || qobject_cast<const QAbstractSpinBox*>(focus));
        if(!editing_text && selectedClip())
        {
            onRemoveClip();
            event->accept();
            return;
        }
    }
    QDialog::keyPressEvent(event);
}

void ActionEditorDialog::updateSelectionActions()
{
    const Action::Clip* clip = selectedClip();
    const bool has = clip != nullptr;
    if(remove_clip_button_)
    {
        remove_clip_button_->setEnabled(has);
    }
    if(props_hint_)
    {
        QString title = QStringLiteral("Select a clip to edit");
        if(has)
        {
            const QList<ActionCatalog::Entry> entries = ActionCatalog::LoadEntries(
                PluginSettingsPaths::ActionToolboxDir(tab_ ? tab_->resource_manager : nullptr));
            title = ActionCatalog::EffectNameForId(
                entries, QString::fromStdString(Action::ClipFileId(*clip)));
        }
        props_hint_->setText(title);
    }
    if(delete_gradient_button_ && gradient_preset_)
    {
        delete_gradient_button_->setEnabled(!gradient_preset_->currentData().toString().isEmpty());
    }
    if(effect_toolbar_)
    {
        bool any_clip = false;
        for(const Action::Track& track : pack_.tracks)
        {
            if(!track.clips.empty())
            {
                any_clip = true;
                break;
            }
        }
        effect_toolbar_->setCurvesEnabled(any_clip);
    }
}

void ActionEditorDialog::onColorDropped(int row_index, int ms, unsigned int rgb)
{
    addClipAt(row_index, ms);
    if(Action::Clip* clip = selectedClip())
    {
        const RGBColor color = (RGBColor)rgb;
        clip->color = color;
        clip->color_from = color;
        clip->color_to = color;
        clip->gradient = {{0.0f, color}};
        applyClipToForm();
        if(timeline_)
        {
            timeline_->requestContentRepaint();
        }
    }
    updateSelectionActions();
}

void ActionEditorDialog::onGradientDropped(int row_index, int ms, const QString& preset_id)
{
    addClipAt(row_index, ms);
    applyGradientPresetToClip(selectedClip(), preset_id);
    applyClipToForm();
    updateSelectionActions();
}

void ActionEditorDialog::onEffectAddRequested(int row_index, int ms, const QString& effect_id)
{
    addClipAt(row_index, ms, effect_id);
}

void ActionEditorDialog::onClipSelected(int track_index, int clip_index)
{
    // Don't abort an in-progress clip drag (cross-track moves emit selection updates).
    if(timeline_ && !timeline_->isClipDragging())
    {
        timeline_->cancelDrag();
    }
    if(selected_track_ != track_index || selected_clip_ != clip_index)
    {
        // Flush the form only when the previous clip is still present. Vertical
        // resize can move/erase the old slot; writing into a sibling would corrupt it.
        if(selected_track_ >= 0 && selected_track_ < (int)pack_.tracks.size()
           && selected_clip_ >= 0
           && selected_clip_ < (int)pack_.tracks[(size_t)selected_track_].clips.size())
        {
            applyFormToSelectedClip();
        }
    }
    selected_track_ = track_index;
    selected_clip_ = clip_index;
    applyClipToForm();
    updateSelectionActions();
}

int ActionEditorDialog::currentTimelineRow() const
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

void ActionEditorDialog::onToolbarEffectClicked(const QString& effect_id)
{
    addClipAt(currentTimelineRow(), timeline_ ? timeline_->playheadMs() : 0, effect_id);
}

void ActionEditorDialog::onClipDeleteRequested(int track_index, int clip_index)
{
    selected_track_ = track_index;
    selected_clip_ = clip_index;
    onRemoveClip();
}

void ActionEditorDialog::onRemoveClip()
{
    if(selected_track_ < 0 || selected_track_ >= (int)pack_.tracks.size())
    {
        return;
    }
    auto& clips = pack_.tracks[(size_t)selected_track_].clips;
    if(selected_clip_ < 0 || selected_clip_ >= (int)clips.size())
    {
        return;
    }
    if(timeline_)
    {
        timeline_->cancelDrag();
    }
    suppress_ui_ = true;
    clips.erase(clips.begin() + selected_clip_);
    if(clips.empty())
    {
        pack_.tracks.erase(pack_.tracks.begin() + selected_track_);
        selected_track_ = -1;
        selected_clip_ = -1;
    }
    else
    {
        selected_clip_ = std::min(selected_clip_, (int)clips.size() - 1);
    }
    if(timeline_)
    {
        timeline_->setPack(&pack_);
        timeline_->setSelectedClip(selected_track_, selected_clip_);
        timeline_->requestContentRepaint();
    }
    markPreviewDirtyIfPlaying();
    suppress_ui_ = false;
    applyClipToForm();
    updateSelectionActions();
}

Action::Clip* ActionEditorDialog::selectedClip()
{
    if(selected_track_ < 0 || selected_track_ >= (int)pack_.tracks.size())
    {
        return nullptr;
    }
    auto& clips = pack_.tracks[(size_t)selected_track_].clips;
    if(selected_clip_ < 0 || selected_clip_ >= (int)clips.size())
    {
        return nullptr;
    }
    return &clips[(size_t)selected_clip_];
}

QString ActionEditorDialog::sanitizeId(const QString& name) const
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

void ActionEditorDialog::applyMetaToPack()
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
        pack_.loop = Action::LoopMode::Forever;
    }
    else if(loop == QStringLiteral("while_active"))
    {
        pack_.loop = Action::LoopMode::WhileActive;
    }
    else
    {
        pack_.loop = Action::LoopMode::Once;
    }
    if(pack_path_.empty())
    {
        pack_.id = sanitizeId(QString::fromStdString(pack_.name)).toStdString();
    }
}

void ActionEditorDialog::onSave()
{
    stopPreview();
    applyFormToSelectedClip();
    applyMetaToPack();
    if(pack_.tracks.empty())
    {
        QMessageBox::warning(this, QStringLiteral("Action Editor"),
                             QStringLiteral("Add at least one clip on the timeline before saving."));
        return;
    }
    bool any_clips = false;
    for(const auto& t : pack_.tracks)
    {
        if(!t.clips.empty())
        {
            any_clips = true;
            break;
        }
    }
    if(!any_clips)
    {
        QMessageBox::warning(this, QStringLiteral("Action Editor"),
                             QStringLiteral("Add at least one clip on the timeline before saving."));
        return;
    }
    if(pack_path_.empty())
    {
        auto* rm = tab_ ? tab_->resource_manager : nullptr;
        if(!rm || !save_dest_combo_)
        {
            return;
        }
        if(save_dest_combo_->currentData().toString() == QStringLiteral("movie-sync"))
        {
            const QString cat = movie_category_combo_
                                    ? movie_category_combo_->currentData().toString()
                                    : QStringLiteral("washes");
            save_dir_ = PluginSettingsPaths::MovieSyncEffectsDir(rm) / cat.toStdString();
        }
        else
        {
            save_dir_ = PluginSettingsPaths::TriggerEffectsDir(rm);
        }
    }
    if(save_dir_.empty())
    {
        return;
    }
    std::error_code ec;
    filesystem::create_directories(save_dir_, ec);
    if(pack_path_.empty())
    {
        filesystem::path candidate = save_dir_ / (pack_.id + Action::kFileSuffix);
        if(filesystem::exists(candidate, ec))
        {
            for(int n = 2;; ++n)
            {
                const std::string id = pack_.id + "_" + std::to_string(n);
                candidate = save_dir_ / (id + Action::kFileSuffix);
                if(!filesystem::exists(candidate, ec))
                {
                    pack_.id = id;
                    break;
                }
            }
        }
        pack_path_ = std::move(candidate);
    }
    std::string err;
    if(!Action::SaveToFile(pack_path_, pack_, &err))
    {
        QMessageBox::warning(this, QStringLiteral("Action Editor"),
                             QStringLiteral("Save failed:\n%1").arg(QString::fromStdString(err)));
        return;
    }
    setWindowTitle(QStringLiteral("Action Editor  -  %1").arg(QString::fromStdString(pack_.name)));
    status_label_->setText(QStringLiteral("Saved %1").arg(QString::fromStdString(pack_path_.filename().string())));
#ifdef _WIN32
    emit packSaved(QString::fromStdWString(pack_path_.wstring()));
#else
    emit packSaved(QString::fromStdString(pack_path_.string()));
#endif
}

void ActionEditorDialog::setPlayingUi(bool playing)
{
    preview_button_->setEnabled(!playing);
    if(preview_menu_button_)
    {
        preview_menu_button_->setEnabled(!playing);
    }
    stop_button_->setEnabled(playing);
    save_button_->setEnabled(!playing);
    updateSelectionActions();
}

void ActionEditorDialog::stopPreview()
{
    const bool was_playing = player_.IsPlaying();
    if(timer_ && timer_->isActive())
    {
        timer_->stop();
    }
    player_.Stop();
    if(was_playing && tab_)
    {
        tab_->ClearActionPreview(true);
    }
    preview_pack_ = Action::Pack();
    preview_solo_ = false;
    setPlayingUi(false);
    emit previewStopped();
    if(status_label_)
    {
        status_label_->setText(QStringLiteral("Preview stopped"));
    }
}

Action::Pack ActionEditorDialog::soloPackForRow(int row_index) const
{
    Action::Pack solo = pack_;
    solo.tracks.clear();
    if(!timeline_)
    {
        return solo;
    }
    const QVector<Action::Target> scopes = timeline_->subtreeTargetsForRow(row_index);
    if(scopes.isEmpty())
    {
        return solo;
    }
    for(const Action::Track& track : pack_.tracks)
    {
        for(const Action::Target& scope : scopes)
        {
            if(Action::TargetEquals(track.target, scope))
            {
                solo.tracks.push_back(track);
                break;
            }
        }
    }
    return solo;
}

Action::Pack ActionEditorDialog::soloPackForTrack(int track_index) const
{
    Action::Pack solo = pack_;
    solo.tracks.clear();
    if(track_index < 0 || track_index >= (int)pack_.tracks.size())
    {
        return solo;
    }
    solo.tracks.push_back(pack_.tracks[(size_t)track_index]);
    return solo;
}

void ActionEditorDialog::startPreview(const Action::Pack& play_pack, int start_ms, const QString& status, bool solo)
{
    if(!tab_)
    {
        return;
    }
    if(player_.IsPlaying())
    {
        stopPreview();
    }
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
    tab_->PrepareActionPreview();
    if(tab_->resource_manager && tab_->resource_manager->GetRGBControllers().empty())
    {
        tab_->ClearActionPreview(true);
        QMessageBox::warning(this, QStringLiteral("Action Editor"),
                             QStringLiteral("No OpenRGB controllers available."));
        return;
    }
    preview_pack_ = play_pack;
    preview_pack_.duration_ms = pack_.duration_ms;
    preview_pack_.loop = pack_.loop;
    preview_pack_.devices = pack_.devices;
    preview_solo_ = solo;
    preview_pack_dirty_ = false;
    emit previewStarted();
    player_.SetPack(preview_pack_);
    const int seek = std::clamp(start_ms, 0, std::max(0, preview_pack_.duration_ms - 1));
    player_.SeekToLocalMs(seek);
    player_.Play();
    wall_.restart();
    last_elapsed_ms_ = 0;
    setPlayingUi(true);
    timer_->start();
    if(timeline_)
    {
        timeline_->setPlayheadMs(player_.LocalMs());
    }
    // First frame now — avoid a timer tick of leftover LED state after Prepare.
    Action::SetGpuAnimMs(player_.ElapsedMs());
    tab_->ApplyActionPreviewFrame(preview_pack_, player_.LocalMs());
    Action::SetGpuAnimMs(-1);
    status_label_->setText(status.isEmpty() ? QStringLiteral("Previewing...") : status);
}

void ActionEditorDialog::onPreview()
{
    onPreviewPack(0);
}

void ActionEditorDialog::onPreviewPack(int start_ms)
{
    if(player_.IsPlaying())
    {
        stopPreview();
        return;
    }
    applyFormToSelectedClip();
    applyMetaToPack();
    startPreview(pack_, start_ms, start_ms > 0
                 ? QStringLiteral("Previewing pack from %1 ms...").arg(start_ms)
                 : QStringLiteral("Previewing pack..."),
                 false);
}

void ActionEditorDialog::onPreviewRow(int row_index, int start_ms)
{
    if(player_.IsPlaying())
    {
        stopPreview();
        return;
    }
    applyFormToSelectedClip();
    applyMetaToPack();
    Action::Pack solo = soloPackForRow(row_index);
    QString label = QStringLiteral("track");
    if(timeline_ && row_index >= 0 && row_index < timeline_->visibleRows().size())
    {
        label = timeline_->visibleRows()[row_index].label;
    }
    if(solo.tracks.empty())
    {
        QMessageBox::information(this, QStringLiteral("Action Editor"),
                                 QStringLiteral("No effects on \"%1\" (or its children).").arg(label));
        return;
    }
    startPreview(solo, start_ms, QStringLiteral("Previewing \"%1\"...").arg(label), true);
}

void ActionEditorDialog::onPreviewTrack(int track_index, int start_ms)
{
    if(player_.IsPlaying())
    {
        stopPreview();
        return;
    }
    applyFormToSelectedClip();
    applyMetaToPack();
    Action::Pack solo = soloPackForTrack(track_index);
    if(solo.tracks.empty())
    {
        QMessageBox::information(this, QStringLiteral("Action Editor"),
                                 QStringLiteral("No effects on that track."));
        return;
    }
    const QString label = QString::fromStdString(solo.tracks.front().name);
    startPreview(solo, start_ms,
                 start_ms > 0
                     ? QStringLiteral("Previewing \"%1\" from %2 ms...").arg(label).arg(start_ms)
                     : QStringLiteral("Previewing \"%1\"...").arg(label),
                 true);
}

void ActionEditorDialog::markPreviewDirtyIfPlaying()
{
    if(player_.IsPlaying())
    {
        preview_pack_dirty_ = true;
    }
}

void ActionEditorDialog::syncPreviewPackFromAuthoring()
{
    if(preview_solo_)
    {
        Action::Pack refreshed = preview_pack_;
        refreshed.tracks.clear();
        for(const Action::Track& playing : preview_pack_.tracks)
        {
            for(const Action::Track& src : pack_.tracks)
            {
                if(Action::TargetEquals(playing.target, src.target))
                {
                    refreshed.tracks.push_back(src);
                    break;
                }
            }
        }
        refreshed.duration_ms = pack_.duration_ms;
        refreshed.loop = pack_.loop;
        refreshed.devices = pack_.devices;
        preview_pack_ = std::move(refreshed);
    }
    else
    {
        preview_pack_ = pack_;
    }
}

void ActionEditorDialog::scheduleTimelineRefresh()
{
    if(timeline_refresh_timer_)
    {
        timeline_refresh_timer_->start();
    }
    else if(timeline_)
    {
        timeline_->requestContentRepaint();
    }
}

void ActionEditorDialog::onTick()
{
    if(!tab_ || !player_.IsPlaying())
    {
        stopPreview();
        return;
    }
    const int elapsed = (int)wall_.elapsed();
    const int dt = std::max(0, elapsed - last_elapsed_ms_);
    last_elapsed_ms_ = elapsed;
    // Freeze authoring pack during preview unless a live edit dirtied it.
    if(preview_pack_dirty_)
    {
        syncPreviewPackFromAuthoring();
        preview_pack_dirty_ = false;
        player_.UpdatePack(preview_pack_);
    }
    if(!player_.Tick(dt, true))
    {
        stopPreview();
        status_label_->setText(QStringLiteral("Preview finished"));
        return;
    }
    Action::SetGpuAnimMs(player_.ElapsedMs());
    tab_->ApplyActionPreviewFrame(preview_pack_, player_.LocalMs());
    Action::SetGpuAnimMs(-1);
    timeline_->setPlayheadMs(player_.LocalMs());
    status_label_->setText(
        QStringLiteral("Preview %1 / %2 ms")
            .arg(player_.LocalMs())
            .arg(std::max(1, pack_.duration_ms)));
}
