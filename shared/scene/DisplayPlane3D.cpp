// SPDX-License-Identifier: GPL-2.0-only

#include "DisplayPlane3D.h"
#include "TransformJson.h"

int DisplayPlane3D::next_id = 1;

DisplayPlane3D::DisplayPlane3D(const std::string& name_value) :
    id(next_id++),
    name(name_value),
    width_mm(1000.0f),
    height_mm(600.0f),
    visible(false),
    reference_point_index(-1)
{
    transform.position = {0.0f, 0.0f, 0.0f};
    transform.rotation = {0.0f, 0.0f, 0.0f};
    transform.scale    = {1.0f, 1.0f, 1.0f};
}

nlohmann::json DisplayPlane3D::ToJson() const
{
    nlohmann::json j;
    j["id"]            = id;
    j["name"]          = name;
    j["width_mm"]      = width_mm;
    j["height_mm"]     = height_mm;
    j["visible"]       = visible;
    j["capture_id"]    = capture_source_id;
    j["capture_label"] = capture_label;
    if(reference_point_index >= 0)
    {
        j["reference_point_index"] = reference_point_index;
    }

    TransformJson::WriteTransform(j, transform);

    return j;
}

std::unique_ptr<DisplayPlane3D> DisplayPlane3D::FromJson(const nlohmann::json& j)
{
    if(j.is_null())
    {
        return nullptr;
    }

    std::string name_value = j.value("name", "Display Plane");
    std::unique_ptr<DisplayPlane3D> plane = std::make_unique<DisplayPlane3D>(name_value);

    if(j.contains("id"))
    {
        plane->id = j["id"].get<int>();
        if(plane->id >= next_id)
        {
            next_id = plane->id + 1;
        }
    }

    plane->width_mm  = j.value("width_mm", 1000.0f);
    plane->height_mm = j.value("height_mm", 600.0f);
    plane->visible   = j.value("visible", true);
    plane->capture_source_id = j.value("capture_id", std::string());
    plane->capture_label     = j.value("capture_label", std::string());
    plane->reference_point_index = j.value("reference_point_index", -1);

    TransformJson::ReadTransform(j, plane->transform);

    return plane;
}
