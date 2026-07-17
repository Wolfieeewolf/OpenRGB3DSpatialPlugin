// SPDX-License-Identifier: GPL-2.0-only

#include "OpenRGB3DSpatialTab.h"
#include "PluginLog.h"
#include <QComboBox>
#include <QVariant>

namespace
{
constexpr int kEffectProfileVersion = 9;
constexpr int kMinSupportedEffectProfileVersion = 8;
}

nlohmann::json OpenRGB3DSpatialTab::BuildEffectProfileJson() const
{
    nlohmann::json profile_json;
    profile_json["version"] = kEffectProfileVersion;

    nlohmann::json stack_json = nlohmann::json::array();
    for(size_t i = 0; i < effect_stack.size(); i++)
    {
        if(effect_stack[i])
        {
            stack_json.push_back(effect_stack[i]->ToJson());
        }
    }
    profile_json["stack"] = stack_json;
    profile_json["selected_stack_index"] = effectStackList() ? effectStackList()->currentRow() : -1;

    if(effectOriginCombo())
    {
        profile_json["origin_item_data"] = effectOriginCombo()->currentData().toInt();
    }

    return profile_json;
}

bool OpenRGB3DSpatialTab::ApplyEffectProfileJson(const nlohmann::json& profile_json)
{
    try
    {
        if(!profile_json.is_object())
        {
            LOG_ERROR("[OpenRGB3DSpatialPlugin] Effect profile root must be a JSON object");
            return false;
        }
        if(!profile_json.contains("version") || !profile_json["version"].is_number_integer())
        {
            LOG_ERROR("[OpenRGB3DSpatialPlugin] Effect profile missing version");
            return false;
        }
        const int version = profile_json["version"].get<int>();
        if(version < kMinSupportedEffectProfileVersion || version > kEffectProfileVersion)
        {
            LOG_ERROR("[OpenRGB3DSpatialPlugin] Effect profile unsupported version %d (need %d-%d)",
                      version, kMinSupportedEffectProfileVersion, kEffectProfileVersion);
            return false;
        }
        if(!profile_json.contains("stack") || !profile_json["stack"].is_array())
        {
            LOG_ERROR("[OpenRGB3DSpatialPlugin] Effect profile missing stack array");
            return false;
        }

        RebuildEffectStackFromJson(profile_json["stack"]);

        int desired_index = -1;
        if(profile_json.contains("selected_stack_index"))
        {
            desired_index = profile_json["selected_stack_index"].get<int>();
        }
        if(desired_index < 0 || desired_index >= (int)effect_stack.size())
        {
            desired_index = effect_stack.empty() ? -1 : 0;
        }

        ApplyLoadedStackSelection(desired_index);

        if(effectOriginCombo() && profile_json.contains("origin_item_data") &&
           profile_json["origin_item_data"].is_number_integer())
        {
            int ref_data = profile_json["origin_item_data"].get<int>();
            int idx = effectOriginCombo()->findData(QVariant(ref_data));
            if(idx >= 0)
            {
                effectOriginCombo()->setCurrentIndex(idx);
            }
        }

        return true;
    }
    catch(const std::exception& e)
    {
        LOG_ERROR("[OpenRGB3DSpatialPlugin] Failed to apply effect profile: %s", e.what());
        return false;
    }
}
