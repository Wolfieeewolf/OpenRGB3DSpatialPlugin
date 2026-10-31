// SPDX-License-Identifier: GPL-2.0-only

#include "AudioEqBandColumn.h"
#include "PluginUiUtils.h"
#include "ui_AudioEqBandColumn.h"

#include <QLabel>
#include <QSlider>

#include <algorithm>

AudioEqBandColumn::AudioEqBandColumn(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::AudioEqBandColumn)
{
    ui->setupUi(this);
}

AudioEqBandColumn::~AudioEqBandColumn()
{
    delete ui;
}

void AudioEqBandColumn::setCaptionText(const QString& text)
{
    ui->captionLabel->setText(text);
    ui->gainSlider->setAccessibleName(QStringLiteral("EQ band %1 gain").arg(text));
}

void AudioEqBandColumn::setCaptionToolTip(const QString& text)
{
    ui->captionLabel->setToolTip(text);
    if(ui->gainSlider->toolTip().isEmpty())
    {
        ui->gainSlider->setToolTip(text);
    }
}

void AudioEqBandColumn::applyCaptionStyle()
{
    PluginUiApplyMutedSecondaryLabel(ui->captionLabel);
    QFont bf = ui->captionLabel->font();
    bf.setPointSize(std::max(7, bf.pointSize() - 2));
    ui->captionLabel->setFont(bf);
}

QSlider* AudioEqBandColumn::gainSlider() const
{
    return ui->gainSlider;
}
