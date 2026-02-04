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
    ui        = nullptr;
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

    return ui;
}

QMenu* OpenRGB3DSpatialPlugin::GetTrayMenu()
{
    return nullptr;
}

void OpenRGB3DSpatialPlugin::Unload()
{
    if(RMPointer && ui != nullptr)
    {
        RMPointer->UnregisterDeviceListChangeCallback(DeviceListChangedCallback, ui);
    }

    ui = nullptr;
}

void OpenRGB3DSpatialPlugin::DeviceListChangedCallback(void* o)
{
    if(!o)
    {
        return;
    }

    QMetaObject::invokeMethod((OpenRGB3DSpatialTab*)o, "UpdateDeviceList", Qt::QueuedConnection);
}

void OpenRGB3DSpatialPlugin::OnProfileAboutToLoad()
{
    /*---------------------------------------------------------*\
    | Called before a profile is loaded. Plugin can prepare     |
    | for incoming profile data (e.g. clear current state).     |
    \*---------------------------------------------------------*/
}

void OpenRGB3DSpatialPlugin::OnProfileLoad(nlohmann::json profile_data)
{
    /*---------------------------------------------------------*\
    | Called when a profile is loaded. The plugin's saved       |
    | state from OnProfileSave() is passed in profile_data.     |
    | Load layout, effect stack, and all plugin state.          |
    \*---------------------------------------------------------*/
    if(!ui || profile_data.is_null() || !profile_data.contains("3DSpatialPlugin"))
    {
        return;
    }

    try
    {
        const nlohmann::json& plugin_data = profile_data["3DSpatialPlugin"];
        
        /*-------------------------------------------------*\
        | Load layout from profile data if present          |
        \*-------------------------------------------------*/
        if(plugin_data.contains("layout"))
        {
            ui->LoadLayoutFromJSON(plugin_data["layout"]);
        }
    }
    catch(const std::exception& e)
    {
        // Log error but don't crash
        (void)e;
    }
}

nlohmann::json OpenRGB3DSpatialPlugin::OnProfileSave()
{
    /*---------------------------------------------------------*\
    | Called when a profile is saved. Return a JSON object      |
    | containing all plugin state that should be saved with     |
    | the profile (layout, effect stack, settings, etc.).       |
    \*---------------------------------------------------------*/
    nlohmann::json plugin_data;
    
    if(!ui)
    {
        return plugin_data;
    }

    try
    {
        /*-------------------------------------------------*\
        | Save current layout state                         |
        \*-------------------------------------------------*/
        nlohmann::json layout_json;
        // TODO: Export layout to JSON (similar to SaveLayout but to JSON object)
        // For now, return empty - will implement after member access migration
        plugin_data["3DSpatialPlugin"]["layout"] = layout_json;
    }
    catch(const std::exception& e)
    {
        // Log error but return what we have
        (void)e;
    }

    return plugin_data;
}

unsigned char* OpenRGB3DSpatialPlugin::OnSDKCommand(unsigned int pkt_id, unsigned char* pkt_data, unsigned int* pkt_size)
{
    /*---------------------------------------------------------*\
    | Handle custom SDK commands for this plugin. Not currently |
    | used, but required by API v5.                             |
    \*---------------------------------------------------------*/
    (void)pkt_id;
    (void)pkt_data;
    *pkt_size = 0;
    return nullptr;
}
