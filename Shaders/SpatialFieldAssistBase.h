// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include <QString>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>

#include "SpatialOffscreenGlPool.h"

/**
 * Shared per-effect owner for an offscreen field engine (volume or strip):
 * lazy engine creation, permanent GL-fail latch, once-per-frame prepare latch.
 * Effects call prepare exactly once per frame from PrepareGpuFields, then only
 * sample through the derived class. Derived classes add the size setter and
 * the sample accessors specific to their engine.
 */
template<typename EngineT>
class SpatialFieldAssistBase
{
public:
    void setFragmentBody(const QString& glsl_body)
    {
        if(body_set_ && body_ == glsl_body)
        {
            return;
        }
        body_ = glsl_body;
        body_set_ = true;
        if(engine_)
        {
            engine_->setFragmentBody(body_);
        }
    }

    QString lastError() const
    {
        return engine_ ? engine_->lastError() : QString();
    }

    /** Rebuild the field for this render_sequence. Idempotent for the same sequence. */
    bool prepare(std::uint64_t render_sequence, float time_sec, const float* params, int param_count)
    {
        if(unavailable_)
        {
            return false;
        }
        if(!body_set_ || body_.trimmed().isEmpty())
        {
            return false;
        }
        if(!SpatialOffscreenGlPool::hostContextReady())
        {
            return false;
        }
        if(!engine_)
        {
            engine_ = std::make_unique<EngineT>();
            engine_->setFragmentBody(body_);
            applyEngineSize(*engine_);
        }

        // Safety net if prepare is invoked more than once for the same sequence.
        // Media textures can change without time/params (GIF); derived assists may force refresh.
        const bool same_frame =
            (render_sequence != 0 && render_sequence == last_sequence_ && engine_->isAvailable());
        const bool same_preview_tick =
            (render_sequence == 0 && engine_->isAvailable() && std::fabs(time_sec - last_time_sec_) < 1e-4f);
        if((same_frame || same_preview_tick) && !engineNeedsRefresh())
        {
            return true;
        }

        typename EngineT::Params p;
        p.time_sec = time_sec;
        p.count = std::clamp(param_count, 0, EngineT::kMaxParams);
        for(int i = 0; i < p.count; ++i)
        {
            p.values[i] = params[i];
        }
        engine_->setParams(p);
        last_sequence_ = render_sequence;
        last_time_sec_ = time_sec;

        if(!engine_->ensureReady())
        {
            unavailable_ = true;
            return false;
        }
        return true;
    }

    bool isAvailable() const
    {
        return !unavailable_ && engine_ && engine_->isAvailable();
    }

protected:
    ~SpatialFieldAssistBase() = default;

    /** Push the derived size (resolution / width) onto a newly created engine. */
    virtual void applyEngineSize(EngineT& engine) = 0;

    /** True when the engine must re-render even if sequence/time are unchanged (e.g. media). */
    virtual bool engineNeedsRefresh() const { return false; }

    std::unique_ptr<EngineT> engine_;

private:
    std::uint64_t last_sequence_ = 0;
    float last_time_sec_ = -1e9f;
    bool unavailable_ = false;
    QString body_;
    bool body_set_ = false;
};
