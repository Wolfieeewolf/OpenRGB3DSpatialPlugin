/*---------------------------------------------------------*\
| SpatialEffectRegisterer.h                                 |
|                                                           |
|   Auto-registration system for 3D spatial effects        |
|                                                           |
|   Date: 2025-09-27                                        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#ifndef SPATIALEFFECTREGISTERER_H
#define SPATIALEFFECTREGISTERER_H

#include "SpatialEffectManager.h"

#define SPATIAL_EFFECT_REGISTERER(_classname, _ui_name, _category, _constructor)                    \
    static class _register                                                                          \
    {                                                                                               \
     public:                                                                                        \
       _register()                                                                                  \
       {                                                                                            \
           SpatialEffectManager::Get()->RegisterEffect(_classname, _ui_name, _category, _constructor); \
        }                                                                                           \
    } _registerer;                                                                                  \

#define REGISTER_SPATIAL_EFFECT(T) T::_register T::_registerer;

#endif // SPATIALEFFECTREGISTERER_H