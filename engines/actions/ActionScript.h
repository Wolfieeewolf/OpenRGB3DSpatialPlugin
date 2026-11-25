// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "Action.h"

namespace Action
{
namespace script
{

struct LedView
{
    const Clip* clip = nullptr;
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

/** Per-LED script inputs for a GPU batch: axis, seed, x, y, z, nx, ny, nz, radius,
    height, span_x, span_y, span_z, dx, dy, dz (defaults match LedView), then
    sample_axis() and three spare slots. */
struct GpuLedInput
{
    float v[20] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.5f, 0.5f, 0.5f,
                   0.0f, 0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                   0.0f, 0.0f, 0.0f, 0.0f};
};
static_assert(sizeof(GpuLedInput) == sizeof(float) * 20, "GpuLedInput is uploaded as a float array");

void SetDirectory(const filesystem::path& dir);
void ReloadDirectory();
bool Has(const std::string& id);
std::string FileId(const Clip& clip);
bool UsesWorld(const std::string& id);
/** Non-empty when `.fx` declares `shader:` / `field:` (GPU volume sample). */
std::string FieldShader(const std::string& id);
bool UsesField(const std::string& id);
/** True when `.fx` declares `particles:` (GPGPU room particle field). */
bool UsesParticles(const std::string& id);
/** Particle mode string (`burst` / `rain` / `fireflies` / `meteors` / `embers` / `swarm`); empty if unused. */
std::string ParticlesMode(const std::string& id);
/** True when `.fx` declares `ribbons:` (GPGPU room ribbon/tube field). */
bool UsesRibbons(const std::string& id);
/** Ribbon mode string (`arc` / `helix` / `sweep` / `cables`); empty if unused. */
std::string RibbonsMode(const std::string& id);
bool ColorEnds(const std::string& id);
bool PreviewPulse(const std::string& id);
bool AxisAngle(const std::string& id);
bool Run(const std::string& id, LedView* led);

/** True when this effect's script was translated to GLSL and the GPU is up. */
bool GpuCapable(const std::string& id);

/** Run the script for every LED in one GPU draw. colors are final (intensity and
    curves applied, like Evaluate*); lit[i] = 0 where the script said `off`.
    Returns false when the GPU path is unavailable: run the CPU path instead. */
bool RunBatchGpu(const std::string& id,
                 const Clip& clip,
                 int local_ms,
                 const std::vector<GpuLedInput>& leds,
                 std::vector<RGBColor>* colors,
                 std::vector<unsigned char>* lit);

} // namespace script

/** GPU batch inputs for one LED, filled exactly like EvaluateClipAtWorld fills LedView. */
void BuildScriptInputAtWorld(const Clip& clip,
                             float x, float y, float z,
                             float min_x, float max_x,
                             float min_y, float max_y,
                             float min_z, float max_z,
                             int seed,
                             script::GpuLedInput* out);

/** GPU batch inputs for one LED on the axis path (as EvaluateClipAtAxis). */
void BuildScriptInputAtAxis(const Clip& clip, float axis_pos, int seed, script::GpuLedInput* out);

void SetActionScriptDirectory(const filesystem::path& dir);
void ReloadActionScripts();
bool ActionScriptUsesWorld(const std::string& id);
bool EffectColorEnds(const std::string& id);
bool EffectPreviewPulse(const std::string& id);
bool EffectAxisAngle(const std::string& id);
std::string ClipFileId(const Clip& clip);

} // namespace Action
