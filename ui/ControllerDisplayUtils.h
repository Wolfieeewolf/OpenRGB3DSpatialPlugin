// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include <QString>
#include <string>

#include "RGBController/RGBController.h"

struct ControllerTransform;

namespace ControllerDisplay
{
QString FromStd(const std::string& text);
bool  IsGenericDeviceName(const QString& name, device_type type);

/** Device name for UI lists; uses OpenRGB GetName(), with description/vendor only when the name is generic. */
QString FormatRgbControllerTitle(RGBControllerInterface* controller);

/** Label for a layout controller row (OpenRGB device, custom virtual, or fallback index). */
QString FormatControllerTransformLabel(const ControllerTransform* ctrl, int index);
}
