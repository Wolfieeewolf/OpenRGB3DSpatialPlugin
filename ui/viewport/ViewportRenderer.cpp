// SPDX-License-Identifier: GPL-2.0-only

#include "ViewportRenderer.h"
#include "ViewportLegacyGL.h"

#include <QImage>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFunctions>
#include <QOpenGLWidget>
#include <QMatrix4x4>
#include <QSize>

#include <cstring>

ViewportRenderer::ViewportRenderer()
    : viewport_width_(1)
    , viewport_height_(1)
    , fovy_degrees_(ViewportGLFormat::kDefaultFovyDegrees)
    , near_plane_(ViewportGLFormat::kDefaultNearPlane)
    , far_plane_(ViewportGLFormat::kDefaultFarPlane)
{
    rebuildProjection();
    frame_.view = ViewportCamera::BuildViewMatrix(camera_);
}

ViewportRenderer::~ViewportRenderer()
{
    /* GL resources are released from LEDViewport3D while the context is current. */
}

bool ViewportRenderer::initialize(QOpenGLWidget* widget)
{
    shutdown();
    widget_ = widget;
    if(!widget)
    {
        return false;
    }

    QOpenGLContext* context = widget->context();
    if(!context || !context->isValid())
    {
        return false;
    }

    gl_ = context->functions();
    if(!gl_)
    {
        return false;
    }

    core_profile_ = (context->format().profile() == QSurfaceFormat::CoreProfile
                     && context->format().majorVersion() >= 3);

    gpu_ready_ = false;
    if(!core_profile_)
    {
        gpu_ready_ = colored_program_.initialize(context) && textured_program_.initialize(context);
    }

    return true;
}

void ViewportRenderer::shutdown()
{
    delete scene_fbo_;
    scene_fbo_ = nullptr;
    rendering_to_scene_fbo_ = false;
    widget_ = nullptr;

    if(gl_)
    {
        floor_grid_mesh_.destroy(gl_);
        dynamic_mesh_.destroy(gl_);
        if(screen_overlay_texture_ != 0)
        {
            gl_->glDeleteTextures(1, &screen_overlay_texture_);
            screen_overlay_texture_ = 0;
        }
    }
    colored_program_.release();
    textured_program_.release();
    gpu_ready_ = false;
    gpu_frame_active_ = false;
    gl_ = nullptr;
}

void ViewportRenderer::ensureSceneFramebuffer()
{
    if(!gl_ || !widget_)
    {
        return;
    }

    const QSize target(viewport_width_, viewport_height_);
    if(scene_fbo_ && scene_fbo_->size() == target)
    {
        return;
    }

    delete scene_fbo_;
    scene_fbo_ = nullptr;

    QOpenGLFramebufferObjectFormat fmt;
    fmt.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
    scene_fbo_ = new QOpenGLFramebufferObject(target, fmt);
}

bool ViewportRenderer::sceneFramebufferReady() const
{
    return scene_fbo_ && scene_fbo_->isValid();
}

void ViewportRenderer::releaseGpuPrograms()
{
    colored_program_.release();
    textured_program_.release();

    if(!gl_)
    {
        return;
    }

    gl_->glUseProgram(0);
    gl_->glBindBuffer(GL_ARRAY_BUFFER, 0);
    gl_->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    gl_->glActiveTexture(GL_TEXTURE0);
    gl_->glBindTexture(GL_TEXTURE_2D, 0);
    if(core_profile_)
    {
        glDisable(GL_PROGRAM_POINT_SIZE);
        gl_->glDisableVertexAttribArray(0);
        gl_->glDisableVertexAttribArray(1);
        gl_->glDisableVertexAttribArray(2);
    }
    else
    {
        glDisableClientState(GL_VERTEX_ARRAY);
        glDisableClientState(GL_COLOR_ARRAY);
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    }
}

void ViewportRenderer::setViewportSize(int width, int height)
{
    viewport_width_ = (width > 0) ? width : 1;
    viewport_height_ = (height > 0) ? height : 1;
    rebuildProjection();
}

void ViewportRenderer::setCamera(const ViewportCameraState& camera)
{
    camera_ = camera;
    frame_.view = ViewportCamera::BuildViewMatrix(camera_);
}

void ViewportRenderer::rebuildProjection()
{
    frame_.projection = ViewportCamera::BuildProjectionMatrix(
        viewport_width_, viewport_height_, fovy_degrees_, near_plane_, far_plane_);
    frame_.viewport[0] = 0;
    frame_.viewport[1] = 0;
    frame_.viewport[2] = viewport_width_;
    frame_.viewport[3] = viewport_height_;
}

void ViewportRenderer::bindColoredProgram()
{
    if(gpu_frame_active_)
    {
        textured_program_.release();
        colored_program_.bind();
        colored_program_.setAlpha(1.0f);
    }
}

bool ViewportRenderer::beginGpuSceneFramebuffer()
{
    if(!gl_ || !gpu_ready_ || !widget_)
    {
        return false;
    }

    ensureSceneFramebuffer();
    if(!sceneFramebufferReady())
    {
        return false;
    }

    if(!scene_fbo_->bind())
    {
        return false;
    }

    rendering_to_scene_fbo_ = true;

    gl_->glViewport(0, 0, viewport_width_, viewport_height_);
    gl_->glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    gl_->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    gl_->glEnable(GL_DEPTH_TEST);
    gl_->glDepthFunc(GL_LESS);
    gl_->glDepthMask(GL_TRUE);
    gl_->glEnable(GL_BLEND);
    gl_->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    gl_->glDisable(GL_LIGHTING);
    gl_->glDisable(GL_TEXTURE_2D);

    if(core_profile_)
    {
        glEnable(GL_PROGRAM_POINT_SIZE);
    }

    if(!colored_program_.bind())
    {
        scene_fbo_->release();
        rendering_to_scene_fbo_ = false;
        return false;
    }

    colored_program_.setAlpha(1.0f);
    gpu_frame_active_ = true;
    return true;
}

void ViewportRenderer::endGpuSceneFramebuffer()
{
    if(!gpu_frame_active_)
    {
        return;
    }

    releaseGpuPrograms();

    if(scene_fbo_ && rendering_to_scene_fbo_)
    {
        scene_fbo_->release();
        rendering_to_scene_fbo_ = false;
    }

    gpu_frame_active_ = false;
}

void ViewportRenderer::beginGpuFrame()
{
    if(!gl_ || !gpu_ready_)
    {
        return;
    }

    gl_->glViewport(0, 0, viewport_width_, viewport_height_);
    gl_->glEnable(GL_DEPTH_TEST);
    gl_->glDepthFunc(GL_LESS);
    gl_->glDepthMask(GL_TRUE);
    gl_->glEnable(GL_BLEND);
    gl_->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    gl_->glDisable(GL_LIGHTING);
    gl_->glDisable(GL_TEXTURE_2D);

    if(core_profile_)
    {
        glEnable(GL_PROGRAM_POINT_SIZE);
    }

    if(!colored_program_.bind())
    {
        return;
    }

    colored_program_.setAlpha(1.0f);
    gpu_frame_active_ = true;
}

void ViewportRenderer::endGpuFrame()
{
    if(!gpu_frame_active_)
    {
        return;
    }

    releaseGpuPrograms();
    gpu_frame_active_ = false;
}

void ViewportRenderer::setMvpUniform(const ViewportMat4& model)
{
    const ViewportMat4 mvp = ViewportMath::ModelViewProjection(frame_.projection, frame_.view, model);
    colored_program_.setMvp(mvp.m);
}

void ViewportRenderer::setMvpUniformTextured(const ViewportMat4& model)
{
    const ViewportMat4 mvp = ViewportMath::ModelViewProjection(frame_.projection, frame_.view, model);
    textured_program_.setMvp(mvp.m);
}

void ViewportRenderer::setFloorGridMesh(const std::vector<float>& positions, const std::vector<float>& colors)
{
    if(!gl_ || !gpu_ready_ || !gpu_frame_active_)
    {
        return;
    }
    floor_grid_mesh_.upload(gl_, positions, colors);
}

void ViewportRenderer::drawFloorGridLines()
{
    if(!gl_ || !gpu_ready_ || !gpu_frame_active_ || !floor_grid_mesh_.isValid())
    {
        return;
    }

    bindColoredProgram();
    setMvpUniform(ViewportMath::Identity());
    glLineWidth(1.0f);
    floor_grid_mesh_.draw(gl_, GL_LINES, core_profile_);
}

void ViewportRenderer::drawFloorGridPerimeter(float max_x, float max_z)
{
    const std::vector<float> positions = {
        0.0f, 0.0f, 0.0f,
        max_x, 0.0f, 0.0f,
        max_x, 0.0f, max_z,
        0.0f, 0.0f, max_z,
        0.0f, 0.0f, 0.0f,
    };
    const std::vector<float> colors = {
        0.6f, 0.8f, 0.6f,
        0.6f, 0.8f, 0.6f,
        0.6f, 0.8f, 0.6f,
        0.6f, 0.8f, 0.6f,
        0.6f, 0.8f, 0.6f,
    };
    drawColoredLines(positions, colors, ViewportMath::Identity(), 2.0f);
}

void ViewportRenderer::drawDynamicMesh(const std::vector<float>& positions,
                                       const std::vector<float>& colors,
                                       unsigned int primitive,
                                       const ViewportMat4& model,
                                       float line_width,
                                       float point_size,
                                       float alpha)
{
    if(!gl_ || !gpu_ready_ || !gpu_frame_active_)
    {
        return;
    }

    bindColoredProgram();
    dynamic_mesh_.upload(gl_, positions, colors);
    if(!dynamic_mesh_.isValid())
    {
        return;
    }

    colored_program_.setAlpha(alpha);
    setMvpUniform(model);
    if(primitive == GL_POINTS)
    {
        colored_program_.setPointSize(point_size);
        if(!core_profile_)
        {
            glPointSize(point_size);
        }
        dynamic_mesh_.draw(gl_, primitive, core_profile_);
    }
    else
    {
        glLineWidth(line_width);
        dynamic_mesh_.draw(gl_, primitive, core_profile_);
    }
    colored_program_.setAlpha(1.0f);
}

void ViewportRenderer::drawColoredLines(const std::vector<float>& positions,
                                        const std::vector<float>& colors,
                                        const ViewportMat4& model,
                                        float line_width)
{
    drawDynamicMesh(positions, colors, GL_LINES, model, line_width, 1.0f, 1.0f);
}

void ViewportRenderer::drawColoredPoints(const std::vector<float>& positions,
                                         const std::vector<float>& colors,
                                         const ViewportMat4& model,
                                         float point_size)
{
    drawDynamicMesh(positions, colors, GL_POINTS, model, 1.0f, point_size, 1.0f);
}

void ViewportRenderer::drawColoredTriangles(const std::vector<float>& positions,
                                            const std::vector<float>& colors,
                                            const ViewportMat4& model,
                                            float alpha)
{
    drawDynamicMesh(positions, colors, GL_TRIANGLES, model, 1.0f, 1.0f, alpha);
}

void ViewportRenderer::drawTexturedQuad(const std::vector<float>& positions_xyz,
                                        const std::vector<float>& uvs,
                                        unsigned int texture_id,
                                        const ViewportMat4& model,
                                        float alpha)
{
    if(!gl_ || !gpu_ready_ || !gpu_frame_active_ || !textured_program_.isReady())
    {
        return;
    }
    if(positions_xyz.size() < 12 || uvs.size() < 8 || texture_id == 0)
    {
        return;
    }

    colored_program_.release();
    if(!textured_program_.bind())
    {
        bindColoredProgram();
        return;
    }

    textured_program_.setAlpha(alpha);
    setMvpUniformTextured(model);

    gl_->glActiveTexture(GL_TEXTURE0);
    gl_->glBindTexture(GL_TEXTURE_2D, texture_id);
    textured_program_.setTextureUnit(0);

    if(core_profile_)
    {
        gl_->glEnableVertexAttribArray(0);
        gl_->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, positions_xyz.data());
        gl_->glEnableVertexAttribArray(2);
        gl_->glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, uvs.data());
        gl_->glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        gl_->glDisableVertexAttribArray(2);
        gl_->glDisableVertexAttribArray(0);
    }
    else
    {
        glVertexPointer(3, GL_FLOAT, 0, positions_xyz.data());
        glEnableClientState(GL_VERTEX_ARRAY);
        glTexCoordPointer(2, GL_FLOAT, 0, uvs.data());
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        gl_->glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
        glDisableClientState(GL_VERTEX_ARRAY);
    }

    gl_->glBindTexture(GL_TEXTURE_2D, 0);
    textured_program_.release();
    bindColoredProgram();
}

void ViewportRenderer::uploadScreenOverlayFromImage(const QImage& image)
{
    if(!gl_ || !gpu_ready_ || image.isNull() || image.width() < 1 || image.height() < 1)
    {
        return;
    }

    QImage rgba = image.convertToFormat(QImage::Format_RGBA8888);
    rgba = rgba.mirrored(false, true);

    const int w = rgba.width();
    const int h = rgba.height();
    if(!rgba.isDetached())
    {
        rgba = rgba.copy();
    }

    if(screen_overlay_texture_ == 0)
    {
        gl_->glGenTextures(1, &screen_overlay_texture_);
    }

    screen_overlay_tex_width_ = w;
    screen_overlay_tex_height_ = h;

    gl_->glBindTexture(GL_TEXTURE_2D, screen_overlay_texture_);
    gl_->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl_->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl_->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl_->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl_->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.constBits());
    gl_->glBindTexture(GL_TEXTURE_2D, 0);
}

void ViewportRenderer::drawScreenOverlay(float alpha)
{
    if(!gl_ || !gpu_ready_ || !gpu_frame_active_ || screen_overlay_texture_ == 0 || !textured_program_.isReady())
    {
        return;
    }

    setGpuDepthState(false, false);
    colored_program_.release();
    if(!textured_program_.bind())
    {
        bindColoredProgram();
        return;
    }

    textured_program_.setAlpha(alpha);
    textured_program_.setMvp(ViewportMath::Identity().m);

    const float positions[12] = {
        -1.0f, -1.0f, 0.0f,
        1.0f, -1.0f, 0.0f,
        1.0f, 1.0f, 0.0f,
        -1.0f, 1.0f, 0.0f,
    };
    const float uvs[8] = {0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f};

    gl_->glActiveTexture(GL_TEXTURE0);
    gl_->glBindTexture(GL_TEXTURE_2D, screen_overlay_texture_);
    textured_program_.setTextureUnit(0);

    if(core_profile_)
    {
        gl_->glEnableVertexAttribArray(0);
        gl_->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, positions);
        gl_->glEnableVertexAttribArray(2);
        gl_->glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, uvs);
        gl_->glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        gl_->glDisableVertexAttribArray(2);
        gl_->glDisableVertexAttribArray(0);
    }
    else
    {
        glVertexPointer(3, GL_FLOAT, 0, positions);
        glEnableClientState(GL_VERTEX_ARRAY);
        glTexCoordPointer(2, GL_FLOAT, 0, uvs);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        gl_->glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
        glDisableClientState(GL_VERTEX_ARRAY);
    }

    gl_->glBindTexture(GL_TEXTURE_2D, 0);
    textured_program_.release();
    bindColoredProgram();
}

void ViewportRenderer::setGpuDepthState(bool depth_test_enabled, bool depth_mask_write)
{
    if(!gl_ || !gpu_frame_active_)
    {
        return;
    }

    if(depth_test_enabled)
    {
        gl_->glEnable(GL_DEPTH_TEST);
    }
    else
    {
        gl_->glDisable(GL_DEPTH_TEST);
    }
    gl_->glDepthMask(depth_mask_write ? GL_TRUE : GL_FALSE);
}
