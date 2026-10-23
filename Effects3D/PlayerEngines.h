// SPDX-License-Identifier: GPL-2.0-only

#ifndef PLAYER_ENGINES_H
#define PLAYER_ENGINES_H

/** Register Reactive, Screen Mirror, Movie Sync, and per-file Shader Field effects. Call once from Load(). */
void RegisterPlayerEngines();

void RegisterReactiveEngine();
void RegisterScreenMirrorEngine();
void RegisterMovieSyncEngine();
void RegisterShaderFieldEffects();

#endif
