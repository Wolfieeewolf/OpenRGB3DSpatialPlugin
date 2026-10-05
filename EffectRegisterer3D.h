// SPDX-License-Identifier: GPL-2.0-only

#ifndef EFFECTREGISTERER3D_H
#define EFFECTREGISTERER3D_H

#include "EffectListManager3D.h"

/** Standard self-registration for SpatialEffect3D subclasses.
 *  Put EFFECT_REGISTERER_3D* inside the class public section; define with REGISTER_EFFECT_3D(T) in the .cpp. */

#define EFFECT_REGISTERER_3D(_classname, _ui_name, _category, _constructor)                \
    static class _register                                                                 \
    {                                                                                      \
     public:                                                                               \
       _register()                                                                         \
       {                                                                                   \
           EffectListManager3D::get()->RegisterEffect(                                     \
               _classname, _ui_name, _category, "", "", _constructor,                      \
               SpatialEffectSettingsLayout::FullWithTransport);                            \
        }                                                                                  \
    } _registerer;

#define EFFECT_REGISTERER_3D_LAYOUT(_classname, _ui_name, _category, _layout, _constructor) \
    static class _register                                                                  \
    {                                                                                       \
     public:                                                                                \
       _register()                                                                          \
       {                                                                                    \
           EffectListManager3D::get()->RegisterEffect(                                      \
               _classname, _ui_name, _category, "", "", _constructor, _layout);             \
        }                                                                                   \
    } _registerer;

#define EFFECT_REGISTERER_3D_GAME(_classname, _ui_name, _game_id, _game_label, _constructor) \
    static class _register                                                                    \
    {                                                                                         \
     public:                                                                                  \
       _register()                                                                            \
       {                                                                                      \
           EffectListManager3D::get()->RegisterEffect(                                        \
               _classname, _ui_name, "Game", _game_id, _game_label, _constructor,             \
               SpatialEffectSettingsLayout::FullWithTransport);                               \
        }                                                                                     \
    } _registerer;

#define REGISTER_EFFECT_3D(T) T::_register T::_registerer;

#endif
