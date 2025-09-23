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
#include <cstring>

enum
{
    NET_PACKET_ID_3D_SPATIAL_GET_GRID           = 0,
    NET_PACKET_ID_3D_SPATIAL_SET_DEVICE_POS     = 1,
    NET_PACKET_ID_3D_SPATIAL_GET_EFFECTS        = 2,
    NET_PACKET_ID_3D_SPATIAL_START_EFFECT       = 3,
    NET_PACKET_ID_3D_SPATIAL_STOP_EFFECT        = 4,
};

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

    info.Icon.load(":/images/OpenRGB3DSpatialPlugin.png");

    return(info);
}

unsigned int OpenRGB3DSpatialPlugin::GetPluginAPIVersion()
{
    return(OPENRGB_PLUGIN_API_VERSION);
}

void OpenRGB3DSpatialPlugin::Load(ResourceManagerInterface* RM)
{
    RMPointer = RM;

    LOG_INFO("[OpenRGB 3D Spatial] Plugin loaded successfully");
}

QWidget* OpenRGB3DSpatialPlugin::GetWidget()
{
    RMPointer->WaitForDeviceDetection();

    ui = new OpenRGB3DSpatialTab(RMPointer);

    ui->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    RMPointer->RegisterDeviceListChangeCallback(DeviceListChangedCallback, ui);

    return ui;
}

QMenu* OpenRGB3DSpatialPlugin::GetTrayMenu()
{
    return(nullptr);
}

void OpenRGB3DSpatialPlugin::Unload()
{
    RMPointer->UnregisterDeviceListChangeCallback(DeviceListChangedCallback, ui);

    LOG_INFO("[OpenRGB 3D Spatial] Plugin unloaded");
}

void OpenRGB3DSpatialPlugin::DeviceListChangedCallback(void* o)
{
    QMetaObject::invokeMethod((OpenRGB3DSpatialTab*)o, "UpdateDeviceList", Qt::QueuedConnection);
}

unsigned char* OpenRGB3DSpatialPlugin::HandleSDK(void* /*instance*/, unsigned int pkt_id, unsigned char* data, unsigned int* data_size)
{
    unsigned char* data_out = nullptr;

    switch(pkt_id)
    {
        case NET_PACKET_ID_3D_SPATIAL_GET_GRID:
            {
                unsigned int response_size = sizeof(unsigned int) + (3 * sizeof(unsigned int));
                data_out = new unsigned char[response_size];
                unsigned int data_ptr = 0;

                memcpy(&data_out[data_ptr], &response_size, sizeof(unsigned int));
                data_ptr += sizeof(unsigned int);

                unsigned int grid_x = 10;
                unsigned int grid_y = 10;
                unsigned int grid_z = 10;

                memcpy(&data_out[data_ptr], &grid_x, sizeof(unsigned int));
                data_ptr += sizeof(unsigned int);
                memcpy(&data_out[data_ptr], &grid_y, sizeof(unsigned int));
                data_ptr += sizeof(unsigned int);
                memcpy(&data_out[data_ptr], &grid_z, sizeof(unsigned int));
                data_ptr += sizeof(unsigned int);

                *data_size = response_size;
            }
            break;

        case NET_PACKET_ID_3D_SPATIAL_SET_DEVICE_POS:
            {
                if(data && *data_size > 0)
                {
                    unsigned int data_ptr = 0;

                    unsigned short device_idx;
                    memcpy(&device_idx, &data[data_ptr], sizeof(unsigned short));
                    data_ptr += sizeof(unsigned short);

                    unsigned int x, y, z;
                    memcpy(&x, &data[data_ptr], sizeof(unsigned int));
                    data_ptr += sizeof(unsigned int);
                    memcpy(&y, &data[data_ptr], sizeof(unsigned int));
                    data_ptr += sizeof(unsigned int);
                    memcpy(&z, &data[data_ptr], sizeof(unsigned int));
                    data_ptr += sizeof(unsigned int);

                    *data_size = 0;
                }
            }
            break;

        default:
            *data_size = 0;
            break;
    }

    return data_out;
}