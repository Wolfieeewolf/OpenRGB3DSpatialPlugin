// SPDX-License-Identifier: GPL-2.0-only

#ifndef SCENEOBJECTSPACINGPANEL_H
#define SCENEOBJECTSPACINGPANEL_H

#include <QWidget>

namespace Ui {
class SceneObjectSpacingPanel;
}

class OpenRGB3DSpatialTab;

class SceneObjectSpacingPanel : public QWidget
{
    Q_OBJECT

public:
    explicit SceneObjectSpacingPanel(QWidget* parent = nullptr);
    ~SceneObjectSpacingPanel() override;

    void bindTab(OpenRGB3DSpatialTab* tab);
    void setTransformIndex(int transform_index);
    void setSpacingMm(float x_mm, float y_mm, float z_mm);
    void setSpacingEnabled(bool enabled);

private:
    void onSpacingChanged();

    Ui::SceneObjectSpacingPanel* ui = nullptr;
    OpenRGB3DSpatialTab*         tab_ = nullptr;
    int                          transform_index_ = -1;
};

#endif
