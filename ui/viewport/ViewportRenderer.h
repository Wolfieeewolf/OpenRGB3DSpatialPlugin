// SPDX-License-Identifier: GPL-2.0-only

#ifndef VIEWPORTRENDERER_H
#define VIEWPORTRENDERER_H

#include "ViewportCamera.h"
#include "ViewportGLFormat.h"
#include "ViewportGpuMesh.h"
#include "ViewportMath.h"
#include "ViewportPrograms.h"

#include <vector>

class QOpenGLContext;
class QOpenGLFunctions;
class QOpenGLWidget;

/** Shader/VBO render boundary for LEDViewport3D (compatibility GL 2.1 context). */
class ViewportRenderer
{
public:
    ViewportRenderer();
    ~ViewportRenderer();

    bool initialize(QOpenGLWidget* widget);
    void shutdown();
    bool gpuPathReady() const { return gpu_ready_; }
    bool coreProfile() const { return core_profile_; }

    void setViewportSize(int width, int height);
    void setCamera(const ViewportCameraState& camera);

    const ViewportFrameMatrices& frameMatrices() const { return frame_; }

    void beginGpuFrame();
    void endGpuFrame();
    /** Render shader scene to an FBO; release() in endGpuSceneFramebuffer() presents to QOpenGLWidget. */
    bool beginGpuSceneFramebuffer();
    void endGpuSceneFramebuffer();
    bool sceneFramebufferReady() const;

    void setFloorGridMesh(const std::vector<float>& positions, const std::vector<float>& colors);
    void drawFloorGridLines();
    void drawFloorGridPerimeter(float max_x, float max_z);

    void drawColoredLines(const std::vector<float>& positions,
                          const std::vector<float>& colors,
                          const ViewportMat4& model,
                          float line_width);
    void drawColoredPoints(const std::vector<float>& positions,
                           const std::vector<float>& colors,
                           const ViewportMat4& model,
                           float point_size);
    void drawColoredTriangles(const std::vector<float>& positions,
                              const std::vector<float>& colors,
                              const ViewportMat4& model,
                              float alpha = 1.0f);
    void drawTexturedQuad(const std::vector<float>& positions_xyz,
                          const std::vector<float>& uvs,
                          unsigned int texture_id,
                          const ViewportMat4& model,
                          float alpha);

    /** Upload RGBA8888 image (same size as viewport) for drawScreenOverlay(). */
    void uploadScreenOverlayFromImage(const class QImage& image);
    /** Full-viewport textured quad in NDC; call during beginGpuFrame(). */
    void drawScreenOverlay(float alpha = 1.0f);

    /** Only valid between beginGpuFrame() and endGpuFrame(). */
    void setGpuDepthState(bool depth_test_enabled, bool depth_mask_write);

    float fovyDegrees() const { return fovy_degrees_; }
    float nearPlane() const { return near_plane_; }
    float farPlane() const { return far_plane_; }

private:
    void rebuildProjection();
    void setMvpUniform(const ViewportMat4& model);
    void drawDynamicMesh(const std::vector<float>& positions,
                         const std::vector<float>& colors,
                         unsigned int primitive,
                         const ViewportMat4& model,
                         float line_width,
                         float point_size,
                         float alpha);

    ViewportFrameMatrices frame_;
    ViewportCameraState camera_;
    int viewport_width_;
    int viewport_height_;
    float fovy_degrees_;
    float near_plane_;
    float far_plane_;

    QOpenGLFunctions* gl_ = nullptr;
    bool gpu_ready_ = false;
    bool core_profile_ = false;
    bool gpu_frame_active_ = false;

    void bindColoredProgram();
    void setMvpUniformTextured(const ViewportMat4& model);
    void ensureSceneFramebuffer();
    void releaseGpuPrograms();

    QOpenGLWidget* widget_ = nullptr;
    class QOpenGLFramebufferObject* scene_fbo_ = nullptr;
    bool rendering_to_scene_fbo_ = false;

    ViewportColoredProgram colored_program_;
    ViewportTexturedProgram textured_program_;
    ViewportGpuMesh floor_grid_mesh_;
    ViewportGpuMesh dynamic_mesh_;
    unsigned int screen_overlay_texture_ = 0;
    int screen_overlay_tex_width_ = 0;
    int screen_overlay_tex_height_ = 0;
};

#endif
