// SPDX-License-Identifier: GPL-2.0-only

#include "ControllerDisplayUtils.h"

#include "RGBController/RGBController.h"

namespace ControllerDisplay
{

QString FromStd(const std::string& text)
{
    return QString::fromStdString(text).trimmed();
}

bool IsGenericDeviceName(const QString& name, device_type type)
{
    if(name.isEmpty())
    {
        return true;
    }

    const QString type_label = QString::fromStdString(device_type_to_str(type));
    if(name.compare(type_label, Qt::CaseInsensitive) == 0)
    {
        return true;
    }

    static const char* kGenericLabels[] = {
        "Cooler",
        "Fan",
        "Mouse",
        "Keyboard",
        "GPU",
        "Motherboard",
        "DRAM",
        "LED Strip",
        "Mousemat",
        "Headset",
        "Light",
        "Speaker",
        "Case",
        "Storage",
        "Virtual",
    };

    for(const char* label : kGenericLabels)
    {
        if(name.compare(QString::fromUtf8(label), Qt::CaseInsensitive) == 0)
        {
            return true;
        }
    }

    return false;
}

QString FormatRgbControllerTitle(RGBController* controller)
{
    if(!controller)
    {
        return QString();
    }

    const QString name = FromStd(controller->GetName());
    if(name.isEmpty() || !IsGenericDeviceName(name, controller->type))
    {
        return name;
    }

    const QString description = FromStd(controller->GetDescription());
    if(!description.isEmpty())
    {
        return description;
    }

    const QString vendor = FromStd(controller->GetVendor());
    if(!vendor.isEmpty())
    {
        return vendor;
    }

    return name;
}

} // namespace ControllerDisplay
