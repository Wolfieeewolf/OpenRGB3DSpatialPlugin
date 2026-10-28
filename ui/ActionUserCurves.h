// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include "Action.h"
#include "filesystem.h"
#include <QString>
#include <vector>

namespace ActionUserCurves
{

struct Entry
{
    QString id;
    QString label;
    std::vector<Action::CurvePoint> points;
};

std::vector<Entry> Load(const filesystem::path& path);
bool Apply(Action::Clip* clip, const QString& preset_id, const filesystem::path& path);
/** Insert or replace a named curve preset and write curves.json (create-on-save). */
bool Upsert(const filesystem::path& path, const Entry& entry);

} // namespace ActionUserCurves
