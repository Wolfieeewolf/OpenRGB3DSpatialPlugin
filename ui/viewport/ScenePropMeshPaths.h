// SPDX-License-Identifier: GPL-2.0-only

#ifndef SCENEPROPMESHPATHS_H
#define SCENEPROPMESHPATHS_H

#include "filesystem.h"

#include <mutex>
#include <string>
#include <system_error>

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

inline bool PathHasDotDot(const filesystem::path& p)
{
    for(const filesystem::path& part : p)
    {
        if(part == "..")
        {
            return true;
        }
    }
    return false;
}

inline bool PathStaysUnderRoot(const filesystem::path& root, const filesystem::path& candidate)
{
    std::error_code ec;
    const filesystem::path root_abs = filesystem::weakly_canonical(root, ec);
    if(ec || root_abs.empty())
    {
        return false;
    }
    const filesystem::path cand_abs = filesystem::weakly_canonical(candidate, ec);
    if(ec || cand_abs.empty())
    {
        return false;
    }
    auto root_it = root_abs.begin();
    auto cand_it = cand_abs.begin();
    for(; root_it != root_abs.end(); ++root_it, ++cand_it)
    {
        if(cand_it == cand_abs.end() || *cand_it != *root_it)
        {
            return false;
        }
    }
    return true;
}

/** Resolve a mesh asset path. Fail closed: no `..`, and must stay under MeshesRoot when set. */
inline filesystem::path Resolve(const std::string& relative_or_absolute)
{
    if(relative_or_absolute.empty())
    {
        return {};
    }
    filesystem::path p(relative_or_absolute);
    if(PathHasDotDot(p))
    {
        return {};
    }
    const filesystem::path root = MeshesRoot();
    if(root.empty())
    {
        return p.is_absolute() ? p : filesystem::path{};
    }
    const filesystem::path candidate = p.is_absolute() ? p : (root / p);
    if(!PathStaysUnderRoot(root, candidate))
    {
        return {};
    }
    return candidate;
}
} // namespace ScenePropMeshPaths

#endif
