// SPDX-License-Identifier: GPL-2.0-only

#include <cmath>
#include <algorithm>
#include <map>
#include <memory>
#include <cstring>

#include "Colors.h"
#include "LEDViewport3D.h"
#include "viewport/ViewportGLIncludes.h"
#include "ScreenCaptureManager.h"

namespace
{
constexpr float kRoomGridOverlayCellEps = 1e-4f;

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
    room_grid_overlay_colors_dirty = true;
}

void LEDViewport3D::ClearRoomGridOverlayBounds()
{
    room_grid_overlay_use_bounds = false;
    room_grid_overlay_last_nx = -1;
    room_grid_overlay_last_ny = -1;
    room_grid_overlay_last_nz = -1;
    room_grid_overlay_last_step = -1;
    room_grid_overlay_colors_dirty = true;
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
    room_grid_color_buffer.clear();
    invalidateRoomGridOverlayColors();
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
    const size_t max_overlay_points = 35000u;
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

void LEDViewport3D::invalidateRoomGridOverlayColors()
{
    room_grid_overlay_colors_dirty = true;
}

void LEDViewport3D::SetRoomGridBrightness(float brightness)
{
    const float b = std::max(0.0f, std::min(1.0f, brightness));
    if(room_grid_brightness == b)
    {
        return;
    }
    room_grid_brightness = b;
    invalidateRoomGridOverlayColors();
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

void LEDViewport3D::SetRoomGridColorBuffer(std::vector<RGBColor> buf)
{
    room_grid_color_buffer = std::move(buf);
    room_grid_overlay_colors_dirty = true;
    update();
}

void LEDViewport3D::SwapRoomGridColorBuffer(std::vector<RGBColor>& buf)
{
    room_grid_color_buffer.swap(buf);
    room_grid_overlay_colors_dirty = true;
    update();
}

void LEDViewport3D::DrawRoomGridOverlay()
{
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);

    int nx = 0, ny = 0, nz = 0;
    GetRoomGridOverlayDimensions(&nx, &ny, &nz);
    const size_t count = (size_t)nx * (size_t)ny * (size_t)nz;
    if(count <= 0) return;

    const bool use_buffer = (room_grid_color_buffer.size() == count);
    const bool use_callback = (!use_buffer && room_grid_color_callback != nullptr);
    const float default_r = 0.0f;
    const float default_g = 0.0f;
    const float default_b = 0.0f;

    const bool layout_changed = room_grid_overlay_positions.size() != (3u * count) ||
                                room_grid_overlay_last_nx != nx || room_grid_overlay_last_ny != ny ||
                                room_grid_overlay_last_nz != nz || room_grid_overlay_last_step != room_grid_step;
    if(layout_changed)
    {
        room_grid_overlay_last_nx = nx;
        room_grid_overlay_last_ny = ny;
        room_grid_overlay_last_nz = nz;
        room_grid_overlay_last_step = room_grid_step;
        room_grid_overlay_colors_dirty = true;
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

    const bool need_color_refill = room_grid_overlay_colors_dirty || room_grid_overlay_colors.size() != (3u * count);
    if(need_color_refill)
    {
        room_grid_overlay_colors_dirty = false;
        room_grid_overlay_colors.resize(3u * count);
        float* col = room_grid_overlay_colors.data();
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
                    float r, g, b;
                    if(use_buffer)
                    {
                        const size_t idx = (size_t)(ix * ny * nz + iy * nz + iz);
                        RGBColor c = room_grid_color_buffer[idx];
                        r = (float)RGBGetRValue(c) / 255.0f * room_grid_brightness;
                        g = (float)RGBGetGValue(c) / 255.0f * room_grid_brightness;
                        b = (float)RGBGetBValue(c) / 255.0f * room_grid_brightness;
                    }
                    else if(use_callback)
                    {
                        RGBColor c = room_grid_color_callback(x, y, z);
                        r = (float)RGBGetRValue(c) / 255.0f * room_grid_brightness;
                        g = (float)RGBGetGValue(c) / 255.0f * room_grid_brightness;
                        b = (float)RGBGetBValue(c) / 255.0f * room_grid_brightness;
                    }
                    else
                    {
                        r = default_r;
                        g = default_g;
                        b = default_b;
                    }
                    *col++ = r;
                    *col++ = g;
                    *col++ = b;
                }
            }
        }
    }

    glPointSize(room_grid_point_size);
    glVertexPointer(3, GL_FLOAT, 0, room_grid_overlay_positions.data());
    glColorPointer(3, GL_FLOAT, 0, room_grid_overlay_colors.data());
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glDrawArrays(GL_POINTS, 0, (GLsizei)count);
    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
    glPointSize(1.0f);
    glColor3f(1.0f, 1.0f, 1.0f);
}

void LEDViewport3D::DrawDisplayPlanes()
{
    if(!display_planes) return;

    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
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

        bool selected = ((int)plane_index == selected_display_plane_idx);
        float fill_color[4]   = { selected ? 0.35f : 0.2f,  selected ? 0.80f : 0.60f, 1.0f, selected ? 0.30f : 0.18f };
        float border_color[4] = { selected ? 0.65f : 0.35f, selected ? 0.90f : 0.70f, 1.0f, selected ? 1.00f : 0.85f };

        if(wants_cal)
        {
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

            glColor4f(1.0f, 0.0f, 0.0f, 0.85f);
            glBegin(GL_QUADS);
            glVertex3f(world_corners[0].x, world_corners[0].y, world_corners[0].z);
            glVertex3f(mid_bottom.x, mid_bottom.y, mid_bottom.z);
            glVertex3f(center.x, center.y, center.z);
            glVertex3f(mid_left.x, mid_left.y, mid_left.z);
            glEnd();

            glColor4f(0.0f, 1.0f, 0.0f, 0.85f);
            glBegin(GL_QUADS);
            glVertex3f(mid_bottom.x, mid_bottom.y, mid_bottom.z);
            glVertex3f(world_corners[1].x, world_corners[1].y, world_corners[1].z);
            glVertex3f(mid_right.x, mid_right.y, mid_right.z);
            glVertex3f(center.x, center.y, center.z);
            glEnd();

            glColor4f(0.0f, 0.0f, 1.0f, 0.85f);
            glBegin(GL_QUADS);
            glVertex3f(center.x, center.y, center.z);
            glVertex3f(mid_right.x, mid_right.y, mid_right.z);
            glVertex3f(world_corners[2].x, world_corners[2].y, world_corners[2].z);
            glVertex3f(mid_top.x, mid_top.y, mid_top.z);
            glEnd();

            glColor4f(1.0f, 1.0f, 0.0f, 0.85f);
            glBegin(GL_QUADS);
            glVertex3f(mid_left.x, mid_left.y, mid_left.z);
            glVertex3f(center.x, center.y, center.z);
            glVertex3f(mid_top.x, mid_top.y, mid_top.z);
            glVertex3f(world_corners[3].x, world_corners[3].y, world_corners[3].z);
            glEnd();
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
                glEnable(GL_TEXTURE_2D);
                glBindTexture(GL_TEXTURE_2D, texture_id);
                glColor4f(1.0f, 1.0f, 1.0f, 0.85f);

                glBegin(GL_QUADS);
                glTexCoord2f(0.0f, 1.0f);
                glVertex3f(world_corners[0].x, world_corners[0].y, world_corners[0].z);
                glTexCoord2f(1.0f, 1.0f);
                glVertex3f(world_corners[1].x, world_corners[1].y, world_corners[1].z);
                glTexCoord2f(1.0f, 0.0f);
                glVertex3f(world_corners[2].x, world_corners[2].y, world_corners[2].z);
                glTexCoord2f(0.0f, 0.0f);
                glVertex3f(world_corners[3].x, world_corners[3].y, world_corners[3].z);
                glEnd();

                glBindTexture(GL_TEXTURE_2D, 0);
                glDisable(GL_TEXTURE_2D);
            }
            else
            {
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

                glColor4f(1.0f, 0.0f, 0.0f, 0.85f);
                glBegin(GL_QUADS);
                glVertex3f(world_corners[0].x, world_corners[0].y, world_corners[0].z);
                glVertex3f(mid_bottom.x, mid_bottom.y, mid_bottom.z);
                glVertex3f(center.x, center.y, center.z);
                glVertex3f(mid_left.x, mid_left.y, mid_left.z);
                glEnd();

                glColor4f(0.0f, 1.0f, 0.0f, 0.85f);
                glBegin(GL_QUADS);
                glVertex3f(mid_bottom.x, mid_bottom.y, mid_bottom.z);
                glVertex3f(world_corners[1].x, world_corners[1].y, world_corners[1].z);
                glVertex3f(mid_right.x, mid_right.y, mid_right.z);
                glVertex3f(center.x, center.y, center.z);
                glEnd();

                glColor4f(0.0f, 0.0f, 1.0f, 0.85f);
                glBegin(GL_QUADS);
                glVertex3f(center.x, center.y, center.z);
                glVertex3f(mid_right.x, mid_right.y, mid_right.z);
                glVertex3f(world_corners[2].x, world_corners[2].y, world_corners[2].z);
                glVertex3f(mid_top.x, mid_top.y, mid_top.z);
                glEnd();

                glColor4f(1.0f, 1.0f, 0.0f, 0.85f);
                glBegin(GL_QUADS);
                glVertex3f(mid_left.x, mid_left.y, mid_left.z);
                glVertex3f(center.x, center.y, center.z);
                glVertex3f(mid_top.x, mid_top.y, mid_top.z);
                glVertex3f(world_corners[3].x, world_corners[3].y, world_corners[3].z);
                glEnd();
            }
        }
        else
        {
            glColor4fv(fill_color);
            glBegin(GL_QUADS);
            for(int i = 0; i < 4; ++i)
            {
                glVertex3f(world_corners[i].x, world_corners[i].y, world_corners[i].z);
            }
            glEnd();
        }

        glColor4fv(border_color);
        glLineWidth(selected ? 3.0f : 2.0f);
        glBegin(GL_LINE_LOOP);
        for(int i = 0; i < 4; ++i)
        {
            glVertex3f(world_corners[i].x, world_corners[i].y, world_corners[i].z);
        }
        glEnd();
        glLineWidth(1.0f);

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

