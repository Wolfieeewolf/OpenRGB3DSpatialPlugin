// SPDX-License-Identifier: GPL-2.0-only

#include "ActionVolumeField.h"
#include "ActionScript.h"
#include "SpatialShaderCatalog.h"
#include "SpatialVolumeFieldAssist.h"

#include <QFile>
#include <QString>
#include <QTextStream>

#include <algorithm>
#include <cmath>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace Action
{
namespace volume_field
{
namespace
{

std::unique_ptr<SpatialVolumeFieldAssist> g_assist;
filesystem::path g_toolbox_dir;
std::string g_loaded_shader_id;
std::mutex g_mu;
float g_ox = 0.5f;
float g_oy = 0.5f;
float g_oz = 0.5f;
float g_size = 1.0f;

void RemapSample01(float* nx, float* ny, float* nz)
{
    const float s = std::max(0.05f, g_size);
    *nx = 0.5f + (*nx - g_ox) / s;
    *ny = 0.5f + (*ny - g_oy) / s;
    *nz = 0.5f + (*nz - g_oz) / s;
}

SpatialVolumeFieldAssist& Assist()
{
    if(!g_assist)
    {
        g_assist = std::make_unique<SpatialVolumeFieldAssist>();
    }
    return *g_assist;
}

bool IsFsHeaderKey(const QString& key)
{
    static const char* kKeys[] = {
        "name", "description", "section", "pattern", "index", "palette", "class", "category",
        "finish", "param", "pattern_key", "colors", "supports_strip_colormap", "supports_height_bands",
        "slider", "resolution", "rainbow", "needs_frequency", "needs_arms", "show_axis",
        "user_colors", "pattern_label", "combo", "option", "pattern_index", "pattern_source",
        "sample", "flow", "global", "media", "drive", "audio_preset", "audio_media", "shader",
        "media_layout", "instances", "duration_ms", "loop", "swatch"
    };
    for(const char* k : kKeys)
    {
        if(key == QLatin1String(k))
        {
            return true;
        }
    }
    return false;
}

QString StripFsHeader(const QString& raw)
{
    QString body;
    const QStringList lines = raw.split(QLatin1Char('\n'));
    bool header = true;
    for(const QString& line : lines)
    {
        const QString trimmed = line.trimmed();
        if(header)
        {
            if(trimmed.isEmpty() || trimmed.startsWith(QLatin1Char('#')))
            {
                continue;
            }
            const int colon = trimmed.indexOf(QLatin1Char(':'));
            if(colon > 0)
            {
                const QString key = trimmed.left(colon).trimmed();
                if(IsFsHeaderKey(key))
                {
                    continue;
                }
            }
            header = false;
        }
        body += line;
        body += QLatin1Char('\n');
    }
    return body;
}

QString LoadFsFromToolbox(const std::string& shader_id)
{
    if(g_toolbox_dir.empty() || shader_id.empty())
    {
        return QString();
    }
    const std::string file_name = shader_id + ".fs";
    std::error_code ec;
    if(!filesystem::is_directory(g_toolbox_dir, ec))
    {
        return QString();
    }
    for(const auto& entry : filesystem::recursive_directory_iterator(g_toolbox_dir, ec))
    {
        if(ec || !entry.is_regular_file())
        {
            continue;
        }
        if(entry.path().filename().string() != file_name)
        {
            continue;
        }
        QFile file(QString::fromStdString(entry.path().string()));
        if(!file.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            return QString();
        }
        return StripFsHeader(QString::fromUtf8(file.readAll()));
    }
    return QString();
}

QString LoadShaderBody(const std::string& shader_id)
{
    QString body = LoadFsFromToolbox(shader_id);
    if(body.trimmed().isEmpty())
    {
        body = SpatialShaderCatalog::LoadEffectShader(QString::fromStdString(shader_id));
    }
    return body;
}

} // namespace

void SetToolboxDirectory(const filesystem::path& dir)
{
    std::lock_guard<std::mutex> lock(g_mu);
    g_toolbox_dir = dir;
}

void Reset()
{
    std::lock_guard<std::mutex> lock(g_mu);
    g_assist.reset();
    g_loaded_shader_id.clear();
    g_ox = g_oy = g_oz = 0.5f;
    g_size = 1.0f;
}

void SetSampleOrigin01(float ox, float oy, float oz, float size)
{
    std::lock_guard<std::mutex> lock(g_mu);
    g_ox = std::clamp(ox, 0.0f, 1.0f);
    g_oy = std::clamp(oy, 0.0f, 1.0f);
    g_oz = std::clamp(oz, 0.0f, 1.0f);
    g_size = std::clamp(size, 0.05f, 2.0f);
}

bool PrepareForClip(const Clip& clip, int local_ms, std::uint64_t render_sequence)
{
    const std::string effect_id = script::FileId(clip);
    const std::string shader_id = script::FieldShader(effect_id);
    if(shader_id.empty())
    {
        return false;
    }

    std::lock_guard<std::mutex> lock(g_mu);
    if(shader_id != g_loaded_shader_id)
    {
        const QString body = LoadShaderBody(shader_id);
        if(body.trimmed().isEmpty())
        {
            g_loaded_shader_id.clear();
            return false;
        }
        Assist().setFragmentBody(body);
        g_loaded_shader_id = shader_id;
    }

    // Span progress: one-shots must not mid-clip wrap when speed > 1 (ClipProgress does).
    const float progress = ClipSpanProgress(clip, local_ms);
    const float intensity = std::clamp(clip.intensity, 0.0f, 1.0f);
    const float size = std::clamp(clip.pulse_length, 0.02f, 1.0f);
    const float speed = std::max(0.01f, clip.speed);
    float params[8] = {
        progress,
        intensity,
        size,
        speed,
        std::clamp(clip.min_intensity, 0.0f, 1.0f),
        std::clamp(clip.max_intensity, 0.0f, 1.0f),
        0.0f,
        0.0f
    };
    // Continuous elapsed so forever/while_active LocalMs wraps do not jump u_time.
    const float time_sec = (float)GpuAnimMsOr(local_ms) * 0.001f * speed;
    return Assist().prepareProgram(render_sequence, time_sec, params, 6);
}

bool IsAvailable()
{
    std::lock_guard<std::mutex> lock(g_mu);
    return g_assist && g_assist->isAvailable();
}

bool EvaluateAtPoints(const float* xyz01, int count, float* out_rgb)
{
    if(!xyz01 || !out_rgb || count <= 0)
    {
        return false;
    }
    std::lock_guard<std::mutex> lock(g_mu);
    if(!g_assist || !g_assist->isAvailable())
    {
        return false;
    }
    std::vector<float> remapped((size_t)count * 3u);
    for(int i = 0; i < count; ++i)
    {
        float x = xyz01[(size_t)i * 3u + 0u];
        float y = xyz01[(size_t)i * 3u + 1u];
        float z = xyz01[(size_t)i * 3u + 2u];
        RemapSample01(&x, &y, &z);
        remapped[(size_t)i * 3u + 0u] = std::clamp(x, 0.0f, 1.0f);
        remapped[(size_t)i * 3u + 1u] = std::clamp(y, 0.0f, 1.0f);
        remapped[(size_t)i * 3u + 2u] = std::clamp(z, 0.0f, 1.0f);
    }
    return g_assist->evaluateAtPoints(remapped.data(), count, out_rgb);
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

float SampleA01(float nx, float ny, float nz)
{
    const SampleRgb s = Sample01(nx, ny, nz);
    return (s.r + s.g + s.b) > 1e-4f ? 1.0f : 0.0f;
}

} // namespace volume_field
} // namespace Action
