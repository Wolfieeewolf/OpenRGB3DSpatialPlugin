// SPDX-License-Identifier: GPL-2.0-or-later

#include "ActionScriptGpu.h"
#include "FieldFullscreenQuad410.h"
#include "PluginLog.h"
#include "SpatialOffscreenGlPool.h"

#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QVector3D>

#include <algorithm>
#include <mutex>
#include <unordered_map>
#include <vector>

#ifndef GL_RGBA32F
#define GL_RGBA32F 0x8814
#endif

namespace Action
{
namespace script_gpu
{
namespace
{

/* One LED per output texel; rows of kRowWidth LEDs. The attribute texture holds
   kTexelsPerLed RGBA32F texels per LED (20 floats, see script::GpuLedInput). */
constexpr int kTexelsPerLed = 5;
constexpr int kFloatsPerLed = kTexelsPerLed * 4;
constexpr int kRowWidth = 512;
constexpr int kMaxLeds = kRowWidth * 512;

struct CachedProgram
{
    std::shared_ptr<const std::string> source;
    std::unique_ptr<QOpenGLShaderProgram> program;
    bool failed = false;
};

std::mutex g_mu;
std::unordered_map<const std::string*, CachedProgram> g_programs;
std::unique_ptr<QOpenGLFramebufferObject> g_fbo;
int g_fbo_w = 0;
int g_fbo_h = 0;
unsigned int g_attr_tex = 0;
std::vector<float> g_attr_upload;
bool g_gl_ok = false;

bool InitGl()
{
    if(g_gl_ok)
    {
        return true;
    }
    if(!SpatialOffscreenGlPool::hostContextReady() || !SpatialOffscreenGlPool::warmUp(nullptr))
    {
        return false;
    }
    g_gl_ok = true;
    return true;
}

QOpenGLShaderProgram* ProgramFor(const std::shared_ptr<const std::string>& source)
{
    /* Keyed by the source object: a script reload makes a new string, so stale
       entries are dropped once their source is the only owner left. */
    for(auto it = g_programs.begin(); it != g_programs.end();)
    {
        if(it->second.source.use_count() == 1)
        {
            it = g_programs.erase(it);
            continue;
        }
        ++it;
    }
    CachedProgram& entry = g_programs[source.get()];
    if(!entry.source)
    {
        entry.source = source; /* holding it keeps the key address from being reused */
    }
    if(entry.failed)
    {
        return nullptr;
    }
    if(entry.program)
    {
        return entry.program.get();
    }
    auto prog = std::make_unique<QOpenGLShaderProgram>();
    if(!prog->addShaderFromSourceCode(QOpenGLShader::Vertex, FieldFullscreenQuad410::VertexShaderSource())
       || !prog->addShaderFromSourceCode(QOpenGLShader::Fragment, QByteArray::fromStdString(*source))
       || !prog->link())
    {
        LOG_WARNING("[OpenRGB3DSpatialPlugin] Action script GPU compile failed, using CPU: %s",
                    prog->log().toStdString().c_str());
        entry.failed = true;
        return nullptr;
    }
    entry.program = std::move(prog);
    return entry.program.get();
}

bool EnsureFbo(int w, int h)
{
    if(g_fbo && g_fbo_w == w && g_fbo_h == h && g_fbo->isValid())
    {
        return true;
    }
    g_fbo = std::make_unique<QOpenGLFramebufferObject>(w, h);
    g_fbo_w = w;
    g_fbo_h = h;
    if(!g_fbo->isValid())
    {
        g_fbo.reset();
        g_fbo_w = g_fbo_h = 0;
        return false;
    }
    return true;
}

bool UploadAttrs(QOpenGLFunctions* gl, const float* attrs, int count, int w, int h)
{
    const size_t row_texels = (size_t)w * (size_t)kTexelsPerLed;
    g_attr_upload.assign(row_texels * (size_t)h * 4u, 0.0f);
    for(int i = 0; i < count; ++i)
    {
        const int row = i / w;
        const int col = i % w;
        float* dst = g_attr_upload.data() + ((size_t)row * row_texels + (size_t)col * kTexelsPerLed) * 4u;
        std::copy(attrs + (size_t)i * kFloatsPerLed, attrs + (size_t)(i + 1) * kFloatsPerLed, dst);
    }
    if(g_attr_tex == 0)
    {
        gl->glGenTextures(1, &g_attr_tex);
        if(g_attr_tex == 0)
        {
            return false;
        }
    }
    gl->glBindTexture(GL_TEXTURE_2D, g_attr_tex);
    gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl->glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    gl->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, w * kTexelsPerLed, h, 0, GL_RGBA, GL_FLOAT, g_attr_upload.data());
    gl->glBindTexture(GL_TEXTURE_2D, 0);
    return true;
}

} // namespace

bool Available()
{
    std::lock_guard<std::mutex> lock(g_mu);
    return InitGl();
}

bool Run(const std::shared_ptr<const std::string>& fragment_source,
         const Uniforms& u,
         const float* attrs,
         int count,
         unsigned char* rgba_out)
{
    if(!fragment_source || fragment_source->empty() || !attrs || !rgba_out || count <= 0 || count > kMaxLeds)
    {
        return false;
    }
    std::lock_guard<std::mutex> lock(g_mu);
    if(!InitGl())
    {
        return false;
    }
    SpatialOffscreenGlPool::Session session;
    if(!session)
    {
        return false;
    }
    QOpenGLContext* ctx = SpatialOffscreenGlPool::sharedContext();
    QOpenGLFunctions* gl = ctx ? ctx->functions() : nullptr;
    if(!gl)
    {
        return false;
    }
    QOpenGLShaderProgram* prog = ProgramFor(fragment_source);
    if(!prog)
    {
        return false;
    }

    const int w = std::min(count, kRowWidth);
    const int h = (count + w - 1) / w;
    if(!EnsureFbo(w, h) || !UploadAttrs(gl, attrs, count, w, h))
    {
        return false;
    }

    prog->bind();
    g_fbo->bind();
    gl->glViewport(0, 0, w, h);
    prog->setUniformValue("u_width", w);
    prog->setUniformValue("u_count", count);
    prog->setUniformValueArray("u_s", u.scalars, 17, 1);
    prog->setUniformValue("u_axis_vec", QVector3D(u.axis_vec[0], u.axis_vec[1], u.axis_vec[2]));
    prog->setUniformValue("u_color", QVector3D(u.color[0], u.color[1], u.color[2]));
    prog->setUniformValue("u_mod", u.intensity_mod);
    const int stops = std::clamp(u.stop_count, 1, 32);
    prog->setUniformValue("u_nstops", stops);
    prog->setUniformValueArray("u_stop_pos", u.stop_pos, 32, 1);
    prog->setUniformValueArray("u_stop_col", u.stop_col, 32, 3);
    gl->glActiveTexture(GL_TEXTURE0);
    gl->glBindTexture(GL_TEXTURE_2D, g_attr_tex);
    prog->setUniformValue("u_attr", 0);

    const bool drawn = FieldFullscreenQuad410::Draw(ctx);
    if(drawn)
    {
        gl->glPixelStorei(GL_PACK_ALIGNMENT, 1);
        std::vector<unsigned char> readback((size_t)w * (size_t)h * 4u);
        gl->glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, readback.data());
        std::copy(readback.begin(), readback.begin() + (ptrdiff_t)count * 4, rgba_out);
    }
    gl->glBindTexture(GL_TEXTURE_2D, 0);
    g_fbo->release();
    prog->release();
    return drawn;
}

} // namespace script_gpu
} // namespace Action
