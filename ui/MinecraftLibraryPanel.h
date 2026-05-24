// SPDX-License-Identifier: GPL-2.0-only

#ifndef MINECRAFTLIBRARYPANEL_H
#define MINECRAFTLIBRARYPANEL_H

#include <QGroupBox>

class QComboBox;
class QWidget;

namespace Ui {
class MinecraftLibraryPanel;
}

class OpenRGB3DSpatialTab;

class MinecraftLibraryPanel : public QGroupBox
{
    Q_OBJECT

public:
    explicit MinecraftLibraryPanel(QWidget* parent = nullptr);
    ~MinecraftLibraryPanel() override;

    void bindTab(OpenRGB3DSpatialTab* tab);

    QComboBox* layerCombo() const;
    QWidget*   hubPreviewHolder() const;

private:
    Ui::MinecraftLibraryPanel* ui = nullptr;
};

#endif
