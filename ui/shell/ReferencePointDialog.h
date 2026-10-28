// SPDX-License-Identifier: GPL-2.0-only

#ifndef REFERENCEPOINTDIALOG_H
#define REFERENCEPOINTDIALOG_H

#include <QDialog>
#include "SpatialEffectTypes.h"

namespace Ui {
class ReferencePointDialog;
}

class VirtualReferencePoint3D;

class ReferencePointDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ReferencePointDialog(QWidget* parent = nullptr);
    ~ReferencePointDialog() override;

    void setCreateMode();
    void setEditMode();
    void loadFrom(const VirtualReferencePoint3D& point);
    void setDefaultColor(unsigned int rgb);

    QString            name() const;
    ReferencePointType type() const;
    unsigned int       displayColorRgb() const;

private slots:
    void onColorButtonClicked();

private:
    Ui::ReferencePointDialog* ui = nullptr;
    unsigned int              color_rgb_ = 0x00808080;
};

#endif
