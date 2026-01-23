/*---------------------------------------------------------*\
| DisplayPlaneManager.h                                     |
|                                                           |
|   Global access to display planes for effects            |
|                                                           |
|   Date: 2025-10-23                                        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#ifndef DISPLAYPLANEMANAGER_H
#define DISPLAYPLANEMANAGER_H

#include "DisplayPlane3D.h"
#include <vector>
#include <mutex>

/**
 * @brief Singleton manager for display planes
 *
 * Provides global access to display planes for effects that need them.
 * The UI tab populates this with its display_planes list.
 */
class DisplayPlaneManager
{
public:
    static DisplayPlaneManager* instance()
    {
        static DisplayPlaneManager inst;
        return &inst;
    }

    /**
     * @brief Update the list of available display planes
     * Called by the UI tab when planes are added/removed/modified
     */
    void SetDisplayPlanes(const std::vector<DisplayPlane3D*>& planes)
    {
        std::lock_guard<std::mutex> lock(mutex);
        display_planes = planes;
    }

    /**
     * @brief Get all available display planes
     * Thread-safe access for effects
     */
    std::vector<DisplayPlane3D*> GetDisplayPlanes() const
    {
        std::lock_guard<std::mutex> lock(mutex);
        return display_planes;
    }

    /**
     * @brief Find a display plane by ID
     * Returns nullptr if not found
     */
    DisplayPlane3D* GetPlaneById(int id) const
    {
        std::lock_guard<std::mutex> lock(mutex);
        for(unsigned int i = 0; i < display_planes.size(); i++)
        {
            DisplayPlane3D* plane = display_planes[i];
            if(plane != nullptr && plane->GetId() == id)
            {
                return plane;
            }
        }
        return nullptr;
    }

private:
    DisplayPlaneManager() {}

    mutable std::mutex mutex;
    std::vector<DisplayPlane3D*> display_planes;
};

#endif // DISPLAYPLANEMANAGER_H
