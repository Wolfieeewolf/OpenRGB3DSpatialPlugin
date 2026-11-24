// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef STRIPKERNELCOLORMAPPANEL_H
#define STRIPKERNELCOLORMAPPANEL_H

#include <QWidget>

class EffectSliderRow;

namespace Ui {
class StripKernelColormapPanel;
}

class StripKernelColormapPanel : public QWidget
{
    Q_OBJECT

public:
    /** Color from: Pattern palette (0) or Effect Look/Custom/Rainbow stack (1). */
    enum ColorFrom : int { ColorFromPattern = 0, ColorFromEffect = 1 };

    explicit StripKernelColormapPanel(QWidget* parent = nullptr);
    ~StripKernelColormapPanel() override;

    bool useStripColormap() const;
    int kernelId() const;
    float kernelRepeats() const;
    int unfoldMode() const;
    float directionDeg() const;
    int colorFrom() const;

    void mirrorStateFromEffect(bool on, int kernel, float rep, int unfold, float dir_deg, int color_from = 0);

signals:
    void colormapChanged();

private slots:
    void onSourceChanged(int);
    void onKernelChanged(int);
    void onColorFromChanged(int);
    void onUnfoldChanged(int);
    void onRepeatsChanged(int);
    void onDirChanged(int);

private:
    static const char* UnfoldLabel(int m);
    void populateCombos();
    void refreshSecondaryEnabled();

    Ui::StripKernelColormapPanel* ui = nullptr;
    EffectSliderRow* repeats_row_ = nullptr;
    EffectSliderRow* dir_row_ = nullptr;
};

#endif
