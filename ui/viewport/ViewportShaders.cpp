// SPDX-License-Identifier: GPL-2.0-only

#include "ViewportShaders.h"
#include "GlProgram.h"

namespace ViewportShaders
{

const char* UnlitColorVertex()
{
    return R"GLSL(
#version 410 core
layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_color;
uniform mat4 u_mvp;
out vec3 v_color;
void main()
{
    v_color = a_color;
    gl_Position = u_mvp * vec4(a_position, 1.0);
}
)GLSL";
}

const char* UnlitColorFragment()
{
    return R"GLSL(
#version 410 core
in vec3 v_color;
uniform float u_alpha;
out vec4 frag_color;
void main()
{
    frag_color = vec4(v_color, u_alpha);
}
)GLSL";
}

const char* UnlitPointVertex()
{
    return R"GLSL(
#version 410 core
layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_color;
uniform mat4 u_mvp;
uniform float u_point_size;
out vec3 v_color;
void main()
{
    v_color = a_color;
    gl_PointSize = u_point_size;
    gl_Position = u_mvp * vec4(a_position, 1.0);
}
)GLSL";
}

const char* UnlitPointFragment()
{
    return UnlitColorFragment();
}

const char* TexturedUnlitVertex()
{
    return R"GLSL(
#version 410 core
layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_color;
layout(location = 2) in vec2 a_texcoord;
uniform mat4 u_mvp;
out vec3 v_color;
out vec2 v_texcoord;
void main()
{
    v_color = a_color;
    v_texcoord = a_texcoord;
    gl_Position = u_mvp * vec4(a_position, 1.0);
}
)GLSL";
}

const char* TexturedUnlitFragment()
{
    return R"GLSL(
#version 410 core
in vec3 v_color;
in vec2 v_texcoord;
uniform sampler2D u_texture;
uniform float u_alpha;
out vec4 frag_color;
void main()
{
    vec4 tex = texture(u_texture, v_texcoord);
    frag_color = vec4(tex.rgb * v_color, tex.a * u_alpha);
}
)GLSL";
}

bool CompileUnlitColor(GlProgram& out, QString* error_log)
{
    return out.Compile(UnlitColorVertex(), UnlitColorFragment(), error_log);
}

bool CompileUnlitPoint(GlProgram& out, QString* error_log)
{
    return out.Compile(UnlitPointVertex(), UnlitPointFragment(), error_log);
}

bool CompileTexturedUnlit(GlProgram& out, QString* error_log)
{
    return out.Compile(TexturedUnlitVertex(), TexturedUnlitFragment(), error_log);
}

} // namespace ViewportShaders
