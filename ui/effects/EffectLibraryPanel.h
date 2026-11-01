// SPDX-License-Identifier: GPL-2.0-only

#ifndef EFFECTLIBRARYPANEL_H
#define EFFECTLIBRARYPANEL_H

#include <QGroupBox>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;

namespace Ui {
class EffectLibraryPanel;
}

class OpenRGB3DSpatialTab;

class EffectLibraryPanel : public QGroupBox
{
    Q_OBJECT

public:
    explicit EffectLibraryPanel(QWidget* parent = nullptr);
    ~EffectLibraryPanel() override;

    void bindTab(OpenRGB3DSpatialTab* tab);

    QComboBox*   categoryCombo() const;
    QLabel*      gameLabel() const;
    QComboBox*   gameCombo() const;
    QLineEdit*   searchEdit() const;
    QListWidget* libraryList() const;
    QCheckBox*   unlockVolumeOnAddCheck() const;
    QPushButton* addToStackButton() const;

private:
    Ui::EffectLibraryPanel* ui = nullptr;
    bool bound_ = false;
};

#endif
