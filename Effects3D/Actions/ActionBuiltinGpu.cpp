// SPDX-License-Identifier: GPL-2.0-only

#include "ActionBuiltinGpu.h"
#include "ActionScript.h"
#include "SpatialVolumeFieldAssist.h"

#include <QString>

#include <algorithm>
#include <cmath>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace Action
{
namespace builtin_gpu
{
namespace
{

std::mutex g_mu;
std::unique_ptr<SpatialVolumeFieldAssist> g_assist;
std::string g_loaded_id;
bool g_ready = false;

const char* BodyFor(const std::string& id)
{
    // Clean-room procedural fields — not ports of any third-party shadertoy/demo.
    if(id == "plasma")
    {
        return R"GLSL(
void volumeMain(out vec4 out_color, vec3 p01)
{
    float t = u_time;
    float n = 0.5 + 0.5 * sin(p01.x * 12.0 + t * 2.2)
                    * sin(p01.y * 10.0 - t * 1.7)
                    * sin(p01.z * 8.0 + t * 1.3);
    float pulse = mix(u_params[4], u_params[5], n);
    vec3 a = vec3(0.2, 0.05, 0.55);
    vec3 b = vec3(0.95, 0.35, 0.15);
    out_color = vec4(mix(a, b, n) * pulse * u_params[1], pulse);
}
)GLSL";
    }
    if(id == "fire")
    {
        return R"GLSL(
void volumeMain(out vec4 out_color, vec3 p01)
{
    float t = u_time;
    float y = p01.y;
    float n = sin((p01.x + p01.z) * 18.0 + t * 6.0) * 0.5 + 0.5;
    n *= sin(p01.x * 31.0 - t * 4.0) * 0.5 + 0.5;
    float flame = pow(clamp(1.0 - y + n * 0.35, 0.0, 1.0), 1.6);
    flame *= smoothstep(0.0, 0.2, y + 0.05);
    vec3 cool = vec3(0.05, 0.0, 0.0);
    vec3 hot = vec3(1.0, 0.55, 0.08);
    vec3 tip = vec3(1.0, 0.95, 0.55);
    vec3 col = mix(cool, hot, clamp(flame * 1.4, 0.0, 1.0));
    col = mix(col, tip, pow(flame, 3.0));
    float inten = flame * u_params[1];
    out_color = vec4(col * inten, inten);
}
)GLSL";
    }
    if(id == "snow")
    {
        return R"GLSL(
float hash13(vec3 p)
{
    p = fract(p * 0.1031);
    p += dot(p, p.zyx + 33.33);
    return fract((p.x + p.y) * p.z);
}
void volumeMain(out vec4 out_color, vec3 p01)
{
    float t = u_time * 0.35;
    float dens = 0.0;
    for(int i = 0; i < 12; ++i)
    {
        float fi = float(i);
        vec3 cell = floor(p01 * (4.0 + fi * 0.35));
        float h = hash13(cell + fi);
        vec3 center = (cell + 0.5) / (4.0 + fi * 0.35);
        center.y = fract(center.y - t * (0.4 + h * 0.8) - h);
        float d = length(p01 - center);
        dens += smoothstep(0.06, 0.0, d) * (0.35 + 0.65 * h);
    }
    dens = clamp(dens, 0.0, 1.0) * u_params[1];
    out_color = vec4(vec3(0.85, 0.92, 1.0) * dens, dens);
}
)GLSL";
    }
    if(id == "colorwash")
    {
        return R"GLSL(
void volumeMain(out vec4 out_color, vec3 p01)
{
    float t = u_time * 0.25;
    float h = fract(p01.x * 0.7 + p01.z * 0.4 + t);
    vec3 c = 0.5 + 0.5 * cos(6.2831853 * (h + vec3(0.0, 0.33, 0.67)));
    float inten = u_params[1];
    out_color = vec4(c * inten, inten);
}
)GLSL";
    }
    return nullptr;
}

} // namespace

bool Uses(const std::string& effect_id)
{
    return BodyFor(effect_id) != nullptr;
}

void Reset()
{
    std::lock_guard<std::mutex> lock(g_mu);
    g_assist.reset();
    g_loaded_id.clear();
    g_ready = false;
}

bool PrepareForClip(const Clip& clip, int local_ms, std::uint64_t render_sequence)
{
    const std::string id = script::FileId(clip);
    const char* body = BodyFor(id);
    if(!body)
    {
        return false;
    }
    // Prefer toolbox `shader:` bodies when present.
    if(!script::FieldShader(id).empty())
    {
        return false;
    }

    std::lock_guard<std::mutex> lock(g_mu);
    if(!g_assist)
    {
        g_assist = std::make_unique<SpatialVolumeFieldAssist>();
    }
    if(g_loaded_id != id)
    {
        g_assist->setFragmentBody(QString::fromUtf8(body));
        g_loaded_id = id;
    }
    const float progress = ClipProgress(clip, local_ms);
    const float speed = std::max(0.05f, clip.speed);
    float params[8] = {
        progress,
        std::clamp(clip.intensity, 0.0f, 1.0f),
        std::clamp(clip.pulse_length, 0.02f, 1.0f),
        speed,
        std::clamp(clip.min_intensity, 0.0f, 1.0f),
        std::clamp(clip.max_intensity, 0.0f, 1.0f),
        0.0f,
        0.0f
    };
    const float time_sec = (float)local_ms * 0.001f * speed;
    g_ready = g_assist->prepareProgram(render_sequence ? render_sequence : 1, time_sec, params, 6);
    return g_ready;
}

bool IsAvailable()
{
    std::lock_guard<std::mutex> lock(g_mu);
    return g_ready && g_assist && g_assist->isAvailable();
}

bool EvaluateAtPoints(const float* xyz01, int count, float* out_rgb)
{
    if(!xyz01 || !out_rgb || count <= 0)
    {
        return false;
    }
    std::lock_guard<std::mutex> lock(g_mu);
    if(!g_ready || !g_assist || !g_assist->isAvailable())
    {
        return false;
    }
    std::vector<float> clamped((size_t)count * 3u);
    for(int i = 0; i < count; ++i)
    {
        clamped[(size_t)i * 3u + 0u] = std::clamp(xyz01[(size_t)i * 3u + 0u], 0.0f, 1.0f);
        clamped[(size_t)i * 3u + 1u] = std::clamp(xyz01[(size_t)i * 3u + 1u], 0.0f, 1.0f);
        clamped[(size_t)i * 3u + 2u] = std::clamp(xyz01[(size_t)i * 3u + 2u], 0.0f, 1.0f);
    }
    return g_assist->evaluateAtPoints(clamped.data(), count, out_rgb);
}

SampleRgb Sample01(float nx, float ny, float nz)
{
    SampleRgb out;
    float xyz[3] = {nx, ny, nz};
    float rgb[3] = {};
    if(!EvaluateAtPoints(xyz, 1, rgb))
    {
        return out;
    }
    out.r = rgb[0];
    out.g = rgb[1];
    out.b = rgb[2];
    return out;
}

} // namespace builtin_gpu
} // namespace Action
