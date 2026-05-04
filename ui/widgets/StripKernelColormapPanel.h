// SPDX-License-Identifier: GPL-2.0-only

#ifndef STRIPKERNELCOLORMAPPANEL_H
#define STRIPKERNELCOLORMAPPANEL_H

#include <QWidget>

class QComboBox;
class QSlider;
class QLabel;

class StripKernelColormapPanel : public QWidget
{
    Q_OBJECT

public:
    explicit StripKernelColormapPanel(QWidget* parent = nullptr);

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
    void refreshSecondaryEnabled();

    QComboBox* source_combo = nullptr;
    QComboBox* color_style_combo = nullptr;
    QComboBox* kernel_combo = nullptr;
    QComboBox* unfold_combo = nullptr;
    QSlider* repeats_slider = nullptr;
    QLabel* repeats_label = nullptr;
    QSlider* dir_slider = nullptr;
    QLabel* dir_label = nullptr;
    QWidget* secondary_row = nullptr;
};

#endif
