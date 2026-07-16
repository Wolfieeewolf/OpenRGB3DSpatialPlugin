// SPDX-License-Identifier: GPL-2.0-only

#ifndef VIEWPORTGLINCLUDES_H
#define VIEWPORTGLINCLUDES_H

/** Fixed-function / GL 1.x entry points not exposed on QOpenGLFunctions (Qt 5). */
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include <GL/gl.h>

#ifndef GL_PROGRAM_POINT_SIZE
#define GL_PROGRAM_POINT_SIZE 0x8642
#endif

#endif
