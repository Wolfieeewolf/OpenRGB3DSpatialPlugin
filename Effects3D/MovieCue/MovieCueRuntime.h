// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include "EffectPacks/EffectPack.h"
#include "EffectPacks/EffectPackPlayer.h"
#include "filesystem.h"
#include <functional>
#include <string>
#include <vector>

namespace MovieCue
{

struct Catalog;

/** Plays catalog packs for Script API pulse/level edges (same model as Event Bindings). */
class CueRuntime
{
public:
    using ApplyFn = std::function<void(const EffectPack::Pack& pack, int local_ms, bool force_hw)>;
    using PrepareFn = std::function<void()>;

    void SetMovieEffectsDir(const filesystem::path& dir) { movie_effects_dir_ = dir; }
    void SetTimelinesDir(const filesystem::path& dir) { timelines_dir_ = dir; }
    void SetCatalog(const Catalog* catalog) { catalog_ = catalog; }
    void SetAllowPhotosensitive(bool allow) { allow_photosensitive_ = allow; }
    void SetApplyCallbacks(PrepareFn prepare, ApplyFn apply)
    {
        prepare_ = std::move(prepare);
        apply_ = std::move(apply);
    }

    void OnEffect(const std::string& effect_id, const std::string& edge, bool active);
    void StopAll();
    bool Tick(int dt_ms);
    bool IsPlaying() const { return !plays_.empty(); }
    const std::string& lastError() const { return last_error_; }
    const std::string& lastWarning() const { return last_warning_; }

private:
    struct ActivePlay
    {
        std::string effect_id;
        EffectPack::Pack pack;
        EffectPack::Player player;
        bool event_active = true;
        bool level = false;
    };

    void ApplyPlays(bool force_hw);

    filesystem::path movie_effects_dir_;
    filesystem::path timelines_dir_;
    const Catalog* catalog_ = nullptr;
    bool allow_photosensitive_ = false;
    std::vector<ActivePlay> plays_;
    PrepareFn prepare_;
    ApplyFn apply_;
    bool prepared_ = false;
    std::string last_error_;
    std::string last_warning_;
};

} // namespace MovieCue
