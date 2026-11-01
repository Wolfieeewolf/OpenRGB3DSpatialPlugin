// SPDX-License-Identifier: GPL-2.0-only
#pragma once

class QOpenGLContext;
class QOpenGLExtraFunctions;

/**
 * Shared fullscreen triangle-strip quad for field-engine atlases (GLSL 410 Core).
 * Lives on the SpatialOffscreenGlPool context; call only while a Session is current.
 */
namespace FieldFullscreenQuad410
{

/** Vertex shader source: layout(location=0) in vec2 a_position. */
const char* VertexShaderSource();

/** Ensure VAO/VBO exist on the current context, then draw the quad. */
bool Draw(QOpenGLContext* ctx);

/** Delete VAO/VBO if they belong to the current context. */
void Destroy(QOpenGLContext* ctx);

} // namespace FieldFullscreenQuad410
