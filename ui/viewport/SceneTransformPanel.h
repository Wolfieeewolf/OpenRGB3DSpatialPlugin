// SPDX-License-Identifier: GPL-2.0-only

#ifndef SCENETRANSFORMPANEL_H
#define SCENETRANSFORMPANEL_H

#include <QWidget>

#include <memory>

class QSlider;
class PositionAxisDragController;
class QDoubleSpinBox;
class QString;

namespace Ui {
class SceneTransformPanel;
}

class OpenRGB3DSpatialTab;

class SceneTransformPanel : public QWidget
{
    Q_OBJECT

public:
    explicit SceneTransformPanel(QWidget* parent = nullptr);
    ~SceneTransformPanel() override;

    void bindTab(OpenRGB3DSpatialTab* tab);
    void setPositionControlsMm(double x_mm, double y_mm, double z_mm);

    QDoubleSpinBox* posXSpin() const;
    QDoubleSpinBox* posYSpin() const;
    QDoubleSpinBox* posZSpin() const;
    QSlider*        posXSlider() const;
    QSlider*        posYSlider() const;
    QSlider*        posZSlider() const;
    QDoubleSpinBox* rotXSpin() const;
    QDoubleSpinBox* rotYSpin() const;
    QDoubleSpinBox* rotZSpin() const;
    QSlider*        rotXSlider() const;
    QSlider*        rotYSlider() const;
    QSlider*        rotZSlider() const;

    static void wireRotationRow(OpenRGB3DSpatialTab* tab, QSlider* sl, QDoubleSpinBox* sp, int axis);

private:
    Ui::SceneTransformPanel* ui;
    std::unique_ptr<PositionAxisDragController> pos_x_drag_;
    std::unique_ptr<PositionAxisDragController> pos_y_drag_;
    std::unique_ptr<PositionAxisDragController> pos_z_drag_;
};

#endif
