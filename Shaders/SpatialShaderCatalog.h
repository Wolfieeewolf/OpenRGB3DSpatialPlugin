// SPDX-License-Identifier: GPL-2.0-only

#ifndef SPATIALSHADERCATALOG_H
#define SPATIALSHADERCATALOG_H

#include <QString>
#include <vector>

namespace SpatialShaderCatalog
{

std::vector<QString> ListPresetPaths();
/** Friendly UI name for a preset path (falls back to filename). */
QString PresetDisplayName(const QString& path);
QString UserShadersFolderPath();
bool EnsureUserShadersFolder();

} // namespace SpatialShaderCatalog

#endif
