// SPDX-License-Identifier: GPL-2.0-only

#ifndef VIEWPORTSHADERS_H
#define VIEWPORTSHADERS_H

#include <QString>

class GlProgram;

/** GLSL 410 sources + compile helpers for the room viewport. */
namespace ViewportShaders
{
const char* UnlitColorVertex();
const char* UnlitColorFragment();
const char* UnlitPointVertex();
const char* UnlitPointFragment();
const char* TexturedUnlitVertex();
const char* TexturedUnlitFragment();

bool CompileUnlitColor(GlProgram& out, QString* error_log = nullptr);
bool CompileUnlitPoint(GlProgram& out, QString* error_log = nullptr);
bool CompileTexturedUnlit(GlProgram& out, QString* error_log = nullptr);
}

#endif
