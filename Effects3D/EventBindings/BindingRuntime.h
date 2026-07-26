// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include "EventBinding.h"
#include "EffectPacks/EffectPack.h"
#include "EffectPacks/EffectPackPlayer.h"
#include "LEDPosition3D.h"
#include "RGBControllerInterface.h"
#include "filesystem.h"
#include <functional>
#include <string>
#include <vector>

class ZoneManager3D;

namespace EffectBinding
{

class BindingRuntime
{
public:
    using ApplyFn = std::function<void(const EffectPack::Pack& pack, int local_ms, bool force_hw)>;
    using PrepareFn = std::function<void()>;

    void SetPacksDir(const filesystem::path& dir) { packs_dir_ = dir; }
    void SetDocument(Document doc) { doc_ = std::move(doc); }
    const Document& document() const { return doc_; }
    Document* mutableDocument() { return &doc_; }

    void SetApplyCallbacks(PrepareFn prepare, ApplyFn apply)
    {
        prepare_ = std::move(prepare);
        apply_ = std::move(apply);
    }

    void OnEvent(const std::string& source, const std::string& event, bool active);
    void StopAll();

    /** Advance active plays; returns true if any frame was applied. */
    bool Tick(int dt_ms);

    bool IsPlaying() const { return !plays_.empty(); }

private:
    struct ActivePlay
    {
        std::string binding_id;
        std::string source;
        std::string event;
        EffectPack::Pack pack;
        EffectPack::Player player;
        bool event_active = true;
    };

    void StartBinding(const Binding& binding, bool event_active);
    void StopMatching(const std::string& source, const std::string& event);
    void ApplyPlays(bool force_hw);

    filesystem::path packs_dir_;
    Document doc_;
    std::vector<ActivePlay> plays_;
    PrepareFn prepare_;
    ApplyFn apply_;
    bool prepared_ = false;
};

} // namespace EffectBinding
