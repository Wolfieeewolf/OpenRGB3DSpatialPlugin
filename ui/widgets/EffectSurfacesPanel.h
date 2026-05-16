// SPDX-License-Identifier: GPL-2.0-only

#ifndef EFFECTSURFACESPANEL_H
#define EFFECTSURFACESPANEL_H

#include <QWidget>

class SpatialEffect3D;

class EffectSurfacesPanel : public QWidget
{
public:
    explicit EffectSurfacesPanel(int surface_mask, SpatialEffect3D* host, QWidget* parent = nullptr);
};

#endif
