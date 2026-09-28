// SPDX-License-Identifier: GPL-2.0-only

#ifndef SCENEPROP3D_H
#define SCENEPROP3D_H

#include "LEDPosition3D.h"

#include <memory>
#include <string>
#include <nlohmann/json.hpp>

/** How one face of a box prop is treated. */
enum class ScenePropFaceKind : int
{
    Solid = 0,   /**< Opaque-ish body panel (user solid colour). */
    Glass = 1,   /**< See-through tinted panel (user glass colour). */
    Blocker = 2, /**< Occluding panel (dark / matte — for light blocking later). */
};

/**
 * Box faces in prop-local space (the object’s own front/back/left/right).
 * At rotation 0, Front faces the user (+Z / room back) — desk/case setup looking
 * at the front wall. Left/Right are from that view.
 */
enum class ScenePropFace : int
{
    Front = 0,  /**< Prop front (+Z local → toward user at rot 0) */
    Back = 1,   /**< Prop back (−Z local → toward front wall at rot 0) */
    Left = 2,   /**< Prop left (+X local when facing Front) */
    Right = 3,  /**< Prop right (−X local when facing Front) */
    Bottom = 4, /**< Prop bottom (−Y local) */
    Top = 5,    /**< Prop top (+Y local) */
    Count = 6
};

/** Non-RGB room furniture / fixture (box for now). LEDs stay on controllers. */
class SceneProp3D
{
public:
    static constexpr int kFaceCount = (int)ScenePropFace::Count;

    explicit SceneProp3D(const std::string& name = "Scene Prop");

    int                 GetId() const { return id; }
    const std::string&  GetName() const { return name; }
    void                SetName(const std::string& new_name) { name = new_name; }

    Transform3D&        GetTransform() { return transform; }
    const Transform3D&  GetTransform() const { return transform; }

    float               GetWidthMM() const { return width_mm; }
    void                SetWidthMM(float w) { width_mm = (w > 1.0f) ? w : 1.0f; }

    float               GetHeightMM() const { return height_mm; }
    void                SetHeightMM(float h) { height_mm = (h > 1.0f) ? h : 1.0f; }

    float               GetDepthMM() const { return depth_mm; }
    void                SetDepthMM(float d) { depth_mm = (d > 1.0f) ? d : 1.0f; }

    /** 0xRRGGBB solid panel colour. */
    unsigned int        GetColor() const { return color; }
    void                SetColor(unsigned int rgb) { color = rgb & 0x00FFFFFFu; }

    /** 0xRRGGBB glass tint. */
    unsigned int        GetGlassColor() const { return glass_color; }
    void                SetGlassColor(unsigned int rgb) { glass_color = rgb & 0x00FFFFFFu; }

    ScenePropFaceKind   GetFaceKind(ScenePropFace face) const;
    void                SetFaceKind(ScenePropFace face, ScenePropFaceKind kind);
    void                SetAllFaces(ScenePropFaceKind kind);

    bool                IsVisible() const { return visible; }
    void                SetVisible(bool v) { visible = v; }

    int                 GetReferencePointIndex() const { return reference_point_index; }
    void                SetReferencePointIndex(int index) { reference_point_index = index; }

    nlohmann::json      ToJson() const;
    static std::unique_ptr<SceneProp3D> FromJson(const nlohmann::json& j);

private:
    int                 id;
    std::string         name;
    Transform3D         transform;
    float               width_mm;
    float               height_mm;
    float               depth_mm;
    unsigned int        color;
    unsigned int        glass_color;
    ScenePropFaceKind   faces[kFaceCount];
    bool                visible;
    int                 reference_point_index;

    static int          next_id;
};

const char* ScenePropFaceKindToString(ScenePropFaceKind kind);
bool ScenePropFaceKindFromString(const std::string& s, ScenePropFaceKind* out);
const char* ScenePropFaceToString(ScenePropFace face);

#endif
