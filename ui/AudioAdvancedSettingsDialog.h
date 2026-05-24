// SPDX-License-Identifier: GPL-2.0-only

#ifndef AUDIOADVANCEDSETTINGSDIALOG_H
#define AUDIOADVANCEDSETTINGSDIALOG_H

#include <QDialog>

#include <nlohmann/json.hpp>

class OpenRGB3DSpatialTab;

namespace Ui {
class AudioAdvancedSettingsDialog;
}

class AudioAdvancedSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AudioAdvancedSettingsDialog(OpenRGB3DSpatialTab* tab, QWidget* parent = nullptr);
    ~AudioAdvancedSettingsDialog() override;

    static void applyFromPluginSettings(const nlohmann::json& settings);
    static void writeToPluginSettings(nlohmann::json& settings);
    static void resetToFactoryDefaults();

    static bool run(OpenRGB3DSpatialTab* tab, QWidget* parent = nullptr);

private:
    void buildRows();
    void loadFromManager();
    void applyToManager() const;

    void onRestoreDefaultsClicked();

    Ui::AudioAdvancedSettingsDialog* ui = nullptr;
    OpenRGB3DSpatialTab*            tab_  = nullptr;

    class EffectSliderRow* smoothing_row_           = nullptr;
    class EffectSliderRow* band_peak_decay_row_     = nullptr;
    class EffectSliderRow* bass_peak_decay_row_     = nullptr;
    class EffectSliderRow* activity_peak_decay_row_ = nullptr;
    class EffectSliderRow* visualizer_decay_row_    = nullptr;
    class EffectSliderRow* visualizer_floor_row_    = nullptr;
    class EffectSliderRow* auto_peak_decay_row_     = nullptr;
    class EffectSliderRow* auto_floor_decay_row_    = nullptr;
    class EffectCheckRow*  auto_level_check_        = nullptr;
    class EffectLabeledSpinRow* bass_xover_row_     = nullptr;
    class EffectLabeledSpinRow* mid_xover_row_      = nullptr;
};

#endif
