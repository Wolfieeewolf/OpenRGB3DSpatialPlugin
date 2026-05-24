// SPDX-License-Identifier: GPL-2.0-only

#ifndef SCENEOBJECTEDITHOSTPANEL_H
#define SCENEOBJECTEDITHOSTPANEL_H

#include <QWidget>

namespace Ui {
class SceneObjectEditHostPanel;
}

class OpenRGB3DSpatialTab;
class SceneObjectSpacingPanel;
class SceneTransformPanel;

class SceneObjectEditHostPanel : public QWidget
{
    Q_OBJECT

public:
    explicit SceneObjectEditHostPanel(QWidget* parent = nullptr);
    ~SceneObjectEditHostPanel() override;

    void bindTab(OpenRGB3DSpatialTab* tab);
    void syncFromSceneRow(int scene_list_row);
    int  sceneListRow() const { return scene_list_row_; }

    SceneObjectSpacingPanel* spacingPanel() const;
    SceneTransformPanel*     transformPanel() const;

private:
    Ui::SceneObjectEditHostPanel* ui = nullptr;
    OpenRGB3DSpatialTab*          tab_ = nullptr;
    int                           scene_list_row_ = -1;
};

#endif
