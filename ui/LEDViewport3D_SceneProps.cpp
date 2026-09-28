// SPDX-License-Identifier: GPL-2.0-only

#include "LEDViewport3D.h"
#include "SceneProp3D.h"
#include "GridSpaceUtils.h"
#include "viewport/MeshGeometry.h"
#include "viewport/ViewportMath.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <vector>

void LEDViewport3D::SetSceneProps(std::vector<std::unique_ptr<SceneProp3D>>* props)
{
    scene_props_ = props;
    if(!scene_props_)
    {
        selected_scene_prop_idx_ = -1;
    }
    else if(selected_scene_prop_idx_ >= (int)scene_props_->size())
    {
        selected_scene_prop_idx_ = -1;
    }
    UpdateGizmoPosition();
    update();
}

void LEDViewport3D::SelectSceneProp(int index)
{
    if(scene_props_ && index >= 0 && index < (int)scene_props_->size())
    {
        deselectRoomViewport();
        selected_controller_indices.clear();
        selected_controller_idx = -1;
        selected_ref_point_idx = -1;
        selected_display_plane_idx = -1;
        selected_scene_prop_idx_ = index;
        gizmo.SetTarget((*scene_props_)[(size_t)index].get());
        gizmo.SetGridSnap(grid_snap_enabled, 1.0f);
        UpdateGizmoPosition();
    }
    else
    {
        selected_scene_prop_idx_ = -1;
        if(selected_controller_idx < 0 && selected_ref_point_idx < 0 && selected_display_plane_idx < 0)
        {
            gizmo.SetTarget(static_cast<SceneProp3D*>(nullptr));
        }
    }
    update();
}

void LEDViewport3D::NotifyScenePropChanged()
{
    if(scene_props_ && selected_scene_prop_idx_ >= 0
       && selected_scene_prop_idx_ < (int)scene_props_->size())
    {
        SceneProp3D* prop = (*scene_props_)[(size_t)selected_scene_prop_idx_].get();
        if(prop)
        {
            const Transform3D& t = prop->GetTransform();
            emit ScenePropPositionChanged(selected_scene_prop_idx_,
                                          t.position.x, t.position.y, t.position.z);
            emit ScenePropRotationChanged(selected_scene_prop_idx_,
                                          t.rotation.x, t.rotation.y, t.rotation.z);
        }
    }
    UpdateGizmoPosition();
    update();
}

void LEDViewport3D::DrawSceneProps()
{
    if(!scene_props_)
    {
        return;
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    auto unpack = [](unsigned int rgb, float* r, float* g, float* b) {
        *r = ((rgb >> 16) & 0xFF) / 255.0f;
        *g = ((rgb >> 8) & 0xFF) / 255.0f;
        *b = (rgb & 0xFF) / 255.0f;
    };

    for(size_t i = 0; i < scene_props_->size(); ++i)
    {
        SceneProp3D* prop = (*scene_props_)[i].get();
        if(!prop || !prop->IsVisible())
        {
            continue;
        }

        const float w = MMToGridUnits(prop->GetWidthMM(), grid_scale_mm);
        const float h = MMToGridUnits(prop->GetHeightMM(), grid_scale_mm);
        const float d = MMToGridUnits(prop->GetDepthMM(), grid_scale_mm);
        if(w <= 0.0f || h <= 0.0f || d <= 0.0f)
        {
            continue;
        }

        const float hw = w * 0.5f;
        const float hh = h * 0.5f;
        const float hd = d * 0.5f;
        float solid_r, solid_g, solid_b;
        float glass_r, glass_g, glass_b;
        unpack(prop->GetColor(), &solid_r, &solid_g, &solid_b);
        unpack(prop->GetGlassColor(), &glass_r, &glass_g, &glass_b);
        /* Blocker: dark matte charcoal (reads as housing / occlusion). */
        const float block_r = 0.12f;
        const float block_g = 0.12f;
        const float block_b = 0.14f;
        const bool selected = ((int)i == selected_scene_prop_idx_);

        std::vector<float> solid_faces;
        std::vector<float> glass_faces;
        std::vector<float> blocker_faces;
        std::vector<float> edges;

        auto append_face = [&](ScenePropFace face, float x0, float y0, float z0,
                               float x1, float y1, float z1,
                               float x2, float y2, float z2,
                               float x3, float y3, float z3) {
            const ScenePropFaceKind kind = prop->GetFaceKind(face);
            float r = solid_r;
            float g = solid_g;
            float b = solid_b;
            std::vector<float>* dest = &solid_faces;
            if(kind == ScenePropFaceKind::Glass)
            {
                r = glass_r;
                g = glass_g;
                b = glass_b;
                dest = &glass_faces;
            }
            else if(kind == ScenePropFaceKind::Blocker)
            {
                r = block_r;
                g = block_g;
                b = block_b;
                dest = &blocker_faces;
            }
            MeshGeometry::PushQuadAsTris(*dest, x0, y0, z0, x1, y1, z1, x2, y2, z2, x3, y3, z3, r, g, b);
        };

        /* Bottom (−Y), Top (+Y). Front (+Z toward user), Back (−Z), Left (+X), Right (−X)
           so Left/Right match when looking at Front at rotation 0. */
        append_face(ScenePropFace::Bottom,
                    -hw, -hh, -hd,  hw, -hh, -hd,  hw, -hh,  hd, -hw, -hh,  hd);
        append_face(ScenePropFace::Top,
                    -hw,  hh, -hd,  hw,  hh, -hd,  hw,  hh,  hd, -hw,  hh,  hd);
        append_face(ScenePropFace::Left,
                     hw, -hh, -hd,  hw, -hh,  hd,  hw,  hh,  hd,  hw,  hh, -hd);
        append_face(ScenePropFace::Right,
                    -hw, -hh,  hd, -hw, -hh, -hd, -hw,  hh, -hd, -hw,  hh,  hd);
        append_face(ScenePropFace::Front,
                     hw, -hh,  hd, -hw, -hh,  hd, -hw,  hh,  hd,  hw,  hh,  hd);
        append_face(ScenePropFace::Back,
                    -hw, -hh, -hd,  hw, -hh, -hd,  hw,  hh, -hd, -hw,  hh, -hd);

        const float er = selected ? 0.95f : 0.55f;
        const float eg = selected ? 0.85f : 0.55f;
        const float eb = selected ? 0.25f : 0.58f;
        MeshGeometry::AppendAxisAlignedBoxEdges(edges, -hw, -hh, -hd, hw, hh, hd, er, eg, eb);

        ViewportMat4 model = ViewportMath::FromTransform3D(prop->GetTransform());

        if(!blocker_faces.empty())
        {
            scene_prop_faces_batch_.Upload(MeshBatch::Layout::PosColor,
                                           blocker_faces.data(), blocker_faces.size() / 6);
            drawUnlitBatch(scene_prop_faces_batch_, MeshBatch::Primitive::Triangles, 1.0f,
                           selected ? 0.88f : 0.82f, &model);
        }
        if(!solid_faces.empty())
        {
            scene_prop_faces_batch_.Upload(MeshBatch::Layout::PosColor,
                                           solid_faces.data(), solid_faces.size() / 6);
            drawUnlitBatch(scene_prop_faces_batch_, MeshBatch::Primitive::Triangles, 1.0f,
                           selected ? 0.62f : 0.50f, &model);
        }
        if(!glass_faces.empty())
        {
            glDepthMask(GL_FALSE);
            scene_prop_faces_batch_.Upload(MeshBatch::Layout::PosColor,
                                           glass_faces.data(), glass_faces.size() / 6);
            drawUnlitBatch(scene_prop_faces_batch_, MeshBatch::Primitive::Triangles, 1.0f,
                           selected ? 0.28f : 0.22f, &model);
            glDepthMask(GL_TRUE);
        }

        scene_prop_edges_batch_.Upload(MeshBatch::Layout::PosColor, edges.data(), edges.size() / 6);
        drawUnlitBatch(scene_prop_edges_batch_, MeshBatch::Primitive::Lines,
                       selected ? 2.5f : 1.5f, selected ? 0.95f : 0.70f, &model);
    }
}

int LEDViewport3D::PickSceneProp(const Ray3D& ray)
{
    if(!scene_props_)
    {
        return -1;
    }

    float closest = FLT_MAX;
    int best = -1;

    for(size_t i = 0; i < scene_props_->size(); ++i)
    {
        SceneProp3D* prop = (*scene_props_)[i].get();
        if(!prop || !prop->IsVisible())
        {
            continue;
        }
        const float w = MMToGridUnits(prop->GetWidthMM(), grid_scale_mm);
        const float h = MMToGridUnits(prop->GetHeightMM(), grid_scale_mm);
        const float d = MMToGridUnits(prop->GetDepthMM(), grid_scale_mm);
        if(w <= 0.0f || h <= 0.0f || d <= 0.0f)
        {
            continue;
        }
        const float hw = w * 0.5f;
        const float hh = h * 0.5f;
        const float hd = d * 0.5f;

        Vector3D local_corners[8] = {
            {-hw, -hh, -hd}, { hw, -hh, -hd}, { hw,  hh, -hd}, {-hw,  hh, -hd},
            {-hw, -hh,  hd}, { hw, -hh,  hd}, { hw,  hh,  hd}, {-hw,  hh,  hd},
        };
        Vector3D world_min = TransformLocalToWorld(local_corners[0], prop->GetTransform());
        Vector3D world_max = world_min;
        for(int c = 1; c < 8; ++c)
        {
            Vector3D wc = TransformLocalToWorld(local_corners[c], prop->GetTransform());
            world_min.x = std::min(world_min.x, wc.x);
            world_min.y = std::min(world_min.y, wc.y);
            world_min.z = std::min(world_min.z, wc.z);
            world_max.x = std::max(world_max.x, wc.x);
            world_max.y = std::max(world_max.y, wc.y);
            world_max.z = std::max(world_max.z, wc.z);
        }

        Box3D box;
        box.min[0] = world_min.x; box.min[1] = world_min.y; box.min[2] = world_min.z;
        box.max[0] = world_max.x; box.max[1] = world_max.y; box.max[2] = world_max.z;

        /* Slab ray–AABB */
        float tmin = 0.0f;
        float tmax = FLT_MAX;
        bool hit = true;
        for(int axis = 0; axis < 3; ++axis)
        {
            const float o = ray.origin[axis];
            const float dir = ray.direction[axis];
            if(std::fabs(dir) < 1e-8f)
            {
                if(o < box.min[axis] || o > box.max[axis])
                {
                    hit = false;
                    break;
                }
                continue;
            }
            float t1 = (box.min[axis] - o) / dir;
            float t2 = (box.max[axis] - o) / dir;
            if(t1 > t2)
            {
                std::swap(t1, t2);
            }
            tmin = std::max(tmin, t1);
            tmax = std::min(tmax, t2);
            if(tmin > tmax)
            {
                hit = false;
                break;
            }
        }
        if(hit && tmin >= 0.0f && tmin < closest)
        {
            closest = tmin;
            best = (int)i;
        }
    }
    return best;
}
