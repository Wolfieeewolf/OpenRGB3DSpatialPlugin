// SPDX-License-Identifier: GPL-2.0-only

#include "OpenRGB3DSpatialPlugin.h"

ResourceManagerInterface* OpenRGB3DSpatialPlugin::RMPointer = nullptr;

OpenRGBPluginInfo OpenRGB3DSpatialPlugin::GetPluginInfo()
{
    OpenRGBPluginInfo info;

    info.Name           = "OpenRGB 3D Spatial LED Control";
    info.Description    = "Organize and control RGB devices in a 3D grid with spatial effects";
    info.Version        = VERSION_STRING;
    info.Commit         = GIT_COMMIT_ID;
    info.URL            = "https://github.com/OpenRGBDevelopers/OpenRGB3DSpatialPlugin";

    info.Label          = "Spatial";
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
    ui        = nullptr;
    game_telemetry_bridge = std::make_unique<GameTelemetryBridge>();

    /* UDP telemetry must start whenever the plugin loads; RM can be null in some call orders. */
    if(game_telemetry_bridge)
    {
        game_telemetry_bridge->Register(RMPointer);
    }
}

QWidget* OpenRGB3DSpatialPlugin::GetWidget()
{
    if(!RMPointer)
    {
        return nullptr;
    }

    RMPointer->WaitForDeviceDetection();

    ui = new OpenRGB3DSpatialTab(RMPointer);

    ui->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    RMPointer->RegisterDeviceListChangeCallback(DeviceListChangedCallback, ui);
    RMPointer->RegisterDetectionProgressCallback(DetectionProgressCallback, ui);

    return ui;
}

QMenu* OpenRGB3DSpatialPlugin::GetTrayMenu()
{
    return nullptr;
}

void OpenRGB3DSpatialPlugin::Unload()
{
    if(RMPointer && game_telemetry_bridge)
    {
        game_telemetry_bridge->Unregister(RMPointer);
    }
    game_telemetry_bridge.reset();

    if(RMPointer && ui != nullptr)
    {
        RMPointer->UnregisterDeviceListChangeCallback(DeviceListChangedCallback, ui);
        RMPointer->UnregisterDetectionProgressCallback(DetectionProgressCallback, ui);
    }

    ui = nullptr;
}

void OpenRGB3DSpatialPlugin::DeviceListChangedCallback(void* o)
{
    if(!o || !RMPointer)
    {
        return;
    }

    if(RMPointer->GetDetectionPercent() != 100)
    {
        return;
    }

    QMetaObject::invokeMethod((OpenRGB3DSpatialTab*)o, "UpdateDeviceList", Qt::QueuedConnection);
}

void OpenRGB3DSpatialPlugin::DetectionProgressCallback(void* o)
{
    if(!o || !RMPointer)
    {
        return;
    }

    if(RMPointer->GetDetectionPercent() != 100)
    {
        return;
    }

    QMetaObject::invokeMethod((OpenRGB3DSpatialTab*)o, "UpdateDeviceList", Qt::QueuedConnection);
}
