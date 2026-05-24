// SPDX-License-Identifier: GPL-2.0-only

#ifndef CUSTOMCONTROLLERDIALOG_H
#define CUSTOMCONTROLLERDIALOG_H

#include <QColor>
#include <QCloseEvent>
#include <QDialog>
#include <QHideEvent>
#include <QShowEvent>
#include <QStyledItemDelegate>
#include <map>
#include <set>
#include <unordered_set>
#include <QVector>
#include <vector>
#include "CustomControllerSourceRef.h"
#include "ResourceManagerInterface.h"
#include "LEDPosition3D.h"
#include "custom-controller-grid/CustomControllerGridCell.h"

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class CustomControllerDeviceList;
class QModelIndex;
class QPainter;
class QPushButton;
class QSize;
class QSpinBox;
class QStyleOptionViewItem;
class QTabWidget;
class QTimer;
class CustomControllerLayoutGrid;
class CustomControllerPreviewDialog;

namespace Ui {
class CustomControllerDialog;
}

struct GridLEDMapping
{
    int x;
    int y;
    int z;
    RGBController* controller;
    unsigned int zone_idx;
    unsigned int led_idx;
    int granularity;
};

class ColorComboDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit ColorComboDelegate(QObject *parent = nullptr);

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override;
};

class CustomControllerDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CustomControllerDialog(ResourceManagerInterface* rm, QWidget *parent = nullptr);
    ~CustomControllerDialog() override;

protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

public:
    void LoadExistingController(const std::string& name, int width, int height, int depth,
                                const std::vector<GridLEDMapping>& mappings,
                                float spacing_x_mm = 10.0f,
                                float spacing_y_mm = 10.0f,
                                float spacing_z_mm = 10.0f);

    void SetLayoutGridScaleMm(float mm) { layout_grid_scale_mm = mm; }
    float GetLayoutGridScaleMm() const { return layout_grid_scale_mm; }

    std::vector<GridLEDMapping> GetLEDMappings() const { return led_mappings; }
    QString GetControllerName() const;
    int GetGridWidth() const;
    int GetGridHeight() const;
    int GetGridDepth() const;
    float GetSpacingX() const;
    float GetSpacingY() const;
    float GetSpacingZ() const;

    bool IsSourceItemAvailable(const CustomControllerSourceRef& source) const;
    bool CanAddSourceToGrid(const CustomControllerSourceRef& source) const;
    bool IsSourceItemOnGrid(const CustomControllerSourceRef& source) const;
    QColor SourceItemColor(const CustomControllerSourceRef& source) const;
    bool selectedGridCellValid() const;

    void PopulateDeviceItemCombo(int controller_index, int granularity, QComboBox* combo) const;

private slots:
    void on_source_selection_changed(const CustomControllerSourceRef& source);
    void on_source_enable_toggled(const CustomControllerSourceRef& source, bool enabled);
    void on_grid_cell_clicked(int column, int row);
    void on_grid_cell_double_clicked(int column, int row);
    void on_grid_selection_changed();
    void on_layer_tab_changed(int index);
    void on_add_layer_clicked();
    void on_remove_layer_clicked();
    void on_dimension_changed();
    void on_fit_device_layout_clicked();
    void on_reset_grid_view_clicked();
    void on_identify_selection_clicked();
    void on_clear_cell_clicked();
    void on_remove_all_leds_clicked();
    void on_save_clicked();
    void on_show_preview_3d_clicked();
    void on_layout_params_changed();
    void refresh_colors();
    void syncPreviewLayoutIfVisible();

    void on_rotate_grid_90();
    void on_rotate_grid_180();
    void on_rotate_grid_270();
    void on_flip_grid_horizontal();
    void on_flip_grid_vertical();

private:
    void SetupUI();
    void RebuildMatrixHoleMask(RGBController* controller, int anchor_x, int anchor_y);
    void RefreshLayoutGridVisuals();
    void PopulateCellVisual(int col,
                            int row,
                            const std::vector<GridLEDMapping>& cell_mappings,
                            CustomControllerGridCellVisual& visual,
                            bool is_selected) const;
    QColor ComputeCellBaseColor(const std::vector<GridLEDMapping>& cell_mappings, bool is_hole) const;
    void ApplyCellFillAndText(CustomControllerGridCellVisual& visual,
                              const QColor& base_color,
                              const std::vector<GridLEDMapping>& cell_mappings,
                              bool is_selected) const;
    bool LayoutCellsCacheMatchesGrid() const;
    std::set<std::pair<int, int>> SelectedGridCells() const;
    bool IsMatrixHoleCell(int x, int y) const;
    bool PlaceProfileLayout(RGBController* controller, int granularity, int item_idx, int start_x, int start_y);
    void UpdateGridDisplay();
    void refreshDeviceList(int controller_index = -1);
    bool assignSource(const CustomControllerSourceRef& source);
    void removeSourceFromGrid(const CustomControllerSourceRef& source);
    CustomControllerSourceRef currentSourceSelection() const;
    RGBController* controllerForSource(const CustomControllerSourceRef& source) const;
    void UpdateSummaryLabel();
    void UpdateGridColors();
    void UpdateCellInfo();
    void UpdateIdentifyButtonUi();
    void RestoreAllIdentifiedLeds();
    void SetIdentifyForCells(const std::set<std::pair<int, int>>& cells, bool enabled);
    void RestoreIdentifyForMappings(const std::vector<GridLEDMapping>& mappings);
    void RebuildLayerTabs();
    void AttachLayoutGridToLayerTab(int layer_index);
    void UpdateLayerTabControls();
    bool IsItemAssigned(RGBController* controller, int granularity, int item_idx) const;
    QColor GetItemColor(RGBController* controller, int granularity, int item_idx) const;
    QColor GetAverageZoneColor(RGBController* controller, unsigned int zone_idx) const;
    QColor GetAverageDeviceColor(RGBController* controller) const;
    QColor GetMappingColor(const GridLEDMapping& mapping) const;
    QString GetMappingDescription(const GridLEDMapping& mapping) const;
    QString GetMappingTooltip(const GridLEDMapping& mapping) const;
    QString GetMappingCellLabel(const std::vector<GridLEDMapping>& cell_mappings) const;
    static QColor RGBToQColor(unsigned int rgb_value);
    void InferMappingGranularity();
    void WarnIfMappingCollisions() const;

    ResourceManagerInterface* resource_manager;
    Ui::CustomControllerDialog* ui = nullptr;

    QLineEdit*      name_edit;
    CustomControllerDeviceList* device_list;
    QSpinBox*       width_spin;
    QSpinBox*       height_spin;
    QSpinBox*       depth_spin;

    QDoubleSpinBox* spacing_x_spin;
    QDoubleSpinBox* spacing_y_spin;
    QDoubleSpinBox* spacing_z_spin;

    QTabWidget*     layer_tabs;

    CustomControllerLayoutGrid* layout_grid;
    QLabel*         cell_info_label;
    QLabel*         summary_label;

    QPushButton*    fit_layout_button;
    QPushButton*    reset_view_button_ = nullptr;
    QPushButton*    identify_button;
    QPushButton*    clear_button;
    QPushButton*    remove_from_grid_button;
    QPushButton*    save_button;

    std::unordered_set<uint64_t> matrix_hole_cells;

    QGroupBox*      transform_group = nullptr;
    QPushButton*    rotate_90_button;
    QPushButton*    rotate_180_button;
    QPushButton*    rotate_270_button;
    QPushButton*    flip_horizontal_button;
    QPushButton*    flip_vertical_button;

    QTimer*         color_refresh_timer;
    CustomControllerPreviewDialog* preview_dialog;

    std::vector<GridLEDMapping> led_mappings;
    std::map<std::pair<RGBController*, unsigned int>, RGBColor> identified_leds;
    float layout_grid_scale_mm;
    int current_layer;
    int selected_row;
    int selected_col;
    bool shutting_down_ = false;

    QVector<CustomControllerGridCellVisual> layout_cells_cache_;
    int layout_cells_cache_w_     = 0;
    int layout_cells_cache_h_     = 0;
    int layout_cells_cache_layer_ = -1;
};

#endif
