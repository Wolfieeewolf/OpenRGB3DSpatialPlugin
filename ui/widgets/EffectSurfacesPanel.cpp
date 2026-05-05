// SPDX-License-Identifier: GPL-2.0-only

#include "EffectSurfacesPanel.h"

#include "SpatialEffect3D.h"
#include "SpatialEffectTypes.h"

#include <QCheckBox>
#include <QGridLayout>

EffectSurfacesPanel::EffectSurfacesPanel(int surface_mask, SpatialEffect3D* host, QWidget* parent)
    : QWidget(parent)
{
    QGridLayout* surf_layout = new QGridLayout(this);

    QCheckBox* cb_floor = new QCheckBox(QStringLiteral("Floor"));
    cb_floor->setChecked((surface_mask & SURF_FLOOR) != 0);
    surf_layout->addWidget(cb_floor, 0, 0);
    connect(cb_floor, &QCheckBox::toggled, host, [host](bool on) { host->SetSurfaceMaskFlag(SURF_FLOOR, on); });

    QCheckBox* cb_ceil = new QCheckBox(QStringLiteral("Ceiling"));
    cb_ceil->setChecked((surface_mask & SURF_CEIL) != 0);
    surf_layout->addWidget(cb_ceil, 0, 1);
    connect(cb_ceil, &QCheckBox::toggled, host, [host](bool on) { host->SetSurfaceMaskFlag(SURF_CEIL, on); });

    QCheckBox* cb_wxm = new QCheckBox(QStringLiteral("Wall -X"));
    cb_wxm->setChecked((surface_mask & SURF_WALL_XM) != 0);
    surf_layout->addWidget(cb_wxm, 0, 2);
    connect(cb_wxm, &QCheckBox::toggled, host, [host](bool on) { host->SetSurfaceMaskFlag(SURF_WALL_XM, on); });

    QCheckBox* cb_wxp = new QCheckBox(QStringLiteral("Wall +X"));
    cb_wxp->setChecked((surface_mask & SURF_WALL_XP) != 0);
    surf_layout->addWidget(cb_wxp, 1, 0);
    connect(cb_wxp, &QCheckBox::toggled, host, [host](bool on) { host->SetSurfaceMaskFlag(SURF_WALL_XP, on); });

    QCheckBox* cb_wzm = new QCheckBox(QStringLiteral("Wall -Z"));
    cb_wzm->setChecked((surface_mask & SURF_WALL_ZM) != 0);
    surf_layout->addWidget(cb_wzm, 1, 1);
    connect(cb_wzm, &QCheckBox::toggled, host, [host](bool on) { host->SetSurfaceMaskFlag(SURF_WALL_ZM, on); });

    QCheckBox* cb_wzp = new QCheckBox(QStringLiteral("Wall +Z"));
    cb_wzp->setChecked((surface_mask & SURF_WALL_ZP) != 0);
    surf_layout->addWidget(cb_wzp, 1, 2);
    connect(cb_wzp, &QCheckBox::toggled, host, [host](bool on) { host->SetSurfaceMaskFlag(SURF_WALL_ZP, on); });
}
