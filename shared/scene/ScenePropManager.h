// SPDX-License-Identifier: GPL-2.0-only

#ifndef SCENEPROPMANAGER_H
#define SCENEPROPMANAGER_H

#include "SceneProp3D.h"

#include <mutex>
#include <vector>

class ScenePropManager
{
public:
    static ScenePropManager* instance()
    {
        static ScenePropManager inst;
        return &inst;
    }

    void SetSceneProps(const std::vector<SceneProp3D*>& props)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        scene_props_ = props;
    }

    std::vector<SceneProp3D*> GetSceneProps() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return scene_props_;
    }

private:
    ScenePropManager() = default;

    mutable std::mutex mutex_;
    std::vector<SceneProp3D*> scene_props_;
};

#endif
