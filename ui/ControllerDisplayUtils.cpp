// SPDX-License-Identifier: GPL-2.0-only

#include "ControllerDisplayUtils.h"

#include "LEDPosition3D.h"
#include "RGBController/RGBController.h"
#include "VirtualController3D.h"

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

QString FormatControllerTransformLabel(const ControllerTransform* ctrl, int index)
{
    if(!ctrl)
    {
        return QStringLiteral("Controller %1").arg(index);
    }

    if(ctrl->virtual_controller)
    {
        const QString custom_name = FromStd(ctrl->virtual_controller->GetName());
        if(!custom_name.isEmpty())
        {
            return QStringLiteral("[Custom] ") + custom_name;
        }
    }

    if(ctrl->controller)
    {
        QString name = FormatRgbControllerTitle(ctrl->controller);
        if(ctrl->granularity == 1 && ctrl->item_idx >= 0 &&
           ctrl->item_idx < static_cast<int>(ctrl->controller->zones.size()))
        {
            name += QStringLiteral(" — ") + FromStd(ctrl->controller->GetZoneName(static_cast<unsigned int>(ctrl->item_idx)));
        }
        if(!name.isEmpty())
        {
            return name;
        }
    }

    return QStringLiteral("Controller %1").arg(index);
}

} // namespace ControllerDisplay
