// SPDX-License-Identifier: GPL-2.0-only

#include "MeshAnim.h"

#include "filesystem.h"

#include <assimp/Importer.hpp>
#include <assimp/GltfMaterial.h>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <QFile>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QByteArray>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <system_error>
#include <unordered_map>

namespace MeshAnim
{
namespace
{
constexpr float kEps = 1e-5f;

Mat4 IdentityMat()
{
    return Mat4{};
}

Mat4 Mul(const Mat4& a, const Mat4& b)
{
    Mat4 out;
    for(int col = 0; col < 4; ++col)
    {
        for(int row = 0; row < 4; ++row)
        {
            out.m[col * 4 + row] =
                a.m[0 * 4 + row] * b.m[col * 4 + 0]
              + a.m[1 * 4 + row] * b.m[col * 4 + 1]
              + a.m[2 * 4 + row] * b.m[col * 4 + 2]
              + a.m[3 * 4 + row] * b.m[col * 4 + 3];
        }
    }
    return out;
}

Mat4 FromTRS(const NodeTRS& trs)
{
    const float x = trs.rotation.x;
    const float y = trs.rotation.y;
    const float z = trs.rotation.z;
    const float w = trs.rotation.w;
    const float x2 = x + x;
    const float y2 = y + y;
    const float z2 = z + z;
    const float xx = x * x2;
    const float xy = x * y2;
    const float xz = x * z2;
    const float yy = y * y2;
    const float yz = y * z2;
    const float zz = z * z2;
    const float wx = w * x2;
    const float wy = w * y2;
    const float wz = w * z2;

    Mat4 r = IdentityMat();
    r.m[0] = 1.0f - (yy + zz);
    r.m[1] = xy + wz;
    r.m[2] = xz - wy;
    r.m[4] = xy - wz;
    r.m[5] = 1.0f - (xx + zz);
    r.m[6] = yz + wx;
    r.m[8] = xz + wy;
    r.m[9] = yz - wx;
    r.m[10] = 1.0f - (xx + yy);

    Mat4 s = IdentityMat();
    s.m[0] = trs.scale.x;
    s.m[5] = trs.scale.y;
    s.m[10] = trs.scale.z;

    Mat4 t = IdentityMat();
    t.m[12] = trs.translation.x;
    t.m[13] = trs.translation.y;
    t.m[14] = trs.translation.z;

    return Mul(t, Mul(r, s));
}

NodeTRS DecomposeAssimp(const aiMatrix4x4& m)
{
    aiVector3D scaling;
    aiQuaternion rotation;
    aiVector3D position;
    m.Decompose(scaling, rotation, position);
    NodeTRS trs;
    trs.translation = {position.x, position.y, position.z};
    trs.rotation = {rotation.x, rotation.y, rotation.z, rotation.w};
    trs.scale = {scaling.x, scaling.y, scaling.z};
    return trs;
}

} // namespace (closed early so TransformPoint can be public — reopened below)

void TransformPoint(const Mat4& m, float x, float y, float z, float* ox, float* oy, float* oz)
{
    *ox = m.m[0] * x + m.m[4] * y + m.m[8]  * z + m.m[12];
    *oy = m.m[1] * x + m.m[5] * y + m.m[9]  * z + m.m[13];
    *oz = m.m[2] * x + m.m[6] * y + m.m[10] * z + m.m[14];
}

namespace
{

void AppendLocalTriangles(const aiMesh* mesh, NodeMesh* out)
{
    if(!mesh || !out || !mesh->HasPositions() || mesh->mNumFaces == 0)
    {
        return;
    }
    /* Only store UVs when Assimp has them — fake (0,0) made hasUvs() true and
       sampled the wrong texel corner in the GPU material path. */
    const bool has_uv = mesh->HasTextureCoords(0);
    for(unsigned int f = 0; f < mesh->mNumFaces; ++f)
    {
        const aiFace& face = mesh->mFaces[f];
        if(face.mNumIndices < 3)
        {
            continue;
        }
        for(unsigned int i = 1; i + 1 < face.mNumIndices; ++i)
        {
            const unsigned int idx[3] = {
                face.mIndices[0],
                face.mIndices[i],
                face.mIndices[i + 1],
            };
            for(unsigned int v = 0; v < 3; ++v)
            {
                const aiVector3D& p = mesh->mVertices[idx[v]];
                out->local_positions.push_back(p.x);
                out->local_positions.push_back(p.y);
                out->local_positions.push_back(p.z);
                if(has_uv)
                {
                    const aiVector3D& uv = mesh->mTextureCoords[0][idx[v]];
                    out->local_uvs.push_back(uv.x);
                    out->local_uvs.push_back(uv.y);
                }
            }
        }
    }
}

bool ImageFromQImage(const QImage& src, ImageRGBA* out, const std::string& key)
{
    if(!out || src.isNull())
    {
        return false;
    }
    const QImage img = src.convertToFormat(QImage::Format_RGBA8888);
    if(img.isNull() || img.width() <= 0 || img.height() <= 0)
    {
        return false;
    }
    out->width = img.width();
    out->height = img.height();
    out->source_key = key;
    out->rgba.resize((size_t)out->width * (size_t)out->height * 4u);
    for(int y = 0; y < out->height; ++y)
    {
        /* Flip V so OpenGL-style V=0 is bottom when sampling with (1-v) or keep Assimp UV as-is.
           Store top-down as QImage; SampleImageRGBA treats v=0 as top row (Assimp/glTF often top-left). */
        const uchar* line = img.constScanLine(y);
        std::memcpy(out->rgba.data() + (size_t)y * (size_t)out->width * 4u,
                    line,
                    (size_t)out->width * 4u);
    }
    return true;
}

int FindOrAddImage(Scene* out, const std::string& key, ImageRGBA&& image)
{
    if(!out || key.empty() || image.empty())
    {
        return -1;
    }
    for(size_t i = 0; i < out->images.size(); ++i)
    {
        if(out->images[i].source_key == key)
        {
            return (int)i;
        }
    }
    out->images.push_back(std::move(image));
    return (int)out->images.size() - 1;
}

int LoadTextureRef(const aiScene* scene,
                   const std::string& mesh_path,
                   const aiString& tex_path,
                   Scene* out)
{
    if(!scene || !out || tex_path.length == 0)
    {
        return -1;
    }
    const std::string path = tex_path.C_Str();
    if(path.empty())
    {
        return -1;
    }

    for(size_t i = 0; i < out->images.size(); ++i)
    {
        if(out->images[i].source_key == path)
        {
            return (int)i;
        }
    }

    ImageRGBA image;
    if(!path.empty() && path[0] == '*')
    {
        const int emb = std::atoi(path.c_str() + 1);
        if(emb < 0 || emb >= (int)scene->mNumTextures || !scene->mTextures[emb])
        {
            return -1;
        }
        const aiTexture* tex = scene->mTextures[emb];
        QImage qimg;
        if(tex->mHeight == 0)
        {
            /* Compressed blob (png/jpg). */
            qimg.loadFromData(reinterpret_cast<const uchar*>(tex->pcData), tex->mWidth);
        }
        else
        {
            /* Assimp aiTexel is BGRA — convert to RGBA for ImageRGBA / GL. */
            qimg = QImage((int)tex->mWidth, (int)tex->mHeight, QImage::Format_RGBA8888);
            for(unsigned int y = 0; y < tex->mHeight; ++y)
            {
                const aiTexel* src = tex->pcData + y * tex->mWidth;
                uchar* dst = qimg.scanLine((int)y);
                for(unsigned int x = 0; x < tex->mWidth; ++x)
                {
                    dst[x * 4 + 0] = src[x].r;
                    dst[x * 4 + 1] = src[x].g;
                    dst[x * 4 + 2] = src[x].b;
                    dst[x * 4 + 3] = src[x].a;
                }
            }
        }
        if(!ImageFromQImage(qimg, &image, path))
        {
            return -1;
        }
    }
    else
    {
        filesystem::path full = filesystem::path(mesh_path).parent_path() / path;
        std::error_code ec;
        if(!filesystem::is_regular_file(full, ec))
        {
            full = filesystem::path(path);
        }
        QImage qimg(QString::fromStdString(full.string()));
        if(!ImageFromQImage(qimg, &image, path))
        {
            return -1;
        }
    }
    return FindOrAddImage(out, path, std::move(image));
}

int LoadMaterialTexture(const aiScene* scene,
                        const std::string& mesh_path,
                        const aiMaterial* mat,
                        aiTextureType type,
                        Scene* out)
{
    if(!mat || mat->GetTextureCount(type) == 0)
    {
        return -1;
    }
    aiString tex_path;
    if(AI_SUCCESS != mat->GetTexture(type, 0, &tex_path))
    {
        return -1;
    }
    return LoadTextureRef(scene, mesh_path, tex_path, out);
}

void LoadMaterials(const aiScene* scene, const std::string& mesh_path, Scene* out)
{
    out->materials.clear();
    out->images.clear();
    if(!scene)
    {
        return;
    }
    out->materials.resize(scene->mNumMaterials);
    for(unsigned int mi = 0; mi < scene->mNumMaterials; ++mi)
    {
        const aiMaterial* mat = scene->mMaterials[mi];
        Material& m = out->materials[mi];
        if(!mat)
        {
            continue;
        }
        aiColor4D col(1.0f, 1.0f, 1.0f, 1.0f);
        if(AI_SUCCESS == aiGetMaterialColor(mat, AI_MATKEY_BASE_COLOR, &col)
           || AI_SUCCESS == aiGetMaterialColor(mat, AI_MATKEY_COLOR_DIFFUSE, &col))
        {
            m.base_r = std::clamp(col.r, 0.0f, 1.0f);
            m.base_g = std::clamp(col.g, 0.0f, 1.0f);
            m.base_b = std::clamp(col.b, 0.0f, 1.0f);
            m.base_a = std::clamp(col.a, 0.0f, 1.0f);
        }
        aiColor4D emis(0.0f, 0.0f, 0.0f, 1.0f);
        if(AI_SUCCESS == aiGetMaterialColor(mat, AI_MATKEY_COLOR_EMISSIVE, &emis))
        {
            m.emis_r = std::clamp(emis.r, 0.0f, 1.0f);
            m.emis_g = std::clamp(emis.g, 0.0f, 1.0f);
            m.emis_b = std::clamp(emis.b, 0.0f, 1.0f);
            if(m.emis_strength < 1e-4f)
            {
                m.emis_strength = std::max({m.emis_r, m.emis_g, m.emis_b, 1.0f});
            }
        }
        float emis_str = 0.0f;
        if(AI_SUCCESS == aiGetMaterialFloat(mat, AI_MATKEY_EMISSIVE_INTENSITY, &emis_str))
        {
            m.emis_strength = std::max(0.0f, emis_str);
        }
        if(m.emis_strength < 1e-4f
           && (m.emis_r > 1e-4f || m.emis_g > 1e-4f || m.emis_b > 1e-4f))
        {
            m.emis_strength = 1.0f;
        }

        aiString alpha_mode;
        if(AI_SUCCESS == mat->Get(AI_MATKEY_GLTF_ALPHAMODE, alpha_mode))
        {
            const char* am = alpha_mode.C_Str();
            if(std::strcmp(am, "MASK") == 0)
            {
                m.alpha_mode = AlphaMode::Mask;
            }
            else if(std::strcmp(am, "BLEND") == 0)
            {
                m.alpha_mode = AlphaMode::Blend;
            }
            else
            {
                m.alpha_mode = AlphaMode::Opaque;
            }
        }
        float cutoff = 0.5f;
        if(AI_SUCCESS == mat->Get(AI_MATKEY_GLTF_ALPHACUTOFF, cutoff))
        {
            m.alpha_cutoff = std::clamp(cutoff, 0.0f, 1.0f);
        }
        int twosided = 0;
        if(AI_SUCCESS == mat->Get(AI_MATKEY_TWOSIDED, twosided))
        {
            m.double_sided = twosided != 0;
        }
        float transmission = 0.0f;
        if(AI_SUCCESS == aiGetMaterialFloat(mat, AI_MATKEY_TRANSMISSION_FACTOR, &transmission))
        {
            m.transmission = std::clamp(transmission, 0.0f, 1.0f);
            if(m.transmission > 0.05f && m.alpha_mode == AlphaMode::Opaque)
            {
                m.alpha_mode = AlphaMode::Blend;
            }
        }
        /* glTF defaults: metallic=1, roughness=1 when factors omitted. */
        float metallic = 1.0f;
        if(AI_SUCCESS == aiGetMaterialFloat(mat, AI_MATKEY_METALLIC_FACTOR, &metallic))
        {
            m.metallic = std::clamp(metallic, 0.0f, 1.0f);
        }
        else
        {
            m.metallic = 1.0f;
        }
        float roughness = 1.0f;
        if(AI_SUCCESS == aiGetMaterialFloat(mat, AI_MATKEY_ROUGHNESS_FACTOR, &roughness))
        {
            m.roughness = std::clamp(roughness, 0.04f, 1.0f);
        }
        float specular = 1.0f;
        if(AI_SUCCESS == aiGetMaterialFloat(mat, AI_MATKEY_SPECULAR_FACTOR, &specular))
        {
            m.specular_factor = std::clamp(specular, 0.0f, 1.0f);
        }

        m.base_tex = LoadMaterialTexture(scene, mesh_path, mat, aiTextureType_BASE_COLOR, out);
        if(m.base_tex < 0)
        {
            m.base_tex = LoadMaterialTexture(scene, mesh_path, mat, aiTextureType_DIFFUSE, out);
        }
        m.emis_tex = LoadMaterialTexture(scene, mesh_path, mat, aiTextureType_EMISSIVE, out);
        if(m.emis_tex >= 0 && m.emis_strength < 1e-4f)
        {
            m.emis_strength = 1.0f;
        }
        if(m.emis_tex >= 0 && m.emis_r < 1e-4f && m.emis_g < 1e-4f && m.emis_b < 1e-4f)
        {
            /* Emissive texture with missing factor — treat as white * strength. */
            m.emis_r = m.emis_g = m.emis_b = 1.0f;
        }
        m.normal_tex = LoadMaterialTexture(scene, mesh_path, mat, aiTextureType_NORMALS, out);
        if(m.normal_tex < 0)
        {
            m.normal_tex = LoadMaterialTexture(scene, mesh_path, mat, aiTextureType_HEIGHT, out);
        }
        m.mr_tex = LoadMaterialTexture(scene, mesh_path, mat, aiTextureType_GLTF_METALLIC_ROUGHNESS, out);
        if(m.mr_tex < 0)
        {
            m.mr_tex = LoadMaterialTexture(scene, mesh_path, mat, aiTextureType_DIFFUSE_ROUGHNESS, out);
        }
        if(m.mr_tex < 0)
        {
            m.mr_tex = LoadMaterialTexture(scene, mesh_path, mat, aiTextureType_METALNESS, out);
        }
        m.occlusion_tex = LoadMaterialTexture(scene, mesh_path, mat, aiTextureType_LIGHTMAP, out);
        if(m.occlusion_tex < 0)
        {
            m.occlusion_tex = LoadMaterialTexture(scene, mesh_path, mat, aiTextureType_AMBIENT_OCCLUSION, out);
        }
        float nscale = 1.0f;
        if(AI_SUCCESS == aiGetMaterialFloat(mat, AI_MATKEY_GLTF_TEXTURE_SCALE(aiTextureType_NORMALS, 0), &nscale))
        {
            m.normal_scale = std::max(0.0f, nscale);
        }
        float occ_str = 1.0f;
        if(AI_SUCCESS == aiGetMaterialFloat(mat, AI_MATKEY_GLTF_TEXTURE_STRENGTH(aiTextureType_LIGHTMAP, 0), &occ_str)
           || AI_SUCCESS == aiGetMaterialFloat(mat, AI_MATKEY_GLTF_TEXTURE_STRENGTH(aiTextureType_AMBIENT_OCCLUSION, 0), &occ_str))
        {
            m.occlusion_strength = std::clamp(occ_str, 0.0f, 1.0f);
        }

        /* Do NOT use Assimp AI_MATKEY_UVTRANSFORM — it remaps into TextureTransformProcess
           space and breaks MASK/emissive Sketchfab assets. Raw KHR_texture_transform is
           overlaid from the glTF/GLB JSON after Load (ApplyGltfKhrTextureTransforms). */
        float su = 1.0f;
        if(AI_SUCCESS == aiGetMaterialFloat(mat, AI_MATKEY_GLTF_TEXTURE_SCALE(aiTextureType_BASE_COLOR, 0), &su))
        {
            m.base_uv_scale_u = su;
            m.base_uv_scale_v = su;
        }
        if(AI_SUCCESS == aiGetMaterialFloat(mat, AI_MATKEY_GLTF_TEXTURE_SCALE(aiTextureType_EMISSIVE, 0), &su))
        {
            m.emis_uv_scale_u = su;
            m.emis_uv_scale_v = su;
        }
    }
}

void ReadTexTransform(const QJsonObject& tex_info,
                      float* su, float* sv, float* ou, float* ov, float* rot)
{
    if(!su || !sv || !ou || !ov || !rot)
    {
        return;
    }
    const QJsonObject ext = tex_info.value(QStringLiteral("extensions")).toObject();
    const QJsonObject tt = ext.value(QStringLiteral("KHR_texture_transform")).toObject();
    if(tt.isEmpty())
    {
        return;
    }
    const QJsonArray scale = tt.value(QStringLiteral("scale")).toArray();
    if(scale.size() >= 2)
    {
        *su = (float)scale.at(0).toDouble(1.0);
        *sv = (float)scale.at(1).toDouble(1.0);
    }
    const QJsonArray offset = tt.value(QStringLiteral("offset")).toArray();
    if(offset.size() >= 2)
    {
        *ou = (float)offset.at(0).toDouble(0.0);
        *ov = (float)offset.at(1).toDouble(0.0);
    }
    if(tt.contains(QStringLiteral("rotation")))
    {
        *rot = (float)tt.value(QStringLiteral("rotation")).toDouble(0.0);
    }
}

bool LoadGltfJsonObject(const std::string& path, QJsonObject* root_out)
{
    if(!root_out || path.empty())
    {
        return false;
    }
    const QString qpath = QString::fromStdString(path);
    const QString lower = qpath.toLower();
    QByteArray json_bytes;
    if(lower.endsWith(QStringLiteral(".glb")))
    {
        QFile f(qpath);
        if(!f.open(QIODevice::ReadOnly))
        {
            return false;
        }
        const QByteArray all = f.readAll();
        if(all.size() < 12 || !all.startsWith("glTF"))
        {
            return false;
        }
        const auto u32 = [&](int at) -> quint32 {
            const uchar* p = reinterpret_cast<const uchar*>(all.constData() + at);
            return (quint32)p[0] | ((quint32)p[1] << 8) | ((quint32)p[2] << 16) | ((quint32)p[3] << 24);
        };
        const quint32 length = u32(8);
        int pos = 12;
        while(pos + 8 <= all.size() && (quint32)pos < length)
        {
            const quint32 clen = u32(pos);
            const char* ctype = all.constData() + pos + 4;
            pos += 8;
            if(pos + (int)clen > all.size())
            {
                break;
            }
            if(std::memcmp(ctype, "JSON", 4) == 0)
            {
                json_bytes = all.mid(pos, (int)clen);
                break;
            }
            pos += (int)clen;
        }
    }
    else if(lower.endsWith(QStringLiteral(".gltf")))
    {
        QFile f(qpath);
        if(!f.open(QIODevice::ReadOnly))
        {
            return false;
        }
        json_bytes = f.readAll();
    }
    else
    {
        return false;
    }
    if(json_bytes.isEmpty())
    {
        return false;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(json_bytes);
    if(!doc.isObject())
    {
        return false;
    }
    *root_out = doc.object();
    return true;
}

void ApplyGltfKhrTextureTransforms(const std::string& path, Scene* out)
{
    if(!out || out->materials.empty())
    {
        return;
    }
    QJsonObject root;
    if(!LoadGltfJsonObject(path, &root))
    {
        return;
    }
    const QJsonArray mats = root.value(QStringLiteral("materials")).toArray();
    const int n = std::min((int)out->materials.size(), (int)mats.size());
    for(int i = 0; i < n; ++i)
    {
        const QJsonObject jm = mats.at(i).toObject();
        Material& m = out->materials[(size_t)i];
        const QJsonObject pbr = jm.value(QStringLiteral("pbrMetallicRoughness")).toObject();
        ReadTexTransform(pbr.value(QStringLiteral("baseColorTexture")).toObject(),
                         &m.base_uv_scale_u, &m.base_uv_scale_v,
                         &m.base_uv_offset_u, &m.base_uv_offset_v,
                         &m.base_uv_rotation);
        ReadTexTransform(jm.value(QStringLiteral("emissiveTexture")).toObject(),
                         &m.emis_uv_scale_u, &m.emis_uv_scale_v,
                         &m.emis_uv_offset_u, &m.emis_uv_offset_v,
                         &m.emis_uv_rotation);

        /* Prefer raw glTF emissiveFactor / emissiveStrength when Assimp under-reads. */
        const QJsonArray ef = jm.value(QStringLiteral("emissiveFactor")).toArray();
        if(ef.size() >= 3)
        {
            m.emis_r = std::clamp((float)ef.at(0).toDouble(0.0), 0.0f, 1.0f);
            m.emis_g = std::clamp((float)ef.at(1).toDouble(0.0), 0.0f, 1.0f);
            m.emis_b = std::clamp((float)ef.at(2).toDouble(0.0), 0.0f, 1.0f);
        }
        const QJsonObject mat_ext = jm.value(QStringLiteral("extensions")).toObject();
        const QJsonObject emis_str_ext =
            mat_ext.value(QStringLiteral("KHR_materials_emissive_strength")).toObject();
        if(emis_str_ext.contains(QStringLiteral("emissiveStrength")))
        {
            m.emis_strength = std::max(0.0f, (float)emis_str_ext.value(
                QStringLiteral("emissiveStrength")).toDouble(1.0));
        }
        if(m.emis_tex >= 0 && m.emis_r < 1e-4f && m.emis_g < 1e-4f && m.emis_b < 1e-4f)
        {
            m.emis_r = m.emis_g = m.emis_b = 1.0f;
        }
        if(m.emis_tex >= 0 && m.emis_strength < 1e-4f)
        {
            m.emis_strength = 1.0f;
        }
    }
}

void FlattenNodes(const aiNode* node,
                  int parent_index,
                  std::vector<int>* parent,
                  std::vector<NodeTRS>* bind_local,
                  std::vector<std::string>* node_names,
                  std::vector<const aiNode*>* order,
                  std::unordered_map<std::string, int>* name_to_index)
{
    if(!node || !parent || !bind_local || !node_names || !order || !name_to_index)
    {
        return;
    }
    const int index = (int)order->size();
    order->push_back(node);
    parent->push_back(parent_index);
    bind_local->push_back(DecomposeAssimp(node->mTransformation));
    std::string name = (node->mName.length > 0) ? std::string(node->mName.C_Str()) : std::string{};
    node_names->push_back(name);
    if(!name.empty())
    {
        (*name_to_index)[name] = index;
    }
    for(unsigned int c = 0; c < node->mNumChildren; ++c)
    {
        FlattenNodes(node->mChildren[c], index, parent, bind_local, node_names, order, name_to_index);
    }
}

void ReadMaterialBaseColor(const aiScene* scene, int material_index, float* r, float* g, float* b)
{
    *r = 1.0f;
    *g = 1.0f;
    *b = 1.0f;
    if(!scene || material_index < 0 || material_index >= (int)scene->mNumMaterials)
    {
        return;
    }
    const aiMaterial* mat = scene->mMaterials[material_index];
    if(!mat)
    {
        return;
    }
    aiColor4D col(1.0f, 1.0f, 1.0f, 1.0f);
    if(AI_SUCCESS == aiGetMaterialColor(mat, AI_MATKEY_BASE_COLOR, &col)
       || AI_SUCCESS == aiGetMaterialColor(mat, AI_MATKEY_COLOR_DIFFUSE, &col))
    {
        *r = std::clamp(col.r, 0.0f, 1.0f);
        *g = std::clamp(col.g, 0.0f, 1.0f);
        *b = std::clamp(col.b, 0.0f, 1.0f);
    }
}

bool CollectMeshes(const aiScene* scene,
                   const std::vector<const aiNode*>& order,
                   const std::vector<std::string>& node_names,
                   Scene* out)
{
    size_t tri_total = 0;
    for(size_t ni = 0; ni < order.size(); ++ni)
    {
        const aiNode* node = order[ni];
        for(unsigned int i = 0; i < node->mNumMeshes; ++i)
        {
            const unsigned int mesh_index = node->mMeshes[i];
            if(mesh_index >= scene->mNumMeshes)
            {
                continue;
            }
            const aiMesh* mesh = scene->mMeshes[mesh_index];
            if(!mesh)
            {
                continue;
            }
            NodeMesh nm;
            nm.node_index = (int)ni;
            nm.material_index = (int)mesh->mMaterialIndex;
            if(nm.material_index >= 0 && nm.material_index < (int)out->materials.size())
            {
                const Material& mat = out->materials[(size_t)nm.material_index];
                nm.base_r = mat.base_r;
                nm.base_g = mat.base_g;
                nm.base_b = mat.base_b;
                nm.base_a = mat.base_a;
            }
            else
            {
                ReadMaterialBaseColor(scene, nm.material_index, &nm.base_r, &nm.base_g, &nm.base_b);
                nm.base_a = 1.0f;
            }

            const std::string node_name =
                (ni < node_names.size() && !node_names[ni].empty())
                    ? node_names[ni]
                    : ("node_" + std::to_string(ni));
            const std::string mesh_name =
                (mesh->mName.length > 0)
                    ? std::string(mesh->mName.C_Str())
                    : ("mesh_" + std::to_string(mesh_index));
            nm.part_id = node_name + "/" + mesh_name;
            nm.display_name = mesh_name;
            if(nm.display_name.empty())
            {
                nm.display_name = nm.part_id;
            }

            AppendLocalTriangles(mesh, &nm);
            const size_t added = nm.local_positions.size() / 9;
            if(tri_total + added > MeshImport::kMaxTriangles)
            {
                return false; /* fail closed — no silent partial mesh */
            }
            tri_total += added;
            if(!nm.local_positions.empty())
            {
                out->meshes.push_back(std::move(nm));
            }
        }
    }
    return true;
}

void FillAssetSummary(const aiScene* scene, Scene* out)
{
    out->summary = AssetSummary{};
    if(!scene)
    {
        return;
    }
    out->summary.mesh_count = out->meshes.size();
    out->summary.material_count = out->materials.size();
    out->summary.clip_count = scene->mNumAnimations;
    out->summary.texture_count = out->images.size();
    size_t tris = 0;
    size_t verts = 0;
    for(const NodeMesh& nm : out->meshes)
    {
        tris += nm.local_positions.size() / 9;
        verts += nm.local_positions.size() / 3;
        if(nm.hasUvs())
        {
            out->summary.has_uvs = true;
        }
    }
    out->summary.triangle_count = tris;
    out->summary.vertex_count = verts;
    for(unsigned int i = 0; i < scene->mNumMeshes; ++i)
    {
        const aiMesh* mesh = scene->mMeshes[i];
        if(!mesh)
        {
            continue;
        }
        if(mesh->HasBones())
        {
            out->summary.has_skins = true;
        }
        if(mesh->mNumAnimMeshes > 0)
        {
            out->summary.has_morphs = true;
        }
    }
}

void RecomputeAabb(MeshImport::TriangleMesh* mesh)
{
    if(!mesh || mesh->positions.size() < 3)
    {
        return;
    }
    float min_x = FLT_MAX, min_y = FLT_MAX, min_z = FLT_MAX;
    float max_x = -FLT_MAX, max_y = -FLT_MAX, max_z = -FLT_MAX;
    for(size_t i = 0; i + 2 < mesh->positions.size(); i += 3)
    {
        const float x = mesh->positions[i];
        const float y = mesh->positions[i + 1];
        const float z = mesh->positions[i + 2];
        min_x = std::min(min_x, x); max_x = std::max(max_x, x);
        min_y = std::min(min_y, y); max_y = std::max(max_y, y);
        min_z = std::min(min_z, z); max_z = std::max(max_z, z);
    }
    mesh->aabb_min[0] = min_x; mesh->aabb_min[1] = min_y; mesh->aabb_min[2] = min_z;
    mesh->aabb_max[0] = max_x; mesh->aabb_max[1] = max_y; mesh->aabb_max[2] = max_z;
}

float SampleScalar(const std::vector<float>& times,
                   const std::vector<float>& values,
                   int components,
                   float t,
                   int component,
                   float fallback)
{
    if(times.empty() || values.size() < (size_t)components || component < 0 || component >= components)
    {
        return fallback;
    }
    if(t <= times.front())
    {
        return values[(size_t)component];
    }
    if(t >= times.back())
    {
        const size_t last = times.size() - 1;
        return values[last * (size_t)components + (size_t)component];
    }
    size_t hi = 1;
    while(hi < times.size() && times[hi] < t)
    {
        ++hi;
    }
    const size_t lo = hi - 1;
    const float t0 = times[lo];
    const float t1 = times[hi];
    const float u = (t1 > t0) ? ((t - t0) / (t1 - t0)) : 0.0f;
    const float a = values[lo * (size_t)components + (size_t)component];
    const float b = values[hi * (size_t)components + (size_t)component];
    return a + (b - a) * u;
}

Quat SampleQuat(const QuatTrack& track, float t, const Quat& fallback)
{
    if(track.times.empty() || track.values.size() < 4)
    {
        return fallback;
    }
    if(t <= track.times.front())
    {
        return {track.values[0], track.values[1], track.values[2], track.values[3]};
    }
    if(t >= track.times.back())
    {
        const size_t last = track.times.size() - 1;
        const size_t o = last * 4;
        return {track.values[o], track.values[o + 1], track.values[o + 2], track.values[o + 3]};
    }
    size_t hi = 1;
    while(hi < track.times.size() && track.times[hi] < t)
    {
        ++hi;
    }
    const size_t lo = hi - 1;
    const float t0 = track.times[lo];
    const float t1 = track.times[hi];
    float u = (t1 > t0) ? ((t - t0) / (t1 - t0)) : 0.0f;
    Quat a{track.values[lo * 4], track.values[lo * 4 + 1], track.values[lo * 4 + 2], track.values[lo * 4 + 3]};
    Quat b{track.values[hi * 4], track.values[hi * 4 + 1], track.values[hi * 4 + 2], track.values[hi * 4 + 3]};
    /* Prefer short path. */
    const float dot = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
    if(dot < 0.0f)
    {
        b.x = -b.x; b.y = -b.y; b.z = -b.z; b.w = -b.w;
    }
    Quat out{
        a.x + (b.x - a.x) * u,
        a.y + (b.y - a.y) * u,
        a.z + (b.z - a.z) * u,
        a.w + (b.w - a.w) * u
    };
    const float len = std::sqrt(out.x * out.x + out.y * out.y + out.z * out.z + out.w * out.w);
    if(len > kEps)
    {
        out.x /= len; out.y /= len; out.z /= len; out.w /= len;
    }
    return out;
}

Vec3 SampleVec3(const Vec3Track& track, float t, const Vec3& fallback)
{
    return {
        SampleScalar(track.times, track.values, 3, t, 0, fallback.x),
        SampleScalar(track.times, track.values, 3, t, 1, fallback.y),
        SampleScalar(track.times, track.values, 3, t, 2, fallback.z)
    };
}

void FillVec3Track(const aiVectorKey* keys, unsigned int count, Vec3Track* out)
{
    out->times.clear();
    out->values.clear();
    out->times.reserve(count);
    out->values.reserve(count * 3);
    for(unsigned int i = 0; i < count; ++i)
    {
        out->times.push_back((float)keys[i].mTime);
        out->values.push_back(keys[i].mValue.x);
        out->values.push_back(keys[i].mValue.y);
        out->values.push_back(keys[i].mValue.z);
    }
}

void FillQuatTrack(const aiQuatKey* keys, unsigned int count, QuatTrack* out)
{
    out->times.clear();
    out->values.clear();
    out->times.reserve(count);
    out->values.reserve(count * 4);
    for(unsigned int i = 0; i < count; ++i)
    {
        out->times.push_back((float)keys[i].mTime);
        out->values.push_back(keys[i].mValue.x);
        out->values.push_back(keys[i].mValue.y);
        out->values.push_back(keys[i].mValue.z);
        out->values.push_back(keys[i].mValue.w);
    }
}

void BakeSoup(const Scene& scene, const std::vector<Mat4>& world, MeshImport::TriangleMesh* out)
{
    out->positions.clear();
    size_t total = 0;
    for(const NodeMesh& nm : scene.meshes)
    {
        total += nm.local_positions.size();
    }
    out->positions.reserve(total);
    for(const NodeMesh& nm : scene.meshes)
    {
        if(nm.node_index < 0 || nm.node_index >= (int)world.size())
        {
            continue;
        }
        const Mat4& xform = world[(size_t)nm.node_index];
        for(size_t i = 0; i + 2 < nm.local_positions.size(); i += 3)
        {
            float ox, oy, oz;
            TransformPoint(xform,
                           nm.local_positions[i],
                           nm.local_positions[i + 1],
                           nm.local_positions[i + 2],
                           &ox, &oy, &oz);
            out->positions.push_back(ox);
            out->positions.push_back(oy);
            out->positions.push_back(oz);
        }
    }
    RecomputeAabb(out);
}

void BuildWorld(const Scene& scene, const std::vector<NodeTRS>& local, std::vector<Mat4>* world)
{
    world->assign(local.size(), IdentityMat());
    for(size_t i = 0; i < local.size(); ++i)
    {
        const Mat4 local_m = FromTRS(local[i]);
        const int p = scene.parent[i];
        if(p < 0)
        {
            (*world)[i] = local_m;
        }
        else
        {
            (*world)[i] = Mul((*world)[(size_t)p], local_m);
        }
    }
}

void SampleAnimatedLocals(const Scene& scene, float time_sec, std::vector<NodeTRS>* local_out)
{
    *local_out = scene.bind_local;
    if(!scene.hasAnimation())
    {
        return;
    }
    float t = time_sec;
    if(scene.duration_sec > kEps)
    {
        t = std::fmod(t, scene.duration_sec);
        if(t < 0.0f)
        {
            t += scene.duration_sec;
        }
    }
    for(const NodeClip& clip : scene.clips)
    {
        if(clip.node_index < 0 || clip.node_index >= (int)local_out->size())
        {
            continue;
        }
        NodeTRS& trs = (*local_out)[(size_t)clip.node_index];
        if(!clip.translation.times.empty())
        {
            trs.translation = SampleVec3(clip.translation, t, trs.translation);
        }
        if(!clip.rotation.times.empty())
        {
            trs.rotation = SampleQuat(clip.rotation, t, trs.rotation);
        }
        if(!clip.scale.times.empty())
        {
            trs.scale = SampleVec3(clip.scale, t, trs.scale);
        }
    }
}

void EvaluateNodeWorldsImpl(const Scene& scene, float time_sec, std::vector<Mat4>* out_world)
{
    if(!out_world)
    {
        return;
    }
    std::vector<NodeTRS> local;
    SampleAnimatedLocals(scene, time_sec, &local);
    BuildWorld(scene, local, out_world);
}
} // namespace

void EvaluateNodeWorlds(const Scene& scene, float time_sec, std::vector<Mat4>* out_world)
{
    EvaluateNodeWorldsImpl(scene, time_sec, out_world);
}

bool Load(const std::string& path, Scene* out, std::string* error_out)
{
    if(!out)
    {
        return false;
    }
    *out = Scene{};

    if(MeshImport::IsBlockedExtension(path))
    {
        if(error_out)
        {
            *error_out =
                "Native Blender (.blend) and IFC files are not supported — "
                "export as glTF (.glb) or OBJ first.";
        }
        return false;
    }

    Assimp::Importer importer;
    /* Do NOT use aiProcess_TransformUVCoords — glTF KHR_texture_transform is
       per-texture (base vs emis can differ wildly); bake would smash that. */
    const unsigned int flags =
        aiProcess_Triangulate
      | aiProcess_JoinIdenticalVertices
      | aiProcess_SortByPType;

    const aiScene* scene = importer.ReadFile(path, flags);
    if(!scene || !scene->mRootNode
       || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE)
       || scene->mNumMeshes == 0)
    {
        if(error_out)
        {
            *error_out = importer.GetErrorString();
            if(error_out->empty())
            {
                *error_out = "Assimp failed to load mesh";
            }
        }
        return false;
    }

    std::vector<const aiNode*> order;
    std::unordered_map<std::string, int> name_to_index;
    FlattenNodes(scene->mRootNode, -1, &out->parent, &out->bind_local, &out->node_names,
                 &order, &name_to_index);
    LoadMaterials(scene, path, out);
    ApplyGltfKhrTextureTransforms(path, out);
    if(!CollectMeshes(scene, order, out->node_names, out))
    {
        if(error_out)
        {
            *error_out = "Mesh exceeds triangle budget — simplify or split the asset";
        }
        return false;
    }
    if(out->meshes.empty())
    {
        if(error_out)
        {
            *error_out = "Mesh contained no triangles";
        }
        return false;
    }
    FillAssetSummary(scene, out);

    if(scene->mNumAnimations > 0 && scene->mAnimations[0])
    {
        const aiAnimation* anim = scene->mAnimations[0];
        const double tps = (anim->mTicksPerSecond > 1e-6) ? anim->mTicksPerSecond : 25.0;
        out->duration_sec = (float)(anim->mDuration / tps);

        for(unsigned int c = 0; c < anim->mNumChannels; ++c)
        {
            const aiNodeAnim* channel = anim->mChannels[c];
            if(!channel)
            {
                continue;
            }
            const auto it = name_to_index.find(std::string(channel->mNodeName.C_Str()));
            if(it == name_to_index.end())
            {
                continue;
            }
            NodeClip clip;
            clip.node_index = it->second;
            FillVec3Track(channel->mPositionKeys, channel->mNumPositionKeys, &clip.translation);
            FillQuatTrack(channel->mRotationKeys, channel->mNumRotationKeys, &clip.rotation);
            FillVec3Track(channel->mScalingKeys, channel->mNumScalingKeys, &clip.scale);
            /* Convert key times from ticks → seconds. */
            const float inv_tps = 1.0f / (float)tps;
            for(float& tm : clip.translation.times) { tm *= inv_tps; }
            for(float& tm : clip.rotation.times) { tm *= inv_tps; }
            for(float& tm : clip.scale.times) { tm *= inv_tps; }
            out->clips.push_back(std::move(clip));
        }
    }

    std::vector<Mat4> world;
    BuildWorld(*out, out->bind_local, &world);
    BakeSoup(*out, world, &out->bind_pose);
    if(out->bind_pose.empty())
    {
        if(error_out)
        {
            *error_out = "Mesh contained no triangles";
        }
        return false;
    }
    return true;
}

void SampleImageRGBA(const ImageRGBA& image, float u, float v,
                     float* r, float* g, float* b, float* a)
{
    /* Missing/empty → black (not white) so Soft glow doesn't wash the room. */
    if(r) { *r = 0.0f; }
    if(g) { *g = 0.0f; }
    if(b) { *b = 0.0f; }
    if(a) { *a = 0.0f; }
    if(image.empty())
    {
        return;
    }
    u = u - std::floor(u);
    v = v - std::floor(v);
    if(u < 0.0f) { u += 1.0f; }
    if(v < 0.0f) { v += 1.0f; }

    const float fx = u * (float)(image.width - 1);
    /* Callers pass glTF UVs (V=0 top) after ApplyGltfUvXform. ImageRGBA is top-down. */
    const float fy = v * (float)(image.height - 1);
    const int x0 = std::clamp((int)std::floor(fx), 0, image.width - 1);
    const int y0 = std::clamp((int)std::floor(fy), 0, image.height - 1);
    const int x1 = std::min(x0 + 1, image.width - 1);
    const int y1 = std::min(y0 + 1, image.height - 1);
    const float tx = fx - (float)x0;
    const float ty = fy - (float)y0;

    auto fetch = [&](int x, int y, float* cr, float* cg, float* cb, float* ca) {
        const size_t i = ((size_t)y * (size_t)image.width + (size_t)x) * 4u;
        *cr = image.rgba[i] / 255.0f;
        *cg = image.rgba[i + 1] / 255.0f;
        *cb = image.rgba[i + 2] / 255.0f;
        *ca = image.rgba[i + 3] / 255.0f;
    };
    float r00, g00, b00, a00, r10, g10, b10, a10, r01, g01, b01, a01, r11, g11, b11, a11;
    fetch(x0, y0, &r00, &g00, &b00, &a00);
    fetch(x1, y0, &r10, &g10, &b10, &a10);
    fetch(x0, y1, &r01, &g01, &b01, &a01);
    fetch(x1, y1, &r11, &g11, &b11, &a11);
    const float r0 = r00 + (r10 - r00) * tx;
    const float r1 = r01 + (r11 - r01) * tx;
    const float g0 = g00 + (g10 - g00) * tx;
    const float g1 = g01 + (g11 - g01) * tx;
    const float b0 = b00 + (b10 - b00) * tx;
    const float b1 = b01 + (b11 - b01) * tx;
    const float a0 = a00 + (a10 - a00) * tx;
    const float a1 = a01 + (a11 - a01) * tx;
    if(r) { *r = r0 + (r1 - r0) * ty; }
    if(g) { *g = g0 + (g1 - g0) * ty; }
    if(b) { *b = b0 + (b1 - b0) * ty; }
    if(a) { *a = a0 + (a1 - a0) * ty; }
}

void ApplyGltfUvXform(float u, float v,
                      float scale_u, float scale_v,
                      float offset_u, float offset_v,
                      float rotation_rad,
                      float* ou, float* ov)
{
    /* Assimp UV → glTF UV, KHR transform, leave as glTF (V=0 top) for SampleImageRGBA. */
    float gx = u;
    float gy = 1.0f - v;
    gx *= scale_u;
    gy *= scale_v;
    const float c = std::cos(rotation_rad);
    const float s = std::sin(rotation_rad);
    const float rx = c * gx - s * gy;
    const float ry = s * gx + c * gy;
    gx = rx + offset_u;
    gy = ry + offset_v;
    if(ou) { *ou = gx; }
    if(ov) { *ov = gy; }
}

void SampleMaterialBase(const Scene& scene, int material_index, float u, float v,
                        float* r, float* g, float* b)
{
    float br = 1.0f, bg = 1.0f, bb = 1.0f;
    if(material_index >= 0 && material_index < (int)scene.materials.size())
    {
        const Material& mat = scene.materials[(size_t)material_index];
        br = mat.base_r;
        bg = mat.base_g;
        bb = mat.base_b;
        if(mat.base_tex >= 0 && mat.base_tex < (int)scene.images.size())
        {
            float su = u, sv = v;
            ApplyGltfUvXform(u, v,
                             mat.base_uv_scale_u, mat.base_uv_scale_v,
                             mat.base_uv_offset_u, mat.base_uv_offset_v,
                             mat.base_uv_rotation, &su, &sv);
            float tr, tg, tb;
            SampleImageRGBA(scene.images[(size_t)mat.base_tex], su, sv, &tr, &tg, &tb, nullptr);
            br *= tr;
            bg *= tg;
            bb *= tb;
        }
    }
    if(r) { *r = std::clamp(br, 0.0f, 1.0f); }
    if(g) { *g = std::clamp(bg, 0.0f, 1.0f); }
    if(b) { *b = std::clamp(bb, 0.0f, 1.0f); }
}

void SampleMaterialEmissive(const Scene& scene, int material_index, float u, float v,
                            float* r, float* g, float* b)
{
    float er = 0.0f, eg = 0.0f, eb = 0.0f;
    if(material_index >= 0 && material_index < (int)scene.materials.size())
    {
        const Material& mat = scene.materials[(size_t)material_index];
        const float s = std::max(0.0f, mat.emis_strength);
        if(mat.emis_tex >= 0 && mat.emis_tex < (int)scene.images.size())
        {
            float su = u, sv = v;
            ApplyGltfUvXform(u, v,
                             mat.emis_uv_scale_u, mat.emis_uv_scale_v,
                             mat.emis_uv_offset_u, mat.emis_uv_offset_v,
                             mat.emis_uv_rotation, &su, &sv);
            float tr, tg, tb;
            SampleImageRGBA(scene.images[(size_t)mat.emis_tex], su, sv, &tr, &tg, &tb, nullptr);
            er = tr * mat.emis_r * s;
            eg = tg * mat.emis_g * s;
            eb = tb * mat.emis_b * s;
        }
        else if(s > 1e-4f)
        {
            er = mat.emis_r * s;
            eg = mat.emis_g * s;
            eb = mat.emis_b * s;
        }
    }
    /* Allow HDR (Sketchfab emissiveStrength >> 1); Soft glow tonemaps later. */
    if(r) { *r = std::max(0.0f, er); }
    if(g) { *g = std::max(0.0f, eg); }
    if(b) { *b = std::max(0.0f, eb); }
}

void SampleMaterialViewportLook(const Scene& scene, int material_index, float u, float v,
                                float bright, float* r, float* g, float* b)
{
    float ar = 1.0f, ag = 1.0f, ab = 1.0f;
    float er = 0.0f, eg = 0.0f, eb = 0.0f;
    float metal = 1.0f;
    float emis_str = 0.0f;
    SampleMaterialBase(scene, material_index, u, v, &ar, &ag, &ab);
    SampleMaterialEmissive(scene, material_index, u, v, &er, &eg, &eb);
    if(material_index >= 0 && material_index < (int)scene.materials.size())
    {
        const Material& mat = scene.materials[(size_t)material_index];
        metal = std::clamp(mat.metallic, 0.0f, 1.0f);
        emis_str = std::max(0.0f, mat.emis_strength);
    }

    /* Neutral exposure for LED bake — Soft glow applies Brightness/Emissive after. */
    const float br = std::max(bright, 0.35f);
    /* Match MeshUnlit: soft lit albedo + HDR emis, emis-dominant mix. */
    const float shade = 0.72f;
    float lr = ar * (1.0f - metal * 0.55f) * shade * br;
    float lg = ag * (1.0f - metal * 0.55f) * shade * br;
    float lb = ab * (1.0f - metal * 0.55f) * shade * br;

    const float eres = br * (1.0f + emis_str * 0.85f);
    float ergb_r = er * eres;
    float ergb_g = eg * eres;
    float ergb_b = eb * eres;

    const float a_lum = std::max(ar, std::max(ag, ab));
    const float e_lum = std::max(ergb_r, std::max(ergb_g, ergb_b));
    const float emis_dom = std::clamp(e_lum / std::max(a_lum + e_lum, 1e-3f), 0.0f, 1.0f);
    if(e_lum > 1e-4f || emis_str > 0.5f)
    {
        lr = lr * (1.0f - emis_dom * 0.88f) + ergb_r * (emis_dom * 0.88f);
        lg = lg * (1.0f - emis_dom * 0.88f) + ergb_g * (emis_dom * 0.88f);
        lb = lb * (1.0f - emis_dom * 0.88f) + ergb_b * (emis_dom * 0.88f);
    }
    const float add = 0.65f + 0.55f * emis_dom;
    float rr = lr + ergb_r * add;
    float rg = lg + ergb_g * add;
    float rb = lb + ergb_b * add;

    /* Peak tonemap keeps neon chromaticity (luma scale + clamp → white soup). */
    const float peak = std::max(rr, std::max(rg, rb));
    if(peak > 1e-6f)
    {
        const float mapped = peak / (1.0f + peak * 0.35f);
        const float s = mapped / peak;
        rr *= s;
        rg *= s;
        rb *= s;
    }

    if(r) { *r = std::clamp(rr, 0.0f, 1.0f); }
    if(g) { *g = std::clamp(rg, 0.0f, 1.0f); }
    if(b) { *b = std::clamp(rb, 0.0f, 1.0f); }
}

Cache* Cache::instance()
{
    static Cache inst;
    return &inst;
}

namespace
{
/* Bump when material/UV/loader semantics change so same path re-parses. */
constexpr int kMeshAnimLoaderFormat = 7;

std::string CacheKey(const std::string& absolute_path)
{
    return absolute_path + "#v" + std::to_string(kMeshAnimLoaderFormat);
}
} // namespace

const Scene* Cache::GetOrLoad(const std::string& absolute_path, std::string* error_out)
{
    if(absolute_path.empty())
    {
        if(error_out)
        {
            *error_out = "Empty mesh path";
        }
        return nullptr;
    }

    const std::string key = CacheKey(absolute_path);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = cache_.find(key);
        if(it != cache_.end())
        {
            return it->second.get();
        }
    }

    auto scene = std::make_shared<Scene>();
    std::string err;
    if(!Load(absolute_path, scene.get(), &err))
    {
        if(error_out)
        {
            *error_out = err;
        }
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    auto [it, inserted] = cache_.emplace(key, std::move(scene));
    (void)inserted;
    return it->second.get();
}

void Cache::Invalidate(const std::string& absolute_path)
{
    std::lock_guard<std::mutex> lock(mutex_);
    cache_.erase(CacheKey(absolute_path));
    /* Also drop any legacy unversioned key from older builds in-process. */
    cache_.erase(absolute_path);
}

void Cache::Clear()
{
    std::lock_guard<std::mutex> lock(mutex_);
    cache_.clear();
}
} // namespace MeshAnim
