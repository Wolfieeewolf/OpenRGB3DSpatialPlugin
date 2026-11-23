// SPDX-License-Identifier: GPL-2.0-or-later

#include <cmath>
#include <algorithm>
#include <cstdio>
#include <map>
#include <memory>
#include <cstring>

#include "LEDViewport3D.h"
#include "viewport/ViewportMath.h"
#include "viewport/MeshBatch.h"
#include "ScreenCaptureManager.h"

#include <QOpenGLContext>

namespace
{
constexpr float kRoomGridOverlayCellEps = 1e-4f;

void PushPC(std::vector<float>& out, float x, float y, float z, float r, float g, float b)
{
    out.push_back(x); out.push_back(y); out.push_back(z);
    out.push_back(r); out.push_back(g); out.push_back(b);
}

void PushTriPC(std::vector<float>& out,
               float x0, float y0, float z0,
               float x1, float y1, float z1,
               float x2, float y2, float z2,
               float r, float g, float b)
{
    PushPC(out, x0, y0, z0, r, g, b);
    PushPC(out, x1, y1, z1, r, g, b);
    PushPC(out, x2, y2, z2, r, g, b);
}

void PushQuadPC(std::vector<float>& out,
                const Vector3D& a, const Vector3D& b, const Vector3D& c, const Vector3D& d,
                float cr, float cg, float cb)
{
    PushTriPC(out, a.x, a.y, a.z, b.x, b.y, b.z, c.x, c.y, c.z, cr, cg, cb);
    PushTriPC(out, a.x, a.y, a.z, c.x, c.y, c.z, d.x, d.y, d.z, cr, cg, cb);
}

void PushTexturedQuad(std::vector<float>& out, const Vector3D corners[4])
{
    // PosColorUv: white tint.
    const float uv[4][2] = {{0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f}};
    auto push_vert = [&](int i) {
        out.push_back(corners[i].x);
        out.push_back(corners[i].y);
        out.push_back(corners[i].z);
        out.push_back(1.0f);
        out.push_back(1.0f);
        out.push_back(1.0f);
        out.push_back(uv[i][0]);
        out.push_back(uv[i][1]);
    };
    push_vert(0); push_vert(1); push_vert(2);
    push_vert(0); push_vert(2); push_vert(3);
}

void AppendCalibrationQuads(std::vector<float>& out, const Vector3D world_corners[4])
{
    /* corners: 0=BL, 1=BR, 2=TR, 3=TL (local +Y = screen top).
       Match ScreenMirrorFillCalibrationPatternBuffer + SampleNearestRgba
       (spatial v=1 → buffer top): TL=R, TR=G, BR=B, BL=Y. */
    Vector3D center;
    center.x = (world_corners[0].x + world_corners[2].x) * 0.5f;
    center.y = (world_corners[0].y + world_corners[2].y) * 0.5f;
    center.z = (world_corners[0].z + world_corners[2].z) * 0.5f;

    Vector3D mid_bottom, mid_top, mid_left, mid_right;
    mid_bottom.x = (world_corners[0].x + world_corners[1].x) * 0.5f;
    mid_bottom.y = (world_corners[0].y + world_corners[1].y) * 0.5f;
    mid_bottom.z = (world_corners[0].z + world_corners[1].z) * 0.5f;
    mid_top.x = (world_corners[2].x + world_corners[3].x) * 0.5f;
    mid_top.y = (world_corners[2].y + world_corners[3].y) * 0.5f;
    mid_top.z = (world_corners[2].z + world_corners[3].z) * 0.5f;
    mid_left.x = (world_corners[0].x + world_corners[3].x) * 0.5f;
    mid_left.y = (world_corners[0].y + world_corners[3].y) * 0.5f;
    mid_left.z = (world_corners[0].z + world_corners[3].z) * 0.5f;
    mid_right.x = (world_corners[1].x + world_corners[2].x) * 0.5f;
    mid_right.y = (world_corners[1].y + world_corners[2].y) * 0.5f;
    mid_right.z = (world_corners[1].z + world_corners[2].z) * 0.5f;

    PushQuadPC(out, world_corners[0], mid_bottom, center, mid_left, 1.0f, 1.0f, 0.0f); /* BL yellow */
    PushQuadPC(out, mid_bottom, world_corners[1], mid_right, center, 0.0f, 0.0f, 1.0f); /* BR blue */
    PushQuadPC(out, center, mid_right, world_corners[2], mid_top, 0.0f, 1.0f, 0.0f);    /* TR green */
    PushQuadPC(out, mid_left, center, mid_top, world_corners[3], 1.0f, 0.0f, 0.0f);    /* TL red */
}

int RoomGridOverlayCellCountAlong(float min_v, float max_v)
{
    return std::max(1, (int)std::floor((max_v - min_v) + 1.0f + kRoomGridOverlayCellEps));
}

int RoomGridOverlaySampleCountAlong(int cell_count, int step)
{
    const int s = std::max(1, step);
    if(cell_count <= 1)
    {
        return 1;
    }
    const int last_cell = cell_count - 1;
    int count = 0;
    for(int c = 0; c <= last_cell; c += s)
    {
        count++;
    }
    const int last_step_cell = (count > 0) ? std::min(last_cell, (count - 1) * s) : 0;
    if(last_step_cell != last_cell)
    {
        count++;
    }
    return std::max(1, count);
}

int RoomGridOverlaySampleCellIndex(int sample_index, int sample_count, int cell_count, int step)
{
    if(cell_count <= 1 || sample_count <= 1)
    {
        return 0;
    }
    const int last_cell = cell_count - 1;
    const int s = std::max(1, step);

    if(sample_index <= 0)
    {
        return 0;
    }
    if(sample_index >= sample_count - 1)
    {
        return last_cell;
    }

    const int ideal_samples = RoomGridOverlaySampleCountAlong(cell_count, step);
    if(sample_count >= ideal_samples)
    {
        return std::min(last_cell, sample_index * s);
    }

    return (sample_index * last_cell) / std::max(1, sample_count - 1);
}
} // namespace

void LEDViewport3D::SetRoomGridOverlayBounds(float min_x, float max_x, float min_y, float max_y, float min_z, float max_z)
{
    room_grid_overlay_use_bounds = true;
    room_grid_overlay_min_x = min_x;
    room_grid_overlay_max_x = max_x;
    room_grid_overlay_min_y = min_y;
    room_grid_overlay_max_y = max_y;
    room_grid_overlay_min_z = min_z;
    room_grid_overlay_max_z = max_z;
    room_grid_overlay_last_nx = -1;
    room_grid_overlay_last_ny = -1;
    room_grid_overlay_last_nz = -1;
    room_grid_overlay_last_step = -1;
}

void LEDViewport3D::ClearRoomGridOverlayBounds()
{
    room_grid_overlay_use_bounds = false;
    room_grid_overlay_last_nx = -1;
    room_grid_overlay_last_ny = -1;
    room_grid_overlay_last_nz = -1;
    room_grid_overlay_last_step = -1;
}

void LEDViewport3D::SetRoomGridStep(int step)
{
    const int s = std::max(1, std::min(24, step));
    if(room_grid_step == s)
    {
        return;
    }
    room_grid_step = s;
    room_grid_overlay_last_nx = -1;
    room_grid_overlay_last_ny = -1;
    room_grid_overlay_last_nz = -1;
    room_grid_overlay_last_step = -1;
    update();
}

void LEDViewport3D::getRoomGridOverlayExtents(float& min_x, float& max_x, float& min_y, float& max_y, float& min_z,
                                              float& max_z) const
{
    if(room_grid_overlay_use_bounds)
    {
        min_x = room_grid_overlay_min_x;
        max_x = room_grid_overlay_max_x;
        min_y = room_grid_overlay_min_y;
        max_y = room_grid_overlay_max_y;
        min_z = room_grid_overlay_min_z;
        max_z = room_grid_overlay_max_z;
        return;
    }

    const GridExtents extents = GetRoomExtents();
    min_x = 0.0f;
    max_x = extents.width_units;
    min_y = 0.0f;
    max_y = extents.height_units;
    min_z = 0.0f;
    max_z = extents.depth_units;
}

void LEDViewport3D::GetRoomGridOverlayDimensions(int* out_nx, int* out_ny, int* out_nz) const
{
    const int step = std::max(1, room_grid_step);
    float min_x = 0.0f;
    float max_x = 0.0f;
    float min_y = 0.0f;
    float max_y = 0.0f;
    float min_z = 0.0f;
    float max_z = 0.0f;
    getRoomGridOverlayExtents(min_x, max_x, min_y, max_y, min_z, max_z);

    int nx = RoomGridOverlaySampleCountAlong(RoomGridOverlayCellCountAlong(min_x, max_x), step);
    int ny = RoomGridOverlaySampleCountAlong(RoomGridOverlayCellCountAlong(min_y, max_y), step);
    int nz = RoomGridOverlaySampleCountAlong(RoomGridOverlayCellCountAlong(min_z, max_z), step);
    /* Allow denser room-grid sampling (~41³) so overlay matches higher volume atlases. */
    const size_t max_overlay_points = 70000u;
    size_t count = (size_t)nx * (size_t)ny * (size_t)nz;
    if(count > max_overlay_points)
    {
        const int cap = std::max(1, (int)std::floor(std::cbrt((double)max_overlay_points * 0.98)));
        nx = std::min(nx, cap);
        ny = std::min(ny, cap);
        nz = std::min(nz, cap);
    }
    if(out_nx) *out_nx = nx;
    if(out_ny) *out_ny = ny;
    if(out_nz) *out_nz = nz;
}

void LEDViewport3D::GetRoomGridOverlaySamplePosition(int ix, int iy, int iz, float& x, float& y, float& z) const
{
    const int step = std::max(1, room_grid_step);
    float min_x = 0.0f;
    float max_x = 0.0f;
    float min_y = 0.0f;
    float max_y = 0.0f;
    float min_z = 0.0f;
    float max_z = 0.0f;
    getRoomGridOverlayExtents(min_x, max_x, min_y, max_y, min_z, max_z);

    const int cells_x = RoomGridOverlayCellCountAlong(min_x, max_x);
    const int cells_y = RoomGridOverlayCellCountAlong(min_y, max_y);
    const int cells_z = RoomGridOverlayCellCountAlong(min_z, max_z);

    int nx = 0;
    int ny = 0;
    int nz = 0;
    GetRoomGridOverlayDimensions(&nx, &ny, &nz);

    const int cell_ix = RoomGridOverlaySampleCellIndex(ix, nx, cells_x, step);
    const int cell_iy = RoomGridOverlaySampleCellIndex(iy, ny, cells_y, step);
    const int cell_iz = RoomGridOverlaySampleCellIndex(iz, nz, cells_z, step);
    x = min_x + (float)cell_ix;
    y = min_y + (float)cell_iy;
    z = min_z + (float)cell_iz;
}

void LEDViewport3D::SetRoomGridBrightness(float brightness)
{
    const float b = std::max(0.0f, std::min(1.0f, brightness));
    if(room_grid_brightness == b)
    {
        return;
    }
    room_grid_brightness = b;
    update();
}

void LEDViewport3D::SetRoomGridPointSize(float size)
{
    const float s = std::max(0.5f, std::min(12.0f, size));
    if(room_grid_point_size == s)
    {
        return;
    }
    room_grid_point_size = s;
    update();
}

void LEDViewport3D::releaseOwnedOverlayAtlasTextures()
{
    if(owned_overlay_atlas_textures_.empty())
    {
        return;
    }
    makeCurrent();
    for(unsigned int tex : owned_overlay_atlas_textures_)
    {
        if(tex != 0)
        {
            GLuint id = tex;
            glDeleteTextures(1, &id);
        }
    }
    owned_overlay_atlas_textures_.clear();
}

void LEDViewport3D::SetGpuRoomGridOverlayLayers(std::vector<GpuRoomGridOverlayLayer> layers)
{
    if(layers.size() > (size_t)GpuRoomGridOverlayLayer::kMaxLayers)
    {
        layers.resize((size_t)GpuRoomGridOverlayLayer::kMaxLayers);
    }

    /* Prefer same-frame pool_src_tex (durable SpatialOverlayHandoff). Borrow the share-group
       name as atlas_tex — do not blit or own it. atlas_rgba only when the pool was cold. */
    releaseOwnedOverlayAtlasTextures();
    makeCurrent();

    /* Colors-and-patterns strip: kPatternWidth × 2 RGBA32F, row 0 = strip value (R),
       row 1 = kernel palette RGB. Float texels keep full colour precision while blending;
       LEDs still finish as 8-bit/channel RGB. */
    auto upload_pattern = [this](GpuRoomGridOverlayLayer& layer) {
        layer.pattern_tex = 0;
        if(!layer.pattern_on)
        {
            return;
        }
        if(layer.pattern_pool_src_tex != 0)
        {
            layer.pattern_tex = layer.pattern_pool_src_tex;
            layer.pattern_pool_src_tex = 0;
            return; /* borrowed handoff — do not glDelete */
        }
        constexpr int n = GpuRoomGridOverlayLayer::kPatternWidth;
        std::vector<float> texels((size_t)n * 2u * 4u, 0.0f);
        for(int x = 0; x < n; ++x)
        {
            texels[(size_t)x * 4u] = layer.pattern_strip[x];
            float* pal = &texels[((size_t)n + (size_t)x) * 4u];
            pal[0] = layer.pattern_lut[x][0];
            pal[1] = layer.pattern_lut[x][1];
            pal[2] = layer.pattern_lut[x][2];
            pal[3] = 1.0f;
        }
        GLuint id = 0;
        glGenTextures(1, &id);
        if(id == 0)
        {
            layer.pattern_on = false;
            return;
        }
        glBindTexture(GL_TEXTURE_2D, id);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, n, 2, 0, GL_RGBA, GL_FLOAT, texels.data());
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        layer.pattern_tex = id;
        owned_overlay_atlas_textures_.push_back(id);
    };

    std::vector<GpuRoomGridOverlayLayer> kept;
    kept.reserve(layers.size());
    for(GpuRoomGridOverlayLayer& layer : layers)
    {
        upload_pattern(layer);
        if(layer.kind == GpuRoomGridOverlayLayer::KindReactive)
        {
            /* Wave list: 4 RGBA32F texels per wave, read with texelFetch (no filtering). */
            const int rows = std::max(1, layer.data_rows);
            std::vector<float> data = layer.data_f32;
            data.resize((size_t)rows * 16u, 0.0f);
            GLuint dst = 0;
            glGenTextures(1, &dst);
            if(dst == 0)
            {
                continue;
            }
            glBindTexture(GL_TEXTURE_2D, dst);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 4, rows, 0, GL_RGBA, GL_FLOAT, data.data());
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            layer.atlas_tex = dst;
            layer.data_f32.clear();
            layer.data_f32.shrink_to_fit();
            owned_overlay_atlas_textures_.push_back(dst);
            kept.push_back(std::move(layer));
            continue;
        }
        /* 2D frames (Shader Field frame, Screen Mirror stacked captures) upload at tex_w × tex_h. */
        const bool projected = (layer.kind == GpuRoomGridOverlayLayer::KindProjected2D
                                || layer.kind == GpuRoomGridOverlayLayer::KindScreenMirror
                                || layer.kind == GpuRoomGridOverlayLayer::KindMinecraft);
        const int n = layer.atlas_n;
        int w = projected ? layer.tex_w : n;
        int h = projected ? layer.tex_h : n * n;
        if(layer.pool_src_tex != 0 && layer.pool_src_w > 0 && layer.pool_src_h > 0)
        {
            w = layer.pool_src_w;
            h = layer.pool_src_h;
        }
        const bool size_ok = projected ? (w >= 1 && h >= 1) : (n >= 2 || (layer.pool_src_tex != 0 && w >= 1 && h >= 1));
        if(!size_ok)
        {
            layer.atlas_tex = 0;
            layer.pool_src_tex = 0;
            layer.atlas_rgba.clear();
            continue;
        }

        GLuint dst = 0;
        bool uploaded = false;
        bool borrowed = false;
        if(layer.pool_src_tex != 0)
        {
            /* Durable handoff from the pool — borrow; do not glDelete. */
            dst = layer.pool_src_tex;
            uploaded = true;
            borrowed = true;
        }
        if(!uploaded)
        {
            const size_t need = (size_t)w * (size_t)h * 4u;
            if(layer.atlas_rgba.size() < need)
            {
                layer.atlas_tex = 0;
                layer.pool_src_tex = 0;
                layer.atlas_rgba.clear();
                continue;
            }
            glGenTextures(1, &dst);
            if(dst == 0)
            {
                layer.atlas_tex = 0;
                layer.pool_src_tex = 0;
                layer.atlas_rgba.clear();
                continue;
            }
            glBindTexture(GL_TEXTURE_2D, dst);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                         layer.atlas_rgba.data());
            uploaded = true;
        }
        if(!uploaded || dst == 0)
        {
            layer.atlas_tex = 0;
            layer.pool_src_tex = 0;
            layer.atlas_rgba.clear();
            continue;
        }
        layer.atlas_tex = dst;
        layer.pool_src_tex = 0;
        layer.pool_src_w = 0;
        layer.pool_src_h = 0;
        layer.pool_src_flip_y = false;
        layer.atlas_rgba.clear();
        layer.atlas_rgba.shrink_to_fit();
        if(projected)
        {
            layer.tex_w = w;
            layer.tex_h = h;
        }
        else
        {
            layer.atlas_n = n > 0 ? n : (int)std::lround(std::sqrt((double)h));
        }
        if(!borrowed)
        {
            owned_overlay_atlas_textures_.push_back(dst);
        }
        kept.push_back(std::move(layer));
    }
    glBindTexture(GL_TEXTURE_2D, 0);

    gpu_room_grid_overlay_layers_ = std::move(kept);
    update();
}

void LEDViewport3D::ClearGpuRoomGridOverlayLayers()
{
    releaseOwnedOverlayAtlasTextures();
    if(gpu_room_grid_overlay_layers_.empty())
    {
        return;
    }
    gpu_room_grid_overlay_layers_.clear();
    update();
}

void LEDViewport3D::DrawRoomGridOverlay()
{
    if(!viewport_shader_programs_ok_)
    {
        return;
    }

    const bool have_effect_layers = !gpu_room_grid_overlay_layers_.empty()
                                    && gl_prog_volume_atlas_point_.IsValid();
    const bool guide_only = !have_effect_layers;
    if(guide_only && !gl_prog_unlit_point_.IsValid())
    {
        return;
    }

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);

    int nx = 0, ny = 0, nz = 0;
    GetRoomGridOverlayDimensions(&nx, &ny, &nz);
    const size_t count = (size_t)nx * (size_t)ny * (size_t)nz;
    if(count <= 0)
    {
        return;
    }

    const bool layout_changed = room_grid_overlay_positions.size() != (3u * count) ||
                                room_grid_overlay_last_nx != nx || room_grid_overlay_last_ny != ny ||
                                room_grid_overlay_last_nz != nz || room_grid_overlay_last_step != room_grid_step;
    if(layout_changed)
    {
        room_grid_overlay_last_nx = nx;
        room_grid_overlay_last_ny = ny;
        room_grid_overlay_last_nz = nz;
        room_grid_overlay_last_step = room_grid_step;
        room_grid_overlay_positions.resize(3u * count);
        float* pos = room_grid_overlay_positions.data();
        for(int ix = 0; ix < nx; ix++)
        {
            for(int iy = 0; iy < ny; iy++)
            {
                for(int iz = 0; iz < nz; iz++)
                {
                    float x = 0.0f;
                    float y = 0.0f;
                    float z = 0.0f;
                    GetRoomGridOverlaySamplePosition(ix, iy, iz, x, y, z);
                    *pos++ = x;
                    *pos++ = y;
                    *pos++ = z;
                }
            }
        }
    }

    /* Guide lattice (no stack effect): black points — sample positions only.
       Effect layers replace this with atlas shading. */
    const float pr = have_effect_layers ? 1.0f : 0.0f;
    const float pg = have_effect_layers ? 1.0f : 0.0f;
    const float pb = have_effect_layers ? 1.0f : 0.0f;

    if(layout_changed || !room_grid_overlay_batch_.IsValid() || guide_only)
    {
        room_grid_overlay_interleaved_.resize(6u * count);
        for(size_t i = 0; i < count; ++i)
        {
            room_grid_overlay_interleaved_[i * 6 + 0] = room_grid_overlay_positions[i * 3 + 0];
            room_grid_overlay_interleaved_[i * 6 + 1] = room_grid_overlay_positions[i * 3 + 1];
            room_grid_overlay_interleaved_[i * 6 + 2] = room_grid_overlay_positions[i * 3 + 2];
            room_grid_overlay_interleaved_[i * 6 + 3] = pr;
            room_grid_overlay_interleaved_[i * 6 + 4] = pg;
            room_grid_overlay_interleaved_[i * 6 + 5] = pb;
        }
        room_grid_overlay_batch_.Upload(MeshBatch::Layout::PosColor,
                                        room_grid_overlay_interleaved_.data(),
                                        count);
    }

    if(guide_only)
    {
        const ViewportMat4 identity = ViewportMath::Identity();
        drawUnlitPoints(room_grid_overlay_batch_, room_grid_point_size, 1.0f, identity);
        return;
    }

    ViewportMat4 projection;
    ViewportMat4 view;
    std::memcpy(projection.m, pick_projection_, sizeof(float) * 16);
    std::memcpy(view.m, pick_scene_modelview_, sizeof(float) * 16);
    const ViewportMat4 identity = ViewportMath::Identity();
    const ViewportMat4 mvp = ViewportMath::ModelViewProjection(projection, view, identity);

    glEnable(GL_PROGRAM_POINT_SIZE);
    gl_prog_volume_atlas_point_.Bind();
    gl_prog_volume_atlas_point_.SetUniformMat4("u_mvp", mvp.m);
    gl_prog_volume_atlas_point_.SetUniform1f("u_point_size", room_grid_point_size);
    gl_prog_volume_atlas_point_.SetUniform1f("u_alpha", 1.0f);
    gl_prog_volume_atlas_point_.SetUniform1f("u_grid_brightness", room_grid_brightness);
    const int layer_count = (int)gpu_room_grid_overlay_layers_.size();
    gl_prog_volume_atlas_point_.SetUniform1i("u_layer_count", layer_count);

    /* Screen Mirror planes are shader-global (one Screen Mirror layer); a second one is skipped. */
    bool screen_mirror_bound = false;
    bool reactive_bound = false;
    bool minecraft_bound = false;
    for(int i = 0; i < GpuRoomGridOverlayLayer::kMaxLayers; ++i)
    {
        const char* atlas_name = (i == 0) ? "u_atlas0" : (i == 1) ? "u_atlas1" : (i == 2) ? "u_atlas2" : "u_atlas3";
        const char* pat_name = (i == 0) ? "u_pat0" : (i == 1) ? "u_pat1" : (i == 2) ? "u_pat2" : "u_pat3";
        glActiveTexture(GL_TEXTURE0 + i);
        if(i < layer_count)
        {
            const GpuRoomGridOverlayLayer& layer = gpu_room_grid_overlay_layers_[(size_t)i];
            glBindTexture(GL_TEXTURE_2D, layer.atlas_tex);
            gl_prog_volume_atlas_point_.SetUniform1i(atlas_name, i);
            int layer_kind = layer.kind;
            if(layer.kind == GpuRoomGridOverlayLayer::KindMinecraft)
            {
                if(minecraft_bound)
                {
                    layer_kind = -1;
                }
                else
                {
                    minecraft_bound = true;
                    for(int s = 0; s < GpuRoomGridOverlayLayer::kMinecraftSlots; ++s)
                    {
                        char mname[32];
                        std::snprintf(mname, sizeof(mname), "u_mc[%d]", s);
                        gl_prog_volume_atlas_point_.SetUniform3f(mname, layer.mc[s][0], layer.mc[s][1], layer.mc[s][2]);
                    }
                }
            }
            if(layer.kind == GpuRoomGridOverlayLayer::KindReactive)
            {
                if(reactive_bound)
                {
                    layer_kind = -1;
                }
                else
                {
                    reactive_bound = true;
                    gl_prog_volume_atlas_point_.SetUniform1i("u_rw_count", layer.data_rows);
                    gl_prog_volume_atlas_point_.SetUniform1f("u_rw_cycles", layer.reactive_cycles);
                    /* Reactive rings are thin shells (sigma can be a few % of LED spacing). On a
                       cubic lattice only the axis-aligned points sit exactly on a sphere, so a thin
                       ring reads as an X/Y/Z cross. Floor the ring width at ~one lattice step so
                       the overlay shows the full 360° shell. Overlay only — LEDs are untouched. */
                    float omin_x = 0.0f, omax_x = 0.0f, omin_y = 0.0f, omax_y = 0.0f, omin_z = 0.0f, omax_z = 0.0f;
                    getRoomGridOverlayExtents(omin_x, omax_x, omin_y, omax_y, omin_z, omax_z);
                    int onx = 1, ony = 1, onz = 1;
                    GetRoomGridOverlayDimensions(&onx, &ony, &onz);
                    const float step_x = (omax_x - omin_x) / (float)std::max(1, onx - 1);
                    const float step_y = (omax_y - omin_y) / (float)std::max(1, ony - 1);
                    const float step_z = (omax_z - omin_z) / (float)std::max(1, onz - 1);
                    const float lattice_step = std::max(step_x, std::max(step_y, step_z));
                    gl_prog_volume_atlas_point_.SetUniform1f("u_rw_min_sigma", 0.75f * std::max(lattice_step, 1e-4f));
                }
            }
            if(layer.kind == GpuRoomGridOverlayLayer::KindScreenMirror)
            {
                if(screen_mirror_bound)
                {
                    layer_kind = -1; /* draws nothing */
                }
                else
                {
                    screen_mirror_bound = true;
                    UploadScreenMirrorOverlayUniforms(layer);
                }
            }

            auto set_i = [&](const char* base, int value) {
                char name[64];
                std::snprintf(name, sizeof(name), "%s[%d]", base, i);
                gl_prog_volume_atlas_point_.SetUniform1i(name, value);
            };
            auto set_f = [&](const char* base, float value) {
                char name[64];
                std::snprintf(name, sizeof(name), "%s[%d]", base, i);
                gl_prog_volume_atlas_point_.SetUniform1f(name, value);
            };
            auto set_3 = [&](const char* base, const float* v) {
                char name[64];
                std::snprintf(name, sizeof(name), "%s[%d]", base, i);
                gl_prog_volume_atlas_point_.SetUniform3f(name, v[0], v[1], v[2]);
            };
            auto set_m3 = [&](const char* base, const float* m) {
                char name[64];
                std::snprintf(name, sizeof(name), "%s[%d]", base, i);
                gl_prog_volume_atlas_point_.SetUniformMat3(name, m);
            };

            set_i("u_atlas_n", layer.atlas_n);
            set_i("u_kind", layer_kind);
            set_i("u_projection", layer.projection);
            set_i("u_blend", layer.blend_mode);
            set_i("u_sample_room", layer.sample_room ? 1 : 0);
            set_i("u_volume_tools", layer.volume_tools ? 1 : 0);
            set_i("u_volume_shape", layer.volume_shape);
            set_i("u_finish", layer.finish_mode);
            set_i("u_rainbow", layer.rainbow ? 1 : 0);
            set_i("u_depth_shade", layer.depth_shade ? 1 : 0);
            set_f("u_layer_brightness", layer.brightness);
            set_f("u_hue_scroll", layer.hue_scroll_deg);
            set_i("u_palette_count", layer.palette_count);
            set_3("u_origin", layer.origin);
            set_3("u_half_ext", layer.half_ext);
            set_3("u_room_min", layer.room_min);
            set_3("u_room_max", layer.room_max);
            const float shear[3] = {layer.shear_xy, layer.shear_xz, layer.shear_yz};
            set_3("u_shear", shear);
            set_m3("u_rot", layer.rot_inv);
            /* Colors and patterns: pattern texture on unit 4 + i (atlases use 0..3). */
            {
                glActiveTexture(GL_TEXTURE4 + i);
                glBindTexture(GL_TEXTURE_2D, layer.pattern_tex);
                gl_prog_volume_atlas_point_.SetUniform1i(pat_name, 4 + i);
                glActiveTexture(GL_TEXTURE0 + i);
                set_i("u_pat_on", (layer.pattern_on && layer.pattern_tex != 0) ? 1 : 0);
                set_i("u_pat_mode", layer.pattern_unfold);
                set_f("u_pat_dir", layer.pattern_dir_deg);
                set_f("u_pat_feature", layer.pattern_feature);
                set_f("u_pat_phase", layer.pattern_phase_s01);
            }
            for(int pi = 0; pi < GpuRoomGridOverlayLayer::kMaxPalette; ++pi)
            {
                char pname[64];
                std::snprintf(pname, sizeof(pname), "u_palette[%d]", i * GpuRoomGridOverlayLayer::kMaxPalette + pi);
                const float* pc = (pi < layer.palette_count) ? layer.palette[pi] : layer.palette[0];
                float z[3] = {0.0f, 0.0f, 0.0f};
                const float* use = (pi < layer.palette_count) ? pc : z;
                gl_prog_volume_atlas_point_.SetUniform3f(pname, use[0], use[1], use[2]);
            }
        }
        else
        {
            glBindTexture(GL_TEXTURE_2D, 0);
            gl_prog_volume_atlas_point_.SetUniform1i(atlas_name, i);
            glActiveTexture(GL_TEXTURE4 + i);
            glBindTexture(GL_TEXTURE_2D, 0);
            gl_prog_volume_atlas_point_.SetUniform1i(pat_name, 4 + i);
        }
    }

    room_grid_overlay_batch_.Draw(MeshBatch::Primitive::Points);
    gl_prog_volume_atlas_point_.Unbind();
    for(int i = 0; i < GpuRoomGridOverlayLayer::kMaxLayers; ++i)
    {
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, 0);
        glActiveTexture(GL_TEXTURE4 + i);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    glActiveTexture(GL_TEXTURE0);
    glDisable(GL_PROGRAM_POINT_SIZE);
}

void LEDViewport3D::DrawDisplayPlanes()
{
    if(!display_planes) return;
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    for(size_t plane_index = 0; plane_index < display_planes->size(); plane_index++)
    {
        DisplayPlane3D* plane_ptr = (*display_planes)[plane_index].get();
        if(!plane_ptr) continue;

        const bool wants_cal = PlaneWantsCalibrationPattern(plane_ptr);
        const bool wants_prev = PlaneWantsScreenPreview(plane_ptr);
        if(!plane_ptr->IsVisible() && !wants_cal && !wants_prev) continue;

        float width_units = MMToGridUnits(plane_ptr->GetWidthMM(), grid_scale_mm);
        float height_units = MMToGridUnits(plane_ptr->GetHeightMM(), grid_scale_mm);

        if(width_units <= 0.0f || height_units <= 0.0f) continue;

        float half_w = width_units * 0.5f;
        float half_h = height_units * 0.5f;

        Vector3D local_corners[4] = {
            { -half_w, -half_h, 0.0f },
            {  half_w, -half_h, 0.0f },
            {  half_w,  half_h, 0.0f },
            { -half_w,  half_h, 0.0f }
        };

        Vector3D world_corners[4];
        for(int i = 0; i < 4; ++i)
        {
            world_corners[i] = TransformLocalToWorld(local_corners[i], plane_ptr->GetTransform());
        }

        const bool selected = ((int)plane_index == selected_display_plane_idx);
        const float fill_r = selected ? 0.35f : 0.2f;
        const float fill_g = selected ? 0.80f : 0.60f;
        const float fill_b = 1.0f;
        const float fill_a = selected ? 0.30f : 0.18f;
        const float border_r = selected ? 0.65f : 0.35f;
        const float border_g = selected ? 0.90f : 0.70f;
        const float border_b = 1.0f;
        const float border_a = selected ? 1.00f : 0.85f;

        if(wants_cal)
        {
            std::vector<float> cal;
            AppendCalibrationQuads(cal, world_corners);
            display_plane_batch_.Upload(MeshBatch::Layout::PosColor, cal.data(), cal.size() / 6);
            drawUnlitBatch(display_plane_batch_, MeshBatch::Primitive::Triangles, 1.0f, 0.85f, nullptr);
        }
        else if(wants_prev)
        {
            std::string source_id = plane_ptr->GetCaptureSourceId();
            GLuint texture_id = 0;
            bool has_texture = false;

            if(!source_id.empty())
            {
                std::map<std::string, GLuint>::iterator texture_it = display_plane_textures.find(source_id);
                if(texture_it != display_plane_textures.end())
                {
                    texture_id = texture_it->second;
                    has_texture = true;
                }
            }

            if(has_texture)
            {
                std::vector<float> textured;
                PushTexturedQuad(textured, world_corners);
                display_plane_batch_.Upload(MeshBatch::Layout::PosColorUv, textured.data(), textured.size() / 8);
                drawTexturedBatch(display_plane_batch_, texture_id, 0.85f, nullptr);
            }
            else
            {
                std::vector<float> cal;
                AppendCalibrationQuads(cal, world_corners);
                display_plane_batch_.Upload(MeshBatch::Layout::PosColor, cal.data(), cal.size() / 6);
                drawUnlitBatch(display_plane_batch_, MeshBatch::Primitive::Triangles, 1.0f, 0.85f, nullptr);
            }
        }
        else
        {
            std::vector<float> fill;
            PushQuadPC(fill, world_corners[0], world_corners[1], world_corners[2], world_corners[3],
                       fill_r, fill_g, fill_b);
            display_plane_batch_.Upload(MeshBatch::Layout::PosColor, fill.data(), fill.size() / 6);
            drawUnlitBatch(display_plane_batch_, MeshBatch::Primitive::Triangles, 1.0f, fill_a, nullptr);
        }

        std::vector<float> border;
        for(int i = 0; i < 4; ++i)
        {
            const Vector3D& a = world_corners[i];
            const Vector3D& b = world_corners[(i + 1) % 4];
            PushPC(border, a.x, a.y, a.z, border_r, border_g, border_b);
            PushPC(border, b.x, b.y, b.z, border_r, border_g, border_b);
        }
        // Reuse controller edges scratch so textured PosColorUv upload is not overwritten mid-draw.
        controller_edges_batch_.Upload(MeshBatch::Layout::PosColor, border.data(), border.size() / 6);
        drawUnlitBatch(controller_edges_batch_, MeshBatch::Primitive::Lines, selected ? 3.0f : 2.0f, border_a, nullptr);
    }

    glDisable(GL_BLEND);
}

void LEDViewport3D::UpdateDisplayPlaneTextures()
{
    if(!display_planes) return;

    ScreenCaptureManager& capture_mgr = ScreenCaptureManager::Instance();
    if(!capture_mgr.IsCaptureSessionActive())
    {
        return;
    }

    if(!capture_mgr.IsInitialized())
    {
        capture_mgr.Initialize();
        if(!capture_mgr.IsInitialized()) return;
    }

    for(size_t i = 0; i < display_planes->size(); i++)
    {
        DisplayPlane3D* plane = (*display_planes)[i].get();
        if(!plane) continue;
        if(!plane->IsVisible() && !PlaneWantsScreenPreview(plane)) continue;

        std::string source_id = plane->GetCaptureSourceId();
        if(source_id.empty()) continue;

        if(!capture_mgr.IsCapturing(source_id))
        {
            capture_mgr.StartCapture(source_id);
        }

        std::shared_ptr<CapturedFrame> frame = capture_mgr.GetLatestFrame(source_id);
        if(!frame || !frame->valid || frame->data.empty()) continue;

        DisplayPlaneTexUploadState& up = display_plane_tex_upload_state[source_id];
        if(up.frame_id == frame->frame_id && up.width == frame->width && up.height == frame->height)
        {
            continue;
        }

        GLuint texture_id = 0;
        std::map<std::string, GLuint>::iterator texture_it = display_plane_textures.find(source_id);
        const bool created_texture = (texture_it == display_plane_textures.end());
        if(created_texture)
        {
            glGenTextures(1, &texture_id);
            display_plane_textures[source_id] = texture_id;
        }
        else
        {
            texture_id = texture_it->second;
        }

        glBindTexture(GL_TEXTURE_2D, texture_id);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        const bool same_size = (up.width == frame->width && up.height == frame->height && !created_texture);
        if(same_size)
        {
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, frame->width, frame->height,
                            GL_RGBA, GL_UNSIGNED_BYTE, frame->data.data());
        }
        else
        {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, frame->width, frame->height,
                         0, GL_RGBA, GL_UNSIGNED_BYTE, frame->data.data());
        }
        if(created_texture || !display_plane_tex_params_set[source_id])
        {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            display_plane_tex_params_set[source_id] = true;
        }
        glBindTexture(GL_TEXTURE_2D, 0);

        up.frame_id = frame->frame_id;
        up.width = frame->width;
        up.height = frame->height;
    }
}

void LEDViewport3D::ClearDisplayPlaneTextures()
{
    for(std::map<std::string, GLuint>::iterator it = display_plane_textures.begin();
        it != display_plane_textures.end(); ++it)
    {
        if(it->second != 0)
        {
            glDeleteTextures(1, &(it->second));
        }
    }
    display_plane_textures.clear();
    display_plane_tex_upload_state.clear();
    display_plane_tex_params_set.clear();
}


void LEDViewport3D::UploadScreenMirrorOverlayUniforms(const GpuRoomGridOverlayLayer& layer)
{
    GlProgram& prog = gl_prog_volume_atlas_point_;
    const int count = std::min((int)layer.screen_planes.size(), GpuRoomGridOverlayLayer::kMaxScreenPlanes);
    prog.SetUniform1i("u_sm_plane_count", count);
    prog.SetUniform1f("u_sm_grid_scale_mm", layer.grid_scale_mm);
    for(int k = 0; k < count; ++k)
    {
        const GpuRoomGridOverlayLayer::ScreenPlane& p = layer.screen_planes[(size_t)k];
        char name[64];
        auto nm = [&](const char* base) -> const char* {
            std::snprintf(name, sizeof(name), "%s[%d]", base, k);
            return name;
        };
        prog.SetUniform3f(nm("u_sm_frame"), (float)p.frame_w, (float)p.frame_h, (float)p.row0);
        prog.SetUniform3f(nm("u_sm_pos"), p.pos[0], p.pos[1], p.pos[2]);
        /* Row-major world→local passed as column-major: GLSL uses delta * M. */
        prog.SetUniformMat3(nm("u_sm_rot"), p.rot);
        prog.SetUniform3f(nm("u_sm_inv_scale"), p.inv_scale[0], p.inv_scale[1], p.inv_scale[2]);
        prog.SetUniform3f(nm("u_sm_half"), p.half_w, p.half_h, (float)p.zone_count);
        prog.SetUniform3f(nm("u_sm_crop_u"), p.crop[0], p.crop[1], 0.0f);
        prog.SetUniform3f(nm("u_sm_crop_v"), p.crop[2], p.crop[3], 0.0f);
        prog.SetUniform3f(nm("u_sm_falloff"), p.max_mm, p.coverage, p.softness);
        prog.SetUniform3f(nm("u_sm_falloff2"), p.curve, p.inverted ? 1.0f : 0.0f, 0.0f);
        prog.SetUniform3f(nm("u_sm_grade0"), p.brightness, p.threshold, p.white_rolloff);
        prog.SetUniform3f(nm("u_sm_grade1"), p.vibrance, p.white_rolloff_max, 0.0f);
        prog.SetUniform3f(nm("u_sm_gain"), p.gain[0], p.gain[1], p.gain[2]);
        for(int z = 0; z < GpuRoomGridOverlayLayer::kMaxScreenZones; ++z)
        {
            char zname[64];
            std::snprintf(zname, sizeof(zname), "u_sm_zone_u[%d]", k * GpuRoomGridOverlayLayer::kMaxScreenZones + z);
            prog.SetUniform3f(zname, p.zones[z][0], p.zones[z][1], 0.0f);
            std::snprintf(zname, sizeof(zname), "u_sm_zone_v[%d]", k * GpuRoomGridOverlayLayer::kMaxScreenZones + z);
            prog.SetUniform3f(zname, p.zones[z][2], p.zones[z][3], 0.0f);
        }
    }
}
