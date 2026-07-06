// SPDX-License-Identifier: GPL-2.0-only

#include "GridSettingsPanel.h"
#include "OpenRGB3DSpatialTab.h"
#include "PluginUiUtils.h"
#include "ControllerLayout3D.h"
#include "Effects3D/ScreenMirror/ScreenMirror.h"
#include "ui_GridSettingsPanel.h"

#include <QFont>
#include <QSignalBlocker>
#include <QSlider>
#include <algorithm>
#include <cmath>
#include <exception>

GridSettingsPanel::GridSettingsPanel(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::GridSettingsPanel)
{
    ui->setupUi(this);

    PluginUiApplyMutedSecondaryLabel(ui->gridScaleHelpLabel->label());
    PluginUiApplyMutedSecondaryLabel(ui->roomHelpLabel->label());
    PluginUiApplyMutedSecondaryLabel(ui->overlayHintLabel->label());

    ui->layoutSizeLabel->setToolTip(
        QStringLiteral("New controller LED grids: X = width count, Y = vertical layers, Z = depth count. "
                       "Matches scene axes (X left/right, Y up, Z front/back)."));
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

    ui->gridXSpin->setRange(1, 100);
    ui->gridXSpin->setValue(tab->custom_grid_x);
    ui->gridXSpin->setToolTip("LED layout width (grid units) for new controllers");

    ui->gridYSpin->setRange(1, 100);
    ui->gridYSpin->setValue(tab->custom_grid_y);
    ui->gridYSpin->setToolTip("LED layout height (grid units) for new controllers");

    ui->gridZSpin->setRange(1, 100);
    ui->gridZSpin->setValue(tab->custom_grid_z);
    ui->gridZSpin->setToolTip("LED layout depth (grid units) for new controllers");

    ui->gridScaleSpin->setRange(0.1, 1000.0);
    ui->gridScaleSpin->setSingleStep(1.0);
    ui->gridScaleSpin->setValue(tab->grid_scale_mm);
    ui->gridScaleSpin->setSuffix(" mm/unit");
    ui->gridScaleSpin->setToolTip(
        "Size of one grid unit in mm. Position in grid units × scale = real size in mm (e.g. scale 10 → 10 mm per unit).");

    ui->gridSnapCheckbox->setToolTip("When moving controllers, snap to grid intersections.");
    if(tab->viewport)
    {
        ui->gridSnapCheckbox->setChecked(tab->viewport->IsGridSnapEnabled());
    }

    QFont selection_font = ui->selectionInfoLabel->font();
    selection_font.setBold(true);
    ui->selectionInfoLabel->setFont(selection_font);

    ui->useManualRoomSizeCheckbox->setChecked(tab->use_manual_room_size);
    ui->useManualRoomSizeCheckbox->setToolTip(
        "Off: room size is derived from LED positions. On: set width, height, depth below.");

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

    const bool manual_room = tab->use_manual_room_size;
    ui->roomWidthSpin->setEnabled(manual_room);
    ui->roomHeightSpin->setEnabled(manual_room);
    ui->roomDepthSpin->setEnabled(manual_room);

    ui->roomGridOverlayCheckbox->setToolTip(
        "Sample the effect stack at room grid points using the same shading as real LEDs "
        "(occlusion, relay, falloff). Lower brightness only dims the preview dots.");

    ui->roomGuideLabelsCheckbox->setToolTip(
        "Wall, floor, ceiling, and origin hints in the 3D view. Gizmo rotate text while dragging is always shown.");
    ui->gpuLabelsCheckbox->setToolTip(
        "Alternative renderer for room guide labels only (not gizmo rotate text). "
        "Uses an offscreen image + small GL shader. Leave off unless labels flicker.");
    ui->frameSelectionButton->setToolTip(
        "Re-center the orbit on the current selection, set zoom from its size, and use the default "
        "3/4 view angle (45° yaw, 30° pitch). Right-drag still orbits afterward.");
    ui->resetCameraButton->setToolTip("Restore default orbit distance, angle, and target (0,0,0). Shortcut: Home.");
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    ui->gpuSceneCheckbox->setToolTip(
        "Draw walls/floor/ceiling with the experimental shader room (Qt 6 OpenRGB only). "
        "Same as OPENRGB3D_VIEWPORT_GPU_SCENE=1. Ignored on Qt 5.15 hosts.");
#else
    if(ui->gpuSceneCheckbox)
    {
        ui->gpuSceneCheckbox->setVisible(false);
    }
#endif

    ui->overlayBrightSlider->setRange(0, 100);
    ui->overlayPointSlider->setRange(1, 12);
    ui->overlayStepSlider->setRange(1, 24);

    ui->overlayBrightSlider->setToolTip(
        "Display gain for overlay points only (100% matches computed LED/light levels).");
    ui->overlayPointSlider->setToolTip("OpenGL point size for each overlay dot (1–12).");
    ui->overlayStepSlider->setToolTip(
        "Sample from the room origin (0) along each axis every N grid cells, through to the far wall. "
        "Smaller N = denser points; very large rooms may thin points for performance but still span the full room.");

    int overlay_bright_pct = 100;
    int overlay_point_size = 3;
    int overlay_step       = 4;

    connect(ui->gridScaleSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            &GridSettingsPanel::onGridScaleChanged);

    bool gpu_labels = false;
    bool gpu_scene = false;
    try
    {
        nlohmann::json settings = tab->GetPluginSettings();
        bool room_guide_labels = true;
        if(settings.contains("Viewport"))
        {
            const nlohmann::json& vp = settings["Viewport"];
            gpu_labels = vp.value("GpuLabels", false);
            gpu_scene = vp.value("GpuScene", false);
            room_guide_labels = vp.value("ShowRoomGuideLabels", true);
        }
        if(tab->viewport)
        {
            tab->viewport->SetPreferGpuLabelOverlay(gpu_labels);
            tab->viewport->SetPreferGpuScene(gpu_scene);
            tab->viewport->SetShowRoomGuideLabels(room_guide_labels);
        }
        ui->gpuLabelsCheckbox->setChecked(gpu_labels);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        ui->gpuSceneCheckbox->setChecked(gpu_scene);
#endif
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
    catch(const std::exception&)
    {
    }

    auto sync_gpu_label_renderer_ui = [this, tab](bool room_labels_on) {
        if(!ui->gpuLabelsCheckbox)
        {
            return;
        }
        ui->gpuLabelsCheckbox->setEnabled(room_labels_on);
        if(!room_labels_on)
        {
            const QSignalBlocker block(ui->gpuLabelsCheckbox);
            ui->gpuLabelsCheckbox->setChecked(false);
            if(tab && tab->viewport)
            {
                tab->viewport->SetPreferGpuLabelOverlay(false);
            }
        }
    };

    sync_gpu_label_renderer_ui(ui->roomGuideLabelsCheckbox->isChecked());

    connect(ui->roomGuideLabelsCheckbox, &QCheckBox::toggled, tab, &OpenRGB3DSpatialTab::roomGuideLabelsToggled);
    connect(ui->roomGuideLabelsCheckbox, &QCheckBox::toggled, this, [this, tab, sync_gpu_label_renderer_ui](bool on) {
        sync_gpu_label_renderer_ui(on);
    });
    connect(ui->gpuLabelsCheckbox, &QCheckBox::toggled, tab, &OpenRGB3DSpatialTab::gpuLabelsToggled);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    connect(ui->gpuSceneCheckbox, &QCheckBox::toggled, tab, &OpenRGB3DSpatialTab::gpuSceneToggled);
#endif
    connect(ui->frameSelectionButton, &QPushButton::clicked, tab, &OpenRGB3DSpatialTab::frameSelectionInView);
    connect(ui->resetCameraButton, &QPushButton::clicked, tab, &OpenRGB3DSpatialTab::resetViewportCamera);
    connect(ui->roomGridOverlayCheckbox, &QCheckBox::toggled, this, &GridSettingsPanel::onRoomGridOverlayToggled);
    connect(ui->overlayBrightSlider, &QSlider::valueChanged, this, &GridSettingsPanel::onOverlayBrightChanged);
    connect(ui->overlayPointSlider, &QSlider::valueChanged, this, &GridSettingsPanel::onOverlayPointChanged);
    connect(ui->overlayStepSlider, &QSlider::valueChanged, this, &GridSettingsPanel::onOverlayStepChanged);

    connect(ui->gridXSpin, QOverload<int>::of(&QSpinBox::valueChanged), tab,
            &OpenRGB3DSpatialTab::gridDimensionsChanged);
    connect(ui->gridYSpin, QOverload<int>::of(&QSpinBox::valueChanged), tab,
            &OpenRGB3DSpatialTab::gridDimensionsChanged);
    connect(ui->gridZSpin, QOverload<int>::of(&QSpinBox::valueChanged), tab,
            &OpenRGB3DSpatialTab::gridDimensionsChanged);
    connect(ui->gridSnapCheckbox, &QCheckBox::toggled, tab, &OpenRGB3DSpatialTab::gridSnapToggled);

    connect(ui->useManualRoomSizeCheckbox, &QCheckBox::toggled, this, &GridSettingsPanel::onUseManualRoomSizeToggled);
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
                                             host_tab_->manual_room_height, host_tab_->use_manual_room_size);
    }
    host_tab_->SavePluginUiSettings();
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

void GridSettingsPanel::onUseManualRoomSizeToggled(bool checked)
{
    if(!host_tab_)
    {
        return;
    }
    host_tab_->use_manual_room_size = checked;
    ui->roomWidthSpin->setEnabled(checked);
    ui->roomHeightSpin->setEnabled(checked);
    ui->roomDepthSpin->setEnabled(checked);
    if(host_tab_->viewport)
    {
        host_tab_->viewport->SetRoomDimensions(host_tab_->manual_room_width, host_tab_->manual_room_depth,
                                             host_tab_->manual_room_height, host_tab_->use_manual_room_size);
    }
    emit host_tab_->GridLayoutChanged();
    host_tab_->SavePluginUiSettings();
}

void GridSettingsPanel::onRoomWidthChanged(double value)
{
    if(!host_tab_)
    {
        return;
    }
    host_tab_->manual_room_width = value;
    if(host_tab_->viewport)
    {
        host_tab_->viewport->SetRoomDimensions(host_tab_->manual_room_width, host_tab_->manual_room_depth,
                                             host_tab_->manual_room_height, host_tab_->use_manual_room_size);
    }
    emit host_tab_->GridLayoutChanged();
    host_tab_->SavePluginUiSettings();
}

void GridSettingsPanel::onRoomHeightChanged(double value)
{
    if(!host_tab_)
    {
        return;
    }
    host_tab_->manual_room_height = value;
    if(host_tab_->viewport)
    {
        host_tab_->viewport->SetRoomDimensions(host_tab_->manual_room_width, host_tab_->manual_room_depth,
                                             host_tab_->manual_room_height, host_tab_->use_manual_room_size);
    }
    emit host_tab_->GridLayoutChanged();
    host_tab_->SavePluginUiSettings();
}

void GridSettingsPanel::onRoomDepthChanged(double value)
{
    if(!host_tab_)
    {
        return;
    }
    host_tab_->manual_room_depth = value;
    if(host_tab_->viewport)
    {
        host_tab_->viewport->SetRoomDimensions(host_tab_->manual_room_width, host_tab_->manual_room_depth,
                                             host_tab_->manual_room_height, host_tab_->use_manual_room_size);
    }
    emit host_tab_->GridLayoutChanged();
    host_tab_->SavePluginUiSettings();
}

QSpinBox* GridSettingsPanel::gridXSpin() const { return ui->gridXSpin; }
QSpinBox* GridSettingsPanel::gridYSpin() const { return ui->gridYSpin; }
QSpinBox* GridSettingsPanel::gridZSpin() const { return ui->gridZSpin; }
QDoubleSpinBox* GridSettingsPanel::gridScaleSpin() const { return ui->gridScaleSpin; }
QCheckBox* GridSettingsPanel::gridSnapCheckbox() const { return ui->gridSnapCheckbox; }
QLabel* GridSettingsPanel::selectionInfoLabel() const { return ui->selectionInfoLabel; }
QCheckBox* GridSettingsPanel::useManualRoomSizeCheckbox() const { return ui->useManualRoomSizeCheckbox; }
QDoubleSpinBox* GridSettingsPanel::roomWidthSpin() const { return ui->roomWidthSpin; }
QDoubleSpinBox* GridSettingsPanel::roomHeightSpin() const { return ui->roomHeightSpin; }
QDoubleSpinBox* GridSettingsPanel::roomDepthSpin() const { return ui->roomDepthSpin; }
QCheckBox* GridSettingsPanel::roomGridOverlayCheckbox() const { return ui->roomGridOverlayCheckbox; }
QCheckBox* GridSettingsPanel::roomGuideLabelsCheckbox() const { return ui->roomGuideLabelsCheckbox; }
QCheckBox* GridSettingsPanel::gpuLabelsCheckbox() const { return ui->gpuLabelsCheckbox; }
