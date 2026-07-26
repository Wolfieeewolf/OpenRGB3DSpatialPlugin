// SPDX-License-Identifier: GPL-2.0-only

#include "EventBinding.h"

#include <chrono>
#include <fstream>
#include <random>
#include <sstream>
#include <system_error>

namespace EffectBinding
{

namespace
{

std::string RandomHex(size_t bytes)
{
    static thread_local std::mt19937_64 rng{
        (uint64_t)std::chrono::high_resolution_clock::now().time_since_epoch().count()
    };
    std::uniform_int_distribution<int> dist(0, 15);
    static const char* hex = "0123456789abcdef";
    std::string out;
    out.reserve(bytes * 2);
    for(size_t i = 0; i < bytes * 2; ++i)
    {
        out.push_back(hex[dist(rng)]);
    }
    return out;
}

} // namespace

std::string MakeBindingId()
{
    return "bind_" + RandomHex(8);
}

nlohmann::json ToJson(const Document& doc)
{
    nlohmann::json j;
    j["format"] = kFormatId;
    j["version"] = kFormatVersion;
    nlohmann::json arr = nlohmann::json::array();
    for(const Binding& b : doc.bindings)
    {
        arr.push_back({
            {"id", b.id},
            {"enabled", b.enabled},
            {"source", b.source},
            {"event", b.event},
            {"pack_id", b.pack_id},
        });
    }
    j["bindings"] = std::move(arr);
    return j;
}

bool FromJson(const nlohmann::json& j, Document* out, std::string* error)
{
    if(!out)
    {
        return false;
    }
    if(!j.is_object())
    {
        if(error) { *error = "bindings root must be an object"; }
        return false;
    }
    if(j.value("format", std::string()) != kFormatId)
    {
        if(error) { *error = "unexpected bindings format id"; }
        return false;
    }
    const int ver = j.value("version", 0);
    if(ver != kFormatVersion)
    {
        if(error) { *error = "unsupported bindings version"; }
        return false;
    }

    Document doc;
    if(j.contains("bindings") && j["bindings"].is_array())
    {
        for(const auto& bj : j["bindings"])
        {
            if(!bj.is_object())
            {
                continue;
            }
            Binding b;
            b.id = bj.value("id", std::string());
            b.enabled = bj.value("enabled", true);
            b.source = bj.value("source", std::string());
            b.event = bj.value("event", std::string());
            b.pack_id = bj.value("pack_id", std::string());
            if(b.id.empty() || b.source.empty() || b.event.empty() || b.pack_id.empty())
            {
                continue;
            }
            doc.bindings.push_back(std::move(b));
        }
    }
    *out = std::move(doc);
    return true;
}

bool LoadFromFile(const filesystem::path& path, Document* out, std::string* error)
{
    std::ifstream in(path, std::ios::binary);
    if(!in)
    {
        if(error) { *error = "failed to open bindings file"; }
        return false;
    }
    nlohmann::json j;
    try
    {
        in >> j;
    }
    catch(const std::exception& ex)
    {
        if(error) { *error = std::string("json parse failed: ") + ex.what(); }
        return false;
    }
    return FromJson(j, out, error);
}

bool SaveToFile(const filesystem::path& path, const Document& doc, std::string* error)
{
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if(!out)
    {
        if(error) { *error = "failed to write bindings file"; }
        return false;
    }
    try
    {
        out << ToJson(doc).dump(2);
    }
    catch(const std::exception& ex)
    {
        if(error) { *error = std::string("json write failed: ") + ex.what(); }
        return false;
    }
    return static_cast<bool>(out);
}

bool LoadOrEmpty(const filesystem::path& path, Document* out, std::string* error)
{
    if(!out)
    {
        return false;
    }
    std::error_code ec;
    if(!filesystem::exists(path, ec) || !filesystem::is_regular_file(path, ec))
    {
        *out = Document{};
        return true;
    }
    return LoadFromFile(path, out, error);
}

} // namespace EffectBinding
