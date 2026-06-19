// SPDX-License-Identifier: GPL-2.0-only

#ifndef EFFECTREGISTERER3D_H
#define EFFECTREGISTERER3D_H

#include "EffectListManager3D.h"

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

#define REGISTER_EFFECT_3D(T) T::_register T::_registerer;

#endif
