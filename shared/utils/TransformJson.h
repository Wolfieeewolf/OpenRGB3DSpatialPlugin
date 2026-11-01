// SPDX-License-Identifier: GPL-2.0-only
// Shared JSON helpers for Transform3D vectors (object {x,y,z} only).

#ifndef TRANSFORMJSON_H
#define TRANSFORMJSON_H

#include "LEDPosition3D.h"
#include <nlohmann/json.hpp>

namespace TransformJson
{

inline void WriteVec3(nlohmann::json& parent, const char* key, float x, float y, float z)
{
    parent[key]["x"] = x;
    parent[key]["y"] = y;
    parent[key]["z"] = z;
}

inline void WriteVec3(nlohmann::json& parent, const char* key, const Vector3D& v)
{
    WriteVec3(parent, key, v.x, v.y, v.z);
}

inline bool ReadVec3(const nlohmann::json& parent, const char* key, float& x, float& y, float& z)
{
    if(!parent.contains(key) || !parent[key].is_object())
    {
        return false;
    }
    const nlohmann::json& v = parent[key];
    if(!v.contains("x") || !v.contains("y") || !v.contains("z"))
    {
        return false;
    }
    x = v["x"].get<float>();
    y = v["y"].get<float>();
    z = v["z"].get<float>();
    return true;
}

inline bool ReadVec3(const nlohmann::json& parent, const char* key, Vector3D& out)
{
    return ReadVec3(parent, key, out.x, out.y, out.z);
}

inline void WriteTransform(nlohmann::json& parent, const Transform3D& transform)
{
    nlohmann::json t = nlohmann::json::object();
    WriteVec3(t, "position", transform.position.x, transform.position.y, transform.position.z);
    WriteVec3(t, "rotation", transform.rotation.x, transform.rotation.y, transform.rotation.z);
    WriteVec3(t, "scale", transform.scale.x, transform.scale.y, transform.scale.z);
    parent["transform"] = std::move(t);
}

inline void ReadTransform(const nlohmann::json& parent, Transform3D& transform)
{
    if(!parent.contains("transform") || !parent["transform"].is_object())
    {
        return;
    }
    const nlohmann::json& t = parent["transform"];
    ReadVec3(t, "position", transform.position.x, transform.position.y, transform.position.z);
    ReadVec3(t, "rotation", transform.rotation.x, transform.rotation.y, transform.rotation.z);
    ReadVec3(t, "scale", transform.scale.x, transform.scale.y, transform.scale.z);
}

} // namespace TransformJson

#endif
