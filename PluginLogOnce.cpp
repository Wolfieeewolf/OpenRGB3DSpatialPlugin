// SPDX-License-Identifier: GPL-2.0-only

#include "PluginLogOnce.h"
#include "LogManager.h"

#include <mutex>
#include <string>
#include <unordered_set>

void LogOnce_CreateEffectFailed(const char* context, const std::string& class_name)
{
    static std::mutex mutex;
    static std::unordered_set<std::string> seen;
    const std::string key = std::string(context ? context : "") + "|" + class_name;
    std::lock_guard<std::mutex> lock(mutex);
    if(!seen.insert(key).second)
    {
        return;
    }
    LOG_ERROR("[OpenRGB3DSpatialPlugin] %s: CreateEffect failed (logged once per class): %s",
              context ? context : "effect",
              class_name.c_str());
}
