// SPDX-License-Identifier: GPL-2.0-only

#ifndef CONTROLLERLISTPANEL_H
#define CONTROLLERLISTPANEL_H

#include <QGroupBox>

class SpatialControllerCardList;

namespace Ui {
class ControllerListPanel;
}

class OpenRGB3DSpatialTab;

class ControllerListPanel : public QGroupBox
{
    Q_OBJECT

public:
    enum class Mode
    {
        Available,
        InScene
    };

    explicit ControllerListPanel(QWidget* parent = nullptr);
    ~ControllerListPanel() override;

    void bindTab(OpenRGB3DSpatialTab* tab, Mode mode);

    SpatialControllerCardList* cardList() const { return card_list_; }

    bool showUndetectedControllers() const;
    void setShowUndetectedControllers(bool checked);

private:
    Ui::ControllerListPanel*   ui         = nullptr;
    SpatialControllerCardList* card_list_ = nullptr;
};

#endif
