// SPDX-License-Identifier: GPL-2.0-only

#ifndef SPATIALSHADERCATALOG_H
#define SPATIALSHADERCATALOG_H

#include <QList>
#include <QString>

namespace SpatialShaderCatalog
{

QString UserShadersFolderPath();
bool EnsureUserShadersFolder();
struct EffectShaderPattern
{
    QString name;
    QString tip;
};

QString LoadEffectShader(const QString& id);
QList<EffectShaderPattern> EffectShaderPatterns(const QString& id);

} // namespace SpatialShaderCatalog

#endif
