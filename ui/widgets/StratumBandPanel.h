// SPDX-License-Identifier: GPL-2.0-only

#ifndef STRATUMBANDPANEL_H
#define STRATUMBANDPANEL_H

#include "EffectStratumBlend.h"

#include <QWidget>
#include <array>

class QComboBox;
class QGroupBox;
class QSlider;
class QLabel;

class StratumBandPanel : public QWidget
{
    Q_OBJECT

public:
    explicit StratumBandPanel(QWidget* parent = nullptr);

    int layoutMode() const { return layout_mode_; }
    void setLayoutMode(int m);

    EffectStratumBlend::BandTuningPct tuning() const { return tuning_; }
    void setTuning(const EffectStratumBlend::BandTuningPct& t);

    void syncWidgetsFromModel();

signals:
    void bandParametersChanged();

private:
    int layout_mode_ = 0;
    EffectStratumBlend::BandTuningPct tuning_{};

    QComboBox* layout_combo_ = nullptr;
    QGroupBox* group_ = nullptr;
    QGroupBox* motion_group_ = nullptr;
    QSlider* speed_sl_[3]{};
    QSlider* tight_sl_[3]{};
    QSlider* phase_sl_[3]{};
    QLabel* speed_lbl_[3]{};
    QLabel* tight_lbl_[3]{};
    QLabel* phase_lbl_[3]{};
    QComboBox* motion_kind_combo_[3]{};
    QSlider* motion_rate_sl_[3]{};
    QLabel* motion_rate_lbl_[3]{};
};

#endif
