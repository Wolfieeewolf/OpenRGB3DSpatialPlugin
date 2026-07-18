// SPDX-License-Identifier: GPL-2.0-only

#include "SceneTransformPanel.h"
#include "OpenRGB3DSpatialTab.h"
#include "PluginUiUtils.h"
#include "PositionAxisDragController.h"
#include "ui_SceneTransformPanel.h"

#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSlider>
#include <cmath>

namespace
{

void ConfigureTransformAxisGrid(QGridLayout* layout, int spin_min_width)
{
    if(!layout)
    {
        return;
    }

    layout->setContentsMargins(6, 6, 6, 6);
    layout->setHorizontalSpacing(10);
    layout->setVerticalSpacing(6);
    layout->setColumnStretch(0, 0);
    layout->setColumnStretch(1, 1);
    layout->setColumnStretch(2, 0);
    layout->setColumnMinimumWidth(2, spin_min_width);
}

void ConfigureTransformAxisRow(QSlider* slider, QDoubleSpinBox* spin, int spin_min_width)
{
    if(slider)
    {
        slider->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        slider->setMinimumWidth(48);
    }
    if(spin)
    {
        spin->setMinimumWidth(spin_min_width);
        spin->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    }
}

} // namespace

void SceneTransformPanel::wireRotationRow(OpenRGB3DSpatialTab* tab, QSlider* sl, QDoubleSpinBox* sp, int axis)
{
    sl->setRange(-180, 180);
    sl->setValue(0);
    sl->setToolTip("Rotation in degrees.");

    sp->setRange(-180, 180);
    sp->setDecimals(1);
    sp->setSuffix(QString::fromUtf8(" °"));
    sp->setToolTip("Rotation in degrees.");

    QObject::connect(sl, &QSlider::valueChanged, tab, [tab, sl, sp, axis](int value) {
        double rot_value = (double)value;
        if(sp)
        {
            QSignalBlocker b(sp);
            sp->setValue(rot_value);
        }
        tab->ApplyRotationComponent(axis, rot_value);
    });
    QObject::connect(sp, QOverload<double>::of(&QDoubleSpinBox::valueChanged), tab, [tab, sl, sp, axis](double value) {
        if(sl)
        {
            QSignalBlocker b(sl);
            sl->setValue((int)std::lround(value));
        }
        tab->ApplyRotationComponent(axis, value);
    });
}

SceneTransformPanel::SceneTransformPanel(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::SceneTransformPanel)
{
    ui->setupUi(this);

    PluginUiApplyMutedSecondaryLabel(ui->transformHelpLabel->label());

    ui->positionGroup->setToolTip(
        "Where the selected controller, reference point, or display plane sits in the room. Not the camera.");
    ui->rotationGroup->setToolTip(
        "Orientation of the selected controller, reference point, or display plane in the room. Not the camera view.");

    // Position spins carry a " mm" suffix; rotation carries " °". Reserve enough width
    // so the slider column cannot collide with the spin boxes on narrow Setup panels.
    constexpr int kPosSpinMinWidth = 110;
    constexpr int kRotSpinMinWidth = 90;

    ConfigureTransformAxisGrid(ui->positionLayout, kPosSpinMinWidth);
    ConfigureTransformAxisGrid(ui->rotationLayout, kRotSpinMinWidth);

    ConfigureTransformAxisRow(ui->posXSlider, ui->posXSpin, kPosSpinMinWidth);
    ConfigureTransformAxisRow(ui->posYSlider, ui->posYSpin, kPosSpinMinWidth);
    ConfigureTransformAxisRow(ui->posZSlider, ui->posZSpin, kPosSpinMinWidth);
    ConfigureTransformAxisRow(ui->rotXSlider, ui->rotXSpin, kRotSpinMinWidth);
    ConfigureTransformAxisRow(ui->rotYSlider, ui->rotYSpin, kRotSpinMinWidth);
    ConfigureTransformAxisRow(ui->rotZSlider, ui->rotZSpin, kRotSpinMinWidth);
}

SceneTransformPanel::~SceneTransformPanel() = default;

void SceneTransformPanel::bindTab(OpenRGB3DSpatialTab* tab)
{
    if(!tab)
    {
        return;
    }

    const QString x_tip =
        QStringLiteral("Left–right in mm. Drag the slider handle to move smoothly across the room width; "
                       "Shift = finer, Ctrl = medium. Wheel on slider: 0.1 mm per step.");
    const QString y_tip =
        QStringLiteral("Floor–ceiling in mm. Drag handle across room height; Shift/Ctrl adjust sensitivity. Clamps to ≥0.");
    const QString z_tip =
        QStringLiteral("Front–back in mm. Drag handle across room depth; Shift/Ctrl adjust sensitivity.");

    if(ui->posXSlider)
    {
        ui->posXSlider->setToolTip(x_tip);
    }
    if(ui->posYSlider)
    {
        ui->posYSlider->setToolTip(y_tip);
    }
    if(ui->posZSlider)
    {
        ui->posZSlider->setToolTip(z_tip);
    }

    ui->posXSpin->setRange(-50000.0, 50000.0);
    ui->posYSpin->setRange(-50000.0, 50000.0);
    ui->posZSpin->setRange(-50000.0, 50000.0);
    ui->posXSpin->setToolTip(x_tip);
    ui->posYSpin->setToolTip(y_tip);
    ui->posZSpin->setToolTip(z_tip);

    pos_x_drag_ = std::make_unique<PositionAxisDragController>(tab, ui->posXSlider, ui->posXSpin, 0, false);
    pos_y_drag_ = std::make_unique<PositionAxisDragController>(tab, ui->posYSlider, ui->posYSpin, 1, true);
    pos_z_drag_ = std::make_unique<PositionAxisDragController>(tab, ui->posZSlider, ui->posZSpin, 2, false);

    SceneTransformPanel::wireRotationRow(tab, ui->rotXSlider, ui->rotXSpin, 0);
    SceneTransformPanel::wireRotationRow(tab, ui->rotYSlider, ui->rotYSpin, 1);
    SceneTransformPanel::wireRotationRow(tab, ui->rotZSlider, ui->rotZSpin, 2);
}

void SceneTransformPanel::setPositionControlsMm(double x_mm, double y_mm, double z_mm)
{
    if(pos_x_drag_)
    {
        pos_x_drag_->setCurrentMm(x_mm);
    }
    if(pos_y_drag_)
    {
        pos_y_drag_->setCurrentMm(y_mm);
    }
    if(pos_z_drag_)
    {
        pos_z_drag_->setCurrentMm(z_mm);
    }
}

QDoubleSpinBox* SceneTransformPanel::posXSpin() const { return ui->posXSpin; }
QDoubleSpinBox* SceneTransformPanel::posYSpin() const { return ui->posYSpin; }
QDoubleSpinBox* SceneTransformPanel::posZSpin() const { return ui->posZSpin; }
QSlider* SceneTransformPanel::posXSlider() const { return ui->posXSlider; }
QSlider* SceneTransformPanel::posYSlider() const { return ui->posYSlider; }
QSlider* SceneTransformPanel::posZSlider() const { return ui->posZSlider; }
QDoubleSpinBox* SceneTransformPanel::rotXSpin() const { return ui->rotXSpin; }
QDoubleSpinBox* SceneTransformPanel::rotYSpin() const { return ui->rotYSpin; }
QDoubleSpinBox* SceneTransformPanel::rotZSpin() const { return ui->rotZSpin; }
QSlider* SceneTransformPanel::rotXSlider() const { return ui->rotXSlider; }
QSlider* SceneTransformPanel::rotYSlider() const { return ui->rotYSlider; }
QSlider* SceneTransformPanel::rotZSlider() const { return ui->rotZSlider; }
