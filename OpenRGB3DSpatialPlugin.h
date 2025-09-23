/*---------------------------------------------------------*\
| OpenRGB3DSpatialPlugin.h                                  |
|                                                           |
|   OpenRGB 3D Spatial LED Control System Plugin           |
|                                                           |
|   Date: 2025-09-23                                        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#ifndef OPENRGB3DSPATIALPLUGIN_H
#define OPENRGB3DSPATIALPLUGIN_H

#include <QObject>
#include <QWidget>
#include <QMenu>

#include "OpenRGBPluginInterface.h"
#include "OpenRGB3DSpatialTab.h"
#include "ResourceManagerInterface.h"

class OpenRGB3DSpatialPlugin : public QObject, public OpenRGBPluginInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID OpenRGBPluginInterface_IID)
    Q_INTERFACES(OpenRGBPluginInterface)

public:
    ~OpenRGB3DSpatialPlugin() {};

    virtual OpenRGBPluginInfo   GetPluginInfo()                                                     override;
    virtual unsigned int        GetPluginAPIVersion()                                               override;

    virtual void                Load(ResourceManagerInterface* resource_manager_ptr)                override;
    virtual QWidget*            GetWidget()                                                         override;
    virtual QMenu*              GetTrayMenu()                                                       override;
    virtual void                Unload()                                                            override;

    static ResourceManagerInterface* RMPointer;

private:
    static void                 DeviceListChangedCallback(void* ptr);

    OpenRGB3DSpatialTab*        ui;
};

#endif