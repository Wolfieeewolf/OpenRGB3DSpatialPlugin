// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include "EffectPacks/EffectPack.h"
#include "filesystem.h"
#include <QString>
#include <vector>

namespace EffectPackUserCurves
{

struct Entry
{
    QString id;
    QString label;
    std::vector<EffectPack::CurvePoint> points;
};

std::vector<Entry> Load(const filesystem::path& path);
bool Apply(EffectPack::Block* block, const QString& preset_id, const filesystem::path& path);
/** Insert or replace a named curve preset and write user-curves.json. */
bool Upsert(const filesystem::path& path, const Entry& entry);

} // namespace EffectPackUserCurves
