// SPDX-License-Identifier: GPL-2.0-only

#ifndef STRIPKERNELCOLORMAPPANEL_H
#define STRIPKERNELCOLORMAPPANEL_H

#include <QWidget>

namespace Ui {
class StripKernelColormapPanel;
}

class StripKernelColormapPanel : public QWidget
{
    Q_OBJECT

public:
    explicit StripKernelColormapPanel(QWidget* parent = nullptr);
    ~StripKernelColormapPanel() override;

    bool useStripColormap() const;
    int kernelId() const;
    float kernelRepeats() const;
    int unfoldMode() const;
    float directionDeg() const;
    int colorStyle() const;

    void mirrorStateFromEffect(bool on, int kernel, float rep, int unfold, float dir_deg, int color_style);

signals:
    void colormapChanged();

private slots:
    void onSourceChanged(int);
    void onKernelChanged(int);
    void onUnfoldChanged(int);
    void onRepeatsChanged(int);
    void onDirChanged(int);
    void onColorStyleChanged(int);

private:
    static const char* UnfoldLabel(int m);
    void populateCombos();
    void refreshSecondaryEnabled();

    Ui::StripKernelColormapPanel* ui = nullptr;
};

#endif
