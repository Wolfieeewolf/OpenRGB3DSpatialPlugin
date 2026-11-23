// SPDX-License-Identifier: GPL-2.0-or-later

#include "PlayerEngines.h"

void RegisterPlayerEngines()
{
    RegisterReactiveEngine();
    RegisterScreenMirrorEngine();
    RegisterMovieSyncEngine();
    RegisterShaderFieldEffects();
}
