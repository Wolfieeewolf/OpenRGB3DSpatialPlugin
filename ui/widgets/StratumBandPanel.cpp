// SPDX-License-Identifier: GPL-2.0-only

#include "StratumBandPanel.h"
#include "ui_StratumBandPanel.h"

#include <QComboBox>
#include <QGroupBox>
#include <QLabel>
#include <QSignalBlocker>
#include <QSlider>

#include <algorithm>

namespace
{
void PopulateMotionKindCombo(QComboBox* combo)
{
    if(!combo || combo->count() > 0)
    {
        return;
    }
    combo->addItem(QStringLiteral("Off"), 0);
    combo->addItem(QStringLiteral("Scroll room X"), 1);
    combo->addItem(QStringLiteral("Scroll room Y"), 2);
    combo->addItem(QStringLiteral("Scroll room Z"), 3);
    combo->addItem(QStringLiteral("Phase drift CW"), 4);
    combo->addItem(QStringLiteral("Phase drift CCW"), 5);
    combo->addItem(QStringLiteral("Roll (around origin)"), 6);
}
}

StratumBandPanel::StratumBandPanel(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::StratumBandPanel)
{
    ui->setupUi(this);
    bindWidgetsFromUi();

    layout_combo_->addItem(QStringLiteral("Single field"), 0);
    layout_combo_->addItem(QStringLiteral("Per height band (floor · mid · ceiling)"), 1);

    ui->floorBandCaption->setText(QStringLiteral("%1:").arg(EffectStratumBlend::BandNameUi(0)));
    ui->midBandCaption->setText(QStringLiteral("%1:").arg(EffectStratumBlend::BandNameUi(1)));
    ui->ceilingBandCaption->setText(QStringLiteral("%1:").arg(EffectStratumBlend::BandNameUi(2)));
    ui->motionFloorCaption->setText(QStringLiteral("%1:").arg(EffectStratumBlend::BandNameUi(0)));
    ui->motionMidCaption->setText(QStringLiteral("%1:").arg(EffectStratumBlend::BandNameUi(1)));
    ui->motionCeilingCaption->setText(QStringLiteral("%1:").arg(EffectStratumBlend::BandNameUi(2)));

    populateMotionCombos();

    connect(layout_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
        if(layout_combo_)
        {
            layout_mode_ = std::clamp(layout_combo_->currentData().toInt(), 0, 1);
        }
        updateBandGroupVisibility();
        emit bandParametersChanged();
    });

    for(int i = 0; i < 3; i++)
    {
        connect(speed_sl_[i], &QSlider::valueChanged, this, [this, i](int v) {
            tuning_.speed[(size_t)i] = std::clamp(v, 0, 200);
            if(speed_lbl_[i])
            {
                speed_lbl_[i]->setText(QString::number(tuning_.speed[(size_t)i]));
            }
            emit bandParametersChanged();
        });
        connect(tight_sl_[i], &QSlider::valueChanged, this, [this, i](int v) {
            tuning_.tight[(size_t)i] = std::clamp(v, 25, 300);
            if(tight_lbl_[i])
            {
                tight_lbl_[i]->setText(QString::number(tuning_.tight[(size_t)i]));
            }
            emit bandParametersChanged();
        });
        connect(phase_sl_[i], &QSlider::valueChanged, this, [this, i](int v) {
            tuning_.phase_deg[(size_t)i] = std::clamp(v, -180, 180);
            if(phase_lbl_[i])
            {
                phase_lbl_[i]->setText(QString::number(tuning_.phase_deg[(size_t)i]));
            }
            emit bandParametersChanged();
        });
        connect(motion_kind_combo_[i], qOverload<int>(&QComboBox::currentIndexChanged), this, [this, i]() {
            if(motion_kind_combo_[i])
            {
                tuning_.motion_kind[(size_t)i] =
                    std::clamp(motion_kind_combo_[i]->currentData().toInt(), 0, 6);
            }
            emit bandParametersChanged();
        });
        connect(motion_rate_sl_[i], &QSlider::valueChanged, this, [this, i](int v) {
            tuning_.motion_rate[(size_t)i] = std::clamp(v, 0, 200);
            if(motion_rate_lbl_[i])
            {
                motion_rate_lbl_[i]->setText(QString::number(tuning_.motion_rate[(size_t)i]));
            }
            emit bandParametersChanged();
        });
    }

    syncWidgetsFromModel();
}

StratumBandPanel::~StratumBandPanel()
{
    delete ui;
}

void StratumBandPanel::bindWidgetsFromUi()
{
    layout_combo_ = ui->layoutCombo;
    group_ = ui->bandTuningGroup;
    motion_group_ = ui->motionGroup;

    speed_sl_[0] = ui->speedSliderFloor;
    speed_sl_[1] = ui->speedSliderMid;
    speed_sl_[2] = ui->speedSliderCeiling;
    tight_sl_[0] = ui->tightSliderFloor;
    tight_sl_[1] = ui->tightSliderMid;
    tight_sl_[2] = ui->tightSliderCeiling;
    phase_sl_[0] = ui->phaseSliderFloor;
    phase_sl_[1] = ui->phaseSliderMid;
    phase_sl_[2] = ui->phaseSliderCeiling;

    speed_lbl_[0] = ui->speedLabelFloor;
    speed_lbl_[1] = ui->speedLabelMid;
    speed_lbl_[2] = ui->speedLabelCeiling;
    tight_lbl_[0] = ui->tightLabelFloor;
    tight_lbl_[1] = ui->tightLabelMid;
    tight_lbl_[2] = ui->tightLabelCeiling;
    phase_lbl_[0] = ui->phaseLabelFloor;
    phase_lbl_[1] = ui->phaseLabelMid;
    phase_lbl_[2] = ui->phaseLabelCeiling;

    motion_kind_combo_[0] = ui->motionKindFloor;
    motion_kind_combo_[1] = ui->motionKindMid;
    motion_kind_combo_[2] = ui->motionKindCeiling;
    motion_rate_sl_[0] = ui->motionRateSliderFloor;
    motion_rate_sl_[1] = ui->motionRateSliderMid;
    motion_rate_sl_[2] = ui->motionRateSliderCeiling;
    motion_rate_lbl_[0] = ui->motionRateLabelFloor;
    motion_rate_lbl_[1] = ui->motionRateLabelMid;
    motion_rate_lbl_[2] = ui->motionRateLabelCeiling;
}

void StratumBandPanel::populateMotionCombos()
{
    for(int i = 0; i < 3; i++)
    {
        PopulateMotionKindCombo(motion_kind_combo_[i]);
    }
}

void StratumBandPanel::updateBandGroupVisibility()
{
    const bool per_band = layout_mode_ == 1;
    if(group_)
    {
        group_->setVisible(per_band);
    }
    if(motion_group_)
    {
        motion_group_->setVisible(per_band);
    }
}

void StratumBandPanel::setLayoutMode(int m)
{
    layout_mode_ = std::clamp(m, 0, 1);
    if(layout_combo_)
    {
        QSignalBlocker b(layout_combo_);
        for(int i = 0; i < layout_combo_->count(); i++)
        {
            if(layout_combo_->itemData(i).toInt() == layout_mode_)
            {
                layout_combo_->setCurrentIndex(i);
                break;
            }
        }
    }
    updateBandGroupVisibility();
}

void StratumBandPanel::setTuning(const EffectStratumBlend::BandTuningPct& t)
{
    tuning_ = t;
    syncWidgetsFromModel();
}

void StratumBandPanel::syncWidgetsFromModel()
{
    if(layout_combo_)
    {
        QSignalBlocker b(layout_combo_);
        for(int i = 0; i < layout_combo_->count(); i++)
        {
            if(layout_combo_->itemData(i).toInt() == layout_mode_)
            {
                layout_combo_->setCurrentIndex(i);
                break;
            }
        }
    }
    updateBandGroupVisibility();
    for(int i = 0; i < 3; i++)
    {
        if(speed_sl_[i])
        {
            QSignalBlocker b(speed_sl_[i]);
            speed_sl_[i]->setValue(tuning_.speed[(size_t)i]);
        }
        if(speed_lbl_[i])
        {
            speed_lbl_[i]->setText(QString::number(tuning_.speed[(size_t)i]));
        }
        if(tight_sl_[i])
        {
            QSignalBlocker b(tight_sl_[i]);
            tight_sl_[i]->setValue(tuning_.tight[(size_t)i]);
        }
        if(tight_lbl_[i])
        {
            tight_lbl_[i]->setText(QString::number(tuning_.tight[(size_t)i]));
        }
        if(phase_sl_[i])
        {
            QSignalBlocker b(phase_sl_[i]);
            phase_sl_[i]->setValue(tuning_.phase_deg[(size_t)i]);
        }
        if(phase_lbl_[i])
        {
            phase_lbl_[i]->setText(QString::number(tuning_.phase_deg[(size_t)i]));
        }
        if(motion_kind_combo_[i])
        {
            const int mk = std::clamp(tuning_.motion_kind[(size_t)i], 0, 6);
            QSignalBlocker bm(motion_kind_combo_[i]);
            for(int j = 0; j < motion_kind_combo_[i]->count(); ++j)
            {
                if(motion_kind_combo_[i]->itemData(j).toInt() == mk)
                {
                    motion_kind_combo_[i]->setCurrentIndex(j);
                    break;
                }
            }
        }
        if(motion_rate_sl_[i])
        {
            QSignalBlocker br(motion_rate_sl_[i]);
            motion_rate_sl_[i]->setValue(tuning_.motion_rate[(size_t)i]);
        }
        if(motion_rate_lbl_[i])
        {
            motion_rate_lbl_[i]->setText(QString::number(tuning_.motion_rate[(size_t)i]));
        }
    }
}
