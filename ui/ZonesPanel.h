// SPDX-License-Identifier: GPL-2.0-only

#ifndef ZONESPANEL_H
#define ZONESPANEL_H

#include <QGroupBox>

class QListWidget;
class QPushButton;

namespace Ui {
class ZonesPanel;
}

class OpenRGB3DSpatialTab;

class ZonesPanel : public QGroupBox
{
    Q_OBJECT

public:
    explicit ZonesPanel(QWidget* parent = nullptr);
    ~ZonesPanel() override;

    void bindTab(OpenRGB3DSpatialTab* tab);

    QListWidget* zonesList() const;
    QPushButton* createZoneButton() const;
    QPushButton* editZoneButton() const;
    QPushButton* deleteZoneButton() const;

private:
    Ui::ZonesPanel* ui = nullptr;
};

#endif
