// SPDX-License-Identifier: GPL-2.0-only

#ifndef EFFECTSURFACESPANEL_H
#define EFFECTSURFACESPANEL_H

#include <QWidget>

class SpatialEffect3D;

namespace Ui {
class EffectSurfacesPanel;
}

class EffectSurfacesPanel : public QWidget
{
public:
    explicit EffectSurfacesPanel(int surface_mask, SpatialEffect3D* host, QWidget* parent = nullptr);
    ~EffectSurfacesPanel() override;

private:
    Ui::EffectSurfacesPanel* ui = nullptr;
};

#endif
