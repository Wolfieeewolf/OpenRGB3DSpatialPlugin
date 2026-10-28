// SPDX-License-Identifier: GPL-2.0-only
//
// Plugin logging shim for OpenRGB Plugin API v5+.
//
// The vendored LogManager.cpp now pulls in the full OpenRGB backend
// (ResourceManager, NetworkServer/Client, serial_port, ...), so the plugin no
// longer compiles it. Instead every LOG_* macro is routed through the plugin
// API's LogEntry() call, which the host wires to its own LogManager. Include
// this header wherever the LOG_* macros are used (in place of "LogManager.h").

#pragma once

#include "LogManager.h"
#include "OpenRGBPluginInterface.h"

/*---------------------------------------------------------*\
| Set in OpenRGB3DSpatialPlugin::Load(). Null until the     |
| plugin is loaded, in which case log calls are no-ops.     |
\*---------------------------------------------------------*/
extern OpenRGBPluginAPIInterface* g_3dspatial_plugin_api;

#undef  LogAppend
#define LogAppend(level, ...)                                                       \
    do                                                                              \
    {                                                                               \
        if(g_3dspatial_plugin_api)                                                  \
        {                                                                           \
            g_3dspatial_plugin_api->LogEntry(__FILE__, __LINE__, level, __VA_ARGS__);\
        }                                                                           \
    } while(0)
