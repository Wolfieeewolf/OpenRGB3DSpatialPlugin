// SPDX-License-Identifier: GPL-2.0-only
#ifndef MESHANIM_FASTGLTF_H
#define MESHANIM_FASTGLTF_H

#include "MeshAnim.h"

#include <string>

namespace MeshAnim
{
/** glTF/GLB via fastgltf. Returns false on parse/convert failure (caller may Assimp-fallback). */
bool LoadFastgltf(const std::string& path, Scene* out, std::string* error_out = nullptr);

bool PathLooksLikeGltf(const std::string& path);
}

#endif
