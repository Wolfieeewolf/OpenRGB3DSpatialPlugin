// SPDX-License-Identifier: GPL-2.0-only

#include "OpenRGB3DSpatialPlugin.h"
#include "OpenRGB3DSpatialTab.h"
#include "Effects3D/FolderVolume/FolderVolumeEffect.h"
#include "Effects3D/PlayerEngines.h"
#include "Game/GameTelemetryBridge.h"
#include "ResourceManagerCallback.h"
#include "ProfileManager.h"

#include <QMetaObject>
#include <QSizePolicy>
#include <QThread>
#include <utility>

OpenRGBPluginAPIInterface* OpenRGB3DSpatialPlugin::APIPointer = nullptr;

OpenRGBPluginAPIInterface* g_3dspatial_plugin_api = nullptr;

namespace
{
template<typename Fn>
void RunOnUiThread(QObject* obj, Fn&& fn)
{
    if(!obj)
    {
        return;
    }

    if(QThread::currentThread() == obj->thread())
    {
        fn();
    }
    else
    {
        QMetaObject::invokeMethod(obj, std::forward<Fn>(fn), Qt::BlockingQueuedConnection);
    }
}
}

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

    info.Icon.load(":/images/OpenRGB3DSpatialPlugin.png");

    return info;
}

unsigned int OpenRGB3DSpatialPlugin::GetPluginAPIVersion()
{
    return OPENRGB_PLUGIN_API_VERSION;
}

void OpenRGB3DSpatialPlugin::Load(OpenRGBPluginAPIInterface* plugin_api_ptr)
{
    APIPointer             = plugin_api_ptr;
    g_3dspatial_plugin_api = plugin_api_ptr;

    if(!game_telemetry_bridge)
    {
        game_telemetry_bridge = std::make_unique<GameTelemetryBridge>();
    }
    game_telemetry_bridge->Register(APIPointer);
    RegisterFolderVolumeEffects();
    RegisterPlayerEngines();

    ui = new OpenRGB3DSpatialTab(APIPointer);
    ui->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

QWidget* OpenRGB3DSpatialPlugin::GetWidget()
{
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

    if(ui)
    {
        ui->SavePluginUiSettings();
    }

    ui = nullptr;
}

void OpenRGB3DSpatialPlugin::OnProfileAboutToLoad()
{
    RunOnUiThread(ui, [this]()
    {
        if(ui)
        {
            ui->OnProfileAboutToLoad();
        }
    });
}

void OpenRGB3DSpatialPlugin::OnProfileLoad(nlohmann::json profile_data)
{
    RunOnUiThread(ui, [this, data = std::move(profile_data)]() mutable
    {
        if(ui)
        {
            ui->OnProfileLoad(data);
        }
    });
}

nlohmann::json OpenRGB3DSpatialPlugin::OnProfileSave()
{
    nlohmann::json result = nlohmann::json::object();
    RunOnUiThread(ui, [this, &result]()
    {
        if(ui)
        {
            result = ui->OnProfileSave();
        }
    });
    return result;
}

unsigned char* OpenRGB3DSpatialPlugin::OnSDKCommand(unsigned int /*pkt_id*/, unsigned char* /*pkt_data*/, unsigned int* /*pkt_size*/)
{
    return nullptr;
}

void OpenRGB3DSpatialPlugin::ProfileManagerUpdated(unsigned int update_reason)
{
    if(!ui)
    {
        return;
    }

    if(update_reason == PROFILEMANAGER_UPDATE_REASON_ACTIVE_PROFILE_CHANGED)
    {
        QMetaObject::invokeMethod(ui, "TryAttachMissingLayoutControllers", Qt::QueuedConnection);
    }
}

void OpenRGB3DSpatialPlugin::ResourceManagerUpdated(unsigned int update_reason)
{
    if(!APIPointer || !ui)
    {
        return;
    }

    if(update_reason == RESOURCEMANAGER_UPDATE_REASON_DEVICE_LIST_UPDATED
    || update_reason == RESOURCEMANAGER_UPDATE_REASON_DETECTION_COMPLETE)
    {
        QMetaObject::invokeMethod(ui, "UpdateDeviceList", Qt::QueuedConnection);
        OpenRGB3DSpatialTab::OnOpenRgbDetectionEnded(ui);
        QMetaObject::invokeMethod(ui, "TryAttachMissingLayoutControllers", Qt::QueuedConnection);
    }
}

void OpenRGB3DSpatialPlugin::SettingsManagerUpdated(unsigned int /*update_reason*/)
{
}
