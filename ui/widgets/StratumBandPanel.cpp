// SPDX-License-Identifier: GPL-2.0-only

#include "StratumBandPanel.h"
#include "EffectSliderPromote.h"
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
} // namespace

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
            emit bandParametersChanged();
        });
        connect(tight_sl_[i], &QSlider::valueChanged, this, [this, i](int v) {
            tuning_.tight[(size_t)i] = std::clamp(v, 25, 300);
            emit bandParametersChanged();
        });
        connect(phase_sl_[i], &QSlider::valueChanged, this, [this, i](int v) {
            tuning_.phase_deg[(size_t)i] = std::clamp(v, -180, 180);
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

    QSlider* speed_src[3] = {ui->speedSliderFloor, ui->speedSliderMid, ui->speedSliderCeiling};
    QLabel* speed_lbl_src[3] = {ui->speedLabelFloor, ui->speedLabelMid, ui->speedLabelCeiling};
    QSlider* tight_src[3] = {ui->tightSliderFloor, ui->tightSliderMid, ui->tightSliderCeiling};
    QLabel* tight_lbl_src[3] = {ui->tightLabelFloor, ui->tightLabelMid, ui->tightLabelCeiling};
    QSlider* phase_src[3] = {ui->phaseSliderFloor, ui->phaseSliderMid, ui->phaseSliderCeiling};
    QLabel* phase_lbl_src[3] = {ui->phaseLabelFloor, ui->phaseLabelMid, ui->phaseLabelCeiling};
    QSlider* rate_src[3] = {ui->motionRateSliderFloor, ui->motionRateSliderMid, ui->motionRateSliderCeiling};
    QLabel* rate_lbl_src[3] = {ui->motionRateLabelFloor, ui->motionRateLabelMid, ui->motionRateLabelCeiling};

    for(int i = 0; i < 3; i++)
    {
        EffectSliderRow* speed_row = PromoteSliderLabelInGrid(ui->bandGrid, speed_src[i], speed_lbl_src[i]);
        EffectSliderRow* tight_row = PromoteSliderLabelInGrid(ui->bandGrid, tight_src[i], tight_lbl_src[i],
                                                               QStringLiteral("%"));
        EffectSliderRow* phase_row = PromoteSliderLabelInGrid(ui->bandGrid, phase_src[i], phase_lbl_src[i],
                                                               QString(QChar(0x00B0)));
        EffectSliderRow* rate_row = PromoteSliderLabelInGrid(ui->motionGrid, rate_src[i], rate_lbl_src[i],
                                                              QStringLiteral("%"));
        speed_sl_[i] = speed_row ? speed_row->slider() : nullptr;
        tight_sl_[i] = tight_row ? tight_row->slider() : nullptr;
        phase_sl_[i] = phase_row ? phase_row->slider() : nullptr;
        motion_rate_sl_[i] = rate_row ? rate_row->slider() : nullptr;
        speed_lbl_[i] = speed_row ? speed_row->valueLabel() : nullptr;
        tight_lbl_[i] = tight_row ? tight_row->valueLabel() : nullptr;
        phase_lbl_[i] = phase_row ? phase_row->valueLabel() : nullptr;
        motion_rate_lbl_[i] = rate_row ? rate_row->valueLabel() : nullptr;
    }

    motion_kind_combo_[0] = ui->motionKindFloor;
    motion_kind_combo_[1] = ui->motionKindMid;
    motion_kind_combo_[2] = ui->motionKindCeiling;
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
        SyncEffectSliderValue(speed_sl_[i], tuning_.speed[(size_t)i]);
        SyncEffectSliderValue(tight_sl_[i], tuning_.tight[(size_t)i]);
        SyncEffectSliderValue(phase_sl_[i], tuning_.phase_deg[(size_t)i]);
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
        SyncEffectSliderValue(motion_rate_sl_[i], tuning_.motion_rate[(size_t)i]);
    }
}
