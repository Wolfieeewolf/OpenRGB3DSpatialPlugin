// SPDX-License-Identifier: GPL-2.0-only

#include "FolderVolumeEffect.h"
#include "EffectListManager3D.h"
#include "EffectSliderRow.h"
#include "EffectStratumBlend.h"
#include "EffectUiRows.h"
#include "MediaTextureAmbienceBlock.h"
#include "MediaTextureEffectUtils.h"
#include "OpenRGB3DSpatialPlugin.h"
#include "PluginSettingsPaths.h"
#include "Shaders/SpatialShaderCatalog.h"
#include "SpatialKernelColormap.h"
#include "SpatialLayerCore.h"
#include "SpatialPatternKernels/SpatialPatternKernels.h"
#include "Effects3D/EffectColorUtils.h"
#include "Effects3D/AudioReactiveUi.h"
#include "Audio/AudioInputManager.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDirIterator>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMovie>
#include <QMutexLocker>
#include <QPushButton>
#include <QTextStream>
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

RGBColor ColorFromHex(const QString& text)
{
    bool ok = false;
    const unsigned int rgb = text.toUInt(&ok, 16);
    if(!ok)
    {
        return 0;
    }
    const unsigned int r = (rgb >> 16) & 0xFF;
    const unsigned int g = (rgb >> 8) & 0xFF;
    const unsigned int b = rgb & 0xFF;
    return (b << 16) | (g << 8) | r;
}

FolderVolumeSpec ReadSpec(const QString& path)
{
    FolderVolumeSpec spec;
    spec.shader_id = QFileInfo(path).completeBaseName().toStdString();
    spec.ui_name = spec.shader_id;
    QFile file(path);
    if(!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return spec;
    }
    QTextStream stream(&file);
    while(!stream.atEnd())
    {
        const QString line = stream.readLine().trimmed();
        if(line.isEmpty() || line.startsWith(QLatin1Char('#')))
        {
            continue;
        }
        const int colon = line.indexOf(QLatin1Char(':'));
        if(colon <= 0)
        {
            break;
        }
        const QString key = line.left(colon).trimmed();
        QString val = line.mid(colon + 1).trimmed();
        QString tip;
        const int bar = val.indexOf(QLatin1Char('|'));
        if(bar >= 0 && key == QStringLiteral("pattern"))
        {
            tip = val.mid(bar + 1).trimmed();
            val = val.left(bar).trimmed();
        }
        if(key == QStringLiteral("global"))
        {
            const QStringList names = val.split(QLatin1Char(' '), Qt::SkipEmptyParts);
            for(const QString& name : names)
            {
                if(name == QStringLiteral("speed")) spec.show_speed = true;
                else if(name == QStringLiteral("brightness")) spec.show_brightness = true;
                else if(name == QStringLiteral("frequency")) spec.show_frequency = true;
                else if(name == QStringLiteral("detail")) spec.show_detail = true;
                else if(name == QStringLiteral("size")) spec.show_size = true;
                else if(name == QStringLiteral("scale")) spec.show_scale = true;
                else if(name == QStringLiteral("fps")) spec.show_fps = true;
                else if(name == QStringLiteral("color")) spec.show_color = true;
                else if(name == QStringLiteral("surface")) spec.show_surface = true;
                else if(name == QStringLiteral("position")) spec.show_position = true;
                else if(name == QStringLiteral("axis") || name == QStringLiteral("path")) spec.show_path = true;
                else if(name == QStringLiteral("thickness")) spec.show_thickness = true;
                else if(name == QStringLiteral("edge")) spec.show_edge = true;
                else if(name == QStringLiteral("count")) spec.show_count = true;
                else if(name == QStringLiteral("plane")) spec.show_plane = true;
                else if(name == QStringLiteral("strip")) spec.strip_colormap = true;
                else if(name == QStringLiteral("bands")) spec.height_bands = true;
                else if(name == QStringLiteral("media")) spec.uses_media = true;
                else if(name == QStringLiteral("audio")) spec.uses_audio = true;
            }
        }
        else if(key == QStringLiteral("pattern_source") && val == QStringLiteral("kernels"))
        {
            spec.pattern_from_kernels = true;
        }
        else if(key == QStringLiteral("class"))
        {
            spec.class_name = val.toStdString();
        }
        else if(key == QStringLiteral("name"))
        {
            spec.ui_name = val.toStdString();
        }
        else if(key == QStringLiteral("category"))
        {
            spec.category = val.toStdString();
        }
        else if(key == QStringLiteral("description"))
        {
            spec.description = val.toStdString();
        }
        else if(key == QStringLiteral("pattern_key"))
        {
            spec.pattern_key = val.toStdString();
        }
        else if(key == QStringLiteral("finish") && val == QStringLiteral("depth"))
        {
            spec.finish_depth = true;
        }
        else if(key == QStringLiteral("finish") && val == QStringLiteral("rgb"))
        {
            spec.finish_rgb = true;
        }
        else if(key == QStringLiteral("media")
                && (val == QStringLiteral("yes") || val == QStringLiteral("true")))
        {
            spec.uses_media = true;
        }
        else if(key == QStringLiteral("supports_strip_colormap") && val == QStringLiteral("true"))
        {
            spec.strip_colormap = true;
        }
        else if(key == QStringLiteral("supports_height_bands") && val == QStringLiteral("true"))
        {
            spec.height_bands = true;
        }
        else if(key == QStringLiteral("param"))
        {
            spec.params.push_back(val.toStdString());
        }
        else if(key == QStringLiteral("colors"))
        {
            const QStringList parts = val.split(QLatin1Char(' '), Qt::SkipEmptyParts);
            for(const QString& part : parts)
            {
                spec.colors.push_back(ColorFromHex(part));
            }
        }
        else if(key == QStringLiteral("slider"))
        {
            QString body = line.mid(colon + 1).trimmed();
            QString label;
            QString slider_tip;
            const int bar1 = body.indexOf(QLatin1Char('|'));
            if(bar1 >= 0)
            {
                label = body.mid(bar1 + 1).trimmed();
                body = body.left(bar1).trimmed();
                const int bar2 = label.indexOf(QLatin1Char('|'));
                if(bar2 >= 0)
                {
                    slider_tip = label.mid(bar2 + 1).trimmed();
                    label = label.left(bar2).trimmed();
                }
            }
            const QStringList parts = body.split(QLatin1Char(' '), Qt::SkipEmptyParts);
            if(parts.size() >= 4)
            {
                FolderVolumeSpec::Slider slider;
                slider.key = parts[0].toStdString();
                slider.min = parts[1].toInt();
                slider.max = parts[2].toInt();
                slider.value = parts[3].toInt();
                slider.label = label;
                slider.tip = slider_tip;
                for(int p = 4; p < parts.size(); ++p)
                {
                    if(parts[p] == QStringLiteral("unit"))
                    {
                        slider.save_unit = true;
                    }
                    if(parts[p] == QStringLiteral("pct"))
                    {
                        slider.show_percent = true;
                    }
                }
                spec.steps.push_back({true, (int)spec.sliders.size()});
                spec.sliders.push_back(std::move(slider));
            }
        }
        else if(key == QStringLiteral("flow"))
        {
            spec.flow.push_back(val.toFloat());
        }
        else if(key == QStringLiteral("pattern_index"))
        {
            spec.pattern_index_default = val.toInt();
        }
        else if(key == QStringLiteral("pattern_label"))
        {
            spec.pattern_label = val;
        }
        else if(key == QStringLiteral("finish") && val == QStringLiteral("hex"))
        {
            spec.finish_hex = true;
        }
        else if(key == QStringLiteral("finish") && val == QStringLiteral("hsv"))
        {
            spec.finish_hsv = true;
        }
        else if(key == QStringLiteral("finish") && val == QStringLiteral("surface"))
        {
            spec.finish_surface = true;
        }
        else if(key == QStringLiteral("sample") && val == QStringLiteral("room"))
        {
            spec.sample_room = true;
        }
        else if(key == QStringLiteral("finish") && val == QStringLiteral("atlas"))
        {
            spec.finish_hex = true;
            spec.finish_atlas = true;
        }
        else if(key == QStringLiteral("resolution"))
        {
            spec.resolution = val.toInt();
        }
        else if(key == QStringLiteral("rainbow") && val == QStringLiteral("true"))
        {
            spec.rainbow = true;
        }
        else if(key == QStringLiteral("needs_frequency"))
        {
            spec.needs_frequency = val == QStringLiteral("true");
        }
        else if(key == QStringLiteral("user_colors"))
        {
            spec.user_colors = val.toInt();
        }
        else if(key == QStringLiteral("finish") && val == QStringLiteral("spiral"))
        {
            spec.finish_spiral = true;
        }
        else if(key == QStringLiteral("finish") && val == QStringLiteral("audio"))
        {
            spec.finish_audio = true;
        }
        else if(key == QStringLiteral("drive") && val == QStringLiteral("audio"))
        {
            spec.uses_audio = true;
        }
        else if(key == QStringLiteral("audio_preset"))
        {
            spec.audio_preset = val.toStdString();
        }
        else if(key == QStringLiteral("audio_media"))
        {
            spec.audio_media = val.toStdString();
        }
        else if(key == QStringLiteral("shader"))
        {
            spec.shader_override = val.toStdString();
        }
        else if(key == QStringLiteral("combo"))
        {
            const int bar = val.indexOf('|');
            QString head = bar < 0 ? val.trimmed() : val.left(bar).trimmed();
            QString label = bar < 0 ? val.trimmed() : val.mid(bar + 1).trimmed();
            const QStringList head_parts = head.split(QLatin1Char(' '), Qt::SkipEmptyParts);
            int start = 0;
            if(head_parts.size() >= 2)
            {
                start = head_parts[1].toInt();
                head = head_parts[0];
            }
            spec.combos.push_back({head.toStdString(), label, start, {}});
            spec.steps.push_back({2, (int)spec.combos.size() - 1});
        }
        else if(key == QStringLiteral("option") && !spec.combos.empty())
        {
            spec.combos.back().options.push_back({val, tip});
        }
        else if(key == QStringLiteral("pattern") && !val.isEmpty())
        {
            if(spec.patterns.empty())
            {
                spec.steps.push_back({false, 0});
            }
            spec.patterns.push_back({val, tip});
        }
        else if(key != QStringLiteral("name") && key != QStringLiteral("description") && key != QStringLiteral("section")
                && key != QStringLiteral("index") && key != QStringLiteral("palette")
                && key != QStringLiteral("pattern_source")
                && key != QStringLiteral("drive") && key != QStringLiteral("audio_preset")
                && key != QStringLiteral("audio_media")
                && key != QStringLiteral("shader"))
        {
            break;
        }
    }
    if(spec.pattern_from_kernels)
    {
        spec.patterns.clear();
        for(int i = 0; i < SpatialPatternKernelCount(); ++i)
        {
            spec.patterns.push_back({QString::fromUtf8(SpatialPatternKernelDisplayName(i)), QString()});
        }
        if(!spec.patterns.empty() && spec.steps.empty())
        {
            spec.steps.push_back({0, 0});
        }
        else if(!spec.patterns.empty())
        {
            bool has_pattern_step = false;
            for(const auto& step : spec.steps)
            {
                if(step.kind == 0)
                {
                    has_pattern_step = true;
                    break;
                }
            }
            if(!has_pattern_step)
            {
                size_t insert_at = 0;
                for(size_t i = 0; i < spec.steps.size(); ++i)
                {
                    if(spec.steps[i].kind == 2)
                    {
                        insert_at = i + 1;
                        break;
                    }
                }
                spec.steps.insert(spec.steps.begin() + (std::ptrdiff_t)insert_at, {0, 0});
            }
        }
    }
    return spec;
}

} // namespace

void RegisterFolderVolumeEffects()
{
    if(!OpenRGB3DSpatialPlugin::APIPointer)
    {
        return;
    }
    PluginSettingsPaths::EnsurePluginDataLayout(OpenRGB3DSpatialPlugin::APIPointer);
    const QString root = QString::fromStdString(
        PluginSettingsPaths::EffectsDir(OpenRGB3DSpatialPlugin::APIPointer).string());
    QDirIterator files(root, QStringList() << QStringLiteral("*.fs"), QDir::Files, QDirIterator::Subdirectories);
    while(files.hasNext())
    {
        const QFileInfo fi(files.next());
        FolderVolumeSpec spec = ReadSpec(fi.absoluteFilePath());
        if(spec.class_name.empty())
        {
            continue;
        }
        const QString abs = fi.absoluteFilePath();
        if(abs.contains(QStringLiteral("/shader-field/")) || abs.contains(QStringLiteral("\\shader-field\\")))
        {
            continue;
        }
        const std::string class_name = spec.class_name;
        const std::string ui_name = spec.ui_name;
        std::string category = "Volume";
        if(abs.contains(QStringLiteral("/audio/")) || abs.contains(QStringLiteral("\\audio\\")))
        {
            category = "Audio";
        }
        else if(abs.contains(QStringLiteral("/media/")) || abs.contains(QStringLiteral("\\media\\")))
        {
            category = "Media";
        }
        else if(abs.contains(QStringLiteral("/spatial/")) || abs.contains(QStringLiteral("\\spatial\\")))
        {
            category = "Volume";
        }
        else if(!spec.category.empty())
        {
            category = spec.category;
        }
        FolderVolumeSpec reg_spec = spec;
        reg_spec.category = category;
        EffectListManager3D::get()->RegisterEffect(
            class_name, ui_name, category, "", "",
            [reg_spec]() { return new FolderVolumeEffect(reg_spec); });
    }
}

FolderVolumeEffect::FolderVolumeEffect(FolderVolumeSpec spec, QWidget* parent)
    : SpatialEffect3D(parent)
    , spec_(std::move(spec))
{
    pattern_index = spec_.pattern_index_default;
    combo_values.resize(spec_.combos.size());
    for(size_t i = 0; i < spec_.combos.size(); ++i)
    {
        combo_values[i] = spec_.combos[i].value;
    }
    slider_values.resize(spec_.sliders.size());
    for(size_t i = 0; i < spec_.sliders.size(); ++i)
    {
        slider_values[i] = spec_.sliders[i].value;
    }
    if(GetColors().empty() && !spec_.colors.empty())
    {
        SetColors(spec_.colors);
    }
    SetRainbowMode(spec_.rainbow);
    if(spec_.class_name == "RotatingConeSpotlights")
    {
        effect_instance_count = 1;
    }
    if(spec_.uses_media)
    {
        gif_frame_timer = new QTimer(this);
        gif_frame_timer->setTimerType(Qt::PreciseTimer);
        connect(gif_frame_timer, &QTimer::timeout, this, &FolderVolumeEffect::OnGifFrameTimerTimeout);
    }
    if(spec_.uses_audio)
    {
        const std::string& preset = spec_.audio_preset;
        if(preset == "level")
            audio_settings = MakeDefaultLevelAudioReactiveSettings3D();
        else if(preset == "beat")
            audio_settings = MakeDefaultBeatAudioReactiveSettings3D();
        else if(preset == "low_punch")
            audio_settings = MakeDefaultLowPunchAudioReactiveSettings3D();
        else if(preset == "high_sparkle")
            audio_settings = MakeDefaultHighSparkleAudioReactiveSettings3D();
        else // spectrum / paint / strip / default
            audio_settings = MakeDefaultSpectrumAudioReactiveSettings3D();
    }
    const QString shader_id = spec_.shader_override.empty()
        ? QString::fromStdString(spec_.shader_id)
        : QString::fromStdString(spec_.shader_override);
    volume_assist_.setFragmentBody(SpatialShaderCatalog::LoadEffectShader(shader_id));
    if(spec_.resolution > 0)
    {
        volume_assist_.setResolution(spec_.resolution);
    }
}

FolderVolumeEffect::~FolderVolumeEffect()
{
    ClearMovie();
}

EffectInfo3D FolderVolumeEffect::GetEffectInfo() const
{
    EffectInfo3D info{};
    info.effect_name = spec_.ui_name.c_str();
    info.effect_description = spec_.description.c_str();
    info.category = spec_.category.c_str();
    info.is_reversible = !(spec_.uses_media || spec_.uses_audio);
    info.supports_random = false;
    info.max_speed = 200;
    info.min_speed = 0;
    info.user_colors = (unsigned int)std::max(0, spec_.user_colors);
    info.has_custom_settings = !spec_.patterns.empty() || !spec_.sliders.empty()
        || !spec_.combos.empty() || spec_.uses_media || spec_.uses_audio;
    info.needs_3d_origin = spec_.uses_media;
    info.effect_type = SPATIAL_EFFECT_FOLDER_VOLUME;
    info.needs_frequency = spec_.needs_frequency;
    info.use_size_parameter = spec_.show_size;
    info.show_speed_control = spec_.show_speed;
    info.show_brightness_control = spec_.show_brightness;
    info.show_frequency_control = spec_.show_frequency;
    info.show_detail_control = spec_.show_detail;
    info.show_size_control = spec_.show_size;
    info.show_scale_control = spec_.show_scale;
    info.show_fps_control = spec_.show_fps;
    info.show_color_controls = spec_.show_color;
    info.show_surface_control = spec_.show_surface;
    info.show_position_offset_control = spec_.show_position;
    info.show_path_axis_control = spec_.show_path;
    info.show_thickness_control = spec_.show_thickness;
    info.show_edge_fade_control = spec_.show_edge;
    info.show_count_control = spec_.show_count;
    info.show_plane_control = spec_.show_plane;
    info.supports_strip_colormap = spec_.strip_colormap;
    info.supports_height_bands = spec_.height_bands;
    return info;
}

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
        browse_button = new QPushButton(tr("Choose image / GIF…"), panel);
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
                tr("Shapes how fast the vignette drops (soft → hard)."));
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

void FolderVolumeEffect::PrepareGpuFields(std::uint64_t render_sequence, float time_sec, const GridContext3D& grid)
{
    this->time_sec = time_sec;
    SpatialLayerCore::MapperSettings strat_st;
    EffectStratumBlend::InitStratumBreaks(strat_st);
    float sw[3];
    EffectStratumBlend::WeightsForYNorm(0.5f, strat_st, sw);
    const EffectStratumBlend::BandBlendScalars bb =
        EffectStratumBlend::BlendBands(GetStratumLayoutMode(), sw, GetStratumTuning());

    speed_mul = bb.speed_mul;
    tight_mul = bb.tight_mul;
    phase_shift = EffectStratumBlend::PhaseShift01(bb);
    progress = CalculateProgress(time_sec) * speed_mul;

    const Vector3D origin = GetEffectOriginGrid(grid);
    const EffectGridAxisHalfExtents he = MakeEffectGpuAtlasHalfExtents(grid, origin, GetNormalizedScale());
    float med = EffectGridGpuAtlasMedianHalfExtent(grid, origin, GetNormalizedScale());
    if(med < 1e-4f)
        med = 1.0f;
    atlas_sx = std::max(0.25f, 2.0f * he.hw / med);
    atlas_sy = std::max(0.25f, 2.0f * he.hh / med);
    atlas_sz = std::max(0.25f, 2.0f * he.hd / med);
    float aspect_med = std::max(he.hw, he.hd);
    if(aspect_med < 1e-4f)
        aspect_med = 1.0f;
    atlas_ax = std::clamp(he.hw / aspect_med, 0.15f, 1.0f);
    atlas_az = std::clamp(he.hd / aspect_med, 0.15f, 1.0f);

    auto world_to_occ = [&](float x, float y, float z) {
        const float hw = std::max(he.hw, 1e-5f);
        const float hh = std::max(he.hh, 1e-5f);
        const float hd = std::max(he.hd, 1e-5f);
        return Vector3D{
            0.5f + 0.5f * (x - origin.x) / hw,
            0.5f + 0.5f * (y - origin.y) / hh,
            0.5f + 0.5f * (z - origin.z) / hd
        };
    };
    const Vector3D pmin = world_to_occ(grid.min_x, grid.min_y, grid.min_z);
    const Vector3D pmax = world_to_occ(grid.max_x, grid.max_y, grid.max_z);
    room_ymin = pmin.y;
    room_ymax = pmax.y;
    room_xmin = pmin.x;
    room_xmax = pmax.x;
    room_zmin = pmin.z;
    room_zmax = pmax.z;

    const float detail = std::max(0.05f, GetScaledDetail()) * bb.tight_mul;
    const float size = GetNormalizedSize();

    if(spec_.uses_media)
    {
        std::shared_ptr<QImage> snap;
        std::shared_ptr<QImage> prev_snap;
        qint64 step_ms = 0;
        int step_interval_ms = 0;
        {
            QMutexLocker lock(&display_mutex);
            snap = display_frame;
            prev_snap = previous_display_frame;
            step_ms = last_gif_step_ms;
            step_interval_ms = gif_step_interval_ms;
        }
        if(!snap || snap->isNull())
        {
            volume_assist_.clearMediaTexture();
            float zp[13] = {};
            volume_assist_.prepare(render_sequence, time_sec, zp, 13);
            return;
        }

        QImage media = *snap;
        constexpr int kGpuMediaEdge = SpatialVolumeFieldEngine::kMaxMediaEdge;
        if(media.width() > kGpuMediaEdge || media.height() > kGpuMediaEdge)
        {
            media = media.scaled(kGpuMediaEdge, kGpuMediaEdge, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }

        const bool is_omni = spec_.class_name == "OmniShapeTexture";
        const unsigned int eff_res = CombineMediaSampling((unsigned int)std::clamp(SliderRaw("media_resolution"), 0, 100));
        if(is_omni && eff_res < 100u)
        {
            const float q = eff_res / 100.0f;
            const int tw = std::max(8, (int)std::lround(4.0f + q * q * (float)(std::max(2, media.width()) - 4)));
            const int th = std::max(8, (int)std::lround(4.0f + q * q * (float)(std::max(2, media.height()) - 4)));
            media = media.scaled(tw, th, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        }

        const float smoothing = GetSmoothing() / 100.0f;
        if(media_is_gif && prev_snap && !prev_snap->isNull() && smoothing > 0.0f && step_interval_ms > 0 && step_ms > 0)
        {
            const qint64 now_ms = QDateTime::currentMSecsSinceEpoch();
            const float elapsed_ms = (float)std::max<qint64>(0, now_ms - step_ms);
            const float blend_window_ms = std::max(1.0f, (float)step_interval_ms * smoothing);
            const float a = std::clamp(elapsed_ms / blend_window_ms, 0.0f, 1.0f);
            if(a < 0.999f)
            {
                QImage cur = media.convertToFormat(QImage::Format_ARGB32);
                QImage prev = prev_snap->scaled(cur.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                                  .convertToFormat(QImage::Format_ARGB32);
                for(int y = 0; y < cur.height(); ++y)
                {
                    QRgb* dst = reinterpret_cast<QRgb*>(cur.scanLine(y));
                    const QRgb* src = reinterpret_cast<const QRgb*>(prev.constScanLine(y));
                    for(int x = 0; x < cur.width(); ++x)
                    {
                        const RGBColor c = MediaTextureEffect::LerpRGB(
                            ToRGBColor(qRed(src[x]), qGreen(src[x]), qBlue(src[x])),
                            ToRGBColor(qRed(dst[x]), qGreen(dst[x]), qBlue(dst[x])), a);
                        dst[x] = qRgba(RGBGetRValue(c), RGBGetGValue(c), RGBGetBValue(c), 255);
                    }
                }
                media = std::move(cur);
            }
        }

        volume_assist_.setMediaTexture(media, tile_repeat_enabled);

        const float tm = std::max(0.25f, bb.tight_mul);
        const bool freeze_motion = GetSpeed() == 0;
        const float scroll_mul = SliderRaw("motion_scroll") / 100.0f;
        const float warp_mul = SliderRaw("motion_warp") / 100.0f;
        const float phase_mul = SliderRaw("motion_phase") / 100.0f;
        const float speed_lin = std::clamp(GetSpeed() / 100.0f, 0.0f, 1.0f);
        const float size_m = std::max(0.08f, size);
        const float prop01 = SliderRaw("ambience_propagation") / 100.0f;

        if(spec_.class_name == "TextureProjection")
        {
            const float detail_s = std::max(0.05f, GetScaledDetail()) * tm;
            const float freq_n = std::clamp(GetNormalizedFrequency(), 0.05f, 1.0f);
            const float scroll_rate =
                freeze_motion
                    ? 0.0f
                    : scroll_mul * (0.22f + 0.48f * speed_lin + 0.18f * freq_n) * bb.speed_mul;
            const float repeat_from_freq = 0.55f + 1.65f * freq_n;
            const float size_zoom_div = std::clamp(0.40f + 0.36f * size_m, 0.32f, 2.2f);
            const float tile = std::clamp(repeat_from_freq / size_zoom_div, 0.12f, 6.5f);
            const float amp =
                freeze_motion ? 0.0f
                              : warp_mul * (0.045f + 0.20f * std::min(1.0f, GetScaledDetail() * 0.12f)) / tm;
            const float q = eff_res / 100.0f;
            const float steps_u = std::max(2.0f, 4.0f + q * q * (float)(std::max(2, media.width()) - 4));
            const float steps_v = std::max(2.0f, 4.0f + q * q * (float)(std::max(2, media.height()) - 4));
            const float packed_v = steps_v + ((eff_res < 100u) ? 1000.0f : 0.0f);
            float vp[13] = {
                (float)std::clamp(ComboIndex("projection_mode"), 0, 3),
                tile,
                scroll_rate,
                phase_mul,
                amp,
                detail_s,
                SliderRaw("ambience_dist_falloff") / 100.0f,
                SliderRaw("ambience_falloff_curve") / 100.0f,
                SliderRaw("ambience_edge_soft") / 100.0f,
                prop01,
                steps_u,
                packed_v,
                tile_repeat_enabled ? 1.0f : 0.0f
            };
            volume_assist_.prepare(render_sequence, time_sec, vp, 13);
            return;
        }

        if(is_omni)
        {
            constexpr int kOmniShapeCount = 6;
            const float spin_percent = (float)SliderRaw("spin_percent");
            const float morph_percent = (float)SliderRaw("morph_percent");
            const float spin_rate =
                freeze_motion
                    ? 0.0f
                    : (0.25f + 2.6f * (spin_percent / 100.0f)) * (0.35f + 0.75f * speed_lin)
                          * (0.55f + 1.35f * scroll_mul) * bb.speed_mul;
            const float yaw_rate = spin_rate;
            const float pitch_rate = spin_rate * 0.71f;
            const float phase_drive =
                freeze_motion ? 0.0f : (phase_mul * 1.55f + scroll_mul * 0.95f);
            const float repeat_from_freq = 0.55f + 1.65f * GetNormalizedFrequency();
            const float size_zoom_div = std::clamp(0.55f + 0.28f * size_m, 0.40f, 2.0f);
            const float tile = std::clamp(repeat_from_freq / size_zoom_div, 0.12f, 6.5f);
            const float detail_raw = std::max(0.05f, GetScaledDetail());
            const float amp =
                freeze_motion ? 0.0f
                              : warp_mul * (0.05f + 0.22f * std::min(1.0f, detail_raw * 0.12f)) / tm;
            const float R_local = std::clamp(0.16f + 1.15f * size_m, 0.12f, 1.65f) / tm;
            const float packed_wrap = (tile_repeat_enabled ? 2.0f : 0.0f) + prop01;
            float vp[13] = {
                (float)std::clamp(ComboIndex("base_shape"), 0, kOmniShapeCount - 1),
                morph_percent / 100.0f,
                tile,
                yaw_rate,
                pitch_rate,
                phase_drive,
                amp,
                detail_raw,
                SliderRaw("ambience_dist_falloff") / 100.0f,
                SliderRaw("ambience_falloff_curve") / 100.0f,
                SliderRaw("ambience_edge_soft") / 100.0f,
                R_local,
                packed_wrap
            };
            volume_assist_.prepare(render_sequence, time_sec, vp, 13);
            return;
        }

        float zp[13] = {};
        volume_assist_.prepare(render_sequence, time_sec, zp, 13);
        return;
    }

    if(spec_.uses_audio)
    {
        const float amplitude = SampleAudioVisualLevel(audio_settings);
        const float alpha = std::clamp(audio_settings.smoothing, 0.0f, 0.99f);
        if(std::fabs(time_sec - audio_last_intensity_time) > 1e-4f)
        {
            audio_smoothed = alpha * audio_smoothed + (1.0f - alpha) * amplitude;
            audio_last_intensity_time = time_sec;

            AudioInputManager* aud = AudioInputManager::instance();
            float bass_t = 0.0f, mid_t = 0.0f, high_t = 0.0f;
            if(aud)
            {
                bass_t = std::clamp(aud->getBandSlowEnergyHz(40.0f, 180.0f), 0.0f, 1.0f);
                mid_t  = std::clamp(aud->getBandSlowEnergyHz(200.0f, 2000.0f), 0.0f, 1.0f);
                high_t = std::clamp(aud->getBandSlowEnergyHz(2500.0f, 12000.0f), 0.0f, 1.0f);
            }
            const float band_alpha = std::clamp(audio_settings.smoothing, 0.0f, 0.95f);
            audio_band_lo  = band_alpha * audio_band_lo  + (1.0f - band_alpha) * bass_t;
            audio_band_mid = band_alpha * audio_band_mid + (1.0f - band_alpha) * mid_t;
            audio_band_hi  = band_alpha * audio_band_hi  + (1.0f - band_alpha) * high_t;

            const float drive = ApplyAudioVisualIntensity(SampleAudioVisualLevel(audio_settings), audio_settings);
            audio_time_boost += (0.35f + 1.8f * audio_band_lo + 0.4f * drive)
                                * std::max(0.0f, GetNormalizedSpeed()) * speed_mul * 0.016f;
            audio_time_boost = std::fmod(audio_time_boost + 1000.0f, 1000.0f);
        }
        else if(alpha <= 0.0f)
        {
            audio_smoothed = amplitude;
        }
        audio_fill_cached = ApplyAudioIntensity(audio_smoothed, audio_settings);
    }

    if(spec_.audio_media == "bands")
    {
        AudioInputManager* aud = AudioInputManager::instance();
        int total_bands = aud ? aud->getBandsCount() : 16;
        if(total_bands <= 0) total_bands = 16;
        float f_min = 20.0f, f_max = 20000.0f;
        if(aud)
        {
            float sr = (float)aud->getSampleRate();
            int fft  = aud->getFFTSize();
            if(sr > 0.0f && fft > 0)
            {
                f_min = std::max(1.0f, sr / (float)fft);
                f_max = sr * 0.5f;
                if(f_max <= f_min) f_max = f_min + 1.0f;
            }
        }
        auto mapHz = [&](float hz) -> int {
            float denom = std::log(f_max / f_min);
            if(std::abs(denom) < 1e-6f) return 0;
            float t = std::log(std::clamp(hz, f_min, f_max) / f_min) / denom;
            return std::clamp((int)(t * total_bands), 0, total_bands - 1);
        };
        audio_spec_band_start = mapHz((float)audio_settings.low_hz);
        audio_spec_band_end   = mapHz((float)audio_settings.high_hz);
        if(audio_spec_band_end < audio_spec_band_start) std::swap(audio_spec_band_end, audio_spec_band_start);
        audio_spec_band_start = std::clamp(audio_spec_band_start, 0, total_bands - 1);
        audio_spec_band_end   = std::clamp(audio_spec_band_end,   audio_spec_band_start, total_bands - 1);

        const int band_count = std::max(1, audio_spec_band_end - audio_spec_band_start + 1);
        if((int)audio_smoothed_bands.size() != band_count)
            audio_smoothed_bands.assign(band_count, 0.0f);

        std::vector<float> raw_bands;
        if(aud) aud->getBands(raw_bands);
        const float b_smooth = std::clamp(audio_settings.smoothing, 0.0f, 0.99f);
        const float boost = std::clamp(audio_settings.peak_boost, 0.25f, 4.0f);
        for(int i = 0; i < band_count; ++i)
        {
            const int idx = audio_spec_band_start + i;
            float s = (idx >= 0 && idx < (int)raw_bands.size()) ? raw_bands[idx] : 0.0f;
            audio_smoothed_bands[i] = b_smooth * audio_smoothed_bands[i] + (1.0f - b_smooth) * s;
        }

        // Gate + boost in host; shader gets a clean gated texture with no second multiply
        QImage img(band_count, 1, QImage::Format_RGBA8888);
        for(int i = 0; i < band_count; ++i)
        {
            float v = AudioVisualNoiseGate(audio_smoothed_bands[i]);
            v = std::clamp(v * boost, 0.0f, 1.0f);
            const int g = (int)std::lround(v * 255.0f);
            img.setPixel(i, 0, qRgba(g, g, g, 255));
        }
        volume_assist_.setMediaTexture(img, false);

        audio_band_count_cached = (float)band_count;
        const float roll_sp = SliderRaw("roll_speed") / 100.0f;
        audio_roll_phase_cached = (roll_sp > 1e-6f)
            ? std::fmod(time_sec * roll_sp * speed_mul + 1000.0f, 1.0f) : 0.0f;
    }

    if(spec_.audio_media == "spectrogram")
    {
        if((int)audio_spectrogram_history.size() != kFvAudioSpectrogramRows)
        {
            audio_spectrogram_history.assign(kFvAudioSpectrogramRows,
                std::vector<float>(kFvAudioSpectrogramCols, 0.0f));
            audio_column_smoothed.assign(kFvAudioSpectrogramCols, 0.0f);
            audio_spectrogram_write_index = 0;
        }

        constexpr float kPushInterval = 1.0f / 60.0f;
        if(audio_last_spectrogram_push == std::numeric_limits<float>::lowest()
           || (time_sec - audio_last_spectrogram_push) >= kPushInterval)
        {
            AudioInputManager* aud = AudioInputManager::instance();
            if(aud && aud->isRunning())
            {
                AudioInputManager::SpectrumSnapshot snap = aud->getSpectrumSnapshot(kFvAudioSpectrogramCols);
                if(!snap.bins.empty())
                {
                    const float fm = snap.min_frequency_hz > 0.0f ? snap.min_frequency_hz : 20.0f;
                    const float fx = snap.max_frequency_hz > fm   ? snap.max_frequency_hz : 20000.0f;
                    auto colFor = [&](float hz) -> int {
                        float denom = std::log(fx / fm);
                        if(std::abs(denom) < 1e-6f) return 0;
                        float t = std::log(std::clamp(hz, fm, fx) / fm) / denom;
                        return std::clamp((int)(t * kFvAudioSpectrogramCols), 0, kFvAudioSpectrogramCols - 1);
                    };
                    int i0 = colFor((float)audio_settings.low_hz);
                    int i1 = colFor((float)audio_settings.high_hz);
                    if(i1 < i0) std::swap(i0, i1);
                    const int row = audio_spectrogram_write_index % kFvAudioSpectrogramRows;
                    auto& hist_row = audio_spectrogram_history[row];
                    hist_row.resize(kFvAudioSpectrogramCols, 0.0f);
                    const float sc_smooth = std::clamp(audio_settings.smoothing, 0.0f, 0.99f);
                    const int eq_bands = std::max(1, aud->getEqBandCount());
                    for(int c = 0; c < kFvAudioSpectrogramCols; ++c)
                    {
                        float v = 0.0f;
                        if(c >= i0 && c <= i1 && c < (int)snap.bins.size())
                        {
                            const int eq_b = std::min((c * eq_bands) / kFvAudioSpectrogramCols, eq_bands - 1);
                            v = std::clamp(snap.bins[c] * aud->getEqGain(eq_b), 0.0f, 1.0f);
                        }
                        hist_row[c] = v;
                        audio_column_smoothed[c] = sc_smooth * audio_column_smoothed[c] + (1.0f - sc_smooth) * v;
                    }
                    audio_spectrogram_write_index++;
                }
            }
            audio_last_spectrogram_push = time_sec;
        }

        const float scroll_sp = SliderRaw("scroll_speed") / 100.0f;
        audio_strip_scroll_cached = std::fmod(time_sec * scroll_sp, 1.0f);
        if(audio_strip_scroll_cached < 0.0f) audio_strip_scroll_cached += 1.0f;
        audio_bar_edge_cached = std::max(0.02f, audio_settings.falloff * 0.0015f);

        const float boost = std::clamp(audio_settings.peak_boost, 0.25f, 4.0f);
        const int disp_mode = ComboIndex("display_mode");
        if(disp_mode == 1)
        {
            QImage img(kFvAudioSpectrogramCols, kFvAudioSpectrogramRows, QImage::Format_RGBA8888);
            const int rows = kFvAudioSpectrogramRows;
            const int newest = (audio_spectrogram_write_index > 0)
                ? ((audio_spectrogram_write_index - 1) % rows) : 0;
            for(int age = 0; age < rows; ++age)
            {
                const int src_row = (newest - age + rows) % rows;
                const auto& rd = (src_row < (int)audio_spectrogram_history.size())
                    ? audio_spectrogram_history[src_row] : audio_column_smoothed;
                for(int c = 0; c < kFvAudioSpectrogramCols; ++c)
                {
                    float v = (c < (int)rd.size()) ? rd[c] : 0.0f;
                    v = AudioVisualNoiseGate(v) * boost;
                    const int g = (int)std::lround(std::clamp(v, 0.0f, 1.0f) * 255.0f);
                    img.setPixel(c, age, qRgba(g, g, g, 255));
                }
            }
            volume_assist_.setMediaTexture(img, false);
        }
        else
        {
            QImage img(kFvAudioSpectrogramCols, 1, QImage::Format_RGBA8888);
            for(int c = 0; c < kFvAudioSpectrogramCols; ++c)
            {
                float v = (c < (int)audio_column_smoothed.size()) ? audio_column_smoothed[c] : 0.0f;
                v = AudioVisualNoiseGate(v) * boost;
                const int g = (int)std::lround(std::clamp(v, 0.0f, 1.0f) * 255.0f);
                img.setPixel(c, 0, qRgba(g, g, g, 255));
            }
            volume_assist_.setMediaTexture(img, false);
        }
    }

    if(spec_.audio_preset == "beat" || spec_.audio_preset == "low_punch")
    {
        // Tick pulse queue
        if(std::fabs(time_sec - audio_last_pulse_tick_time) > 1e-5f)
        {
            const float dt = (audio_last_pulse_tick_time == std::numeric_limits<float>::lowest())
                ? 0.0f : std::clamp(time_sec - audio_last_pulse_tick_time, 0.0f, 0.1f);
            audio_last_pulse_tick_time = time_sec;

            AudioInputManager* aud = AudioInputManager::instance();
            const float onset_thr = std::clamp(SliderRaw("onset_trigger") / 100.0f, 0.05f, 0.95f);
            float strength = 0.0f;
            if(aud && aud->isRunning() && TryTriggerAudioPulse(dt, audio_settings, audio_pulse_trigger,
                onset_thr, AudioReactiveOnsetSmoothAlpha(audio_settings),
                AudioReactiveBeatPulseHoldSec(), strength))
            {
                AudioPulseEntry p;
                p.birth_time  = time_sec;
                p.strength    = strength;
                p.color_slot  = audio_next_pulse_color_slot++;
                audio_pulses.push_back(p);
            }

            const float exp_decay = BeatWaveShellDecay(audio_settings, 2.0f);
            audio_pulses.erase(std::remove_if(audio_pulses.begin(), audio_pulses.end(),
                [&](const AudioPulseEntry& p) {
                    const float age = time_sec - p.birth_time;
                    return age > 2.6f || AudioReactivePulseFade(p.strength, age, exp_decay) < 0.003f;
                }), audio_pulses.end());
        }

        // Compute grid-derived geometry
        Vector3D origin = GetEffectOriginGrid(grid);
        constexpr float kExplosionFill = 3.0f;
        float radius_basis = EffectGridGpuAtlasMedianHalfExtent(grid, origin, GetNormalizedScale())
            * 1.7320508f * kExplosionFill;
        radius_basis = std::max(radius_basis, 1e-3f);
        const EffectGridAxisHalfExtents extents = MakeEffectGpuAtlasHalfExtents(grid, origin, GetNormalizedScale());

        const AudioBeatWaveMode wave_mode = static_cast<AudioBeatWaveMode>(audio_settings.beat_wave_mode);
        const bool classic = (wave_mode == AudioBeatWaveMode::ClassicWave);
        const float pulse_speed = BeatWaveScaledSpeed(GetNormalizedSpeed() * speed_mul, audio_settings);
        const float decay_v     = classic ? BeatWaveShellDecay(audio_settings, 2.0f)
                                          : BeatWaveShellDecay(audio_settings, 2.35f);
        const float half_w      = AudioRingHalfWidthFromFalloff(radius_basis, audio_settings.falloff, size, tight_mul, detail);
        const float max_travel  = classic ? BeatWaveBurstPhaseCap(audio_settings)
                                          : radius_basis * (0.88f + 0.12f * size);

        audio_radius_basis_cached = radius_basis;
        audio_pulse_half_w_cached = half_w;
        audio_max_travel_cached   = max_travel;
        audio_pulse_speed_cached  = pulse_speed;
        audio_pulse_hw_cached     = std::max(1e-5f, extents.hw);
        audio_pulse_hh_cached     = std::max(1e-5f, extents.hh);
        audio_pulse_hd_cached     = std::max(1e-5f, extents.hd);
        audio_pulse_decay_cached  = decay_v;

        // Fill params 0..13 via generic system
        float vp[24] = {};
        const int p_count = std::min(14, (int)spec_.params.size());
        for(int i = 0; i < p_count; ++i)
            vp[i] = ParamValue(spec_.params[(size_t)i], detail, size);

        // Pack pulse state into vp[14..23]
        for(int i = 0; i < 5; ++i) { vp[14 + i*2] = -1.0f; vp[15 + i*2] = 0.0f; }
        std::fill(std::begin(audio_packed_color_slots), std::end(audio_packed_color_slots), 0u);
        audio_packed_pulse_count = 0;
        const int start = std::max(0, (int)audio_pulses.size() - 5);
        for(int pi = start; pi < (int)audio_pulses.size() && audio_packed_pulse_count < 5; ++pi)
        {
            const AudioPulseEntry& p = audio_pulses[pi];
            const float age = time_sec - p.birth_time;
            if(age < 0.0f || p.strength <= 0.0f) continue;
            const int slot = audio_packed_pulse_count++;
            vp[14 + slot*2] = age;
            vp[15 + slot*2] = p.strength;
            audio_packed_color_slots[slot] = p.color_slot;
        }

        volume_assist_.prepare(render_sequence, time_sec, vp, 24);
        return;
    }

    if(spec_.audio_preset == "high_sparkle")
    {
        AudioInputManager* aud = AudioInputManager::instance();
        audio_note_count = 0;
        std::fill(std::begin(audio_note_hues), std::end(audio_note_hues), 0.0f);
        std::fill(std::begin(audio_note_amps), std::end(audio_note_amps), 0.0f);
        audio_hi_drive_cached = 0.0f;

        if(aud && aud->isRunning())
        {
            audio_hi_drive_cached = std::clamp(
                std::max(aud->getNoteDrive01(), aud->getBandSlowEnergyHz(2800.0f, 14000.0f)),
                0.0f, 1.0f);
            const auto notes = aud->getActiveNotes();
            for(const auto& n : notes)
            {
                if(audio_note_count >= 4) break;
                if(n.amp < 0.10f) continue;
                float mean = n.mean;
                if(mean < 0.0f)   mean += 12.0f;
                if(mean >= 12.0f) mean -= 12.0f;
                audio_note_hues[audio_note_count] = std::clamp(mean / 12.0f, 0.0f, 1.0f);
                audio_note_amps[audio_note_count] = std::clamp(n.amp, 0.0f, 1.0f);
                ++audio_note_count;
            }
            if(audio_note_count == 0 && aud->getNoteDrive01() > 0.05f)
            {
                audio_note_hues[0] = aud->getDominantNoteHue01();
                audio_note_amps[0] = aud->getNoteDrive01();
                audio_note_count = 1;
            }
        }

        // Generic params 0..11
        float vp[24] = {};
        const int p_count = std::min(12, (int)spec_.params.size());
        for(int i = 0; i < p_count; ++i)
            vp[i] = ParamValue(spec_.params[(size_t)i], detail, size);
        // Override vp[6]: high01 = max(noteDrive, highBand)
        vp[6] = audio_hi_drive_cached;

        // Pack notes into vp[12..19]
        for(int i = 0; i < 4; ++i)
        {
            vp[12 + i*2] = audio_note_hues[i];
            vp[13 + i*2] = audio_note_amps[i];
        }
        vp[20] = (float)audio_note_count;
        vp[21] = time_sec * speed_mul * GetNormalizedSpeed();

        volume_assist_.prepare(render_sequence, time_sec, vp, 22);
        return;
    }

    float vp[24] = {};
    const int count = std::min(24, (int)spec_.params.size());
    for(int i = 0; i < count; ++i)
    {
        vp[i] = ParamValue(spec_.params[(size_t)i], detail, size);
    }
    if(count > 0)
    {
        volume_assist_.prepare(render_sequence, time_sec, vp, count);
    }
}

RGBColor FolderVolumeEffect::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    Vector3D origin = GetEffectOriginGrid(grid);
    float rel_x = x - origin.x;
    float rel_y = y - origin.y;
    float rel_z = z - origin.z;
    if(!IsWithinEffectBoundary(rel_x, rel_y, rel_z, grid))
    {
        return 0x00000000;
    }

    progress = CalculateProgress(time);
    const float detail = std::max(0.05f, GetScaledDetail());
    const float size_multiplier = GetNormalizedSize();
    Vector3D rotated_pos{x, y, z};
    float n1 = 0.5f, n2 = 0.5f, n3 = 0.5f;
    if(spec_.sample_room)
    {
        SampleGpuRoomVolume01(rotated_pos.x, rotated_pos.y, rotated_pos.z, grid, &n1, &n2, &n3);
    }
    else if(!SampleGpuVolumeOriginLocal01(rotated_pos.x, rotated_pos.y, rotated_pos.z, grid, origin,
                                    GetNormalizedScale(), &n1, &n2, &n3))
    {
        return 0x00000000;
    }
    if(!volume_assist_.isAvailable())
    {
        return 0x00000000;
    }

    const float coord2 = SampleStratumYNorm01(rotated_pos.y, grid, origin);
    SpatialLayerCore::MapperSettings strat_map;
    EffectStratumBlend::InitStratumBreaks(strat_map);
    float stratum_w[3];
    EffectStratumBlend::WeightsForYNorm(coord2, strat_map, stratum_w);
    const EffectStratumBlend::BandBlendScalars bb =
        EffectStratumBlend::BlendBands(GetStratumLayoutMode(), stratum_w, GetStratumTuning());
    const float stratum_mot01 = ComputeStratumMotion01(stratum_w, grid, x, y, z, origin, time);
    const float prog = progress * bb.speed_mul;
    const float pshift = EffectStratumBlend::PhaseShift01(bb);
    if(spec_.finish_rgb)
    {
        const QVector3D samp = volume_assist_.sample01(n1, n2, n3);
        return ToRGBColor((int)(std::clamp(samp.x(), 0.0f, 1.0f) * 255.0f + 0.5f),
                          (int)(std::clamp(samp.y(), 0.0f, 1.0f) * 255.0f + 0.5f),
                          (int)(std::clamp(samp.z(), 0.0f, 1.0f) * 255.0f + 0.5f));
    }
    if(spec_.finish_surface)
    {
        const QVector3D samp = volume_assist_.sample01(n1, n2, n3);
        float intensity = samp.x();
        const float plasma = samp.y();
        const float hot = samp.z();
        if(GetStratumLayoutMode() == 1)
            intensity = EffectStratumBlend::ApplyMotionToUnit01(intensity, stratum_mot01, 0.18f);
        if(intensity < 0.01f)
            return 0x00000000;

        const float phase01 = EffectStratumBlend::CombinedPhase01(bb, stratum_mot01);
        const int style = ComboIndex("style");
        RGBColor c = 0;
        if(style > 0 && style < 8)
        {
            const float p = std::clamp(plasma, 0.0f, 1.0f);
            const float h = std::clamp(hot, 0.0f, 1.0f);
            const float spread = std::clamp(0.9f + GetNormalizedFrequency() * 0.9f, 0.9f, 2.6f);
            const float shimmer =
                std::sin(time * GetColorCycleHz() * 6.2831853f * bb.speed_mul + p * 6.28318f) *
                (5.0f + 9.0f * p);
            if(style == 7)
            {
                const float cool = 150.0f + p * 70.0f;
                const float warm = 190.0f + p * 50.0f;
                const float mix = std::clamp(0.35f + 0.5f * p + h * 0.25f, 0.0f, 1.0f);
                const unsigned char r = (unsigned char)std::clamp((int)(cool * (1.0f - mix) + warm * mix + h * 40.0f), 0, 255);
                const unsigned char g = (unsigned char)std::clamp((int)(cool * 0.95f + p * 40.0f + h * 35.0f), 0, 255);
                const unsigned char b = (unsigned char)std::clamp((int)(cool + (1.0f - p) * 35.0f + h * 25.0f), 0, 255);
                c = (RGBColor)((b << 16) | (g << 8) | r);
            }
            else
            {
                float hue0 = 0.0f;
                float span = 60.0f;
                switch(style)
                {
                case 1: hue0 = 6.0f;   span = 58.0f; break;
                case 2: hue0 = 165.0f; span = 75.0f; break;
                case 3: hue0 = 70.0f;  span = 70.0f; break;
                case 4: hue0 = 0.0f;   span = 68.0f; break;
                case 5: hue0 = 4.0f;   span = 52.0f; break;
                case 6: hue0 = 175.0f; span = 70.0f; break;
                default: hue0 = p * 360.0f; span = 0.0f; break;
                }
                float hue = hue0 + p * span * spread + shimmer + phase01 * 14.0f + h * 18.0f;
                hue = std::fmod(hue, 360.0f);
                if(hue < 0.0f)
                    hue += 360.0f;
                c = GetRainbowColor(hue);
                if(style == 1 || style == 4 || style == 5)
                {
                    const float whiten = std::clamp(h * h * 0.92f, 0.0f, 1.0f);
                    const int r = (int)((c & 0xFF) * (1.0f - whiten) + 255.0f * whiten);
                    const int g = (int)(((c >> 8) & 0xFF) * (1.0f - whiten) + 235.0f * whiten);
                    const int b = (int)(((c >> 16) & 0xFF) * (1.0f - whiten) + 160.0f * whiten);
                    c = (RGBColor)((std::clamp(b, 0, 255) << 16) | (std::clamp(g, 0, 255) << 8) | std::clamp(r, 0, 255));
                }
                else if(style == 2 || style == 6)
                {
                    const float glint = std::clamp(h * 0.75f, 0.0f, 1.0f);
                    const int r = (int)((c & 0xFF) * (1.0f - glint * 0.55f) + 220.0f * glint);
                    const int g = (int)(((c >> 8) & 0xFF) * (1.0f - glint * 0.35f) + 240.0f * glint);
                    const int b = (int)(((c >> 16) & 0xFF) * (1.0f - glint * 0.15f) + 255.0f * glint);
                    c = (RGBColor)((std::clamp(b, 0, 255) << 16) | (std::clamp(g, 0, 255) << 8) | std::clamp(r, 0, 255));
                }
                else if(style == 3)
                {
                    const float gloss = std::clamp(h * 0.85f, 0.0f, 1.0f);
                    const int r = (int)((c & 0xFF) * (1.0f - gloss * 0.4f) + 210.0f * gloss);
                    const int g = (int)(((c >> 8) & 0xFF) * (1.0f - gloss * 0.25f) + 255.0f * gloss);
                    const int b = (int)(((c >> 16) & 0xFF) * (1.0f - gloss * 0.45f) + 160.0f * gloss);
                    c = (RGBColor)((std::clamp(b, 0, 255) << 16) | (std::clamp(g, 0, 255) << 8) | std::clamp(r, 0, 255));
                }
            }
        }
        else if(UseEffectStripColormap())
        {
            const float ph01 = std::fmod(time * GetColorCycleHz() * bb.speed_mul +
                                             phase01 + plasma * 0.08f + 1.f, 1.f);
            const float pal = SampleEffectStripColormap01(GetEffectStripColormapRepeats(),
                                                         GetEffectStripColormapUnfold(),
                                                         GetEffectStripColormapDirectionDeg(),
                                                         ph01,
                                                         time,
                                                         grid,
                                                         GetNormalizedSize(),
                                                         origin,
                                                         rotated_pos);
            c = ResolveStripKernelFinalColor(GetEffectStripColormapKernel(),
                                             std::clamp(pal, 0.0f, 1.0f),
                                             time);
        }
        else if(GetRainbowMode())
        {
            float hue = std::fmod(plasma * 360.0f + time * GetColorCycleHz() * 360.0f * bb.speed_mul
                                      + phase01 * 360.0f, 360.0f);
            if(hue < 0.0f)
                hue += 360.0f;
            c = GetRainbowColor(hue);
        }
        else
        {
            c = GetColorAtPosition(plasma);
        }
        const int r = std::min(255, std::max(0, (int)((c & 0xFF) * intensity)));
        const int g = std::min(255, std::max(0, (int)(((c >> 8) & 0xFF) * intensity)));
        const int b = std::min(255, std::max(0, (int)(((c >> 16) & 0xFF) * intensity)));
        return (RGBColor)((b << 16) | (g << 8) | r);
    }
    if(spec_.finish_hsv)
    {
        const QVector3D samp = volume_assist_.sample01(n1, n2, n3);
        const float sat = samp.x();
        float val = samp.y();
        float h = samp.z();
        if(val <= 1e-5f)
            return 0x00000000;
        h = std::fmod(h + time * GetColorCycleHz() * bb.speed_mul
                      + EffectStratumBlend::CombinedPhase01(bb, stratum_mot01) + 1.0f, 1.0f);
        if(UseEffectStripColormap())
        {
            const float ph01 = std::fmod(h + 1.0f, 1.0f);
            float pal01 = SampleEffectStripColormap01(GetEffectStripColormapRepeats(),
                                                       GetEffectStripColormapUnfold(),
                                                       GetEffectStripColormapDirectionDeg(),
                                                       ph01,
                                                       time,
                                                       grid,
                                                       GetNormalizedScale(),
                                                       origin,
                                                       rotated_pos);
            RGBColor c = ResolveStripKernelFinalColor(GetEffectStripColormapKernel(),
                                                      std::clamp(pal01, 0.0f, 1.0f), time);
            const int cr = (int)(c & 0xFF);
            const int cg = (int)((c >> 8) & 0xFF);
            const int cb = (int)((c >> 16) & 0xFF);
            const float cv = std::max({cr, cg, cb}) / 255.0f;
            return EffectHsv01ToBgr(h, sat, std::clamp(val * cv, 0.0f, 1.0f));
        }
        if(GetRainbowMode())
        {
            SpatialLayerCore::Basis basis;
            SpatialLayerCore::MakeBasisFromEffectEulerDegrees(GetRotationYaw(), GetRotationPitch(), GetRotationRoll(), basis);
            SpatialLayerCore::MapperSettings map;
            EffectStratumBlend::InitStratumBreaks(map);
            SpatialLayerCore::SamplePoint sp{};
            sp.grid_x = x;
            sp.grid_y = y;
            sp.grid_z = z;
            sp.origin_x = origin.x;
            sp.origin_y = origin.y;
            sp.origin_z = origin.z;
            sp.y_norm = coord2;
            float hue_deg = h * 360.0f;
            hue_deg = ApplySpatialRainbowHue(hue_deg, h, basis, sp, map, time, &grid);
            h = std::fmod(hue_deg / 360.0f + 1.0f, 1.0f);
        }
        return EffectHsv01ToBgr(h, sat, val);
    }
    if(spec_.finish_hex)
    {
        const QVector3D samp = volume_assist_.sample01(n1, n2, n3);
        float v = samp.x();
        float h01 = samp.y();
        if(spec_.finish_atlas)
        {
            const float hue_rad = std::atan2(samp.y() * 2.0f - 1.0f, samp.x() * 2.0f - 1.0f);
            h01 = std::fmod(hue_rad / 6.2831853f + 1.0f, 1.0f);
            v = samp.z() > 0.0f ? std::clamp(samp.z(), 0.0f, 1.0f) : 1.0f;
        }
        RGBColor c = 0;
        if(UseEffectStripColormap())
        {
            const float p01 = SampleEffectStripColormap01(GetEffectStripColormapRepeats(),
                                                           GetEffectStripColormapUnfold(),
                                                           GetEffectStripColormapDirectionDeg(),
                                                           h01,
                                                           time,
                                                           grid,
                                                           GetNormalizedSize(),
                                                           origin,
                                                           rotated_pos);
            c = ResolveStripKernelFinalColor(GetEffectStripColormapKernel(), p01, time);
        }
        else if(GetRainbowMode())
        {
            SpatialLayerCore::Basis basis;
            SpatialLayerCore::MakeBasisFromEffectEulerDegrees(GetRotationYaw(), GetRotationPitch(), GetRotationRoll(), basis);
            SpatialLayerCore::MapperSettings map;
            SpatialLayerCore::InitAudioEffectMapperSettings(map, GetNormalizedScale(), std::max(0.05f, GetScaledDetail()));
            SpatialLayerCore::SamplePoint sp{};
            sp.grid_x = x;
            sp.grid_y = y;
            sp.grid_z = z;
            sp.origin_x = origin.x;
            sp.origin_y = origin.y;
            sp.origin_z = origin.z;
            const float hue = ApplySpatialRainbowHue(h01 * 360.0f, h01, basis, sp, map, time, &grid);
            c = GetRainbowColor(std::fmod(hue + 720.0f, 360.0f));
        }
        else
        {
            c = GetColorAtPosition(h01);
        }
        if(GetEdgeFade() > 0)
        {
            const float fade = std::clamp(GetEdgeFade() / 100.0f, 0.0f, 1.0f);
            const float u = RoomXZEdgeProximity01(x, z, grid);
            const float t = std::clamp(u, 0.0f, 1.0f);
            v *= std::max(0.0f, 1.0f - fade * (t * t * (3.0f - 2.0f * t)));
        }
        const int r = std::clamp((int)((float)(c & 0xFF) * v), 0, 255);
        const int g = std::clamp((int)((float)((c >> 8) & 0xFF) * v), 0, 255);
        const int b = std::clamp((int)((float)((c >> 16) & 0xFF) * v), 0, 255);
        return (RGBColor)((b << 16) | (g << 8) | r);
    }
    if(spec_.finish_audio)
    {
        const QVector3D samp = volume_assist_.sample01(n1, n2, n3);
        float intensity = samp.x();
        if(GetStratumLayoutMode() == 1)
            intensity = EffectStratumBlend::ApplyMotionToUnit01(intensity, stratum_mot01, 0.18f);
        if(intensity <= 0.001f)
            return 0x00000000;

        // Note sparkle: shader outputs (intensity, note_hue01, gradient)
        if(spec_.audio_preset == "high_sparkle")
        {
            const float note_hue01  = std::clamp(samp.y(), 0.0f, 1.0f);
            const float gradient_pos = std::clamp(samp.z(), 0.0f, 1.0f);
            const auto mode = static_cast<AudioPulseColorMode>(audio_settings.pulse_color_mode);
            RGBColor color;
            if(GetRainbowMode()
               && mode != AudioPulseColorMode::PerBeatCycle
               && mode != AudioPulseColorMode::Uniform)
            {
                color = GetRainbowColor(gradient_pos * 360.0f);
            }
            else if(mode == AudioPulseColorMode::FollowNotes)
            {
                color = GetRainbowColor(note_hue01 * 360.0f);
            }
            else
            {
                AudioReactiveColorParams cp{};
                cp.gradient_pos01 = gradient_pos;
                cp.intensity       = intensity;
                cp.beat_color_slot = (uint32_t)std::floor(note_hue01 * 12.0f);
                cp.time = time; cp.grid_x = x; cp.grid_y = y; cp.grid_z = z;
                cp.grid = &grid; cp.origin = origin; cp.rotated_pos = rotated_pos;
                cp.y_norm01 = coord2; cp.stratum_mot01 = stratum_mot01; cp.band_scalars = &bb;
                color = ResolveAudioReactiveColor(audio_settings, cp);
            }
            return BrightenAudioEffectColor(color, intensity);
        }

        // Pulse: shader outputs (energy, gradient, pulse_idx01)
        float gradient_pos = samp.y();
        uint32_t beat_slot = (uint32_t)std::floor(time * 2.5f);
        if((spec_.audio_preset == "beat" || spec_.audio_preset == "low_punch")
           && audio_packed_pulse_count > 0)
        {
            const int pulse_slot = std::clamp((int)std::floor(samp.z() * 5.0f), 0, 4);
            beat_slot = audio_packed_color_slots[pulse_slot];

            const bool classic = (static_cast<AudioBeatWaveMode>(audio_settings.beat_wave_mode)
                                  == AudioBeatWaveMode::ClassicWave);
            intensity = std::min(1.0f, intensity * (classic ? 1.45f : 1.22f));
            if(intensity <= 0.006f) return 0x00000000;
        }

        AudioReactiveColorParams color_params{};
        color_params.gradient_pos01 = gradient_pos;
        color_params.intensity      = intensity;
        color_params.beat_color_slot = beat_slot;
        color_params.time = time; color_params.grid_x = x; color_params.grid_y = y; color_params.grid_z = z;
        color_params.grid = &grid; color_params.origin = origin; color_params.rotated_pos = rotated_pos;
        color_params.y_norm01 = coord2; color_params.stratum_mot01 = stratum_mot01; color_params.band_scalars = &bb;

        RGBColor color = ResolveAudioReactiveColor(audio_settings, color_params);
        return BrightenAudioEffectColor(color, intensity);
    }

    float value = volume_assist_.sampleScalar01(n1, n2, n3);
    value = EffectStratumBlend::ApplyMotionToUnit01(value, stratum_mot01, 0.28f);

    float depth_factor = 1.0f;
    if(spec_.finish_depth)
    {
        const float rot_rel_x = rotated_pos.x - origin.x;
        const float rot_rel_y = rotated_pos.y - origin.y;
        const float rot_rel_z = rotated_pos.z - origin.z;
        const float radial_distance = sqrtf(rot_rel_x * rot_rel_x + rot_rel_y * rot_rel_y + rot_rel_z * rot_rel_z);
        const float max_radius = EffectGridGpuAtlasMedianHalfExtent(grid, origin, GetNormalizedScale()) * 1.7320508f;
        if(max_radius > 0.001f)
        {
            const float normalized_dist = fmin(1.0f, radial_distance / max_radius);
            depth_factor = 0.45f + 0.55f * (1.0f - normalized_dist * 0.6f);
        }
    }

    SpatialLayerCore::Basis basis;
    SpatialLayerCore::MakeBasisFromEffectEulerDegrees(GetRotationYaw(), GetRotationPitch(), GetRotationRoll(), basis);
    SpatialLayerCore::MapperSettings map;
    EffectStratumBlend::InitStratumBreaks(map);
    map.blend_softness = std::clamp(0.09f + 0.08f * (1.0f - detail), 0.05f, 0.20f);
    map.center_size = std::clamp(0.10f + 0.22f * GetNormalizedScale(), 0.06f, 0.50f);
    map.directional_sharpness = std::clamp(0.95f + detail * 0.1f, 0.85f, 2.2f);
    SpatialLayerCore::SamplePoint sp{};
    sp.grid_x = x;
    sp.grid_y = y;
    sp.grid_z = z;
    sp.origin_x = origin.x;
    sp.origin_y = origin.y;
    sp.origin_z = origin.z;
    sp.y_norm = coord2;

    RGBColor final_color = 0;
    const float phase01 = std::fmod(prog + pshift + 1.0f, 1.0f);
    const float arms = std::max(1, SliderRaw("num_arms"));
    const float coil01 = SliderRaw("coil_amount") / 100.0f;
    const float height01 = SliderRaw("height_coil_amount") / 100.0f;
    float spiral_angle = 0.0f;
    if(spec_.finish_spiral)
    {
        const float rot_rel_x = rotated_pos.x - origin.x;
        const float rot_rel_z = rotated_pos.z - origin.z;
        const float angle = atan2f(rot_rel_z, rot_rel_x);
        const EffectGridAxisHalfExtents ex = MakeEffectGpuAtlasHalfExtents(grid, origin, GetNormalizedScale());
        float norm_radius = EffectGridHorizontalRadialNorm01(EffectGridHorizontalRadialNormXZ(rot_rel_x, rot_rel_z, ex.hw, ex.hd));
        norm_radius = fmaxf(0.0f, fminf(1.0f, norm_radius));
        const float detail_e = detail * bb.tight_mul;
        const float two_pi = 6.2831853f;
        const float radial_coil = norm_radius * coil01 * two_pi * (2.2f + 1.4f * std::clamp(detail_e / 20.0f, 0.0f, 1.5f));
        const float z_twist = coord2 * height01 * two_pi * (1.0f + 0.75f * std::clamp(detail_e / 20.0f, 0.0f, 1.5f));
        spiral_angle = angle * arms + radial_coil + z_twist - prog * 1.35f;
        spiral_angle += EffectStratumBlend::PhaseShiftRad(bb);
        spiral_angle += stratum_mot01 * two_pi * 0.55f;
    }

    if(UseEffectStripColormap())
    {
        const float strip_p01 = SampleEffectStripColormap01(GetEffectStripColormapRepeats(),
                                                             GetEffectStripColormapUnfold(),
                                                             GetEffectStripColormapDirectionDeg(),
                                                             phase01,
                                                             time,
                                                             grid,
                                                             size_multiplier,
                                                             origin,
                                                             rotated_pos);
        const float tinted = spec_.finish_spiral
            ? std::fmod(strip_p01 + value * 0.35f + 1.0f, 1.0f)
            : strip_p01;
        final_color = ResolveStripKernelFinalColor(GetEffectStripColormapKernel(), tinted, time);
    }
    else if(spec_.finish_spiral && (pattern_index == 1 || pattern_index == 2 || pattern_index == 5 || pattern_index == 6) && !GetRainbowMode())
    {
        float arm_index = fmodf(spiral_angle / (6.28318f / arms), arms);
        if(arm_index < 0.0f)
        {
            arm_index += arms;
        }
        float pos = fmodf((arm_index / arms) + time * GetColorCycleHz() * bb.speed_mul, 1.0f);
        if(pos < 0.0f)
        {
            pos += 1.0f;
        }
        final_color = GetColorAtPosition(ApplySpatialPalette01(pos, basis, sp, map, time, &grid));
    }
    else if(GetRainbowMode() && spec_.finish_spiral)
    {
        float hue = spiral_angle * 57.2958f + value * 200.0f + coord2 * 40.0f + time * GetColorCycleHz() * bb.speed_mul * 360.0f;
        hue = ApplySpatialRainbowHue(hue, fmodf(value + 0.25f, 1.0f), basis, sp, map, time, &grid);
        float p01 = std::fmod(hue / 360.0f, 1.0f);
        if(p01 < 0.0f)
        {
            p01 += 1.0f;
        }
        final_color = GetRainbowColor(p01 * 360.0f);
    }
    else if(spec_.finish_spiral)
    {
        float pos = fmodf(value + time * GetColorCycleHz() * bb.speed_mul, 1.0f);
        if(pos < 0.0f)
        {
            pos += 1.0f;
        }
        final_color = GetColorAtPosition(ApplySpatialPalette01(pos, basis, sp, map, time, &grid));
    }
    else if(GetRainbowMode())
    {
        float hue = value * 360.0f + time * GetColorCycleHz() * 360.0f;
        hue = ApplySpatialRainbowHue(hue, value, basis, sp, map, time, &grid);
        float p01 = std::fmod(hue / 360.0f, 1.0f);
        if(p01 < 0.0f)
        {
            p01 += 1.0f;
        }
        final_color = GetRainbowColor(p01 * 360.0f);
    }
    else
    {
        final_color = GetColorAtPosition(ApplySpatialPalette01(value, basis, sp, map, time, &grid));
    }

    unsigned char r = final_color & 0xFF;
    unsigned char g = (final_color >> 8) & 0xFF;
    unsigned char b = (final_color >> 16) & 0xFF;
    float mask = depth_factor;
    if(spec_.finish_spiral)
    {
        mask = std::pow(std::clamp(value, 0.0f, 1.0f), 0.85f);
    }
    if(GetEdgeFade() > 0)
    {
        const float fade = std::clamp(GetEdgeFade() / 100.0f, 0.0f, 1.0f);
        const float u = RoomXZEdgeProximity01(x, z, grid);
        const float t = std::clamp(u, 0.0f, 1.0f);
        mask *= std::max(0.0f, 1.0f - fade * (t * t * (3.0f - 2.0f * t)));
    }
    r = (unsigned char)std::clamp((int)std::lround((float)r * mask), 0, 255);
    g = (unsigned char)std::clamp((int)std::lround((float)g * mask), 0, 255);
    b = (unsigned char)std::clamp((int)std::lround((float)b * mask), 0, 255);
    return (b << 16) | (g << 8) | r;
}

nlohmann::json FolderVolumeEffect::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    if(!spec_.patterns.empty())
    {
        j[spec_.pattern_key] = pattern_index;
    }
    for(size_t i = 0; i < spec_.combos.size(); ++i)
    {
        j[spec_.combos[i].key] = combo_values[i];
    }
    for(size_t i = 0; i < spec_.sliders.size(); ++i)
    {
        if(spec_.sliders[i].save_unit)
        {
            j[spec_.sliders[i].key] = slider_values[i] / 100.0;
        }
        else
        {
            j[spec_.sliders[i].key] = slider_values[i];
        }
    }
    if(spec_.uses_audio)
    {
        AudioReactiveSaveToJson(j, audio_settings);
    }
    if(spec_.uses_media)
    {
        j["media_path"] = media_path.toStdString();
        j["tile_repeat_enabled"] = tile_repeat_enabled;
    }
    return j;
}

void FolderVolumeEffect::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    auto take_thickness = [&](const char* key) {
        if(!settings.contains("band_thickness") && settings.contains(key) && settings[key].is_number())
            effect_band_thickness = (unsigned int)std::clamp((int)std::lround(settings[key].get<double>() * 100.0), 0, 100);
    };
    take_thickness("surface_thickness");
    take_thickness("ring_thickness");
    take_thickness("thickness");
    if(!settings.contains("edge_fade") && settings.contains("surface_edge_fade") && settings["surface_edge_fade"].is_number())
        effect_edge_fade = (unsigned int)std::clamp((int)std::lround(settings["surface_edge_fade"].get<double>()), 0, 100);
    auto take_count = [&](const char* key) {
        if(!settings.contains("instance_count") && settings.contains(key) && settings[key].is_number())
            effect_instance_count = (unsigned int)std::clamp((int)std::lround(settings[key].get<double>()), 1, 48);
    };
    take_count("num_stars");
    take_count("pf_count");
    take_count("max_bubbles");
    take_count("ball_count");
    if(!settings.contains("path_axis"))
    {
        if(settings.contains("depth_axis") && settings["depth_axis"].is_number_integer())
            effect_path_axis = std::clamp(settings["depth_axis"].get<int>(), 0, 2);
        else if(settings.contains("travel_axis") && settings["travel_axis"].is_number_integer())
            effect_path_axis = std::clamp(settings["travel_axis"].get<int>(), 0, 2);
    }
    if(thickness_slider)
        thickness_slider->setValue((int)effect_band_thickness);
    if(edge_fade_slider)
        edge_fade_slider->setValue((int)effect_edge_fade);
    if(count_slider)
        count_slider->setValue((int)effect_instance_count);
    if(path_axis_combo)
        path_axis_combo->setCurrentIndex(effect_path_axis);
    if(!settings.contains("strip_cmap_unfold") && settings.contains("shellpattern_unfold_mode") && settings["shellpattern_unfold_mode"].is_number_integer())
        effect_strip_cmap_unfold = std::clamp(settings["shellpattern_unfold_mode"].get<int>(), 0, 8);
    if(!settings.contains("strip_cmap_dir") && settings.contains("shellpattern_direction_deg") && settings["shellpattern_direction_deg"].is_number())
        effect_strip_cmap_dir = std::fmod(settings["shellpattern_direction_deg"].get<float>() + 360.0f, 360.0f);
    if(!settings.contains("strip_cmap_rep") && settings.contains("shellpattern_repeats") && settings["shellpattern_repeats"].is_number())
        effect_strip_cmap_rep = std::max(1.0f, std::min(40.0f, settings["shellpattern_repeats"].get<float>()));
    if(!settings.contains("band_thickness") && settings.contains("shellpattern_surface_thickness") && settings["shellpattern_surface_thickness"].is_number())
        effect_band_thickness = (unsigned int)std::clamp((int)std::lround(settings["shellpattern_surface_thickness"].get<float>() * 100.0f), 0, 100);
    if(!settings.contains("edge_fade") && settings.contains("shellpattern_edge_fade_pct") && settings["shellpattern_edge_fade_pct"].is_number())
        effect_edge_fade = (unsigned int)std::clamp((int)std::lround(settings["shellpattern_edge_fade_pct"].get<float>()), 0, 100);
    SyncEffectStripColormapPanelFromModel();
    if(!spec_.pattern_key.empty() && settings.contains(spec_.pattern_key) && settings[spec_.pattern_key].is_number_integer())
    {
        const int listed = (int)spec_.patterns.size();
        const int pattern_count = std::max(1, pattern_combo ? pattern_combo->count() : listed);
        pattern_index = std::clamp(settings[spec_.pattern_key].get<int>(), 0, pattern_count - 1);
    }
    if(pattern_combo)
    {
        pattern_combo->setCurrentIndex(pattern_index);
    }
    for(size_t i = 0; i < spec_.combos.size(); ++i)
    {
        const std::string& key = spec_.combos[i].key;
        if(!settings.contains(key) || !settings[key].is_number_integer())
        {
            continue;
        }
        const int count = std::max(1, (int)spec_.combos[i].options.size());
        combo_values[i] = std::clamp(settings[key].get<int>(), 0, count - 1);
    }
    for(size_t i = 0; i < spec_.sliders.size(); ++i)
    {
        const std::string& key = spec_.sliders[i].key;
        if(!settings.contains(key) || !settings[key].is_number())
        {
            continue;
        }
        int stored = 0;
        if(spec_.sliders[i].save_unit)
        {
            stored = (int)std::lround(settings[key].get<double>() * 100.0);
        }
        else
        {
            stored = (int)std::lround(settings[key].get<double>());
        }
        slider_values[i] = std::clamp(stored, spec_.sliders[i].min, spec_.sliders[i].max);
    }
    if(settings.contains("cone_spot_surface") && settings["cone_spot_surface"].is_number_integer())
    {
        const int surf = settings["cone_spot_surface"].get<int>();
        if(surf == 2 || surf == 3 || surf == 4)
        {
            const int combo = (surf == 2) ? 1 : (surf == 3) ? 2 : 3;
            for(size_t i = 0; i < spec_.combos.size(); ++i)
            {
                if(spec_.combos[i].key == "cone_spot_surface")
                    combo_values[i] = combo;
            }
        }
    }
    if(settings.contains("cone_spot_mirror") && settings["cone_spot_mirror"].is_boolean())
    {
        for(size_t i = 0; i < spec_.combos.size(); ++i)
        {
            if(spec_.combos[i].key == "cone_spot_mirror")
                combo_values[i] = settings["cone_spot_mirror"].get<bool>() ? 1 : 0;
        }
    }
    auto set_slider_from_float = [&](const char* key, float value, float mul) {
        for(size_t i = 0; i < spec_.sliders.size(); ++i)
        {
            if(spec_.sliders[i].key != key)
                continue;
            slider_values[i] = std::clamp((int)std::lround(value * mul),
                                          spec_.sliders[i].min, spec_.sliders[i].max);
        }
    };
    if(settings.contains("cone_spot_scale") && settings["cone_spot_scale"].is_number()
       && settings["cone_spot_scale"].get<double>() <= 1.0)
        set_slider_from_float("cone_spot_scale", (float)settings["cone_spot_scale"].get<double>(), 1000.0f);
    if(settings.contains("cone_spot_hue01") && settings["cone_spot_hue01"].is_number()
       && settings["cone_spot_hue01"].get<double>() <= 1.0)
        set_slider_from_float("cone_spot_hue01", (float)settings["cone_spot_hue01"].get<double>(), 1000.0f);
    if(settings.contains("cone_spot_motion") && settings["cone_spot_motion"].is_number()
       && settings["cone_spot_motion"].get<double>() <= 10.0)
        set_slider_from_float("cone_spot_motion", (float)settings["cone_spot_motion"].get<double>(), 100.0f);
    if(settings.contains("cone_spot_wander") && settings["cone_spot_wander"].is_number()
       && settings["cone_spot_wander"].get<double>() <= 5.0)
        set_slider_from_float("cone_spot_wander", (float)settings["cone_spot_wander"].get<double>(), 100.0f);
    if(settings.contains("cone_spot_apex_u") && settings["cone_spot_apex_u"].is_array()
       && settings.contains("cone_spot_apex_v") && settings["cone_spot_apex_v"].is_array())
    {
        const auto& au = settings["cone_spot_apex_u"];
        const auto& av = settings["cone_spot_apex_v"];
        const char* ukeys[] = {"cone1_u", "cone2_u", "cone3_u", "cone4_u"};
        const char* vkeys[] = {"cone1_v", "cone2_v", "cone3_v", "cone4_v"};
        for(int i = 0; i < 4; ++i)
        {
            if(i < (int)au.size() && au[i].is_number())
                set_slider_from_float(ukeys[i], au[i].get<float>(), 100.0f);
            if(i < (int)av.size() && av[i].is_number())
                set_slider_from_float(vkeys[i], av[i].get<float>(), 100.0f);
        }
    }
    if(spec_.uses_audio)
    {
        AudioReactiveLoadFromJson(audio_settings, settings);
        audio_smoothed = 0.0f;
        audio_last_intensity_time = std::numeric_limits<float>::lowest();
        audio_band_lo = 0.0f;
        audio_band_mid = 0.0f;
        audio_band_hi = 0.0f;
        audio_time_boost = 0.0f;
        // Reset spectrum/spectrogram state
        audio_band_count_cached = 0.0f;
        audio_smoothed_bands.clear();
        audio_spectrogram_history.clear();
        audio_column_smoothed.clear();
        audio_spectrogram_write_index = 0;
        audio_last_spectrogram_push = std::numeric_limits<float>::lowest();
        // Reset pulse state
        audio_pulses.clear();
        audio_pulse_trigger = {};
        audio_last_pulse_tick_time = std::numeric_limits<float>::lowest();
        audio_next_pulse_color_slot = 0u;
        audio_packed_pulse_count = 0;
        std::fill(std::begin(audio_packed_color_slots), std::end(audio_packed_color_slots), 0u);
        // Reset note state
        audio_note_count = 0;
        audio_hi_drive_cached = 0.0f;
        std::fill(std::begin(audio_note_hues), std::end(audio_note_hues), 0.0f);
        std::fill(std::begin(audio_note_amps), std::end(audio_note_amps), 0.0f);
        AudioReactiveUi::SyncSettingsToHost(GetCustomSettingsHost(), audio_settings);
    }
    if(spec_.uses_media)
    {
        if(settings.contains("media_path") && settings["media_path"].is_string())
        {
            LoadMediaFile(QString::fromStdString(settings["media_path"].get<std::string>()));
        }
        if(settings.contains("tile_repeat_enabled") && settings["tile_repeat_enabled"].is_boolean())
        {
            tile_repeat_enabled = settings["tile_repeat_enabled"].get<bool>();
            if(tile_repeat_check)
            {
                tile_repeat_check->setChecked(tile_repeat_enabled);
            }
        }
    }
}
