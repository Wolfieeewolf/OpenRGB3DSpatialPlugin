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

    void                Load(OpenRGBPluginAPIInterface* plugin_api_ptr)                     override;
    QWidget*            GetWidget()                                                         override;
    QMenu*              GetTrayMenu()                                                       override;
    void                Unload()                                                            override;

    void                OnProfileAboutToLoad()                                              override;
    void                OnProfileLoad(nlohmann::json profile_data)                          override;
    nlohmann::json      OnProfileSave()                                                     override;
    unsigned char*      OnSDKCommand(unsigned int pkt_id, unsigned char* pkt_data, unsigned int* pkt_size) override;

    void                ProfileManagerUpdated(unsigned int update_reason)                   override;
    void                ResourceManagerUpdated(unsigned int update_reason)                  override;
    void                SettingsManagerUpdated(unsigned int update_reason)                  override;

    static OpenRGBPluginAPIInterface* APIPointer;

private:
    OpenRGB3DSpatialTab*        ui = nullptr;
    std::unique_ptr<GameTelemetryBridge> game_telemetry_bridge;
};

#endif
