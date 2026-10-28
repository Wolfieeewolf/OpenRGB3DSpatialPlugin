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
class QGroupBox;
class QLabel;
class QWidget;
class OpenRGBPluginAPIInterface;
class ScenePropPreviewWidget;

class ScenePropDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ScenePropDialog(QWidget* parent = nullptr);

    void setResourceManager(OpenRGBPluginAPIInterface* rm) { resource_manager_ = rm; }
    void setRoomSizeMm(float width_mm, float height_mm, float depth_mm);

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
    ScenePropShape shape() const;
    QString meshAssetRelative() const;
    ScenePropFaceKind meshBodyKind() const;

public slots:
    void accept() override;

private slots:
    void onPickSolidColor();
    void onPickGlassColor();
    void onSetAllFaces();
    void onShapeChanged(int index);
    void onImportMesh();
    void refreshPreview();

private:
    void syncColorButton(QPushButton* button, unsigned int rgb);
    QComboBox* makeFaceCombo();
    void updateShapeUi();
    void setStatus(const QString& text);
    void collectFaceKinds(ScenePropFaceKind out[SceneProp3D::kFaceCount]) const;

    OpenRGBPluginAPIInterface* resource_manager_ = nullptr;
    float room_width_mm_ = 1000.0f;
    float room_height_mm_ = 1000.0f;
    float room_depth_mm_ = 1000.0f;

    ScenePropPreviewWidget* preview_ = nullptr;
    QComboBox* shape_combo_ = nullptr;
    QLineEdit* name_edit_ = nullptr;
    QDoubleSpinBox* width_spin_ = nullptr;
    QDoubleSpinBox* height_spin_ = nullptr;
    QDoubleSpinBox* depth_spin_ = nullptr;
    QPushButton* color_button_ = nullptr;
    QPushButton* glass_color_button_ = nullptr;
    QLabel* glass_color_label_ = nullptr;
    QGroupBox* faces_box_ = nullptr;
    QComboBox* face_combos_[SceneProp3D::kFaceCount] = {};
    QComboBox* set_all_combo_ = nullptr;
    QComboBox* mesh_body_combo_ = nullptr;
    QLabel* mesh_path_label_ = nullptr;
    QLabel* mesh_body_label_ = nullptr;
    QPushButton* import_mesh_button_ = nullptr;
    QWidget* mesh_panel_ = nullptr;
    QLabel* status_label_ = nullptr;
    unsigned int color_rgb_ = 0x555555u;
    unsigned int glass_color_rgb_ = 0x4AA8C8u;
    QString mesh_asset_relative_;
};

#endif
