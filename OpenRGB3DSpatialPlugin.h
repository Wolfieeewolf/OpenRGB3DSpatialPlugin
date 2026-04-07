// SPDX-License-Identifier: GPL-2.0-only
// OpenRGB 3D Spatial Plugin

#ifndef OPENRGB3DSPATIALPLUGIN_H
#define OPENRGB3DSPATIALPLUGIN_H

#include <QObject>
#include <memory>
#include "OpenRGBPluginInterface.h"

class GameTelemetryBridge;
class OpenRGB3DSpatialTab;
class QMenu;
class ResourceManagerInterface;
class QWidget;

class OpenRGB3DSpatialPlugin : public QObject, public OpenRGBPluginInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID OpenRGBPluginInterface_IID)
    Q_INTERFACES(OpenRGBPluginInterface)

public:
    OpenRGB3DSpatialPlugin();

    ~OpenRGB3DSpatialPlugin() override;

    OpenRGBPluginInfo   GetPluginInfo()                                                     override;
    unsigned int        GetPluginAPIVersion()                                               override;

    void                Load(ResourceManagerInterface* resource_manager_ptr)                override;
    QWidget*            GetWidget()                                                         override;
    QMenu*              GetTrayMenu()                                                       override;
    void                Unload()                                                            override;

    static ResourceManagerInterface* RMPointer;

private:
    static void                 DeviceListChangedCallback(void* ptr);
    static void                 DetectionProgressCallback(void* ptr);

    OpenRGB3DSpatialTab*        ui = nullptr;
    std::unique_ptr<GameTelemetryBridge> game_telemetry_bridge;
};

#endif
