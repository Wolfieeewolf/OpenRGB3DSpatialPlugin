/*---------------------------------------------------------*\
| OpenRGB3DSpatialSDK.h                                     |
|                                                           |
|   Lightweight C-style SDK surface for the 3D Grid         |
|   Published via a Qt application property for same-process |
|   plugins to consume.                                      |
|                                                           |
|   Retrieval:                                              |
|     const ORGB3DGridAPI* api = nullptr;                   |
|     QVariant v = qApp->property("OpenRGB3DSpatialGridAPI");|
|     if(v.isValid()) api = reinterpret_cast<const ORGB3DGridAPI*>(|
|         (uintptr_t)v.value<qulonglong>());                 |
|                                                           |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#pragma once

#include <cstddef>

struct ORGB3DGridAPI
{
    int     api_version;   // increment on breaking changes (starts at 1)

    // Grid and room
    float (*GetGridScaleMM)();
    void  (*GetRoomDimensions)(float* width_mm, float* depth_mm, float* height_mm, bool* use_manual);

    // Controllers
    size_t (*GetControllerCount)();
    // name_buf is UTF-8, returns false if idx out of range
    bool   (*GetControllerName)(size_t idx, char* name_buf, size_t buf_size);
    bool   (*IsControllerVirtual)(size_t idx);
    int    (*GetControllerGranularity)(size_t idx); // -1 virtual, 0 device, 1 zone, 2 led
    int    (*GetControllerItemIndex)(size_t idx);

    // LEDs (in current layout snapshot)
    size_t (*GetLEDCount)(size_t ctrl_idx);
    bool   (*GetLEDWorldPosition)(size_t ctrl_idx, size_t led_idx, float* x, float* y, float* z);
    bool   (*GetLEDWorldPositions)(size_t ctrl_idx, float* xyz_interleaved, size_t max_triplets, size_t* out_count);

    // Aggregate helpers
    size_t (*GetTotalLEDCount)();
    bool   (*GetAllLEDWorldPositions)(float* xyz_interleaved, size_t max_triplets, size_t* out_count);
    // Aggregate with controller offsets (prefix-sum, length = controllers+1)
    bool   (*GetAllLEDWorldPositionsWithOffsets)(float* xyz_interleaved,
                                                 size_t max_triplets,
                                                 size_t* out_triplets,
                                                 size_t* ctrl_offsets,
                                                 size_t offsets_capacity,
                                                 size_t* out_controllers);

    // Change notification (optional): called when transforms/layout change
    // Callback signature: void (*cb)(void* user)
    bool   (*RegisterGridLayoutCallback)(void (*cb)(void*), void* user);
    bool   (*UnregisterGridLayoutCallback)(void (*cb)(void*), void* user);

    // Write paths
    bool   (*SetControllerColors)(size_t ctrl_idx, const unsigned int* bgr_colors, size_t count);
    bool   (*SetSingleLEDColor)(size_t ctrl_idx, size_t led_idx, unsigned int bgr_color);
    // Grid-order write (concatenated across controllers in publication order)
    bool   (*SetGridOrderColors)(const unsigned int* bgr_colors_by_grid, size_t count);
    bool   (*SetGridOrderColorsWithOrder)(int order, const unsigned int* bgr_colors_by_grid, size_t count);
};

// Optional C exports (not required if using Qt property retrieval)
extern "C" const ORGB3DGridAPI* OpenRGB3DSpatial_GetAPI();
extern "C" void OpenRGB3DSpatial_SetAPI(const ORGB3DGridAPI* api);

