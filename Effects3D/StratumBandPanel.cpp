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

StratumBandPanel::StratumBandPanel(QWidget* parent) : QWidget(parent)
{
    QVBoxLayout* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(4);

    QHBoxLayout* row = new QHBoxLayout();
    row->addWidget(new QLabel(QStringLiteral("Height bands:")));
    layout_combo_ = new QComboBox();
    layout_combo_->addItem(QStringLiteral("Single field"), 0);
    layout_combo_->addItem(QStringLiteral("Per band (floor · mid · ceiling)"), 1);
    layout_combo_->setToolTip(
        QStringLiteral("Optional: each vertical stratum blends its own speed, tightness, and phase "
                       "(soft transitions at band edges; same origin and geometry)."));
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
        emit bandParametersChanged();
    });

    group_ = new QGroupBox(QStringLiteral("Band tuning"));
    group_->setToolTip(
        QStringLiteral("Speed %% scales time motion in that band. Tightness %% scales spatial detail. "
                       "Phase ° adds hue / pattern twist per band."));
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
    }
}
