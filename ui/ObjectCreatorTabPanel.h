// SPDX-License-Identifier: GPL-2.0-only

#ifndef OBJECTCREATORTABPANEL_H
#define OBJECTCREATORTABPANEL_H

#include <QWidget>

class QLabel;
class QListWidget;
class QPushButton;

namespace Ui {
class ObjectCreatorTabPanel;
}

class OpenRGB3DSpatialTab;

class ObjectCreatorTabPanel : public QWidget
{
    Q_OBJECT

public:
    explicit ObjectCreatorTabPanel(QWidget* parent = nullptr);
    ~ObjectCreatorTabPanel() override;

    void bindTab(OpenRGB3DSpatialTab* tab);

    void showCustomControllerSection();
    void showReferencePointSection();
    void showDisplayPlaneSection();

    QLabel*         statusLabel() const;
    QListWidget*    customControllersList() const;
    QWidget*        customControllersEmptyLabel() const;
    QPushButton*    exportCustomControllerButton() const;
    QPushButton*    editCustomControllerButton() const;
    QPushButton*    deleteCustomControllerButton() const;
    QListWidget*    referencePointsList() const;
    QWidget*        refPointsEmptyLabel() const;
    QPushButton*    createReferencePointButton() const;
    QPushButton*    editReferencePointButton() const;
    QPushButton*    removeRefPointButton() const;
    QListWidget*    displayPlanesList() const;
    QWidget*        displayPlanesEmptyLabel() const;
    QPushButton*    createDisplayPlaneButton() const;
    QPushButton*    editDisplayPlaneButton() const;
    QPushButton*    removeDisplayPlaneButton() const;

private slots:
    void onObjectTypeChanged(int index);
    void onReferencePointListRowChanged(int list_row);

private:
    void applyVisualStyles();

    Ui::ObjectCreatorTabPanel* ui;
    OpenRGB3DSpatialTab*       host_tab_ = nullptr;
};

#endif
