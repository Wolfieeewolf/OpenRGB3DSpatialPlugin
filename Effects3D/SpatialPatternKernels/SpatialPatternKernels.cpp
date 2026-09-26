// SPDX-License-Identifier: GPL-2.0-only

#include "SpatialPatternKernels.h"
#include "OpenRGB3DSpatialPlugin.h"
#include "PluginSettingsPaths.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

#include <algorithm>
#include <cmath>
#include <vector>

namespace
{

struct KernelFile
{
    int index = 0;
    std::string name;
    std::string palette;
    QString body;
};

std::vector<KernelFile> g_kernels;
bool g_loaded = false;

void LoadKernels()
{
    if(g_loaded || !OpenRGB3DSpatialPlugin::APIPointer)
    {
        return;
    }
    g_loaded = true;
    PluginSettingsPaths::EnsurePluginDataLayout(OpenRGB3DSpatialPlugin::APIPointer);
    const QString root = QString::fromStdString(
        PluginSettingsPaths::PatternsDir(OpenRGB3DSpatialPlugin::APIPointer).string());
    QDir dir(root);
    if(!dir.exists())
    {
        return;
    }
    const QFileInfoList files = dir.entryInfoList(QStringList() << QStringLiteral("*.kernel"), QDir::Files, QDir::Name);
    for(const QFileInfo& fi : files)
    {
        QFile file(fi.absoluteFilePath());
        if(!file.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            continue;
        }
        KernelFile kernel;
        kernel.index = (int)g_kernels.size();
        kernel.name = fi.completeBaseName().toStdString();
        QString body;
        QTextStream stream(&file);
        bool header = true;
        while(!stream.atEnd())
        {
            const QString line = stream.readLine();
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
                    const QString val = trimmed.mid(colon + 1).trimmed();
                    if(key == QStringLiteral("name"))
                    {
                        kernel.name = val.toStdString();
                        continue;
                    }
                    if(key == QStringLiteral("index"))
                    {
                        kernel.index = val.toInt();
                        continue;
                    }
                    if(key == QStringLiteral("palette"))
                    {
                        kernel.palette = val.toStdString();
                        continue;
                    }
                }
                header = false;
            }
            body += line;
            body += QLatin1Char('\n');
        }
        kernel.body = body;
        g_kernels.push_back(std::move(kernel));
    }
    std::sort(g_kernels.begin(), g_kernels.end(), [](const KernelFile& a, const KernelFile& b) {
        if(a.index != b.index)
        {
            return a.index < b.index;
        }
        return a.name < b.name;
    });
}

const KernelFile* KernelAt(int id)
{
    LoadKernels();
    if(g_kernels.empty())
    {
        return nullptr;
    }
    const int n = (int)g_kernels.size();
    if(id < 0)
    {
        id = 0;
    }
    if(id >= n)
    {
        id = n - 1;
    }
    return &g_kernels[(size_t)id];
}

} // namespace

void SpatialPatternKernelsReload()
{
    g_kernels.clear();
    g_loaded = false;
    LoadKernels();
}

int SpatialPatternKernelCount()
{
    LoadKernels();
    return g_kernels.empty() ? 1 : (int)g_kernels.size();
}

int SpatialPatternKernelClamp(int id)
{
    const int n = SpatialPatternKernelCount();
    if(id < 0)
    {
        return 0;
    }
    if(id >= n)
    {
        return n - 1;
    }
    return id;
}

const char* SpatialPatternKernelDisplayName(int kernel_id)
{
    const KernelFile* kernel = KernelAt(kernel_id);
    if(!kernel)
    {
        return "Sine";
    }
    return kernel->name.c_str();
}

const char* SpatialPatternKernelPaletteName(int kernel_id)
{
    const KernelFile* kernel = KernelAt(kernel_id);
    if(!kernel)
    {
        return "";
    }
    return kernel->palette.c_str();
}

float EvalSpatialPatternKernel(int, float s01, float phase01, float rep, float time_sec)
{
    const float r = 1.0f + (std::max(1.0f, rep) - 1.0f) * 0.72f;
    const float ph = (phase01 * 0.35f + time_sec * 0.08f);
    const float wrapped = ph - std::floor(ph);
    return std::sin(6.2831853f * (s01 * r + wrapped));
}

QString SpatialPatternKernelShader()
{
    LoadKernels();
    QString body;
    body += QStringLiteral(
        "float fractf(float x) { return x - floor(x); }\n"
        "float hash11(float x) { return fractf(sin(x * 12.9898) * 43758.547); }\n"
        "float smstep(float e0, float e1, float x)\n"
        "{\n"
        "    float t = clamp((x - e0) / max(e1 - e0, 1e-5), 0.0, 1.0);\n"
        "    return t * t * (3.0 - 2.0 * t);\n"
        "}\n"
        "float evalStripKernelSigned(int kid, float s01, float phase01, float repeats, float time_sec)\n"
        "{\n"
        "    float rep = max(repeats, 1.0);\n"
        "    float tsec = time_sec * 0.35;\n"
        "    float ph = fractf(phase01 * 0.35 + time_sec * 0.08);\n"
        "    float r = 1.0 + (rep - 1.0) * 0.72;\n"
        "    float u_phase = fractf(s01 * r + ph + 1000.0);\n"
        "    float TWO_PI = 6.2831853;\n"
        "    float k = sin(TWO_PI * (s01 * r + ph));\n");
    for(size_t i = 0; i < g_kernels.size(); ++i)
    {
        body += (i == 0)
            ? QStringLiteral("    if(kid == 0) {\n")
            : QStringLiteral("    else if(kid == %1) {\n").arg((int)i);
        body += g_kernels[i].body;
        body += QStringLiteral("    }\n");
    }
    body += QStringLiteral(
        "    return clamp(k, -1.0, 1.0);\n"
        "}\n"
        "void stripMain(out vec4 out_color, in float s01)\n"
        "{\n"
        "    int kid = int(u_params[0] + 0.5);\n"
        "    float phase01 = u_params[1];\n"
        "    float repeats = max(u_params[2], 1.0);\n"
        "    float time_sec = u_params[3];\n"
        "    float k = evalStripKernelSigned(kid, s01, phase01, repeats, time_sec);\n"
        "    float enc = clamp((k + 1.0) * 0.5, 0.0, 1.0);\n"
        "    out_color = vec4(enc, enc, enc, 1.0);\n"
        "}\n");
    return body;
}
