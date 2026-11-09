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
/** Mesh-anim: base/emis/MR/normal + cheap IBL + HDR emis (tonemap in bloom). */
const char* MeshUnlitVertex();
const char* MeshUnlitFragment();
/** Depth-to-color for soft shadow map (directional light). */
const char* MeshDepthVertex();
const char* MeshDepthFragment();
const char* FullscreenVertex();
const char* BloomBrightFragment();
const char* BloomBlurFragment();
/** Tonemap(HDR scene) + bloom halo → LDR widget FBO. */
const char* BloomCompositeFragment();

bool CompileUnlitColor(GlProgram& out, QString* error_log = nullptr);
bool CompileUnlitPoint(GlProgram& out, QString* error_log = nullptr);
bool CompileTexturedUnlit(GlProgram& out, QString* error_log = nullptr);
bool CompileMeshUnlit(GlProgram& out, QString* error_log = nullptr);
bool CompileMeshDepth(GlProgram& out, QString* error_log = nullptr);
bool CompileBloomBright(GlProgram& out, QString* error_log = nullptr);
bool CompileBloomBlur(GlProgram& out, QString* error_log = nullptr);
bool CompileBloomComposite(GlProgram& out, QString* error_log = nullptr);
}

#endif
