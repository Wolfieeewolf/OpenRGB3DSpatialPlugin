// SPDX-License-Identifier: GPL-2.0-only

#ifndef SPATIALSHADERCATALOG_H
#define SPATIALSHADERCATALOG_H

#include <QString>
#include <vector>

namespace SpatialShaderCatalog
{

std::vector<QString> ListPresetPaths();
QString UserShadersFolderPath();
bool EnsureUserShadersFolder();

} // namespace SpatialShaderCatalog

#endif
