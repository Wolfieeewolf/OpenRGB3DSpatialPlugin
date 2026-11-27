// SPDX-License-Identifier: GPL-2.0-or-later

#include "OpenRGB3DSpatialPlugin.h"
#include "OpenRGB3DSpatialTab.h"
#include "FolderVolumeEffect.h"
#include "PlayerEngines.h"
#include "Game/GameTelemetryBridge.h"
#include "ResourceManagerCallback.h"
#include "ProfileManager.h"
#include "LogManager.h"

#include <QMetaObject>
#include <QSizePolicy>
#include <QThread>
#include <QtGlobal>
#include <cstdio>
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
    info.URL            = "https://github.com/Wolfieeewolf/OpenRGB3DSpatialPlugin";

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

    /* One line that says what this build expects, for install troubleshooting (Linux distro
       OpenRGB builds use the system Qt; a plugin built for another Qt misbehaves or crashes). */
    LOG_INFO("[OpenRGB3DSpatialPlugin] %s loaded: built with Qt %s, running Qt %s, plugin API %u",
             VERSION_STRING, QT_VERSION_STR, qVersion(), (unsigned int)OPENRGB_PLUGIN_API_VERSION);
    {
        int built_major = 0, built_minor = 0, run_major = 0, run_minor = 0;
        std::sscanf(QT_VERSION_STR, "%d.%d", &built_major, &built_minor);
        std::sscanf(qVersion(), "%d.%d", &run_major, &run_minor);
        if(run_major != built_major || run_minor < built_minor)
        {
            LOG_WARNING("[OpenRGB3DSpatialPlugin] Qt mismatch: this plugin was built for Qt %s but OpenRGB runs "
                        "Qt %s. Rebuild for your system (scripts/install-linux-user.sh on Linux) or use the "
                        "matching release.",
                        QT_VERSION_STR, qVersion());
        }
    }

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
