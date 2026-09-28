// SPDX-License-Identifier: GPL-2.0-only

#include "SceneProp3D.h"
#include "TransformJson.h"

int SceneProp3D::next_id = 1;

const char* ScenePropFaceKindToString(ScenePropFaceKind kind)
{
    switch(kind)
    {
        case ScenePropFaceKind::Glass: return "glass";
        case ScenePropFaceKind::Blocker: return "blocker";
        case ScenePropFaceKind::Solid:
        default: return "solid";
    }
}

bool ScenePropFaceKindFromString(const std::string& s, ScenePropFaceKind* out)
{
    if(!out)
    {
        return false;
    }
    if(s == "glass")
    {
        *out = ScenePropFaceKind::Glass;
        return true;
    }
    if(s == "blocker")
    {
        *out = ScenePropFaceKind::Blocker;
        return true;
    }
    if(s == "solid")
    {
        *out = ScenePropFaceKind::Solid;
        return true;
    }
    return false;
}

const char* ScenePropFaceToString(ScenePropFace face)
{
    switch(face)
    {
        case ScenePropFace::Front: return "front";
        case ScenePropFace::Back: return "back";
        case ScenePropFace::Left: return "left";
        case ScenePropFace::Right: return "right";
        case ScenePropFace::Bottom: return "bottom";
        case ScenePropFace::Top: return "top";
        case ScenePropFace::Count:
        default: return "front";
    }
}

SceneProp3D::SceneProp3D(const std::string& name_value)
    : id(next_id++)
    , name(name_value)
    , width_mm(200.0f)
    , height_mm(450.0f)
    , depth_mm(450.0f)
    , color(0x555555u)
    , glass_color(0x4AA8C8u)
    , visible(false)
    , reference_point_index(-1)
{
    transform.position = {0.0f, 0.0f, 0.0f};
    transform.rotation = {0.0f, 0.0f, 0.0f};
    transform.scale    = {1.0f, 1.0f, 1.0f};
    for(int i = 0; i < kFaceCount; ++i)
    {
        faces[i] = ScenePropFaceKind::Solid;
    }
}

ScenePropFaceKind SceneProp3D::GetFaceKind(ScenePropFace face) const
{
    const int i = (int)face;
    if(i < 0 || i >= kFaceCount)
    {
        return ScenePropFaceKind::Solid;
    }
    return faces[i];
}

void SceneProp3D::SetFaceKind(ScenePropFace face, ScenePropFaceKind kind)
{
    const int i = (int)face;
    if(i < 0 || i >= kFaceCount)
    {
        return;
    }
    faces[i] = kind;
}

void SceneProp3D::SetAllFaces(ScenePropFaceKind kind)
{
    for(int i = 0; i < kFaceCount; ++i)
    {
        faces[i] = kind;
    }
}

void SceneProp3D::FaceLocalCorners(ScenePropFace face,
                                   float hw, float hh, float hd,
                                   Vector3D out_corners[4])
{
    if(!out_corners)
    {
        return;
    }

    switch(face)
    {
        case ScenePropFace::Bottom:
            out_corners[0] = {-hw, -hh, -hd};
            out_corners[1] = { hw, -hh, -hd};
            out_corners[2] = { hw, -hh,  hd};
            out_corners[3] = {-hw, -hh,  hd};
            break;
        case ScenePropFace::Top:
            out_corners[0] = {-hw,  hh, -hd};
            out_corners[1] = { hw,  hh, -hd};
            out_corners[2] = { hw,  hh,  hd};
            out_corners[3] = {-hw,  hh,  hd};
            break;
        case ScenePropFace::Left:
            out_corners[0] = {-hw, -hh,  hd};
            out_corners[1] = {-hw, -hh, -hd};
            out_corners[2] = {-hw,  hh, -hd};
            out_corners[3] = {-hw,  hh,  hd};
            break;
        case ScenePropFace::Right:
            out_corners[0] = { hw, -hh, -hd};
            out_corners[1] = { hw, -hh,  hd};
            out_corners[2] = { hw,  hh,  hd};
            out_corners[3] = { hw,  hh, -hd};
            break;
        case ScenePropFace::Front:
            out_corners[0] = { hw, -hh,  hd};
            out_corners[1] = {-hw, -hh,  hd};
            out_corners[2] = {-hw,  hh,  hd};
            out_corners[3] = { hw,  hh,  hd};
            break;
        case ScenePropFace::Back:
            out_corners[0] = {-hw, -hh, -hd};
            out_corners[1] = { hw, -hh, -hd};
            out_corners[2] = { hw,  hh, -hd};
            out_corners[3] = {-hw,  hh, -hd};
            break;
        case ScenePropFace::Count:
        default:
            out_corners[0] = out_corners[1] = out_corners[2] = out_corners[3] = {0.0f, 0.0f, 0.0f};
            break;
    }
}

nlohmann::json SceneProp3D::ToJson() const
{
    nlohmann::json j;
    j["id"]          = id;
    j["name"]        = name;
    j["width_mm"]    = width_mm;
    j["height_mm"]   = height_mm;
    j["depth_mm"]    = depth_mm;
    j["color"]       = color;
    j["glass_color"] = glass_color;
    j["visible"]     = visible;
    if(reference_point_index >= 0)
    {
        j["reference_point_index"] = reference_point_index;
    }
    nlohmann::json fj = nlohmann::json::object();
    for(int i = 0; i < kFaceCount; ++i)
    {
        fj[ScenePropFaceToString((ScenePropFace)i)] = ScenePropFaceKindToString(faces[i]);
    }
    j["faces"] = fj;
    TransformJson::WriteTransform(j, transform);
    return j;
}

std::unique_ptr<SceneProp3D> SceneProp3D::FromJson(const nlohmann::json& j)
{
    if(j.is_null() || !j.is_object())
    {
        return nullptr;
    }

    std::unique_ptr<SceneProp3D> prop =
        std::make_unique<SceneProp3D>(j.value("name", std::string("Scene Prop")));

    if(j.contains("id") && j["id"].is_number_integer())
    {
        prop->id = j["id"].get<int>();
        if(prop->id >= next_id)
        {
            next_id = prop->id + 1;
        }
    }

    prop->SetWidthMM(j.value("width_mm", 200.0f));
    prop->SetHeightMM(j.value("height_mm", 450.0f));
    prop->SetDepthMM(j.value("depth_mm", 450.0f));
    prop->SetColor(j.value("color", 0x555555u));
    prop->SetGlassColor(j.value("glass_color", 0x4AA8C8u));
    prop->visible = j.value("visible", false);
    prop->reference_point_index = j.value("reference_point_index", -1);

    if(j.contains("faces") && j["faces"].is_object())
    {
        const auto& fj = j["faces"];
        for(int i = 0; i < kFaceCount; ++i)
        {
            const char* key = ScenePropFaceToString((ScenePropFace)i);
            if(fj.contains(key) && fj[key].is_string())
            {
                ScenePropFaceKind kind = ScenePropFaceKind::Solid;
                if(ScenePropFaceKindFromString(fj[key].get<std::string>(), &kind))
                {
                    prop->faces[i] = kind;
                }
            }
        }
    }

    TransformJson::ReadTransform(j, prop->transform);
    return prop;
}
