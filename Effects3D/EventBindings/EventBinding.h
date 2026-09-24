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
    std::string source;
    std::string event;
    std::string pack_id;
};

struct Document
{
    std::vector<Binding> bindings;
    std::vector<std::string> catalog_enabled;
};

std::string MakeBindingId();

nlohmann::json ToJson(const Document& doc);
bool FromJson(const nlohmann::json& j, Document* out, std::string* error);
bool LoadFromFile(const filesystem::path& path, Document* out, std::string* error);
bool SaveToFile(const filesystem::path& path, const Document& doc, std::string* error);

/** Empty / missing file → empty document (not an error). */
bool LoadOrEmpty(const filesystem::path& path, Document* out, std::string* error);

/** Read-only recipes in a folder. Each *.json uses the bindings format. Missing folder → empty. */
void LoadCatalog(const filesystem::path& dir, std::vector<Binding>* out, std::vector<std::string>* warnings);

} // namespace EffectBinding
