// SPDX-License-Identifier: GPL-2.0-only

#ifndef DISPLAYPLANE_CAPTURE_LABELS_H
#define DISPLAYPLANE_CAPTURE_LABELS_H

#include "ScreenCaptureManager.h"

#include <algorithm>
#include <string>
#include <vector>

/** 1-based Windows/Qt monitor index from `screen_N` ids. Returns 0 if unknown. */
inline int MonitorNumberFromSourceId(const std::string& source_id)
{
    const std::string::size_type pos = source_id.find_last_of('_');
    if(pos == std::string::npos || pos + 1 >= source_id.size())
    {
        return 0;
    }
    try
    {
        const int index = std::stoi(source_id.substr(pos + 1));
        if(index < 0)
        {
            return 0;
        }
        return index + 1;
    }
    catch(...)
    {
        return 0;
    }
}

inline int CaptureSourceDisplayNumber(const CaptureSourceInfo& source)
{
    if(source.display_number > 0)
    {
        return source.display_number;
    }
    return MonitorNumberFromSourceId(source.id);
}

/** Human label: "Monitor 1 — \\\\.\\DISPLAY1 [Primary] (1920x1080)". */
inline std::string FormatCaptureSourceLabel(const CaptureSourceInfo& source)
{
    const int number = std::max(CaptureSourceDisplayNumber(source), 1);
    std::string label = "Monitor " + std::to_string(number);
    if(!source.name.empty())
    {
        label += " — " + source.name;
    }
    if(source.is_primary)
    {
        label += " [Primary]";
    }
    if(source.width > 0 && source.height > 0)
    {
        label += " (" + std::to_string(source.width) + "x" + std::to_string(source.height) + ")";
    }
    return label;
}

inline void SortCaptureSourcesByDisplayNumber(std::vector<CaptureSourceInfo>& sources)
{
    std::sort(sources.begin(), sources.end(),
              [](const CaptureSourceInfo& a, const CaptureSourceInfo& b) {
                  const int na = CaptureSourceDisplayNumber(a);
                  const int nb = CaptureSourceDisplayNumber(b);
                  if(na != nb)
                  {
                      return na < nb;
                  }
                  return a.id < b.id;
              });
}

/** Prefer an unused primary, then any unused screen, then primary, then first. */
inline std::string SuggestCaptureSourceId(const std::vector<CaptureSourceInfo>& sources,
                                          const std::vector<std::string>& used_ids)
{
    auto is_used = [&](const std::string& id) {
        return std::find(used_ids.begin(), used_ids.end(), id) != used_ids.end();
    };

    const CaptureSourceInfo* primary = nullptr;
    const CaptureSourceInfo* first_unused = nullptr;
    const CaptureSourceInfo* first = nullptr;
    for(const CaptureSourceInfo& source : sources)
    {
        if(source.id.empty() || !source.is_available)
        {
            continue;
        }
        if(!first)
        {
            first = &source;
        }
        if(source.is_primary)
        {
            primary = &source;
        }
        if(!is_used(source.id))
        {
            if(source.is_primary)
            {
                return source.id;
            }
            if(!first_unused)
            {
                first_unused = &source;
            }
        }
    }
    if(first_unused)
    {
        return first_unused->id;
    }
    if(primary)
    {
        return primary->id;
    }
    if(first)
    {
        return first->id;
    }
    return {};
}

#endif
