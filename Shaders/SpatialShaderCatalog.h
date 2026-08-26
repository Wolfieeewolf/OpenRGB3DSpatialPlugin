// SPDX-License-Identifier: GPL-2.0-only

#ifndef SPATIALSHADERCATALOG_H
#define SPATIALSHADERCATALOG_H

#include <QString>

namespace SpatialShaderCatalog
{

QString UserShadersFolderPath();
bool EnsureUserShadersFolder();

} // namespace SpatialShaderCatalog

#endif
