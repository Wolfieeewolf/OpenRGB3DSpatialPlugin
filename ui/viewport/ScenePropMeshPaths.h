// SPDX-License-Identifier: GPL-2.0-only

#ifndef SCENEPROPMESHPATHS_H
#define SCENEPROPMESHPATHS_H

#include "filesystem.h"

#include <mutex>
#include <string>

namespace ScenePropMeshPaths
{
namespace detail
{
inline filesystem::path& RootStorage()
{
    static filesystem::path root;
    return root;
}

inline std::mutex& Mutex()
{
    static std::mutex mutex;
    return mutex;
}
} // namespace detail

inline void SetMeshesRoot(const filesystem::path& root)
{
    std::lock_guard<std::mutex> lock(detail::Mutex());
    detail::RootStorage() = root;
}

inline filesystem::path MeshesRoot()
{
    std::lock_guard<std::mutex> lock(detail::Mutex());
    return detail::RootStorage();
}

inline filesystem::path Resolve(const std::string& relative_or_absolute)
{
    if(relative_or_absolute.empty())
    {
        return {};
    }
    filesystem::path p(relative_or_absolute);
    if(p.is_absolute())
    {
        return p;
    }
    const filesystem::path root = MeshesRoot();
    if(root.empty())
    {
        return p;
    }
    return root / p;
}
} // namespace ScenePropMeshPaths

#endif
