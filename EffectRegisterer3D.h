/*---------------------------------------------------------*\
| EffectRegisterer3D.h                                      |
|                                                           |
|   Auto-registration system for 3D spatial effects        |
|                                                           |
|   Date: 2025-10-01                                        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#ifndef EFFECTREGISTERER3D_H
#define EFFECTREGISTERER3D_H

#include "EffectListManager3D.h"

/*---------------------------------------------------------*\
| Effect Registration Macro                                |
| Usage in header:                                         |
|   EFFECT_REGISTERER_3D("Wave3D", "3D Wave",             |
|                        "3D Spatial", [](){return new Wave3D;}) |
\*---------------------------------------------------------*/
#define EFFECT_REGISTERER_3D(_classname, _ui_name, _category, _constructor) \
    static class _register                                                   \
    {                                                                        \
     public:                                                                 \
       _register()                                                           \
       {                                                                     \
           EffectListManager3D::get()->RegisterEffect(_classname, _ui_name, \
                                                       _category, _constructor); \
        }                                                                    \
    } _registerer;

/*---------------------------------------------------------*\
| Effect Registration in CPP                               |
| Usage in cpp file:                                       |
|   REGISTER_EFFECT_3D(Wave3D)                            |
\*---------------------------------------------------------*/
#define REGISTER_EFFECT_3D(T) T::_register T::_registerer;

#endif
