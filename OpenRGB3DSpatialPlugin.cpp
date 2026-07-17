// SPDX-License-Identifier: GPL-2.0-only

#include "OpenRGB3DSpatialPlugin.h"
#include "OpenRGB3DSpatialTab.h"
#include "Game/GameTelemetryBridge.h"
#include "ResourceManagerCallback.h"

OpenRGBPluginAPIInterface* OpenRGB3DSpatialPlugin::APIPointer = nullptr;

/*-------------------------------------------------------------*\
| Global used by PluginLog.h to route LOG_* through the host.   |
\*-------------------------------------------------------------*/
OpenRGBPluginAPIInterface* g_3dspatial_plugin_api = nullptr;

OpenRGB3DSpatialPlugin::OpenRGB3DSpatialPlugin() = default;

OpenRGB3DSpatialPlugin::~OpenRGB3DSpatialPlugin() = default;

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

void OpenRGB3DSpatialPlugin::Load(OpenRGBPluginAPIInterface* plugin_api_ptr)
{
    APIPointer            = plugin_api_ptr;
    g_3dspatial_plugin_api = plugin_api_ptr;
    ui                    = nullptr;
    if(!game_telemetry_bridge)
    {
        game_telemetry_bridge = std::make_unique<GameTelemetryBridge>();
    }
    game_telemetry_bridge->Register(APIPointer);
}

QWidget* OpenRGB3DSpatialPlugin::GetWidget()
{
    if(!APIPointer)
    {
        return nullptr;
    }

    ui = new OpenRGB3DSpatialTab(APIPointer);

    ui->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    return ui;
}

QMenu* OpenRGB3DSpatialPlugin::GetTrayMenu()
{
    return nullptr;
}

void OpenRGB3DSpatialPlugin::Unload()
{
    if(game_telemetry_bridge)
    {
        game_telemetry_bridge->Unregister(APIPointer);
        game_telemetry_bridge.reset();
    }

    if(APIPointer && ui != nullptr)
    {
        ui->SavePluginUiSettings();
    }

    ui = nullptr;
}

void OpenRGB3DSpatialPlugin::OnProfileAboutToLoad()
{
}

void OpenRGB3DSpatialPlugin::OnProfileLoad(nlohmann::json profile_data)
{
    if(ui != nullptr)
    {
        ui->OnProfileLoad(profile_data);
    }
}

nlohmann::json OpenRGB3DSpatialPlugin::OnProfileSave()
{
    if(ui != nullptr)
    {
        return ui->OnProfileSave();
    }
    return nlohmann::json::object();
}

unsigned char* OpenRGB3DSpatialPlugin::OnSDKCommand(unsigned int /*pkt_id*/, unsigned char* /*pkt_data*/, unsigned int* /*pkt_size*/)
{
    return nullptr;
}

void OpenRGB3DSpatialPlugin::ProfileManagerUpdated(unsigned int /*update_reason*/)
{
}

void OpenRGB3DSpatialPlugin::ResourceManagerUpdated(unsigned int update_reason)
{
    if(!APIPointer || ui == nullptr)
    {
        return;
    }

    if(update_reason == RESOURCEMANAGER_UPDATE_REASON_DEVICE_LIST_UPDATED
    || update_reason == RESOURCEMANAGER_UPDATE_REASON_DETECTION_COMPLETE)
    {
        QMetaObject::invokeMethod(ui, "UpdateDeviceList", Qt::QueuedConnection);
        OpenRGB3DSpatialTab::OnOpenRgbDetectionEnded(ui);
    }
}

void OpenRGB3DSpatialPlugin::SettingsManagerUpdated(unsigned int /*update_reason*/)
{
}
