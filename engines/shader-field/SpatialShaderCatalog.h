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

/** Strip `#` / `key: value` header lines from a `.fs` file; returns GLSL body. */
QString LoadShaderBodyFromPath(const QString& path);
QString LoadEffectShader(const QString& id);
QList<EffectShaderPattern> EffectShaderPatterns(const QString& id);

/** Defaults for header params beyond the four engine params (zoom/contrast/hue/detail),
 *  in `param:` order, from each param's `slider:` default (pct / unit → /100). These feed
 *  u_params[4..7] so a look renders as authored, the same header contract Volume uses. */
QList<float> LoadExtraParamDefaults(const QString& path);

/** One on-screen slider for a header param beyond the four engine params. */
struct ExtraParamSlider
{
    QString name;     /* param / slider key, used to save the value */
    QString label;
    QString tip;
    int min = 0;
    int max = 100;
    int def = 0;
    bool pct = false; /* pct / unit → value / 100 in u_params */
};
/** Sliders for u_params[4..7] in `param:` order (params with no `slider:` line get 0..100 / 0). */
QList<ExtraParamSlider> LoadExtraParamSliders(const QString& path);

} // namespace SpatialShaderCatalog

#endif
