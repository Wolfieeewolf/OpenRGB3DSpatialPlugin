// SPDX-License-Identifier: GPL-2.0-only

#ifndef CUSTOMCONTROLLERDIALOG_H
#define CUSTOMCONTROLLERDIALOG_H

#include <QDialog>
#include <QListWidget>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QTableWidget>
#include <QLabel>
#include <QLineEdit>
#include <QTabWidget>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QTimer>
#include <QSlider>
#include <QCheckBox>
#include <vector>
#include "ResourceManagerInterface.h"
#include "LEDPosition3D.h"

struct GridLEDMapping
{
    int x;
    int y;
    int z;
    RGBController* controller;
    unsigned int zone_idx;
    unsigned int led_idx;
    int granularity; // 0=whole device, 1=zone, 2=LED
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
    ~CustomControllerDialog() override = default;

    void LoadExistingController(const std::string& name, int width, int height, int depth,
                                const std::vector<GridLEDMapping>& mappings,
                                float spacing_x_mm = 10.0f,
                                float spacing_y_mm = 10.0f,
                                float spacing_z_mm = 10.0f);

    std::vector<GridLEDMapping> GetLEDMappings() const { return led_mappings; }
    QString GetControllerName() const;
    int GetGridWidth() const;
    int GetGridHeight() const;
    int GetGridDepth() const;
    float GetSpacingX() const;
    float GetSpacingY() const;
    float GetSpacingZ() const;

signals:
    /** Emitted when user clicks "Preview in 3D View"; caller should add current dialog state to viewport as a temporary transform. */
    void previewRequested();

private slots:
    void on_controller_selected(int index);
    void on_granularity_changed(int index);
    void on_allow_reuse_toggled(bool checked);
    void on_grid_cell_clicked(int row, int column);
    void on_grid_cell_double_clicked(int row, int column);
    void on_grid_current_cell_changed(int current_row, int current_col, int, int);
    void on_layer_tab_changed(int index);
    void on_dimension_changed();
    void on_assign_clicked();
    void on_clear_cell_clicked();
    void on_remove_all_leds_clicked();
    void on_save_clicked();
    void refresh_colors();

    // LED Layout Transform slots
    void on_lock_transform_toggled(bool locked);
    void on_rotation_angle_changed(int angle);
    void on_rotate_grid_90();
    void on_rotate_grid_180();
    void on_rotate_grid_270();
    void on_flip_grid_horizontal();
    void on_flip_grid_vertical();
    void on_apply_preview_remap_clicked();

private:
    void SetupUI();
    void ApplyGridTableHeaderStyle();
    void UpdateItemCombo();
    void UpdateGridDisplay();
    void UpdateGridColors();
    void UpdateCellInfo();
    void RebuildLayerTabs();
    bool IsItemAssigned(RGBController* controller, int granularity, int item_idx);
    QColor GetItemColor(RGBController* controller, int granularity, int item_idx);
    QColor GetAverageZoneColor(RGBController* controller, unsigned int zone_idx);
    QColor GetAverageDeviceColor(RGBController* controller);
    QColor GetMappingColor(const GridLEDMapping& mapping);
    static QColor RGBToQColor(unsigned int rgb_value);
    void InferMappingGranularity();

    ResourceManagerInterface* resource_manager;

    QLineEdit*      name_edit;
    QListWidget*    available_controllers;
    QComboBox*      granularity_combo;
    QCheckBox*      allow_reuse_checkbox;
    QComboBox*      item_combo;

    QSpinBox*       width_spin;
    QSpinBox*       height_spin;
    QSpinBox*       depth_spin;

    QDoubleSpinBox* spacing_x_spin;
    QDoubleSpinBox* spacing_y_spin;
    QDoubleSpinBox* spacing_z_spin;

    QTabWidget*     layer_tabs;

    QTableWidget*   grid_table;
    QLabel*         cell_info_label;

    QPushButton*    assign_button;
    QPushButton*    clear_button;
    QPushButton*    remove_from_grid_button;
    QPushButton*    save_button;

    // LED Layout Transform controls
    QCheckBox*      lock_transform_checkbox;
    QSlider*        rotate_angle_slider;
    QSpinBox*       rotate_angle_spin;
    QPushButton*    rotate_90_button;
    QPushButton*    rotate_180_button;
    QPushButton*    rotate_270_button;
    QPushButton*    flip_horizontal_button;
    QPushButton*    flip_vertical_button;
    QPushButton*    apply_preview_button;

    QTimer*         color_refresh_timer;

    std::vector<GridLEDMapping> led_mappings;
    std::vector<GridLEDMapping> original_led_mappings;  // Locked original positions
    std::vector<GridLEDMapping> preview_led_mappings;    // Preview-only mappings when locked
    bool transform_locked;  // Whether we've locked the original layout
    int current_layer;
    int selected_row;
    int selected_col;
};

#endif
