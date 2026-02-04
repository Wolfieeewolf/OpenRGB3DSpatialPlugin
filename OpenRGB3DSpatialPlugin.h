// SPDX-License-Identifier: GPL-2.0-only
// OpenRGB 3D Spatial Plugin

#ifndef OPENRGB3DSPATIALPLUGIN_H
#define OPENRGB3DSPATIALPLUGIN_H

#include <QObject>
#include <QWidget>
#include <QMenu>
#include <nlohmann/json.hpp>
#include "OpenRGBPluginInterface.h"
#include "ResourceManagerInterface.h"
#include "OpenRGB3DSpatialTab.h"

class OpenRGB3DSpatialPlugin : public QObject, public OpenRGBPluginInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID OpenRGBPluginInterface_IID)
    Q_INTERFACES(OpenRGBPluginInterface)

public:
    OpenRGB3DSpatialPlugin()
        : ui(nullptr)
    {
    }

    ~OpenRGB3DSpatialPlugin() override
    {
    }

    virtual OpenRGBPluginInfo   GetPluginInfo()                                                     override;
    virtual unsigned int        GetPluginAPIVersion()                                               override;

    virtual void                Load(ResourceManagerInterface* resource_manager_ptr)                override;
    virtual QWidget*            GetWidget()                                                         override;
    virtual QMenu*              GetTrayMenu()                                                       override;
    virtual void                Unload()                                                            override;
    virtual void                OnProfileAboutToLoad()                                              override;
    virtual void                OnProfileLoad(nlohmann::json profile_data)                          override;
    virtual nlohmann::json      OnProfileSave()                                                     override;
    virtual unsigned char*      OnSDKCommand(unsigned int pkt_id, unsigned char * pkt_data, unsigned int *pkt_size)      override;

    static ResourceManagerInterface* RMPointer;

private:
    static void                 DeviceListChangedCallback(void* ptr, unsigned int update_reason);

    OpenRGB3DSpatialTab*        ui;
};

#endif
