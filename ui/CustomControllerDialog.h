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
#include <functional>
#include <unordered_set>
#include <QVector>
#include <vector>
#include "CustomControllerSourceRef.h"
#include "ResourceManagerInterface.h"
#include "LEDPosition3D.h"
#include "CustomControllerClipboard.h"
#include "CustomControllerTypes.h"
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
    void LoadExistingController(const std::string& name,
                                int width,
                                int height,
                                int depth,
                                const std::vector<GridLEDMapping>& mappings,
                                const std::vector<CustomControllerLightBlocker>& light_blockers = {},
                                const std::vector<float>& column_widths_mm = {},
                                const std::vector<float>& row_heights_mm = {},
                                const std::vector<float>& layer_depths_mm = {},
                                const std::vector<std::string>& layer_names = {},
                                int leds_per_cluster = 1);

    void SetLayoutGridScaleMm(float mm) { layout_grid_scale_mm = mm; }
    float GetLayoutGridScaleMm() const { return layout_grid_scale_mm; }

    std::vector<GridLEDMapping> GetLEDMappings() const { return led_mappings; }
    std::vector<CustomControllerLightBlocker> GetLightBlockers() const;
    QString GetControllerName() const;
    int GetGridWidth() const;
    int GetGridHeight() const;
    int GetGridDepth() const;
    float GetSpacingX() const;
    float GetSpacingY() const;
    float GetSpacingZ() const;
    std::vector<float> GetColumnWidthsMm() const;
    std::vector<float> GetRowHeightsMm() const;
    std::vector<float> GetLayerDepthsMm() const;
    std::vector<std::string> GetLayerNames() const;
    int GetLedsPerCluster() const;

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
    void on_layer_tab_double_clicked(int index);
    void on_add_layer_clicked();
    void on_remove_layer_clicked();
    void on_dimension_changed();
    void on_fit_device_layout_clicked();
    void on_reset_grid_view_clicked();
    void on_identify_selection_clicked();
    void on_clear_cell_clicked();
    void on_copy_selection_clicked();
    void on_cut_selection_clicked();
    void on_paste_selection_clicked();
    void on_grid_context_menu_requested(const QPoint& global_pos);
    void on_grid_column_header_clicked(int column);
    void on_grid_row_header_clicked(int row);
    void on_remove_all_leds_clicked();
    void on_add_light_blocker_clicked();
    void on_save_clicked();
    void on_show_preview_3d_clicked();
    void on_layer_depth_changed(double value_mm);
    void on_grid_column_width_changed(int column, float width_mm);
    void on_grid_row_height_changed(int row, float height_mm);
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
    void RefreshSelectionFillTints();
    void ShowGridContextMenu(const QPoint& global_pos);
    void ShowGridHeaderContextMenu(const QPoint& global_pos, int column_header, int row_header);
    bool PromptGridDimensionMm(const QString& title, const QString& prompt, float current_mm, float* out_mm) const;
    void ApplyColumnWidthMm(int column, float width_mm);
    void ApplyRowHeightMm(int row, float height_mm);
    void ApplyColumnWidthsForColumns(const std::set<int>& columns, float width_mm);
    void ApplyRowHeightsForRows(const std::set<int>& rows, float height_mm);
    void ApplyColumnWidthsForSelection(float width_mm);
    void ApplyRowHeightsForSelection(float height_mm);
    void ApplyAllColumnWidths(float width_mm);
    void ApplyAllRowHeights(float height_mm);
    void PromptAndApplyColumnWidth(int column);
    void PromptAndApplyRowHeight(int row);
    bool BuildClipboardFromSelection(CustomControllerClipboardRegion& out) const;
    bool PasteClipboardRegion(const CustomControllerClipboardRegion& region);
    void ClearSelectedCellContents();
    std::pair<int, int> SelectionPasteAnchor() const;
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
    bool IsLightBlockerCell(int x, int y, int layer) const;
    void TransformLightBlockerCells(const std::function<void(int& x, int& y, int& z)>& transform_fn);
    void TrimLightBlockerCells(int max_width, int max_height, int max_depth);
    void EnsureDialogGridSizeArrays() const;
    void EnsureLayerNamesArray() const;
    QString LayerTabLabel(int layer_index) const;
    void SyncLayerDepthSpinFromCurrentLayer() const;
    QVector<float> ColumnWidthsQVector() const;
    QVector<float> RowHeightsQVector() const;
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
    QComboBox*      leds_per_section_combo = nullptr;

    QDoubleSpinBox* layer_depth_spin = nullptr;

    mutable std::vector<float> column_widths_mm_;
    mutable std::vector<float> row_heights_mm_;
    mutable std::vector<float> layer_depths_mm_;
    mutable std::vector<std::string> layer_names_;

    QTabWidget*     layer_tabs;

    CustomControllerLayoutGrid* layout_grid;
    QLabel*         cell_info_label;
    QLabel*         summary_label;

    QPushButton*    fit_layout_button;
    QPushButton*    reset_view_button_ = nullptr;
    QPushButton*    identify_button;
    QPushButton*    copy_button = nullptr;
    QPushButton*    cut_button = nullptr;
    QPushButton*    paste_button = nullptr;
    QPushButton*    clear_button;
    QPushButton*    remove_from_grid_button;
    QPushButton*    save_button;

    std::unordered_set<uint64_t> matrix_hole_cells;
    std::unordered_set<uint64_t> light_blocker_cells_;

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
    std::set<std::pair<int, int>> selection_fill_cache_;
    CustomControllerClipboardRegion clipboard_region_;
};

#endif
