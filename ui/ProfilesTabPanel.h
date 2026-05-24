// SPDX-License-Identifier: GPL-2.0-only

#ifndef PROFILESTABPANEL_H
#define PROFILESTABPANEL_H

#include <QWidget>

class QCheckBox;
class QComboBox;
class QPushButton;

namespace Ui {
class ProfilesTabPanel;
}

class OpenRGB3DSpatialTab;

class ProfilesTabPanel : public QWidget
{
    Q_OBJECT

public:
    explicit ProfilesTabPanel(QWidget* parent = nullptr);
    ~ProfilesTabPanel() override;

    void connectTab(OpenRGB3DSpatialTab* tab);

    QComboBox*  layoutProfilesCombo() const;
    QPushButton* saveLayoutButton() const;
    QPushButton* saveAsLayoutButton() const;
    QPushButton* loadLayoutButton() const;
    QPushButton* deleteLayoutButton() const;
    QCheckBox*  autoLoadLayoutCheckbox() const;
    QComboBox*  effectProfilesCombo() const;
    QPushButton* saveEffectProfileButton() const;
    QPushButton* loadEffectProfileButton() const;
    QPushButton* deleteEffectProfileButton() const;
    QCheckBox*  autoLoadEffectProfileCheckbox() const;
    QPushButton* openConfigFolderButton() const;

private:
    Ui::ProfilesTabPanel* ui;
};

#endif
