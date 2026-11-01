// SPDX-License-Identifier: GPL-2.0-only

#ifndef SCENEPROPMESHCACHE_H
#define SCENEPROPMESHCACHE_H

#include "MeshImport.h"

#include <mutex>
#include <string>
#include <unordered_map>

class ScenePropMeshCache
{
public:
    static ScenePropMeshCache* instance()
    {
        static ScenePropMeshCache inst;
        return &inst;
    }

    const MeshImport::TriangleMesh* GetOrLoad(const std::string& absolute_path,
                                              std::string* error_out = nullptr)
    {
        if(absolute_path.empty())
        {
            if(error_out)
            {
                *error_out = "Empty mesh path";
            }
            return nullptr;
        }

        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = cache_.find(absolute_path);
            if(it != cache_.end())
            {
                return &it->second;
            }
        }

        MeshImport::TriangleMesh mesh;
        std::string err;
        if(!MeshImport::LoadTriangleMesh(absolute_path, &mesh, &err))
        {
            if(error_out)
            {
                *error_out = err;
            }
            return nullptr;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        auto [it, inserted] = cache_.emplace(absolute_path, std::move(mesh));
        (void)inserted;
        return &it->second;
    }

    void Put(const std::string& absolute_path, MeshImport::TriangleMesh mesh)
    {
        if(absolute_path.empty() || mesh.empty())
        {
            return;
        }
        std::lock_guard<std::mutex> lock(mutex_);
        cache_[absolute_path] = std::move(mesh);
    }

    void Invalidate(const std::string& absolute_path)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        cache_.erase(absolute_path);
    }

    void Clear()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        cache_.clear();
    }

private:
    ScenePropMeshCache() = default;

    mutable std::mutex mutex_;
    std::unordered_map<std::string, MeshImport::TriangleMesh> cache_;
};

#endif
