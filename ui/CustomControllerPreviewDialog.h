// SPDX-License-Identifier: GPL-2.0-only

#ifndef CUSTOMCONTROLLERPREVIEWDIALOG_H
#define CUSTOMCONTROLLERPREVIEWDIALOG_H

#include <QDialog>
class QShowEvent;
class QHideEvent;
#include <memory>
#include <vector>
#include "LEDPosition3D.h"
#include "VirtualController3D.h"

class QTimer;
class LEDViewport3D;
struct GridLEDMapping;

namespace Ui {
class CustomControllerPreviewDialog;
}

class CustomControllerDialog;

class CustomControllerPreviewDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CustomControllerPreviewDialog(QWidget* parent = nullptr);
    ~CustomControllerPreviewDialog() override;

    void UpdatePreview(CustomControllerDialog* source_editor,
                       const std::string& name,
                       int grid_w,
                       int grid_h,
                       int grid_d,
                       float spacing_x_mm,
                       float spacing_y_mm,
                       float spacing_z_mm,
                       float grid_scale_mm,
                       const std::vector<GridLEDMapping>& mappings,
                       bool recenter_transform = true);

    void RefreshPreviewFromEditor();
    void RefreshPreviewColors();

protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    void RebuildPreviewScene(const std::string& name,
                             int grid_w,
                             int grid_h,
                             int grid_d,
                             float spacing_x_mm,
                             float spacing_y_mm,
                             float spacing_z_mm,
                             float grid_scale_mm,
                             const std::vector<GridLEDMapping>& mappings,
                             bool recenter_transform);
    void ClampTransformToPreviewGrid();
    void FrameCameraOnLayout();

    int preview_grid_extent;
    float preview_scale_x;
    float preview_scale_y;
    float preview_scale_z;

    Ui::CustomControllerPreviewDialog* ui = nullptr;
    LEDViewport3D* viewport = nullptr;
    QTimer* color_timer = nullptr;
    CustomControllerDialog* source_editor;

    std::unique_ptr<VirtualController3D> virtual_controller;
    std::vector<std::unique_ptr<ControllerTransform>> transforms;
};

#endif
