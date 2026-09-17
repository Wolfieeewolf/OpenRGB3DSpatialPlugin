// SPDX-License-Identifier: GPL-2.0-only

#ifndef AUDIOEQBANDCOLUMN_H
#define AUDIOEQBANDCOLUMN_H

#include <QWidget>

class QLabel;
class QSlider;

namespace Ui {
class AudioEqBandColumn;
}

class AudioEqBandColumn : public QWidget
{
public:
    explicit AudioEqBandColumn(QWidget* parent = nullptr);
    ~AudioEqBandColumn() override;

    void setCaptionText(const QString& text);
    void setCaptionToolTip(const QString& text);
    void applyCaptionStyle();

    QSlider* gainSlider() const;

private:
    Ui::AudioEqBandColumn* ui = nullptr;
};

#endif
