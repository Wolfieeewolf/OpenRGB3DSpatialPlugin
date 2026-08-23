// SPDX-License-Identifier: GPL-2.0-only

#include "GridSettingsPanel.h"
#include "OpenRGB3DSpatialTab.h"
#include "PluginUiUtils.h"
#include "ControllerLayout3D.h"
#include "Effects3D/ScreenMirror/ScreenMirror.h"
#include "ui_GridSettingsPanel.h"

#include <QFont>
#include <QSlider>
#include <algorithm>
#include <cmath>
#include <exception>
#include "PluginLog.h"

GridSettingsPanel::GridSettingsPanel(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::GridSettingsPanel)
{
    ui->setupUi(this);

    PluginUiApplyMutedSecondaryLabel(ui->roomHelpLabel->label());
    PluginUiApplyMutedSecondaryLabel(ui->overlayHintLabel->label());
    PluginUiApplyBoldLabel(ui->overlaySectionLabel);
}

GridSettingsPanel::~GridSettingsPanel()
{
    delete ui;
}

void GridSettingsPanel::bindTab(OpenRGB3DSpatialTab* tab)
{
    if(!tab)
    {
        return;
    }

    host_tab_ = tab;

    ui->gridScaleSpin->setRange(0.1, 1000.0);
    ui->gridScaleSpin->setSingleStep(1.0);
    ui->gridScaleSpin->setValue(tab->grid_scale_mm);
    ui->gridScaleSpin->setSuffix(" mm/unit");
    ui->gridScaleSpin->setToolTip(
        "One grid unit = this many mm. Positions × scale = real size "
        "(e.g. scale 10 → 10 mm per unit).");

    ui->gridSnapCheckbox->setToolTip("When moving controllers, snap to grid intersections.");
    if(tab->viewport)
    {
        ui->gridSnapCheckbox->setChecked(tab->viewport->IsGridSnapEnabled());
    }

    QFont selection_font = ui->selectionInfoLabel->font();
    selection_font.setBold(true);
    ui->selectionInfoLabel->setFont(selection_font);

    tab->use_manual_room_size = true;

    auto setup_room_spin = [](QDoubleSpinBox* spin, double value, const QString& tooltip) {
        spin->setRange(100.0, 50000.0);
        spin->setSingleStep(10.0);
        spin->setValue(value);
        spin->setSuffix(" mm");
        spin->setToolTip(tooltip);
    };

    setup_room_spin(ui->roomWidthSpin, tab->manual_room_width, "Left to right, in mm");
    setup_room_spin(ui->roomHeightSpin, tab->manual_room_height, "Floor to ceiling, in mm");
    setup_room_spin(ui->roomDepthSpin, tab->manual_room_depth, "Front to back, in mm");

    ui->roomGridOverlayCheckbox->setToolTip(
        "Sample the effect stack at room grid points using the same shading as real LEDs "
        "(occlusion, relay, falloff). Lower brightness only dims the preview dots.");

    ui->roomGuideLabelsCheckbox->setToolTip(
        "Wall, floor, ceiling, and origin hints in the 3D view. Gizmo rotate text while dragging is always shown.");
    ui->frameSelectionButton->setToolTip(
        "Re-center the orbit on the current selection, set zoom from its size, and use the default "
        "3/4 view angle (45° yaw, 30° pitch). Right-drag still orbits afterward.");
    ui->resetCameraButton->setToolTip("Restore default orbit distance, angle, and target (0,0,0). Shortcut: Home.");

    ui->overlayBrightSlider->setRange(0, 100);
    ui->overlayPointSlider->setRange(1, 12);
    ui->overlayStepSlider->setRange(1, 24);

    ui->overlayBrightSlider->setToolTip(
        "Display gain for overlay points only (100% matches computed LED/light levels).");
    ui->overlayPointSlider->setToolTip("OpenGL point size for each overlay dot (1–12).");
    ui->overlayStepSlider->setToolTip(
        "Overlay sample step: sample from the room origin (0) along each axis every N grid cells, "
        "through to the far wall. Smaller N = denser preview dots; does not change room or LED layout size. "
        "Very large rooms may thin points for performance but still span the full room.");

    int overlay_bright_pct = 100;
    int overlay_point_size = 3;
    int overlay_step       = 4;

    connect(ui->gridScaleSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            &GridSettingsPanel::onGridScaleChanged);

    try
    {
        nlohmann::json settings = tab->GetPluginSettings();
        bool room_guide_labels = true;
        if(settings.contains("Viewport"))
        {
            const nlohmann::json& vp = settings["Viewport"];
            room_guide_labels = vp.value("ShowRoomGuideLabels", true);
        }
        if(tab->viewport)
        {
            tab->viewport->SetShowRoomGuideLabels(room_guide_labels);
        }
        ui->roomGuideLabelsCheckbox->setChecked(room_guide_labels);

        if(settings.contains("RoomGrid"))
        {
            const nlohmann::json& rg = settings["RoomGrid"];
            bool show                = rg.value("Show", false);
            overlay_bright_pct       = (int)(rg.value("Brightness", 1.0) * 100.0);
            overlay_point_size       = (int)rg.value("PointSize", 3.0);
            overlay_step             = (int)rg.value("Step", 4);
            overlay_bright_pct       = std::max(0, std::min(100, overlay_bright_pct));
            overlay_point_size       = std::max(1, std::min(12, overlay_point_size));
            overlay_step             = std::max(1, std::min(24, overlay_step));
            ui->roomGridOverlayCheckbox->setChecked(show);
            ui->overlayBrightSlider->setValue(overlay_bright_pct);
            ui->overlayPointSlider->setValue(overlay_point_size);
            ui->overlayStepSlider->setValue(overlay_step);
            ui->overlayBrightLabel->setText(QString::number(overlay_bright_pct) + "%");
            ui->overlayPointLabel->setText(QString::number(overlay_point_size));
            ui->overlayStepLabel->setText(QString::number(overlay_step));
            if(tab->viewport)
            {
                tab->viewport->SetShowRoomGridOverlay(show);
                tab->viewport->SetRoomGridBrightness((float)overlay_bright_pct / 100.0f);
                tab->viewport->SetRoomGridPointSize((float)overlay_point_size);
                tab->viewport->SetRoomGridStep(overlay_step);
            }
        }
    }
    catch(const std::exception& e)
    {
        LOG_WARNING("[OpenRGB3DSpatialPlugin] Failed to restore room grid overlay settings: %s", e.what());
    }

    connect(ui->roomGuideLabelsCheckbox, &QCheckBox::toggled, tab, &OpenRGB3DSpatialTab::roomGuideLabelsToggled);
    connect(ui->frameSelectionButton, &QPushButton::clicked, tab, &OpenRGB3DSpatialTab::frameSelectionInView);
    connect(ui->resetCameraButton, &QPushButton::clicked, tab, &OpenRGB3DSpatialTab::resetViewportCamera);
    connect(ui->roomGridOverlayCheckbox, &QCheckBox::toggled, this, &GridSettingsPanel::onRoomGridOverlayToggled);
    connect(ui->overlayBrightSlider, &QSlider::valueChanged, this, &GridSettingsPanel::onOverlayBrightChanged);
    connect(ui->overlayPointSlider, &QSlider::valueChanged, this, &GridSettingsPanel::onOverlayPointChanged);
    connect(ui->overlayStepSlider, &QSlider::valueChanged, this, &GridSettingsPanel::onOverlayStepChanged);

    connect(ui->gridSnapCheckbox, &QCheckBox::toggled, tab, &OpenRGB3DSpatialTab::gridSnapToggled);

    connect(ui->roomWidthSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            &GridSettingsPanel::onRoomWidthChanged);
    connect(ui->roomHeightSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            &GridSettingsPanel::onRoomHeightChanged);
    connect(ui->roomDepthSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            &GridSettingsPanel::onRoomDepthChanged);
}

void GridSettingsPanel::onGridScaleChanged(double value)
{
    if(!host_tab_)
    {
        return;
    }
    host_tab_->grid_scale_mm = (float)value;
    if(host_tab_->viewport)
    {
        host_tab_->viewport->SetGridScaleMM(host_tab_->grid_scale_mm);
        host_tab_->viewport->SetRoomDimensions(host_tab_->manual_room_width, host_tab_->manual_room_depth,
                                             host_tab_->manual_room_height, true);
    }
    host_tab_->SetLayoutDirty();
    if(host_tab_->current_effect_ui)
    {
        ScreenMirror* sm = qobject_cast<ScreenMirror*>(host_tab_->current_effect_ui);
        if(sm)
        {
            sm->SetGridScaleMM(host_tab_->grid_scale_mm);
        }
    }
    for(unsigned int i = 0; i < host_tab_->controller_transforms.size(); i++)
    {
        host_tab_->RegenerateLEDPositions(host_tab_->controller_transforms[i].get());
        ControllerLayout3D::UpdateWorldPositions(host_tab_->controller_transforms[i].get());
    }
    if(host_tab_->viewport)
    {
        host_tab_->viewport->SetControllerTransforms(&host_tab_->controller_transforms);
        host_tab_->viewport->update();
    }
}

void GridSettingsPanel::onRoomGridOverlayToggled(bool checked)
{
    if(!host_tab_)
    {
        return;
    }
    if(host_tab_->viewport)
    {
        host_tab_->viewport->SetShowRoomGridOverlay(checked);
    }
    host_tab_->PersistRoomGridOverlayToSettings();
}

void GridSettingsPanel::onOverlayBrightChanged(int v)
{
    if(!host_tab_)
    {
        return;
    }
    v = std::max(0, std::min(100, v));
    ui->overlayBrightLabel->setText(QString::number(v) + "%");
    if(host_tab_->viewport)
    {
        host_tab_->viewport->SetRoomGridBrightness((float)v / 100.0f);
    }
    host_tab_->PersistRoomGridOverlayToSettings();
}

void GridSettingsPanel::onOverlayPointChanged(int v)
{
    if(!host_tab_)
    {
        return;
    }
    v = std::max(1, std::min(12, v));
    ui->overlayPointLabel->setText(QString::number(v));
    if(host_tab_->viewport)
    {
        host_tab_->viewport->SetRoomGridPointSize((float)v);
    }
    host_tab_->PersistRoomGridOverlayToSettings();
}

void GridSettingsPanel::onOverlayStepChanged(int v)
{
    if(!host_tab_)
    {
        return;
    }
    v = std::max(1, std::min(24, v));
    ui->overlayStepLabel->setText(QString::number(v));
    if(host_tab_->viewport)
    {
        host_tab_->viewport->SetRoomGridStep(v);
    }
    host_tab_->PersistRoomGridOverlayToSettings();
}

void GridSettingsPanel::onRoomWidthChanged(double value)
{
    if(!host_tab_)
    {
        return;
    }
    host_tab_->use_manual_room_size = true;
    host_tab_->manual_room_width    = value;
    if(host_tab_->viewport)
    {
        host_tab_->viewport->SetRoomDimensions(host_tab_->manual_room_width, host_tab_->manual_room_depth,
                                             host_tab_->manual_room_height, true);
    }
    emit host_tab_->GridLayoutChanged();
    host_tab_->SetLayoutDirty();
}

void GridSettingsPanel::onRoomHeightChanged(double value)
{
    if(!host_tab_)
    {
        return;
    }
    host_tab_->use_manual_room_size = true;
    host_tab_->manual_room_height   = value;
    if(host_tab_->viewport)
    {
        host_tab_->viewport->SetRoomDimensions(host_tab_->manual_room_width, host_tab_->manual_room_depth,
                                             host_tab_->manual_room_height, true);
    }
    emit host_tab_->GridLayoutChanged();
    host_tab_->SetLayoutDirty();
}

void GridSettingsPanel::onRoomDepthChanged(double value)
{
    if(!host_tab_)
    {
        return;
    }
    host_tab_->use_manual_room_size = true;
    host_tab_->manual_room_depth    = value;
    if(host_tab_->viewport)
    {
        host_tab_->viewport->SetRoomDimensions(host_tab_->manual_room_width, host_tab_->manual_room_depth,
                                             host_tab_->manual_room_height, true);
    }
    emit host_tab_->GridLayoutChanged();
    host_tab_->SetLayoutDirty();
}

QDoubleSpinBox* GridSettingsPanel::gridScaleSpin() const { return ui->gridScaleSpin; }
QCheckBox* GridSettingsPanel::gridSnapCheckbox() const { return ui->gridSnapCheckbox; }
QLabel* GridSettingsPanel::selectionInfoLabel() const { return ui->selectionInfoLabel; }
QDoubleSpinBox* GridSettingsPanel::roomWidthSpin() const { return ui->roomWidthSpin; }
QDoubleSpinBox* GridSettingsPanel::roomHeightSpin() const { return ui->roomHeightSpin; }
QDoubleSpinBox* GridSettingsPanel::roomDepthSpin() const { return ui->roomDepthSpin; }
QCheckBox* GridSettingsPanel::roomGridOverlayCheckbox() const { return ui->roomGridOverlayCheckbox; }
QCheckBox* GridSettingsPanel::roomGuideLabelsCheckbox() const { return ui->roomGuideLabelsCheckbox; }
