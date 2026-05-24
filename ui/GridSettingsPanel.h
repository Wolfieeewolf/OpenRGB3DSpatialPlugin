// SPDX-License-Identifier: GPL-2.0-only

#ifndef GRIDSETTINGSPANEL_H
#define GRIDSETTINGSPANEL_H

#include <QWidget>

class QCheckBox;
class QDoubleSpinBox;
class QLabel;
class QSpinBox;

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

    QSpinBox*       gridXSpin() const;
    QSpinBox*       gridYSpin() const;
    QSpinBox*       gridZSpin() const;
    QDoubleSpinBox* gridScaleSpin() const;
    QCheckBox*      gridSnapCheckbox() const;
    QLabel*         selectionInfoLabel() const;
    QCheckBox*      useManualRoomSizeCheckbox() const;
    QDoubleSpinBox* roomWidthSpin() const;
    QDoubleSpinBox* roomHeightSpin() const;
    QDoubleSpinBox* roomDepthSpin() const;
    QCheckBox*      roomGridOverlayCheckbox() const;
    QCheckBox*      roomGuideLabelsCheckbox() const;
    QCheckBox*      gpuLabelsCheckbox() const;

private slots:
    void onGridScaleChanged(double value);
    void onRoomGridOverlayToggled(bool checked);
    void onOverlayBrightChanged(int v);
    void onOverlayPointChanged(int v);
    void onOverlayStepChanged(int v);
    void onUseManualRoomSizeToggled(bool checked);
    void onRoomWidthChanged(double value);
    void onRoomHeightChanged(double value);
    void onRoomDepthChanged(double value);

private:
    Ui::GridSettingsPanel*   ui;
    OpenRGB3DSpatialTab*     host_tab_ = nullptr;
};

#endif
