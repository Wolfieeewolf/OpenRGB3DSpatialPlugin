/*---------------------------------------------------------*\
| OpenRGB3DSpatialPlugin.cpp                                |
|                                                           |
|   OpenRGB 3D Spatial LED Control System Plugin           |
|                                                           |
|   Date: 2025-09-23                                        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#include "OpenRGB3DSpatialPlugin.h"
#include "LogManager.h"

ResourceManagerInterface* OpenRGB3DSpatialPlugin::RMPointer = nullptr;

OpenRGBPluginInfo OpenRGB3DSpatialPlugin::GetPluginInfo()
{
    OpenRGBPluginInfo info;

    info.Name           = "OpenRGB 3D Spatial LED Control";
    info.Description    = "Organize and control RGB devices in a 3D grid with spatial effects";
    info.Version        = VERSION_STRING;
    info.Commit         = GIT_COMMIT_ID;
    info.URL            = "https://gitlab.com/OpenRGBDevelopers/OpenRGB3DSpatialPlugin";

    info.Label          = "3D Spatial";
    info.Location       = OPENRGB_PLUGIN_LOCATION_TOP;


    return info;
}

unsigned int OpenRGB3DSpatialPlugin::GetPluginAPIVersion()
{
    return OPENRGB_PLUGIN_API_VERSION;
}

void OpenRGB3DSpatialPlugin::Load(ResourceManagerInterface* RM)
{
    RMPointer = RM;

    LOG_INFO("[OpenRGB3DSpatialPlugin] ========== PLUGIN LOAD CALLED ==========");
    LOG_WARNING("[OpenRGB3DSpatialPlugin] ========== PLUGIN LOAD WARNING TEST ==========");
    LOG_ERROR("[OpenRGB3DSpatialPlugin] ========== PLUGIN LOAD ERROR TEST ==========");
}

QWidget* OpenRGB3DSpatialPlugin::GetWidget()
{
    LOG_INFO("[OpenRGB3DSpatialPlugin] ========== GET WIDGET CALLED ==========");

    RMPointer->WaitForDeviceDetection();

    LOG_INFO("[OpenRGB3DSpatialPlugin] Creating OpenRGB3DSpatialTab...");
    ui = new OpenRGB3DSpatialTab(RMPointer);
    LOG_INFO("[OpenRGB3DSpatialPlugin] OpenRGB3DSpatialTab created");

    ui->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    RMPointer->RegisterDeviceListChangeCallback(DeviceListChangedCallback, ui);

    LOG_INFO("[OpenRGB3DSpatialPlugin] Returning UI widget");
    return ui;
}

QMenu* OpenRGB3DSpatialPlugin::GetTrayMenu()
{
    return nullptr;
}

void OpenRGB3DSpatialPlugin::Unload()
{
    RMPointer->UnregisterDeviceListChangeCallback(DeviceListChangedCallback, ui);

    LOG_INFO("[OpenRGB3DSpatialPlugin] Plugin unloaded");
}

void OpenRGB3DSpatialPlugin::DeviceListChangedCallback(void* o)
{
    QMetaObject::invokeMethod((OpenRGB3DSpatialTab*)o, "UpdateDeviceList", Qt::QueuedConnection);
}