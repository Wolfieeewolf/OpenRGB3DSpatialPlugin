// SPDX-License-Identifier: GPL-2.0-only

#include "EffectGlobalSettingsPanel.h"
#include "OpenRGB3DSpatialTab.h"
#include "EffectListManager3D.h"
#include "SpatialEffect3D.h"
#include "ui_EffectGlobalSettingsPanel.h"

#include <QComboBox>

EffectGlobalSettingsPanel::EffectGlobalSettingsPanel(QWidget* parent)
    : QGroupBox(parent)
    , ui(new Ui::EffectGlobalSettingsPanel)
{
    ui->setupUi(this);
}

EffectGlobalSettingsPanel::~EffectGlobalSettingsPanel()
{
    delete ui;
}

void EffectGlobalSettingsPanel::bindTab(OpenRGB3DSpatialTab* tab)
{
    if(!tab)
    {
        return;
    }

    ui->effectCombo->setToolTip(
        tr("Select an effect layer from the stack to edit its controls."));
    ui->zoneCombo->setToolTip(
        tr("Target selection for the selected layer. "
           "Target zone bounds (below) needs a real zone or a meaningful selection so the local grid can be built."));
    ui->originCombo->setToolTip(
        tr("Where patterns attach in space for this layer. "
           "\"Room box center\" maps the pattern across the full room (replaces the old Room coordinates option). "
           "\"Mapped lights center\" keeps origin-based math on your hardware layout."));
    ui->boundsCombo->setToolTip(
        tr("Global: sample using the normal room (or world) grid. "
           "Target zone: sample positions mapped across the zone (or target) bounding box so motion and detail read on "
           "that volume."));

    connect(ui->effectCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), tab,
            &OpenRGB3DSpatialTab::on_effect_changed);

    tab->PopulateZoneTargetCombo(ui->zoneCombo, -1);
    connect(ui->zoneCombo, SIGNAL(currentIndexChanged(int)), tab, SLOT(on_effect_zone_changed(int)));

    ui->originCombo->addItem(tr("Mapped lights center (recommended)"), QVariant(-4));
    ui->originCombo->addItem(tr("Room box center"), QVariant(-1));
    ui->originCombo->addItem(tr("Target zone center"), QVariant(-2));
    ui->originCombo->addItem(tr("No anchor (world 0,0,0)"), QVariant(-3));
    connect(ui->originCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), tab,
            &OpenRGB3DSpatialTab::on_effect_origin_changed);

    ui->boundsCombo->addItem(tr("Global bounds"), QVariant((int)SpatialEffect3D::BOUNDS_MODE_GLOBAL));
    ui->boundsCombo->setItemData(
        0,
        tr("Use the full room or world grid bounds for this layer's pattern math (same space as the 3D viewport)."),
        Qt::ToolTipRole);
    ui->boundsCombo->addItem(tr("Target zone bounds"), QVariant((int)SpatialEffect3D::BOUNDS_MODE_TARGET_ZONE));
    ui->boundsCombo->setItemData(
        1,
        tr("Build a local grid from the Zone target's bounding box and sample the effect in that space—useful when LEDs "
           "only cover part of the room."),
        Qt::ToolTipRole);
    connect(ui->boundsCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), tab,
            &OpenRGB3DSpatialTab::on_effect_bounds_changed);

    ui->stackEffectTypeCombo->clear();
    ui->stackEffectTypeCombo->addItem(QStringLiteral("None"), QString());
    const std::vector<EffectRegistration3D> effect_list = EffectListManager3D::get()->GetAllEffects();
    for(const EffectRegistration3D& reg : effect_list)
    {
        ui->stackEffectTypeCombo->addItem(QString::fromStdString(reg.ui_name),
                                          QString::fromStdString(reg.class_name));
    }
    connect(ui->stackEffectTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), tab,
            &OpenRGB3DSpatialTab::on_stack_effect_type_changed);

    connect(ui->stackEffectZoneCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), tab,
            &OpenRGB3DSpatialTab::on_stack_effect_zone_changed);

    tab->UpdateEffectCombo();
    tab->UpdateStackEffectZoneCombo();
}

QLabel* EffectGlobalSettingsPanel::effectRowLabel() const { return ui->effectRowLabel; }
QComboBox* EffectGlobalSettingsPanel::effectCombo() const { return ui->effectCombo; }
QLabel* EffectGlobalSettingsPanel::zoneLabel() const { return ui->zoneLabel; }
QComboBox* EffectGlobalSettingsPanel::zoneCombo() const { return ui->zoneCombo; }
QLabel* EffectGlobalSettingsPanel::originLabelWidget() const { return ui->originLabel; }
QComboBox* EffectGlobalSettingsPanel::originCombo() const { return ui->originCombo; }
QLabel* EffectGlobalSettingsPanel::boundsLabel() const { return ui->boundsLabel; }
QComboBox* EffectGlobalSettingsPanel::boundsCombo() const { return ui->boundsCombo; }
QComboBox* EffectGlobalSettingsPanel::stackEffectTypeCombo() const { return ui->stackEffectTypeCombo; }
QComboBox* EffectGlobalSettingsPanel::stackEffectZoneCombo() const { return ui->stackEffectZoneCombo; }
QWidget* EffectGlobalSettingsPanel::roomOutputSection() const { return ui->roomOutputSection; }
QWidget* EffectGlobalSettingsPanel::roomOutputHost() const { return ui->roomOutputHost; }
