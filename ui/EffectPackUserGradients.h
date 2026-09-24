// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include "EffectPacks/EffectPack.h"
#include "filesystem.h"
#include <QPixmap>
#include <QString>
#include <vector>

namespace EffectPackUserGradients
{

struct Entry
{
    QString id;
    QString label;
    std::vector<EffectPack::GradientStop> stops;
};

std::vector<Entry> Load(const filesystem::path& path);
std::vector<Entry> Visible(const filesystem::path& path);
std::vector<Entry> Hidden(const filesystem::path& path);
bool Save(const filesystem::path& path, const std::vector<Entry>& entries);
bool Replace(const filesystem::path& path, const QString& id, const QString& label, const std::vector<EffectPack::GradientStop>& stops);
bool Remove(const filesystem::path& path, const QString& id);
bool Reset(const filesystem::path& path, const QString& id);
bool IsBuiltin(const QString& id);
bool IsCustomized(const filesystem::path& path, const QString& id);
bool Apply(EffectPack::Block* block, const QString& preset_id, const filesystem::path& path);
QPixmap Preview(const std::vector<EffectPack::GradientStop>& stops, int w = 36, int h = 14);
QString MakeId(const QString& label);

} // namespace EffectPackUserGradients
