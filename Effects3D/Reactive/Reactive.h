// SPDX-License-Identifier: GPL-2.0-only

#ifndef REACTIVE_H
#define REACTIVE_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"
#include "ReactiveInputTypes.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

class Reactive : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit Reactive(QWidget* parent = nullptr);

    EFFECT_REGISTERER_3D("Reactive", "Reactive", "Spatial", []() { return new Reactive; })

    EffectInfo3D GetEffectInfo() const override;
    void SetupCustomUI(QWidget* parent) override;
    void PrepareGpuFields(std::uint64_t render_sequence, float time_sec, const GridContext3D& grid) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;
    bool RequiresWorldSpaceCoordinates() const override { return false; }
    bool UsesSpatialSamplingQuantization() const override { return false; }
    bool SkipsSpatialSampleWarp() const override { return true; }

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private:
    enum TriggerMode
    {
        TRIGGER_PRESS = 0,
        TRIGGER_RELEASE,
        TRIGGER_WHILE_HELD,
        TRIGGER_COUNT
    };

    enum WaveMode
    {
        WAVE_ONCE = 0,
        WAVE_PULSE_2,
        WAVE_PULSE_4,
        WAVE_CONTINUAL,
        WAVE_COUNT
    };

    enum SpreadMode
    {
        SPREAD_SHELL = 0,
        SPREAD_PLANE,
        SPREAD_AXIS,
        SPREAD_CROSS,
        SPREAD_TRIAD,
        SPREAD_COUNT
    };

    enum SpreadOrient
    {
        ORIENT_A = 0,
        ORIENT_B,
        ORIENT_C,
        SPREAD_ORIENT_COUNT
    };

    enum LookMode
    {
        LOOK_RING = 0,
        LOOK_SOLID,
        LOOK_SHOCK,
        LOOK_FLASH,
        LOOK_DOUBLE,
        LOOK_COUNT
    };

    enum FootprintShape
    {
        SHAPE_SPHERE = 0,
        SHAPE_CUBE,
        SHAPE_OCTAHEDRON,
        SHAPE_CYLINDER,
        SHAPE_TRIANGLE,
        SHAPE_SQUARE,
        SHAPE_PENTAGON,
        SHAPE_HEXAGON,
        SHAPE_COUNT
    };

    struct WaveImpact
    {
        Vector3D origin{};
        float    birth_time = 0.0f;
        float    strength   = 1.0f;
        float    spread     = 0.0f;
        float    spacing    = 0.0f;
        uint32_t color_slot = 0;
    };

    static const char* TriggerName(int mode);
    static const char* WaveName(int mode);
    static const char* SpreadName(int mode);
    static const char* SpreadOrientName(int spread, int orient);
    static const char* LookName(int mode);
    static const char* ShapeName(int mode);
    static uint64_t OriginKey(const Vector3D& p);

    bool SourceEnabled(ReactiveSourceKind kind) const;
    bool ShouldSpawnOnEdge(bool down) const;
    bool UsesHeldRepeats() const;
    int  PulseBudget() const;
    float RepeatIntervalSec() const;
    void SpawnWave(const Vector3D& origin,
                   float time,
                   float strength,
                   float spread,
                   float spacing);
    void TickWaves(float time, const GridContext3D& grid);

    bool listen_keyboard_ = true;
    bool listen_mouse_ = true;
    bool listen_gamepad_ = true;
    int  trigger_mode_ = TRIGGER_PRESS;
    int  wave_mode_ = WAVE_ONCE;
    int  spread_mode_ = SPREAD_SHELL;
    int  spread_orient_ = ORIENT_B;
    int  look_mode_ = LOOK_RING;
    int  footprint_shape_ = SHAPE_SPHERE;
    int  repeat_rate_deci_hz_ = 5;
    int  travel_pct_ = 40;
    int  ring_width_pct_ = 8;

    std::vector<WaveImpact> waves_;
    std::unordered_map<uint64_t, float> last_spawn_time_;
    std::unordered_map<uint64_t, int> spawn_counts_;
    uint32_t next_color_slot_ = 0;
    float last_tick_time_ = 0.0f;
    bool have_tick_time_ = false;
};

#endif
