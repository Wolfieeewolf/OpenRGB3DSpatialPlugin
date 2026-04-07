// SPDX-License-Identifier: GPL-2.0-only

#ifndef DISPLAYPLANEMANAGER_H
#define DISPLAYPLANEMANAGER_H

#include "DisplayPlane3D.h"
#include <vector>
#include <mutex>

class DisplayPlaneManager
{
public:
    static DisplayPlaneManager* instance()
    {
        static DisplayPlaneManager inst;
        return &inst;
    }

    void SetDisplayPlanes(const std::vector<DisplayPlane3D*>& planes)
    {
        std::lock_guard<std::mutex> lock(mutex);
        display_planes = planes;
    }

    std::vector<DisplayPlane3D*> GetDisplayPlanes() const
    {
        std::lock_guard<std::mutex> lock(mutex);
        return display_planes;
    }

private:
    DisplayPlaneManager() {}

    mutable std::mutex mutex;
    std::vector<DisplayPlane3D*> display_planes;
};

#endif // DISPLAYPLANEMANAGER_H
