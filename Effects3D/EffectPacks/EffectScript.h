// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include "EffectPack.h"

namespace EffectPack
{
namespace script
{

struct LedView
{
    const Block* block = nullptr;
    int local_ms = 0;
    float progress = 0.0f;
    float axis = 0.0f;
    int seed = 0;
    float intensity = 1.0f;
    RGBColor color = 0;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float min_x = 0.0f;
    float max_x = 1.0f;
    float min_y = 0.0f;
    float max_y = 1.0f;
    float min_z = 0.0f;
    float max_z = 1.0f;
    float nx = 0.5f;
    float ny = 0.5f;
    float nz = 0.5f;
    float radius = 0.0f;
    float height = 0.5f;
    float span_x = 0.0f;
    float span_y = 0.0f;
    float span_z = 0.0f;
    float dx = 0.0f;
    float dy = 0.0f;
    float dz = 0.0f;
    /** Set by script `off` — Evaluate* must not fall back to a gradient paint. */
    bool turned_off = false;
};

void SetDirectory(const filesystem::path& dir);
void ReloadDirectory();
bool Has(const std::string& id);
std::string FileId(const Block& block);
bool UsesWorld(const std::string& id);
bool ColorEnds(const std::string& id);
bool PreviewPulse(const std::string& id);
bool AxisAngle(const std::string& id);
bool Run(const std::string& id, LedView* led);

} // namespace script

void SetEffectScriptDirectory(const filesystem::path& dir);
void ReloadEffectScripts();
bool EffectScriptUsesWorld(const std::string& id);
bool EffectColorEnds(const std::string& id);
bool EffectPreviewPulse(const std::string& id);
bool EffectAxisAngle(const std::string& id);
std::string BlockFileId(const Block& block);

} // namespace EffectPack
