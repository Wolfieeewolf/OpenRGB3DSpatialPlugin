// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include "filesystem.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace EffectBinding
{

constexpr int kFormatVersion = 1;
constexpr const char* kFormatId = "openrgb3d.effect_bindings";

struct Binding
{
    std::string id;
    bool enabled = true;
    std::string source;   // manual | windows | …
    std::string event;    // fire | session_lock | …
    std::string pack_id;
};

struct Document
{
    std::vector<Binding> bindings;
};

std::string MakeBindingId();

nlohmann::json ToJson(const Document& doc);
bool FromJson(const nlohmann::json& j, Document* out, std::string* error);
bool LoadFromFile(const filesystem::path& path, Document* out, std::string* error);
bool SaveToFile(const filesystem::path& path, const Document& doc, std::string* error);

/** Empty / missing file → empty document (not an error). */
bool LoadOrEmpty(const filesystem::path& path, Document* out, std::string* error);

} // namespace EffectBinding
