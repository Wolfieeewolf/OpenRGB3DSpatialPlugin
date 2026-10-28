// SPDX-License-Identifier: GPL-2.0-only

#ifndef EFFECTSTACKPANEL_H
#define EFFECTSTACKPANEL_H

#include <QGroupBox>

class QListWidget;
class QPushButton;

namespace Ui {
class EffectStackPanel;
}

class OpenRGB3DSpatialTab;

class EffectStackPanel : public QGroupBox
{
    Q_OBJECT

public:
    explicit EffectStackPanel(QWidget* parent = nullptr);
    ~EffectStackPanel() override;

    void bindTab(OpenRGB3DSpatialTab* tab);

    QListWidget* stackList() const;
    QListWidget* presetsList() const;
    QPushButton* startAllButton() const;
    QPushButton* stopAllButton() const;

private:
    Ui::EffectStackPanel* ui = nullptr;
};

#endif
