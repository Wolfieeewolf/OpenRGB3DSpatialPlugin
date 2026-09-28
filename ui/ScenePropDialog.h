// SPDX-License-Identifier: GPL-2.0-only

#ifndef SCENEPROPDIALOG_H
#define SCENEPROPDIALOG_H

#include "SceneProp3D.h"

#include <QDialog>
#include <QString>

class QLineEdit;
class QDoubleSpinBox;
class QPushButton;
class QComboBox;

class ScenePropDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ScenePropDialog(QWidget* parent = nullptr);

    void setCreateMode();
    void setEditMode();
    void setCreateDefaults(const QString& suggested_name,
                           float width_mm,
                           float height_mm,
                           float depth_mm,
                           unsigned int color_rgb);
    void loadFrom(const SceneProp3D& prop);
    void applyTo(SceneProp3D* prop) const;

    QString name() const;
    float widthMm() const;
    float heightMm() const;
    float depthMm() const;
    unsigned int colorRgb() const;
    unsigned int glassColorRgb() const;

private slots:
    void onPickSolidColor();
    void onPickGlassColor();
    void onSetAllFaces();

private:
    void syncColorButton(QPushButton* button, unsigned int rgb);
    QComboBox* makeFaceCombo();

    QLineEdit* name_edit_ = nullptr;
    QDoubleSpinBox* width_spin_ = nullptr;
    QDoubleSpinBox* height_spin_ = nullptr;
    QDoubleSpinBox* depth_spin_ = nullptr;
    QPushButton* color_button_ = nullptr;
    QPushButton* glass_color_button_ = nullptr;
    QComboBox* face_combos_[SceneProp3D::kFaceCount] = {};
    QComboBox* set_all_combo_ = nullptr;
    unsigned int color_rgb_ = 0x555555u;
    unsigned int glass_color_rgb_ = 0x4AA8C8u;
};

#endif
