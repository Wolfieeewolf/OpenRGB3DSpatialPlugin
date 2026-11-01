// SPDX-License-Identifier: GPL-2.0-only

#ifndef SPATIALCONTROLLERCARDLIST_H
#define SPATIALCONTROLLERCARDLIST_H

#include "SpatialControllerEntryKey.h"
#include "SpatialControllerCardWidget.h"
#include <QWidget>
#include <vector>

class QScrollArea;
class QVBoxLayout;
class OpenRGB3DSpatialTab;

namespace Ui {
class SpatialControllerCardList;
}

class SpatialControllerCardList : public QWidget
{
    Q_OBJECT

public:
    explicit SpatialControllerCardList(SpatialControllerCardWidget::Mode mode, QWidget* parent = nullptr);
    ~SpatialControllerCardList() override;

    void rebuildAvailable(OpenRGB3DSpatialTab* host);
    void rebuildInScene(OpenRGB3DSpatialTab* host);

    void refreshAvailableFromHost();
    void refreshInSceneSpacingFromHost();
    void setSelectedSceneRow(int scene_list_row, bool scroll_into_view = true);
    void setSelectedAvailableKey(const SpatialControllerEntryKey& key);

    SpatialControllerEntryKey selectedAvailableKey() const { return selected_available_key_; }

signals:
    void availableSelectionChanged(const SpatialControllerEntryKey& key);
    void sceneSelectionChanged(int scene_list_row);

private:
    void clearWidgets();
    void applyAvailableSelection();
    void applySceneSelection();
    SpatialControllerCardWidget* widgetForAvailableKey(const SpatialControllerEntryKey& key) const;
    SpatialControllerCardWidget* widgetForSceneRow(int scene_list_row) const;

    Ui::SpatialControllerCardList*             ui = nullptr;
    SpatialControllerCardWidget::Mode          mode_;
    QScrollArea*                               scroll_area_;
    QWidget*                                   content_widget_;
    QVBoxLayout*                               content_layout_;
    std::vector<SpatialControllerCardWidget*>  cards_;
    SpatialControllerEntryKey                  selected_available_key_;
    int                                        selected_scene_row_;
};

#endif
