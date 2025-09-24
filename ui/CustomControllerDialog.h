/*---------------------------------------------------------*\
| CustomControllerDialog.h                                  |
|                                                           |
|   Dialog for creating custom 3D LED controllers          |
|                                                           |
|   Date: 2025-09-24                                        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#ifndef CUSTOMCONTROLLERDIALOG_H
#define CUSTOMCONTROLLERDIALOG_H

#include <QDialog>
#include <QListWidget>
#include <QComboBox>
#include <QSpinBox>
#include <QPushButton>
#include <QTableWidget>
#include <QLabel>
#include <QLineEdit>
#include <QTabWidget>
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
};

class CustomControllerDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CustomControllerDialog(ResourceManagerInterface* rm, QWidget *parent = nullptr);
    ~CustomControllerDialog();

    void LoadExistingController(const std::string& name, int width, int height, int depth,
                                const std::vector<GridLEDMapping>& mappings);

    std::vector<GridLEDMapping> GetLEDMappings() const { return led_mappings; }
    QString GetControllerName() const;
    int GetGridWidth() const;
    int GetGridHeight() const;
    int GetGridDepth() const;

private slots:
    void on_controller_selected(int index);
    void on_granularity_changed(int index);
    void on_grid_cell_clicked(int row, int column);
    void on_layer_tab_changed(int index);
    void on_dimension_changed();
    void on_assign_clicked();
    void on_clear_cell_clicked();
    void on_remove_from_grid_clicked();
    void on_save_clicked();

private:
    void SetupUI();
    void UpdateItemCombo();
    void UpdateGridDisplay();
    void UpdateCellInfo();
    void RebuildLayerTabs();
    bool IsItemAssigned(RGBController* controller, int granularity, int item_idx);

    ResourceManagerInterface* resource_manager;

    QLineEdit*      name_edit;
    QListWidget*    available_controllers;
    QComboBox*      granularity_combo;
    QComboBox*      item_combo;

    QSpinBox*       width_spin;
    QSpinBox*       height_spin;
    QSpinBox*       depth_spin;
    QTabWidget*     layer_tabs;

    QTableWidget*   grid_table;
    QLabel*         cell_info_label;

    QPushButton*    assign_button;
    QPushButton*    clear_button;
    QPushButton*    remove_from_grid_button;
    QPushButton*    save_button;

    std::vector<GridLEDMapping> led_mappings;
    int current_layer;
    int selected_row;
    int selected_col;
};

#endif