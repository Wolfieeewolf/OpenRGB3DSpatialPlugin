// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QString>

/** Upload uniform float u_name[count]. Qt setUniformValueArray is unreliable on
 *  #version 110 array uniforms (location -1 or silently no-op). */
inline void SetGlslFloatUniformArray(QOpenGLShaderProgram& program,
                                     QOpenGLFunctions* gl,
                                     const char* name,
                                     const float* values,
                                     int count)
{
    if(!gl || !name || !values || count <= 0)
    {
        return;
    }
    int loc = program.uniformLocation(name);
    if(loc < 0)
    {
        loc = program.uniformLocation(QStringLiteral("%1[0]").arg(QLatin1String(name)));
    }
    if(loc >= 0)
    {
        gl->glUniform1fv(loc, count, values);
        return;
    }
    for(int i = 0; i < count; ++i)
    {
        const int li = program.uniformLocation(QStringLiteral("%1[%2]").arg(QLatin1String(name)).arg(i));
        if(li >= 0)
        {
            program.setUniformValue(li, values[i]);
        }
    }
}
