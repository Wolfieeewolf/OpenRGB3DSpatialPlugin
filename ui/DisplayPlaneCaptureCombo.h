// SPDX-License-Identifier: GPL-2.0-only

#ifndef DISPLAYPLANE_CAPTURE_COMBO_H
#define DISPLAYPLANE_CAPTURE_COMBO_H

#include "DisplayPlaneCaptureLabels.h"

#include <string>
#include <vector>

class QComboBox;
class DisplayPlane3D;

enum class CaptureComboEmptyPolicy
{
    SelectNone,
    SelectSuggested
};

void FillDisplayPlaneCaptureCombo(QComboBox* combo,
                                  const std::string& prefer_source_id,
                                  const std::vector<std::string>& used_ids = {},
                                  CaptureComboEmptyPolicy empty_policy = CaptureComboEmptyPolicy::SelectNone);

std::string CaptureComboSourceId(const QComboBox* combo);
std::string CaptureComboSourceLabel(const QComboBox* combo);
void ApplyCaptureComboToPlane(DisplayPlane3D* plane, const QComboBox* combo);

#endif
