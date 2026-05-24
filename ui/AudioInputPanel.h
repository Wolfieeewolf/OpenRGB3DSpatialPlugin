// SPDX-License-Identifier: GPL-2.0-only

#ifndef AUDIOINPUTPANEL_H
#define AUDIOINPUTPANEL_H

#include <QGroupBox>

class QComboBox;
class QLabel;
class QProgressBar;
class QPushButton;
class QScrollArea;
class QSlider;

namespace Ui {
class AudioInputPanel;
}

class OpenRGB3DSpatialTab;

class AudioInputPanel : public QGroupBox
{
    Q_OBJECT

public:
    explicit AudioInputPanel(QWidget* parent = nullptr);
    ~AudioInputPanel() override;

    void bindTab(OpenRGB3DSpatialTab* tab);

    QPushButton*   startButton() const;
    QPushButton*   stopButton() const;
    QProgressBar*  levelBar() const;
    QProgressBar*  bassBar() const;
    QProgressBar*  midBar() const;
    QProgressBar*  highBar() const;
    QProgressBar*  kickStemBar() const;
    QProgressBar*  snareStemBar() const;
    QProgressBar*  hihatStemBar() const;
    QProgressBar*  bassStemBar() const;
    QLabel*        spectrumLabel() const;
    QComboBox*     deviceCombo() const;
    QSlider*       gainSlider() const;
    QLabel*        gainValueLabel() const;
    QSlider*       claritySlider() const;
    QLabel*        clarityValueLabel() const;
    QSlider*       isolationSlider() const;
    QLabel*        isolationValueLabel() const;
    QComboBox*     mixPresetCombo() const;
    QComboBox*     bandsCombo() const;
    QComboBox*     fftCombo() const;
    QLabel*        eqCaption() const;
    QScrollArea*   eqScroll() const;

private:
    Ui::AudioInputPanel* ui = nullptr;
};

#endif
