// SPDX-License-Identifier: GPL-2.0-only

#include "FolderVolumeEffect.h"
#include "EffectSliderRow.h"
#include "EffectUiRows.h"
#include "MediaTextureAmbienceBlock.h"
#include "MediaTextureEffectUtils.h"
#include "AudioReactiveUi.h"
#include "SpatialPatternKernels/SpatialPatternKernels.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMovie>
#include <QMutexLocker>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

namespace
{

bool IsMediaAmbienceSliderKey(const std::string& key)
{
    return key == "ambience_dist_falloff" || key == "ambience_falloff_curve"
        || key == "ambience_edge_soft" || key == "ambience_propagation"
        || key == "motion_scroll" || key == "motion_warp" || key == "motion_phase"
        || key == "media_resolution";
}

} // namespace
void FolderVolumeEffect::SetupCustomUI(QWidget* parent)
{
    if(spec_.patterns.empty() && spec_.sliders.empty() && spec_.combos.empty()
       && !spec_.uses_media && !spec_.uses_audio)
    {
        return;
    }
    QWidget* panel = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    const auto on_changed = [this]() { emit ParametersChanged(); };

    // Target layout for sliders/combos; redirected into a collapsible when audio is active.
    QVBoxLayout* step_target = layout;

    if(spec_.uses_audio)
    {
        // 1. Listen / frequency band section (shared with global: audio)
        AudioReactiveUi::AppendStandardFrequencyBandSection(layout, audio_settings, this, on_changed);

        // Beat wave section for pulse effects
        if(spec_.audio_preset == "beat" || spec_.audio_preset == "low_punch")
        {
            AudioReactiveUi::AudioBeatUiOptions beat_opts;
            beat_opts.include_pulse_color = true;
            beat_opts.include_shell_falloff = true;
            beat_opts.include_spread_fade = true;
            AudioReactiveUi::AppendStandardBeatWaveSection(layout, audio_settings, this, on_changed, beat_opts);
        }

        // 2. Effect-specific controls inside a collapsible "Effect" section
        if(!spec_.sliders.empty() || !spec_.combos.empty() || !spec_.patterns.empty())
        {
            QVBoxLayout* effect_body = EffectUiRows::AppendCollapsibleSectionBody(
                layout, QStringLiteral("Effect"));
            QWidget* effect_section = EffectUiRows::NewEffectPanel();
            effect_section->setObjectName(
                QString::fromStdString(spec_.class_name + "EffectSettings"));
            step_target = EffectUiRows::PanelLayout(effect_section);
            if(effect_body)
                effect_body->addWidget(effect_section);
            else
                layout->addWidget(effect_section);
        }
    }

    if(spec_.uses_media)
    {
        auto* pick_row = new QHBoxLayout();
        pick_row->setContentsMargins(0, 0, 0, 0);
        browse_button = new QPushButton(tr("Choose image / GIF..."), panel);
        browse_button->setObjectName(QStringLiteral("browseButton"));
        path_label = new QLabel(media_path.isEmpty() ? tr("(no file)") : media_path, panel);
        path_label->setObjectName(QStringLiteral("pathLabel"));
        path_label->setWordWrap(true);
        path_label->setMinimumWidth(120);
        pick_row->addWidget(browse_button);
        pick_row->addWidget(path_label);
        layout->addLayout(pick_row);
        connect(browse_button, &QPushButton::clicked, this, &FolderVolumeEffect::OnBrowseMedia);
    }
    for(const FolderVolumeSpec::Step& step : spec_.steps)
    {
        if(step.kind == 2)
        {
            const FolderVolumeSpec::Combo& combo = spec_.combos[(size_t)step.index];
            EffectLabeledComboRow* row = EffectUiRows::AppendComboRow(step_target, combo.label);
            QComboBox* box = row->combo();
            for(int i = 0; i < (int)combo.options.size(); ++i)
            {
                box->addItem(combo.options[(size_t)i].name);
                if(!combo.options[(size_t)i].tip.isEmpty())
                {
                    box->setItemData(i, combo.options[(size_t)i].tip, Qt::ToolTipRole);
                }
            }
            const int combo_count = std::max(1, box->count());
            const int start = std::clamp(combo_values[(size_t)step.index], 0, combo_count - 1);
            box->setCurrentIndex(start);
            combo_values[(size_t)step.index] = start;
            connect(box, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, index = step.index](int value) {
                combo_values[(size_t)index] = std::clamp(value, 0, std::max(0, (int)spec_.combos[(size_t)index].options.size() - 1));
                emit ParametersChanged();
            });
            continue;
        }
        if(step.kind == 0)
        {
            EffectLabeledComboRow* pattern_row = EffectUiRows::AppendComboRow(step_target, spec_.pattern_label);
            pattern_row->setObjectName(QStringLiteral("patternRow"));
            pattern_combo = pattern_row->combo();
            for(int i = 0; i < (int)spec_.patterns.size(); ++i)
            {
                pattern_combo->addItem(spec_.patterns[(size_t)i].name);
                if(!spec_.patterns[(size_t)i].tip.isEmpty())
                {
                    pattern_combo->setItemData(i, spec_.patterns[(size_t)i].tip, Qt::ToolTipRole);
                }
            }
            const int pattern_count = std::max(1, pattern_combo->count());
            pattern_combo->setCurrentIndex(std::clamp(pattern_index, 0, pattern_count - 1));
            pattern_index = pattern_combo->currentIndex();
            connect(pattern_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
                pattern_index = std::clamp(index, 0, std::max(0, pattern_combo->count() - 1));
                emit ParametersChanged();
            });
            continue;
        }
        const int i = step.index;
        const FolderVolumeSpec::Slider& slider = spec_.sliders[(size_t)i];
        if(spec_.uses_media && IsMediaAmbienceSliderKey(slider.key))
        {
            continue;
        }
        EffectSliderRow* row = EffectUiRows::AppendSliderRow(
            step_target,
            slider.label,
            slider.min,
            slider.max,
            slider_values[(size_t)i],
            slider.tip);
        row->bindValueChanged(
            this,
            [this, i](int value) { slider_values[(size_t)i] = value; },
            [slider](int value) {
                return slider.show_percent ? QString::number(value) + QStringLiteral("%") : QString::number(value);
            },
            [this]() { emit ParametersChanged(); });
    }
    if(spec_.uses_media)
    {
        BindMediaAmbienceBlock(layout);
    }
    if(spec_.uses_audio)
    {
        // 3. Feel / response section
        AudioReactiveUi::AudioResponseUiOptions response_opts;
        if(spec_.audio_preset == "level")
        {
            response_opts.include_falloff = true;
            response_opts.falloff_label = QStringLiteral("Fill edge:");
            response_opts.falloff_slider_max = 500;
            response_opts.falloff_tooltip =
                QStringLiteral("Steepness of the lit region versus dark below the fill boundary.");
        }
        else if(spec_.audio_preset == "paint" || spec_.audio_preset == "spectrum")
        {
            response_opts.include_falloff = true;
        }
        else if(spec_.audio_media == "spectrogram")
        {
            response_opts.include_falloff = true;
            response_opts.falloff_label = QStringLiteral("Bar edge:");
            response_opts.falloff_slider_min = 20;
            response_opts.falloff_slider_max = 800;
            response_opts.falloff_tooltip =
                QStringLiteral("Sharpness of bar top edge (spectrogram ignores this).");
        }
        else if(spec_.audio_preset == "beat" || spec_.audio_preset == "low_punch")
        {
            response_opts.use_onset_smoothing_label = true;
        }
        else if(spec_.audio_preset == "high_sparkle")
        {
            response_opts.include_falloff = true;
            response_opts.falloff_label = QStringLiteral("Shell edge:");
            response_opts.falloff_slider_max = 500;
            response_opts.falloff_tooltip =
                QStringLiteral("Thickness / steepness of the hull and particle glow.");
        }
        AudioReactiveUi::AppendStandardResponseSection(layout, audio_settings, this, on_changed, response_opts);

        // 4. Color section
        AudioReactiveUi::AppendAudioSectionBody(layout, QStringLiteral("Color"));
        AudioReactiveUi::AppendAudioPulseColorModeRow(layout, audio_settings, this, on_changed);
    }
    AddWidgetToParent(panel, parent);
}

float FolderVolumeEffect::ParamValue(const std::string& name, float detail, float size) const
{
    if(name == "progress")
    {
        return progress;
    }
    if(name == "detail")
    {
        return detail;
    }
    if(name == "size")
    {
        return size;
    }
    if(name == "pattern")
    {
        return (float)pattern_index;
    }
    if(name == "frequency")
    {
        return GetNormalizedFrequency();
    }
    if(name == "freq_scale")
    {
        return std::min(8.0f, detail * 0.8f / std::fmax(0.1f, size));
    }
    if(name == "spiral_freq")
    {
        return detail * 0.15f / std::fmax(0.1f, size);
    }
    if(name == "detail_norm")
    {
        return std::max(0.05f, GetNormalizedDetail());
    }
    if(name == "size_floor")
    {
        return std::max(0.2f, size);
    }
    if(name == "hue_time")
    {
        return time_sec * GetColorCycleHz();
    }
    if(name == "progress_tau")
    {
        return CalculateProgress(time_sec) * 6.2831853f;
    }
    if(name == "plane")
    {
        return (float)GetPlane();
    }
    if(name == "freq_spin")
    {
        return time_sec * GetColorCycleHz() * 6.2831853f * (progress / std::fmax(0.0001f, CalculateProgress(time_sec)));
    }
    if(name.rfind("sign ", 0) == 0)
    {
        return ComboIndex(name.substr(5)) == 0 ? 1.0f : -1.0f;
    }
    if(name == "time")
    {
        return time_sec;
    }
    if(name == "motion")
    {
        return GetNormalizedSpeed();
    }
    if(name == "motion_hz")
    {
        return GetMotionHz();
    }
    if(name.rfind("combo ", 0) == 0)
    {
        return (float)ComboIndex(name.substr(6));
    }
    if(name.rfind("div10 ", 0) == 0)
    {
        return SliderRaw(name.substr(6)) / 10.0f;
    }
    if(name.rfind("cent ", 0) == 0)
    {
        return SliderRaw(name.substr(5)) / 100.0f;
    }
    if(name.rfind("turn ", 0) == 0)
    {
        return SliderRaw(name.substr(5)) / 360.0f;
    }
    if(name.rfind("deg ", 0) == 0)
    {
        return SliderRaw(name.substr(4)) * 0.017453292f;
    }
    if(name == "flow_mul" || name == "flow_progress")
    {
        float mul = 1.0f;
        if(!spec_.flow.empty())
        {
            const int index = std::clamp(pattern_index, 0, (int)spec_.flow.size() - 1);
            mul = spec_.flow[(size_t)index];
        }
        if(name == "flow_mul")
        {
            return mul;
        }
        return CalculateProgress(time_sec) * mul;
    }
    if(name == "progress_raw")
    {
        return CalculateProgress(time_sec);
    }
    if(name == "progress_wrap")
    {
        return std::fmod(progress + 1000.0f, 1.0f);
    }
    if(name == "progress_wrap1")
    {
        return std::fmod(progress + 1.0f, 1.0f);
    }
    if(name == "travel_progress")
    {
        float p = progress + phase_shift;
        p = std::fmod(p, 1.0f);
        if(p < 0.0f)
        {
            p += 1.0f;
        }
        return p;
    }
    if(name == "size_star")
    {
        return std::clamp(size, 0.35f, 2.5f);
    }
    if(name == "size_ring")
    {
        return std::clamp(size, 0.25f, 2.5f);
    }
    if(name == "size_depth")
    {
        return std::clamp(size, 0.15f, 2.5f);
    }
    if(name == "anim_time")
    {
        return GetSpeed() == 0 ? 0.0f : time_sec;
    }
    if(name == "hue_scroll1")
    {
        const float anim = GetSpeed() == 0 ? 0.0f : time_sec;
        return std::fmod(anim * GetColorCycleHz() + 1.0f, 1.0f);
    }
    if(name == "hue_scroll_s")
    {
        return std::fmod(time_sec * GetColorCycleHz() * speed_mul + 1000.0f, 1.0f);
    }
    if(name == "phase37")
    {
        return std::fmod(time_sec * 0.37f * speed_mul + 1000.0f, 1.0f);
    }
    if(name == "ndetail")
    {
        return std::clamp(GetNormalizedDetail(), 0.05f, 1.0f);
    }
    if(name == "freq_n")
    {
        return std::clamp(GetNormalizedFrequency(), 0.0f, 1.0f);
    }
    if(name == "motion_clock")
    {
        return time_sec * std::max(0.0f, GetMotionHz() * speed_mul);
    }
    if(name == "speed_mul")
    {
        return std::max(0.15f, speed_mul);
    }
    if(name == "tight_mul")
    {
        return std::max(0.25f, tight_mul);
    }
    if(name == "tight_inv")
    {
        return 1.0f / std::max(0.25f, tight_mul);
    }
    if(name == "path_axis")
    {
        return (float)std::clamp(GetPathAxis(), 0, 2);
    }
    if(name == "atlas_sx")
    {
        return atlas_sx;
    }
    if(name == "atlas_sy")
    {
        return atlas_sy;
    }
    if(name == "atlas_sz")
    {
        return atlas_sz;
    }
    if(name == "atlas_ax")
    {
        return atlas_ax;
    }
    if(name == "atlas_az")
    {
        return atlas_az;
    }
    if(name == "strip_unfold")
    {
        return (float)GetEffectStripColormapUnfold();
    }
    if(name == "strip_dir")
    {
        return GetEffectStripColormapDirectionDeg();
    }
    if(name == "strip_reps")
    {
        return std::max(1.0f, GetEffectStripColormapRepeats());
    }
    if(name == "room_ymin") return room_ymin;
    if(name == "room_ymax") return room_ymax;
    if(name == "room_xmin") return room_xmin;
    if(name == "room_xmax") return room_xmax;
    if(name == "room_zmin") return room_zmin;
    if(name == "room_zmax") return room_zmax;
    if(name == "cone_clock")
    {
        const float rate = std::max(0.2f, SliderRaw("cone_spot_motion") / 100.0f);
        return time_sec * GetMotionHz() * speed_mul * rate;
    }
    if(name == "cone_scale")
    {
        const float raw = std::max(0.02f, SliderRaw("cone_spot_scale") / 1000.0f);
        return std::max(1e-5f, raw * (0.5f + 0.5f * size));
    }
    if(name.rfind("milli ", 0) == 0)
    {
        return SliderRaw(name.substr(6)) / 1000.0f;
    }
    if(name == "surface_mask")
    {
        int mask = GetSurfaceMask();
        if(mask == 0)
            mask = 1;
        return (float)mask;
    }
    if(name == "shell_sigma")
    {
        const float band = std::max(GetBandThickness() / 100.0f, 0.03f);
        const float size_m = std::clamp(size, 0.08f, 3.0f);
        return std::clamp(band * (0.85f + 0.35f * size_m), 0.03f, 0.85f);
    }
    if(name == "shell_amp")
    {
        const float amp = SliderRaw("shellpattern_wave_amplitude") / 100.0f;
        const float size_m = std::clamp(size, 0.08f, 3.0f);
        const float size_boost = std::clamp(0.75f + 0.45f * size_m, 0.75f, 2.2f);
        return std::clamp(amp * tight_mul * size_boost, 0.25f, 2.5f);
    }
    if(name == "sa_motion")
    {
        const int style = ComboIndex("style");
        if(style > 0)
            return 0.0f;
        return (float)std::clamp(ComboIndex("motion"), 0, 6);
    }
    if(name == "sa_h_pct")
    {
        const float scale_n = std::max(0.2f, GetNormalizedScale());
        return std::clamp(0.12f + 0.40f * std::min(scale_n, 1.75f), 0.08f, 0.98f);
    }
    if(name == "sa_sigma")
    {
        const float tm = std::max(0.25f, tight_mul);
        return std::max((GetBandThickness() / 100.0f) * 0.5f, 0.02f) / tm;
    }
    if(name == "sa_freq")
    {
        const float d = std::max(0.05f, detail) ;
        return std::clamp(0.28f + d * 0.22f, 0.22f, 3.0f);
    }
    if(name == "sa_feature")
    {
        return std::clamp(size, 0.45f, 3.0f);
    }
    if(name == "gthickness")
    {
        return GetBandThickness() / 100.0f;
    }
    if(name == "wave_freq")
    {
        return std::clamp(0.2f + GetNormalizedFrequency() * 3.8f, 0.2f, 4.0f);
    }
    if(name.rfind("gcount ", 0) == 0)
    {
        const QStringList parts = QString::fromStdString(name).split(QLatin1Char(' '), Qt::SkipEmptyParts);
        int lo = 1;
        int hi = 48;
        if(parts.size() >= 3)
        {
            lo = parts[1].toInt();
            hi = parts[2].toInt();
        }
        return (float)std::clamp(GetInstanceCount(), lo, hi);
    }
    if(name == "hue_density")
    {
        return GetHueBandDensity();
    }
    if(name == "ball_sim")
    {
        return time_sec * GetMotionHz() * 2.8f * speed_mul;
    }
    if(name == "ball_radius")
    {
        return std::clamp(0.045f + 0.11f * size, 0.03f, 0.22f);
    }
    if(name == "const1")
    {
        return 1.0f;
    }
    if(name == "dna_radius")
    {
        const float size_m = std::max(0.25f, size);
        return std::clamp((SliderRaw("dna_helix_radius_pct") / 100.0f) * (0.75f + 0.50f * size_m), 0.10f, 0.95f);
    }
    if(name == "dna_twists")
    {
        const float detail_n = std::max(0.05f, GetNormalizedDetail());
        return std::clamp((SliderRaw("dna_helix_twists") / 100.0f) * (0.85f + 0.55f * detail_n) * (0.55f + 0.90f * GetNormalizedFrequency()), 0.4f, 28.0f);
    }
    if(name == "dna_thick")
    {
        const float size_m = std::max(0.25f, size);
        return std::clamp((SliderRaw("dna_helix_thickness_pct") / 100.0f) * (0.55f + 0.55f * size_m), 0.012f, 0.52f);
    }
    if(name == "dna_rung")
    {
        return std::clamp(SliderRaw("dna_helix_rung_pct") / 100.0f, 0.0f, 1.0f);
    }
    if(name == "psize")
    {
        return std::clamp(std::max(0.15f, SliderRaw("pf_size") / 100.0f) * size * 0.26f, 0.030f, 0.30f);
    }
    if(name == "pthick")
    {
        return std::clamp(std::max(0.20f, SliderRaw("pf_thickness") / 100.0f) * 0.130f, 0.022f, 0.22f);
    }
    if(name == "bubble_thick")
    {
        const float detail_s = std::max(0.05f, GetScaledDetail());
        return std::clamp(std::max(0.02f, SliderRaw("bubble_thickness") / 100.0f) * 0.11f / std::max(0.35f, detail_s), 0.014f, 0.14f);
    }
    if(name == "bubble_rise")
    {
        return std::clamp(SliderRaw("rise_speed") / 100.0f, 0.1f, 4.0f) * GetMotionHz() * speed_mul;
    }
    if(name == "bubble_interval")
    {
        return std::clamp(SliderRaw("spawn_interval") / 100.0f, 0.25f, 2.5f) / std::max(0.85f, 0.85f + 0.45f * GetNormalizedSpeed());
    }
    if(name == "bubble_radius")
    {
        return std::clamp(std::clamp(SliderRaw("max_radius") / 100.0f, 0.5f, 3.5f) * size * 0.28f, 0.06f, 0.72f);
    }
    if(name == "depth_pos")
    {
        return std::fmod(CalculateProgress(time_sec) + time_sec * GetColorCycleHz() * 0.35f + 1000.0f, 1.0f);
    }
    if(name == "travel_freq")
    {
        return std::min(6.0f, std::max(0.02f, 0.15f + GetNormalizedFrequency() * 2.4f));
    }
    if(name == "harmonic_motion")
    {
        const float flow = std::clamp(SliderRaw("harmonic_flow_amount") / 100.0f, 0.4f, 2.5f);
        return std::clamp(GetMotionHz() * flow, 0.0f, 2.5f);
    }
    if(name == "harmonic_freq")
    {
        const float detail_n = std::max(0.05f, GetNormalizedDetail());
        const float size_m = std::max(0.25f, size);
        return std::clamp((1.2f + GetNormalizedFrequency() * 22.0f + detail_n * 10.0f) * (0.65f + 0.55f * size_m), 0.6f, 32.0f);
    }
    if(name == "harmonic_density")
    {
        return std::clamp(0.85f + 0.65f * std::max(0.25f, size), 0.4f, 2.4f);
    }
    if(name.rfind("raw ", 0) == 0)
    {
        return (float)SliderRaw(name.substr(4));
    }
    if(name.rfind("unit ", 0) == 0)
    {
        return SliderRaw(name.substr(5)) / 100.0f;
    }
    if(name == "audio_fill")
    {
        return audio_fill_cached;
    }
    if(name == "audio_band_count")  { return audio_band_count_cached; }
    if(name == "audio_roll_phase")  { return audio_roll_phase_cached; }
    if(name == "audio_bar_edge")    { return audio_bar_edge_cached; }
    if(name == "audio_strip_scroll"){ return audio_strip_scroll_cached; }
    if(name == "audio_falloff")     { return std::clamp(audio_settings.falloff, 0.2f, 5.0f); }
    if(name == "audio_beat_mode")   { return (float)audio_settings.beat_wave_mode; }
    if(name == "audio_pulse_speed") { return audio_pulse_speed_cached; }
    if(name == "audio_radius_basis"){ return audio_radius_basis_cached; }
    if(name == "audio_pulse_half_w"){ return audio_pulse_half_w_cached; }
    if(name == "audio_max_travel")  { return audio_max_travel_cached; }
    if(name == "audio_pulse_decay") { return audio_pulse_decay_cached; }
    if(name == "audio_pulse_hw")    { return audio_pulse_hw_cached; }
    if(name == "audio_pulse_hh")    { return audio_pulse_hh_cached; }
    if(name == "audio_pulse_hd")    { return audio_pulse_hd_cached; }
    if(name == "audio_hi_drive")    { return audio_hi_drive_cached; }
    if(name == "audio_time_e")
    {
        return time_sec * GetNormalizedSpeed() * speed_mul;
    }
    if(name == "size_audio")
    {
        if(spec_.class_name == "AudioPaintBrush")
            return std::clamp(GetNormalizedSize(), 0.35f, 2.5f);
        return std::max(0.35f, GetNormalizedSize());
    }
    if(name == "audio_wave_freq")
    {
        return std::max(0.2f, 0.35f + GetNormalizedFrequency() * 2.2f * tight_mul);
    }
    if(name == "audio_bass")
    {
        return ApplyAudioIntensity(audio_band_lo, audio_settings);
    }
    if(name == "audio_mid_val")
    {
        return ApplyAudioIntensity(audio_band_mid, audio_settings);
    }
    if(name == "audio_hi_val")
    {
        return ApplyAudioIntensity(audio_band_hi, audio_settings);
    }
    if(name == "audio_tboost")
    {
        return audio_time_boost;
    }
    if(name == "audio_hue_scroll")
    {
        return std::fmod(time_sec * GetColorCycleHz() * speed_mul + 1000.0f, 1.0f);
    }
    return 0.0f;
}

int FolderVolumeEffect::SliderRaw(const std::string& key) const
{
    for(size_t i = 0; i < spec_.sliders.size(); ++i)
    {
        if(spec_.sliders[i].key == key)
        {
            return slider_values[i];
        }
    }
    return 0;
}

int FolderVolumeEffect::ComboIndex(const std::string& key) const
{
    if(key == spec_.pattern_key || key == "pattern")
    {
        return pattern_index;
    }
    for(size_t i = 0; i < spec_.combos.size(); ++i)
    {
        if(spec_.combos[i].key == key)
        {
            return combo_values[i];
        }
    }
    return 0;
}

bool FolderVolumeEffect::UsesSpatialSamplingQuantization() const
{
    return !spec_.uses_media;
}

void FolderVolumeEffect::SetSpeed(unsigned int speed)
{
    SpatialEffect3D::SetSpeed(speed);
    if(spec_.uses_media)
    {
        ApplyGifPlaybackSpeed();
    }
}

void FolderVolumeEffect::BindMediaAmbienceBlock(QVBoxLayout* layout)
{
    const auto on_changed = [this]() { emit ParametersChanged(); };
    const auto int_format = [](int v) { return QString::number(v); };
    auto* media = new MediaTextureAmbienceBlock(layout->parentWidget());
    media->setObjectName(QStringLiteral("mediaBlock"));
    layout->addWidget(media);

    auto bind_slider = [&](EffectSliderRow* row, const char* key, const QString& caption, const QString& tip) {
        int idx = -1;
        for(size_t i = 0; i < spec_.sliders.size(); ++i)
        {
            if(spec_.sliders[i].key == key)
            {
                idx = (int)i;
                break;
            }
        }
        if(idx < 0 || !row)
        {
            return;
        }
        const FolderVolumeSpec::Slider& spec_slider = spec_.sliders[(size_t)idx];
        row->setCaptionText(caption);
        row->configure(spec_slider.min, spec_slider.max, slider_values[(size_t)idx], tip);
        row->bindValueChanged(
            this,
            [this, idx](int v) {
                slider_values[(size_t)idx] = std::clamp(v, spec_.sliders[(size_t)idx].min, spec_.sliders[(size_t)idx].max);
            },
            int_format,
            on_changed);
    };

    bind_slider(media->ambienceDistRow(), "ambience_dist_falloff", tr("Distance dim:"),
                tr("Darkens LEDs farther from the effect origin."));
    bind_slider(media->ambienceCurveRow(), "ambience_falloff_curve", tr("Falloff curve:"),
                tr("Shapes how fast the vignette drops (soft to hard)."));
    bind_slider(media->ambienceEdgeRow(), "ambience_edge_soft", tr("Edge fade:"),
                tr("Fades toward room walls/floor/ceiling."));
    bind_slider(media->ambiencePropRow(), "ambience_propagation", tr("Wave delay:"),
                tr("Motion lags farther from the origin."));
    bind_slider(media->motionScrollRow(), "motion_scroll", tr("Scroll:"),
                tr("Pans the texture continuously. 0 = still."));
    bind_slider(media->motionWarpRow(), "motion_warp", tr("Warp:"),
                tr("Waves the UV. 0 = off."));
    bind_slider(media->motionPhaseRow(), "motion_phase", tr("Phase:"),
                tr("Steers scroll direction and warp tempo."));
    bind_slider(media->mediaResolutionRow(), "media_resolution", tr("Resolution:"),
                tr("Per-layer sampling (0 = blocky, 100 = full)."));

    tile_repeat_check = media->tileRepeatCheck();
    tile_repeat_check->setChecked(tile_repeat_enabled);
    tile_repeat_check->setToolTip(tr(
        "On = tile/repeat the image. Off = one copy (motion can still loop)."));
    connect(tile_repeat_check, &QCheckBox::toggled, this, [this](bool on) {
        tile_repeat_enabled = on;
        emit ParametersChanged();
    });
}

void FolderVolumeEffect::OnBrowseMedia()
{
    const QString path = QFileDialog::getOpenFileName(
        this,
        tr("Open image or GIF"),
        QString(),
        tr("Images (*.png *.jpg *.jpeg *.bmp *.webp);;GIF (*.gif);;All files (*.*)"));
    if(path.isEmpty())
    {
        return;
    }
    LoadMediaFile(path);
}

void FolderVolumeEffect::OnGifFrameTimerTimeout()
{
    if(!movie || !media_is_gif)
    {
        return;
    }
    const int fc = movie->frameCount();
    if(fc <= 0)
    {
        if(gif_frame_timer)
        {
            gif_frame_timer->stop();
        }
        return;
    }
    const int cur = movie->currentFrameNumber();
    const int next = (cur + 1) % fc;
    {
        QMutexLocker lock(&display_mutex);
        previous_display_frame = display_frame;
        last_gif_step_ms = QDateTime::currentMSecsSinceEpoch();
    }
    (void)movie->jumpToFrame(next);
    PublishDisplayFrame(movie->currentImage());
}

void FolderVolumeEffect::ClearMovie()
{
    if(gif_frame_timer)
    {
        gif_frame_timer->stop();
    }
    if(movie)
    {
        movie->stop();
        disconnect(movie, nullptr, this, nullptr);
        delete movie;
        movie = nullptr;
    }
    media_is_gif = false;
    last_gif_step_ms = 0;
    gif_step_interval_ms = 0;
    QMutexLocker lock(&display_mutex);
    previous_display_frame.reset();
}

void FolderVolumeEffect::PublishDisplayFrame(const QImage& src)
{
    if(src.isNull())
    {
        QMutexLocker lock(&display_mutex);
        previous_display_frame.reset();
        display_frame.reset();
        return;
    }
    QImage conv = src.convertToFormat(QImage::Format_ARGB32);
    constexpr int kMaxSampleEdge = 1536;
    if(conv.width() > kMaxSampleEdge || conv.height() > kMaxSampleEdge)
    {
        conv = conv.scaled(kMaxSampleEdge, kMaxSampleEdge, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        conv = conv.convertToFormat(QImage::Format_ARGB32);
    }
    std::shared_ptr<QImage> shot = std::make_shared<QImage>(std::move(conv));
    QMutexLocker lock(&display_mutex);
    display_frame = std::move(shot);
}

void FolderVolumeEffect::LoadMediaFile(const QString& path)
{
    media_path = path;
    if(path_label)
    {
        path_label->setText(path.isEmpty() ? tr("(no file)") : path);
    }

    ClearMovie();
    {
        QMutexLocker lock(&display_mutex);
        previous_display_frame.reset();
        display_frame.reset();
    }

    if(path.isEmpty())
    {
        emit ParametersChanged();
        return;
    }

    const bool is_gif = path.endsWith(QLatin1String(".gif"), Qt::CaseInsensitive);
    if(is_gif)
    {
        movie = new QMovie(path, QByteArray(), this);
        if(!movie->isValid())
        {
            delete movie;
            movie = nullptr;
            if(path_label)
            {
                path_label->setText(tr("Invalid or unsupported GIF"));
            }
            emit ParametersChanged();
            return;
        }
        media_is_gif = true;
        movie->start();
        movie->setPaused(true);
        (void)movie->jumpToFrame(0);
        PublishDisplayFrame(movie->currentImage());
        {
            QMutexLocker lock(&display_mutex);
            previous_display_frame.reset();
        }
        last_gif_step_ms = 0;
        ApplyGifPlaybackSpeed();
    }
    else
    {
        QImage img(path);
        if(img.isNull())
        {
            emit ParametersChanged();
            return;
        }
        PublishDisplayFrame(img);
    }

    emit ParametersChanged();
}

void FolderVolumeEffect::ApplyGifPlaybackSpeed()
{
    if(!movie || !media_is_gif || !gif_frame_timer)
    {
        if(gif_frame_timer)
        {
            gif_frame_timer->stop();
        }
        return;
    }
    const unsigned int fps = GetSpeed();
    if(fps == 0)
    {
        gif_frame_timer->stop();
        movie->setPaused(true);
        gif_step_interval_ms = 0;
        return;
    }
    movie->setPaused(true);
    const int interval_ms = std::max(1, (int)std::lround(1000.0 / (double)fps));
    gif_frame_timer->stop();
    gif_frame_timer->setInterval(interval_ms);
    gif_step_interval_ms = interval_ms;
    gif_frame_timer->start();
}

