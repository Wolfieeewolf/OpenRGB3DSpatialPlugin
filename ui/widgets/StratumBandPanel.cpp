// SPDX-License-Identifier: GPL-2.0-only

#include "StratumBandPanel.h"

#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QSlider>
#include <QVBoxLayout>

#include <algorithm>

StratumBandPanel::StratumBandPanel(QWidget* parent) : QWidget(parent)
{
    QVBoxLayout* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(4);

    QHBoxLayout* row = new QHBoxLayout();
    row->addWidget(new QLabel(QStringLiteral("Height bands:")));
    layout_combo_ = new QComboBox();
    layout_combo_->addItem(QStringLiteral("Single field"), 0);
    layout_combo_->addItem(QStringLiteral("Per height band (floor · mid · ceiling)"), 1);
    layout_combo_->setToolTip(
        QStringLiteral("Per band: blend speed / tightness / phase for one pattern pass, plus optional spatial motion "
                       "(scroll room axes, phase spin, roll) so calm effects still drift across the room."));
    row->addWidget(layout_combo_, 1);
    outer->addLayout(row);

    connect(layout_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
        if(layout_combo_)
        {
            layout_mode_ = std::clamp(layout_combo_->currentData().toInt(), 0, 1);
        }
        if(group_)
        {
            group_->setVisible(layout_mode_ == 1);
        }
        if(motion_group_)
        {
            motion_group_->setVisible(layout_mode_ == 1);
        }
        emit bandParametersChanged();
    });

    group_ = new QGroupBox(QStringLiteral("Band tuning"));
    group_->setToolTip(
        QStringLiteral("Speed %% scales time motion in that band. Tightness %% scales spatial detail in the pattern. "
                       "Phase ° adds pattern phase offset per band."));
    QGridLayout* band_grid = new QGridLayout(group_);
    band_grid->addWidget(new QLabel(QString()), 0, 0);
    band_grid->addWidget(new QLabel(QStringLiteral("Speed %")), 0, 1);
    band_grid->addWidget(new QLabel(QStringLiteral("Tight %")), 0, 3);
    band_grid->addWidget(new QLabel(QStringLiteral("Phase °")), 0, 5);

    for(int i = 0; i < 3; i++)
    {
        const int r = i + 1;
        band_grid->addWidget(new QLabel(QStringLiteral("%1:").arg(EffectStratumBlend::BandNameUi(i))), r, 0);

        speed_sl_[i] = new QSlider(Qt::Horizontal);
        speed_sl_[i]->setRange(0, 200);
        speed_sl_[i]->setValue(tuning_.speed[(size_t)i]);
        speed_lbl_[i] = new QLabel(QString::number(tuning_.speed[(size_t)i]));
        speed_lbl_[i]->setMinimumWidth(28);
        band_grid->addWidget(speed_sl_[i], r, 1);
        band_grid->addWidget(speed_lbl_[i], r, 2);
        connect(speed_sl_[i], &QSlider::valueChanged, this, [this, i](int v) {
            tuning_.speed[(size_t)i] = std::clamp(v, 0, 200);
            if(speed_lbl_[i])
            {
                speed_lbl_[i]->setText(QString::number(tuning_.speed[(size_t)i]));
            }
            emit bandParametersChanged();
        });

        tight_sl_[i] = new QSlider(Qt::Horizontal);
        tight_sl_[i]->setRange(25, 300);
        tight_sl_[i]->setValue(tuning_.tight[(size_t)i]);
        tight_lbl_[i] = new QLabel(QString::number(tuning_.tight[(size_t)i]));
        tight_lbl_[i]->setMinimumWidth(28);
        band_grid->addWidget(tight_sl_[i], r, 3);
        band_grid->addWidget(tight_lbl_[i], r, 4);
        connect(tight_sl_[i], &QSlider::valueChanged, this, [this, i](int v) {
            tuning_.tight[(size_t)i] = std::clamp(v, 25, 300);
            if(tight_lbl_[i])
            {
                tight_lbl_[i]->setText(QString::number(tuning_.tight[(size_t)i]));
            }
            emit bandParametersChanged();
        });

        phase_sl_[i] = new QSlider(Qt::Horizontal);
        phase_sl_[i]->setRange(-180, 180);
        phase_sl_[i]->setValue(tuning_.phase_deg[(size_t)i]);
        phase_lbl_[i] = new QLabel(QString::number(tuning_.phase_deg[(size_t)i]));
        phase_lbl_[i]->setMinimumWidth(32);
        band_grid->addWidget(phase_sl_[i], r, 5);
        band_grid->addWidget(phase_lbl_[i], r, 6);
        connect(phase_sl_[i], &QSlider::valueChanged, this, [this, i](int v) {
            tuning_.phase_deg[(size_t)i] = std::clamp(v, -180, 180);
            if(phase_lbl_[i])
            {
                phase_lbl_[i]->setText(QString::number(tuning_.phase_deg[(size_t)i]));
            }
            emit bandParametersChanged();
        });
    }

    group_->setVisible(layout_mode_ == 1);
    outer->addWidget(group_);

    motion_group_ = new QGroupBox(QStringLiteral("Spatial motion by band"));
    motion_group_->setToolTip(
        QStringLiteral("When height bands are on: each floor / mid / ceiling row can add scroll along room X/Y/Z, "
                       "clockwise or counter-clockwise phase drift, or horizontal roll around the effect origin. "
                       "Rate scales strength."));
    QGridLayout* mot_grid = new QGridLayout(motion_group_);
    mot_grid->addWidget(new QLabel(QString()), 0, 0);
    mot_grid->addWidget(new QLabel(QStringLiteral("Motion")), 0, 1);
    mot_grid->addWidget(new QLabel(QStringLiteral("Rate %")), 0, 3);
    for(int i = 0; i < 3; i++)
    {
        const int r = i + 1;
        mot_grid->addWidget(new QLabel(QStringLiteral("%1:").arg(EffectStratumBlend::BandNameUi(i))), r, 0);
        motion_kind_combo_[i] = new QComboBox();
        motion_kind_combo_[i]->addItem(QStringLiteral("Off"), 0);
        motion_kind_combo_[i]->addItem(QStringLiteral("Scroll room X"), 1);
        motion_kind_combo_[i]->addItem(QStringLiteral("Scroll room Y"), 2);
        motion_kind_combo_[i]->addItem(QStringLiteral("Scroll room Z"), 3);
        motion_kind_combo_[i]->addItem(QStringLiteral("Phase drift CW"), 4);
        motion_kind_combo_[i]->addItem(QStringLiteral("Phase drift CCW"), 5);
        motion_kind_combo_[i]->addItem(QStringLiteral("Roll (around origin)"), 6);
        const int mk = std::clamp(tuning_.motion_kind[(size_t)i], 0, 6);
        for(int j = 0; j < motion_kind_combo_[i]->count(); ++j)
        {
            if(motion_kind_combo_[i]->itemData(j).toInt() == mk)
            {
                motion_kind_combo_[i]->setCurrentIndex(j);
                break;
            }
        }
        mot_grid->addWidget(motion_kind_combo_[i], r, 1, 1, 2);
        connect(motion_kind_combo_[i], qOverload<int>(&QComboBox::currentIndexChanged), this, [this, i]() {
            if(motion_kind_combo_[i])
            {
                tuning_.motion_kind[(size_t)i] =
                    std::clamp(motion_kind_combo_[i]->currentData().toInt(), 0, 6);
            }
            emit bandParametersChanged();
        });

        motion_rate_sl_[i] = new QSlider(Qt::Horizontal);
        motion_rate_sl_[i]->setRange(0, 200);
        motion_rate_sl_[i]->setValue(tuning_.motion_rate[(size_t)i]);
        motion_rate_lbl_[i] = new QLabel(QString::number(tuning_.motion_rate[(size_t)i]));
        motion_rate_lbl_[i]->setMinimumWidth(28);
        mot_grid->addWidget(motion_rate_sl_[i], r, 3);
        mot_grid->addWidget(motion_rate_lbl_[i], r, 4);
        connect(motion_rate_sl_[i], &QSlider::valueChanged, this, [this, i](int v) {
            tuning_.motion_rate[(size_t)i] = std::clamp(v, 0, 200);
            if(motion_rate_lbl_[i])
            {
                motion_rate_lbl_[i]->setText(QString::number(tuning_.motion_rate[(size_t)i]));
            }
            emit bandParametersChanged();
        });
    }
    motion_group_->setVisible(layout_mode_ == 1);
    outer->addWidget(motion_group_);
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
    if(group_)
    {
        group_->setVisible(layout_mode_ == 1);
    }
    if(motion_group_)
    {
        motion_group_->setVisible(layout_mode_ == 1);
    }
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
    if(group_)
    {
        group_->setVisible(layout_mode_ == 1);
    }
    if(motion_group_)
    {
        motion_group_->setVisible(layout_mode_ == 1);
    }
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
