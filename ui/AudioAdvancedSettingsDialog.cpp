// SPDX-License-Identifier: GPL-2.0-only

#include "AudioAdvancedSettingsDialog.h"
#include "OpenRGB3DSpatialTab.h"
#include "Audio/AudioInputManager.h"
#include "EffectCheckRow.h"
#include "EffectLabeledSpinRow.h"
#include "EffectSliderRow.h"
#include "EffectUiRows.h"
#include "PluginUiUtils.h"
#include "ui_AudioAdvancedSettingsDialog.h"

#include <QDialogButtonBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <cmath>

namespace
{

constexpr int kSmoothingDefault        = 80;
constexpr int kBandPeakDecayDefault    = 83;
constexpr int kBassPeakDecayDefault    = 96;
constexpr int kActivityDecayDefault    = 77;
constexpr int kVisualizerDecayDefault  = 50;
constexpr int kVisualizerFloorDefault  = 40;
constexpr int kAutoPeakDecayDefault    = 50;
constexpr int kAutoFloorDecayDefault   = 99;
constexpr int kBassXoverDefault        = 200;
constexpr int kMidXoverDefault         = 2000;

int pctFromDecay(float coeff, float lo, float hi)
{
    if(hi <= lo + 1e-6f)
    {
        return 0;
    }
    const float t = std::clamp((coeff - lo) / (hi - lo), 0.0f, 1.0f);
    return static_cast<int>(std::lround(t * 100.0f));
}

float decayFromPct(int pct, float lo, float hi)
{
    const float t = std::clamp(pct / 100.0f, 0.0f, 1.0f);
    return lo + t * (hi - lo);
}

int floorPctFromValue(float f)
{
    const float logv = std::log10(std::max(f, 1e-8f));
    return static_cast<int>(std::lround(std::clamp((logv + 6.0f) / 4.0f * 100.0f, 0.0f, 100.0f)));
}

float floorValueFromPct(int pct)
{
    const float t = std::clamp(pct / 100.0f, 0.0f, 1.0f);
    const float logv = -6.0f + t * 4.0f;
    return std::pow(10.0f, logv);
}

} // namespace

AudioAdvancedSettingsDialog::AudioAdvancedSettingsDialog(OpenRGB3DSpatialTab* tab, QWidget* parent)
    : QDialog(parent)
    , ui(new Ui::AudioAdvancedSettingsDialog)
    , tab_(tab)
{
    ui->setupUi(this);
    resize(480, 560);
    PluginUiApplyMutedSecondaryLabel(ui->introLabel->label());

    buildRows();
    loadFromManager();

    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    if(QPushButton* restore = ui->buttonBox->button(QDialogButtonBox::RestoreDefaults))
    {
        connect(restore, &QPushButton::clicked, this, &AudioAdvancedSettingsDialog::onRestoreDefaultsClicked);
    }
}

AudioAdvancedSettingsDialog::~AudioAdvancedSettingsDialog()
{
    delete ui;
}

void AudioAdvancedSettingsDialog::buildRows()
{
    QVBoxLayout* layout = ui->contentLayout;
    if(!layout)
    {
        return;
    }

    QVBoxLayout* band_layout = EffectUiRows::AppendCollapsibleSectionBody(layout, QStringLiteral("Band analyzer"));
    if(!band_layout)
    {
        band_layout = layout;
    }

    smoothing_row_ = EffectUiRows::AppendSliderRow(
        band_layout,
        QStringLiteral("Band level smoothing"),
        0,
        99,
        kSmoothingDefault,
        QStringLiteral("Higher = smoother band meters and effect drive (EMA on each band)."));
    smoothing_row_->setValueLabelMinimumWidth(36);

    band_peak_decay_row_ = EffectUiRows::AppendSliderRow(
        band_layout,
        QStringLiteral("Band peak hold"),
        0,
        100,
        kBandPeakDecayDefault,
        QStringLiteral("How slowly per-band running peaks fall; higher keeps energy visible longer."));
    band_peak_decay_row_->setValueLabelMinimumWidth(36);

    bass_peak_decay_row_ = EffectUiRows::AppendSliderRow(
        band_layout,
        QStringLiteral("Bass peak hold"),
        0,
        100,
        kBassPeakDecayDefault,
        QStringLiteral("Extra hold on low bands for kick/bass-heavy content."));
    bass_peak_decay_row_->setValueLabelMinimumWidth(36);

    activity_peak_decay_row_ = EffectUiRows::AppendSliderRow(
        band_layout,
        QStringLiteral("Transient peak hold"),
        0,
        100,
        kActivityDecayDefault,
        QStringLiteral("Used when Mix clarity separates instruments; higher = longer activity tails."));
    activity_peak_decay_row_->setValueLabelMinimumWidth(36);

    QVBoxLayout* spectrum_layout = EffectUiRows::AppendCollapsibleSectionBody(layout, QStringLiteral("Spectrum preview"));
    if(!spectrum_layout)
    {
        spectrum_layout = layout;
    }

    visualizer_decay_row_ = EffectUiRows::AppendSliderRow(
        spectrum_layout,
        QStringLiteral("Spectrum peak decay"),
        0,
        100,
        kVisualizerDecayDefault,
        QStringLiteral("Falloff of white peak trace in the Audio Input spectrum strip."));
    visualizer_decay_row_->setValueLabelMinimumWidth(36);

    visualizer_floor_row_ = EffectUiRows::AppendSliderRow(
        spectrum_layout,
        QStringLiteral("Spectrum noise floor"),
        0,
        100,
        kVisualizerFloorDefault,
        QStringLiteral("Minimum displayed peak level (log scale; higher = more visible quiet bins)."));
    visualizer_floor_row_->setValueLabelMinimumWidth(36);

    QVBoxLayout* auto_level_layout = EffectUiRows::AppendCollapsibleSectionBody(layout, QStringLiteral("Auto level normalization"));
    if(!auto_level_layout)
    {
        auto_level_layout = layout;
    }

    auto_level_check_ = EffectUiRows::AppendCheckRow(
        auto_level_layout,
        QStringLiteral("Auto-normalize capture level"),
        true,
        QStringLiteral("Tracks running peak/floor so quiet sources still move the level meter."));

    auto_peak_decay_row_ = EffectUiRows::AppendSliderRow(
        auto_level_layout,
        QStringLiteral("Level peak decay"),
        0,
        100,
        kAutoPeakDecayDefault,
        QStringLiteral("How fast the tracked peak falls when audio is quiet."));
    auto_peak_decay_row_->setValueLabelMinimumWidth(36);

    auto_floor_decay_row_ = EffectUiRows::AppendSliderRow(
        auto_level_layout,
        QStringLiteral("Level floor decay"),
        0,
        100,
        kAutoFloorDecayDefault,
        QStringLiteral("How slowly the noise floor rises toward the current RMS."));
    auto_floor_decay_row_->setValueLabelMinimumWidth(36);

    QVBoxLayout* split_layout = EffectUiRows::AppendCollapsibleSectionBody(layout, QStringLiteral("Bass / mid / treble split"));
    if(!split_layout)
    {
        split_layout = layout;
    }

    bass_xover_row_ = EffectUiRows::AppendSpinRow(split_layout,
                                                  QStringLiteral("Bass upper (Hz)"),
                                                  40,
                                                  800,
                                                  kBassXoverDefault,
                                                  QStringLiteral("Frequencies below this count as bass."));
    mid_xover_row_ = EffectUiRows::AppendSpinRow(split_layout,
                                                 QStringLiteral("Mid upper (Hz)"),
                                                 400,
                                                 12000,
                                                 kMidXoverDefault,
                                                 QStringLiteral("Mid band ends here; above counts as treble."));
}

void AudioAdvancedSettingsDialog::loadFromManager()
{
    AudioInputManager* audio = AudioInputManager::instance();
    if(!audio)
    {
        return;
    }

    auto set_pct_label = [](EffectSliderRow* row, int v) {
        if(row)
        {
            row->slider()->setValue(v);
            row->valueLabel()->setText(QString::number(v) + QStringLiteral("%"));
        }
    };

    if(smoothing_row_)
    {
        set_pct_label(smoothing_row_, pctFromDecay(audio->getSmoothing(), 0.0f, 0.99f));
    }
    if(band_peak_decay_row_)
    {
        set_pct_label(band_peak_decay_row_, pctFromDecay(audio->getBandPeakDecay(), 0.97f, 0.999f));
    }
    if(bass_peak_decay_row_)
    {
        set_pct_label(bass_peak_decay_row_, pctFromDecay(audio->getBassPeakDecay(), 0.98f, 0.999f));
    }
    if(activity_peak_decay_row_)
    {
        set_pct_label(activity_peak_decay_row_, pctFromDecay(audio->getActivityPeakDecay(), 0.96f, 0.999f));
    }
    if(visualizer_decay_row_)
    {
        set_pct_label(visualizer_decay_row_, pctFromDecay(audio->getVisualizerPeakDecay(), 0.85f, 0.99f));
    }
    if(visualizer_floor_row_)
    {
        const int fv = floorPctFromValue(audio->getVisualizerFloor());
        visualizer_floor_row_->slider()->setValue(fv);
        visualizer_floor_row_->valueLabel()->setText(QString::number(fv) + QStringLiteral("%"));
    }
    if(auto_level_check_)
    {
        auto_level_check_->checkBox()->setChecked(audio->isAutoLevelEnabled());
    }
    if(auto_peak_decay_row_)
    {
        set_pct_label(auto_peak_decay_row_, pctFromDecay(audio->getAutoLevelPeakDecay(), 0.98f, 0.999f));
    }
    if(auto_floor_decay_row_)
    {
        set_pct_label(auto_floor_decay_row_, pctFromDecay(audio->getAutoLevelFloorDecay(), 0.98f, 0.9999f));
    }
    if(bass_xover_row_)
    {
        bass_xover_row_->spinBox()->setValue(static_cast<int>(std::lround(audio->getBassUpperHz())));
    }
    if(mid_xover_row_)
    {
        mid_xover_row_->spinBox()->setValue(static_cast<int>(std::lround(audio->getMidUpperHz())));
    }
}

void AudioAdvancedSettingsDialog::applyToManager() const
{
    AudioInputManager* audio = AudioInputManager::instance();
    if(!audio)
    {
        return;
    }

    if(smoothing_row_)
    {
        audio->setSmoothing(decayFromPct(smoothing_row_->slider()->value(), 0.0f, 0.99f));
    }
    if(band_peak_decay_row_)
    {
        audio->setBandPeakDecay(decayFromPct(band_peak_decay_row_->slider()->value(), 0.97f, 0.999f));
    }
    if(bass_peak_decay_row_)
    {
        audio->setBassPeakDecay(decayFromPct(bass_peak_decay_row_->slider()->value(), 0.98f, 0.999f));
    }
    if(activity_peak_decay_row_)
    {
        audio->setActivityPeakDecay(decayFromPct(activity_peak_decay_row_->slider()->value(), 0.96f, 0.999f));
    }
    if(visualizer_decay_row_)
    {
        audio->setVisualizerPeakDecay(decayFromPct(visualizer_decay_row_->slider()->value(), 0.85f, 0.99f));
    }
    if(visualizer_floor_row_)
    {
        audio->setVisualizerFloor(floorValueFromPct(visualizer_floor_row_->slider()->value()));
    }
    if(auto_level_check_)
    {
        audio->setAutoLevelEnabled(auto_level_check_->checkBox()->isChecked());
    }
    if(auto_peak_decay_row_)
    {
        audio->setAutoLevelPeakDecay(decayFromPct(auto_peak_decay_row_->slider()->value(), 0.98f, 0.999f));
    }
    if(auto_floor_decay_row_)
    {
        audio->setAutoLevelFloorDecay(decayFromPct(auto_floor_decay_row_->slider()->value(), 0.98f, 0.9999f));
    }
    if(bass_xover_row_ && mid_xover_row_)
    {
        audio->setCrossovers(static_cast<float>(bass_xover_row_->spinBox()->value()),
                             static_cast<float>(mid_xover_row_->spinBox()->value()));
    }
}

void AudioAdvancedSettingsDialog::onRestoreDefaultsClicked()
{
    resetToFactoryDefaults();
    loadFromManager();
}

void AudioAdvancedSettingsDialog::resetToFactoryDefaults()
{
    AudioInputManager::instance()->resetAnalyzerTuning();
}

void AudioAdvancedSettingsDialog::applyFromPluginSettings(const nlohmann::json& settings)
{
    AudioInputManager* audio = AudioInputManager::instance();
    if(!audio)
    {
        return;
    }

    if(settings.contains("AudioSmoothingPct"))
    {
        audio->setSmoothing(decayFromPct(settings["AudioSmoothingPct"].get<int>(), 0.0f, 0.99f));
    }
    if(settings.contains("AudioBandPeakDecayPct"))
    {
        audio->setBandPeakDecay(decayFromPct(settings["AudioBandPeakDecayPct"].get<int>(), 0.97f, 0.999f));
    }
    if(settings.contains("AudioBassPeakDecayPct"))
    {
        audio->setBassPeakDecay(decayFromPct(settings["AudioBassPeakDecayPct"].get<int>(), 0.98f, 0.999f));
    }
    if(settings.contains("AudioActivityPeakDecayPct"))
    {
        audio->setActivityPeakDecay(decayFromPct(settings["AudioActivityPeakDecayPct"].get<int>(), 0.96f, 0.999f));
    }
    if(settings.contains("AudioVisualizerPeakDecayPct"))
    {
        audio->setVisualizerPeakDecay(decayFromPct(settings["AudioVisualizerPeakDecayPct"].get<int>(), 0.85f, 0.99f));
    }
    if(settings.contains("AudioVisualizerFloorPct"))
    {
        audio->setVisualizerFloor(floorValueFromPct(settings["AudioVisualizerFloorPct"].get<int>()));
    }
    if(settings.contains("AudioAutoLevelEnabled"))
    {
        audio->setAutoLevelEnabled(settings["AudioAutoLevelEnabled"].get<bool>());
    }
    if(settings.contains("AudioAutoLevelPeakDecayPct"))
    {
        audio->setAutoLevelPeakDecay(decayFromPct(settings["AudioAutoLevelPeakDecayPct"].get<int>(), 0.98f, 0.999f));
    }
    if(settings.contains("AudioAutoLevelFloorDecayPct"))
    {
        audio->setAutoLevelFloorDecay(decayFromPct(settings["AudioAutoLevelFloorDecayPct"].get<int>(), 0.98f, 0.9999f));
    }
    if(settings.contains("AudioBassCrossoverHz") && settings.contains("AudioMidCrossoverHz"))
    {
        audio->setCrossovers(static_cast<float>(settings["AudioBassCrossoverHz"].get<int>()),
                             static_cast<float>(settings["AudioMidCrossoverHz"].get<int>()));
    }
}

void AudioAdvancedSettingsDialog::writeToPluginSettings(nlohmann::json& settings)
{
    AudioInputManager* audio = AudioInputManager::instance();
    if(!audio)
    {
        return;
    }

    settings["AudioSmoothingPct"]            = pctFromDecay(audio->getSmoothing(), 0.0f, 0.99f);
    settings["AudioBandPeakDecayPct"]        = pctFromDecay(audio->getBandPeakDecay(), 0.97f, 0.999f);
    settings["AudioBassPeakDecayPct"]        = pctFromDecay(audio->getBassPeakDecay(), 0.98f, 0.999f);
    settings["AudioActivityPeakDecayPct"]     = pctFromDecay(audio->getActivityPeakDecay(), 0.96f, 0.999f);
    settings["AudioVisualizerPeakDecayPct"]  = pctFromDecay(audio->getVisualizerPeakDecay(), 0.85f, 0.99f);
    settings["AudioVisualizerFloorPct"]      = floorPctFromValue(audio->getVisualizerFloor());
    settings["AudioAutoLevelEnabled"]        = audio->isAutoLevelEnabled();
    settings["AudioAutoLevelPeakDecayPct"]   = pctFromDecay(audio->getAutoLevelPeakDecay(), 0.98f, 0.999f);
    settings["AudioAutoLevelFloorDecayPct"]   = pctFromDecay(audio->getAutoLevelFloorDecay(), 0.98f, 0.9999f);
    settings["AudioBassCrossoverHz"]          = static_cast<int>(std::lround(audio->getBassUpperHz()));
    settings["AudioMidCrossoverHz"]          = static_cast<int>(std::lround(audio->getMidUpperHz()));
}

bool AudioAdvancedSettingsDialog::run(OpenRGB3DSpatialTab* tab, QWidget* parent)
{
    if(!tab)
    {
        return false;
    }

    AudioAdvancedSettingsDialog dialog(tab, parent);
    if(dialog.exec() != QDialog::Accepted)
    {
        return false;
    }

    dialog.applyToManager();
    nlohmann::json settings = tab->GetPluginSettings();
    writeToPluginSettings(settings);
    tab->SetPluginSettings(settings);
    return true;
}
