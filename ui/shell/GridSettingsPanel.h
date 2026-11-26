// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef GRIDSETTINGSPANEL_H
#define GRIDSETTINGSPANEL_H

#include <QWidget>

class EffectSliderRow;
class QCheckBox;
class QDoubleSpinBox;
class QLabel;
class QSlider;

namespace Ui {
class GridSettingsPanel;
}

class OpenRGB3DSpatialTab;

class GridSettingsPanel : public QWidget
{
    Q_OBJECT

public:
    explicit GridSettingsPanel(QWidget* parent = nullptr);
    ~GridSettingsPanel() override;

    void bindTab(OpenRGB3DSpatialTab* tab);

    QDoubleSpinBox* gridScaleSpin() const;
    QCheckBox*      gridSnapCheckbox() const;
    QCheckBox*      rotateSnapCheckbox() const;
    QLabel*         selectionInfoLabel() const;
    QDoubleSpinBox* roomWidthSpin() const;
    QDoubleSpinBox* roomHeightSpin() const;
    QDoubleSpinBox* roomDepthSpin() const;
    QCheckBox*      roomGridOverlayCheckbox() const;
    QCheckBox*      roomGuideLabelsCheckbox() const;

private slots:
    void onGridScaleChanged(double value);
    void onRoomGridOverlayToggled(bool checked);
    void onOverlayBrightChanged(int v);
    void onOverlayPointChanged(int v);
    void onOverlayStepChanged(int v);
    void onOverlayModeChanged(int index);
    void onRealLightReachChanged(int mm);
    void onRoomWidthChanged(double value);
    void onRoomHeightChanged(double value);
    void onRoomDepthChanged(double value);

private:
    Ui::GridSettingsPanel* ui;
    OpenRGB3DSpatialTab*   host_tab_ = nullptr;
    EffectSliderRow* overlay_bright_row_ = nullptr;
    EffectSliderRow* overlay_point_row_ = nullptr;
    EffectSliderRow* overlay_step_row_ = nullptr;
    QSlider* overlay_bright_slider_ = nullptr;
    QSlider* overlay_point_slider_ = nullptr;
    QSlider* overlay_step_slider_ = nullptr;
    EffectSliderRow* real_light_reach_row_ = nullptr;
    QSlider* real_light_reach_slider_ = nullptr;
    QLabel* real_light_reach_caption_ = nullptr;
    void syncOverlayModeUi();
};

#endif
