/*---------------------------------------------------------*\
| Plasma.h                                                  |
|                                                           |
|   Plasma effect with multiple sine waves                  |
|                                                           |
|   Date: 2025-09-27                                        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#ifndef PLASMA_H
#define PLASMA_H

#include "SpatialEffect.h"
#include "SpatialEffectRegisterer.h"

class Plasma : public SpatialEffect
{
public:
    Plasma();
    ~Plasma();

    SPATIAL_EFFECT_REGISTERER(ClassName(), UI_Name(), CAT_PLASMA, [](){return new Plasma;});

    static std::string const ClassName() { return "Plasma"; }
    static std::string const UI_Name() { return "Plasma"; }

    SpatialEffectInfo GetEffectInfo() override;
    RGBColor CalculateColor(Vector3D position, float time_offset, const SpatialEffectParams& params) override;
};

#endif // PLASMA_H