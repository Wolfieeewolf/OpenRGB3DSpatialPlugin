// SPDX-License-Identifier: GPL-2.0-only

#ifndef SPATIALCONTROLLERCARDWIDGET_H
#define SPATIALCONTROLLERCARDWIDGET_H

#include "SpatialControllerEntryKey.h"
#include <QString>
#include <QWidget>

class QResizeEvent;
class QComboBox;
class QDoubleSpinBox;
class QFrame;
class PluginClickableLabel;
class QToolButton;

namespace Ui {
class SpatialControllerCardWidget;
}

class OpenRGB3DSpatialTab;

class SpatialControllerCardWidget : public QWidget
{
    Q_OBJECT

public:
    enum class Mode
    {
        Available,
        InScene,
    };

    explicit SpatialControllerCardWidget(Mode mode,
                                         const QString& title,
                                         const SpatialControllerEntryKey& key,
                                         bool show_granularity_controls,
                                         bool show_spacing_controls,
                                         float spacing_x_mm,
                                         float spacing_y_mm,
                                         float spacing_z_mm,
                                         OpenRGB3DSpatialTab* host,
                                         int scene_list_row,
                                         int transform_index,
                                         QWidget* parent = nullptr);
    ~SpatialControllerCardWidget() override;

    Mode                       mode() const { return mode_; }
    SpatialControllerEntryKey  key() const { return key_; }
    int                        sceneListRow() const { return scene_list_row_; }
    int                        transformIndex() const { return transform_index_; }

    void spacingMm(float* x_mm, float* y_mm, float* z_mm) const;
    void setSpacingMm(float x_mm, float y_mm, float z_mm);

    void setRowSelected(bool selected);
    void refreshFromHost();

protected:
    void resizeEvent(QResizeEvent* event) override;

signals:
    void cardActivated(const SpatialControllerEntryKey& key);
    void addRequested(const SpatialControllerEntryKey& key,
                      int                            granularity,
                      int                            item_index,
                      float                          spacing_x_mm,
                      float                          spacing_y_mm,
                      float                          spacing_z_mm);
    void removeRequested(int scene_list_row);
    void editRequested(int scene_list_row);

private slots:
    void nameClicked();
    void granularityChanged(int index);
    void itemChanged(int index);
    void actionClicked();
    void editClicked();
    void spacingChanged(double value);

private:
    void applyCompactComboStyle(QComboBox* combo);
    void applyGranularityComboStyle(QComboBox* combo);
    void applyItemComboStyle(QComboBox* combo);
    void applySpacingSpinStyle(QDoubleSpinBox* spin);
    void applyOptionalRowVisibility();
    void rebuildItemCombo();
    void updateActionIcon();
    void updateActionEnabled();
    void notifySpacingEdited();
    void updateNameLabelLayout();
    void applyColumnStretches();
    int  nameLabelContentWidth() const;

    static constexpr int kLastContentColumn = 3;
    static constexpr int kActionColumn      = 4;

    Ui::SpatialControllerCardWidget* ui = nullptr;

    Mode                      mode_;
    QString                   title_text_;
    SpatialControllerEntryKey key_;
    bool                      show_granularity_controls_;
    bool                      show_spacing_controls_;
    OpenRGB3DSpatialTab*      host_;
    int                       scene_list_row_;
    int                       transform_index_;
    bool                      row_selected_;
};

#endif
