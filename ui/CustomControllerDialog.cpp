/*---------------------------------------------------------*\
| CustomControllerDialog.cpp                                |
|                                                           |
|   Dialog for creating custom 3D LED controllers          |
|                                                           |
|   Date: 2025-09-24                                        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#include "CustomControllerDialog.h"
#include "ControllerLayout3D.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QMessageBox>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QIcon>
#include <QPixmap>

CustomControllerDialog::CustomControllerDialog(ResourceManagerInterface* rm, QWidget *parent)
    : QDialog(parent),
      resource_manager(rm),
      current_layer(0),
      selected_row(-1),
      selected_col(-1)
{
    setWindowTitle("Create Custom 3D Controller");
    resize(1000, 600);
    SetupUI();
}

CustomControllerDialog::~CustomControllerDialog()
{
}

void CustomControllerDialog::SetupUI()
{
    QVBoxLayout* main_layout = new QVBoxLayout(this);

    QHBoxLayout* name_layout = new QHBoxLayout();
    name_layout->addWidget(new QLabel("Controller Name:"));
    name_edit = new QLineEdit();
    name_edit->setPlaceholderText("Enter custom controller name");
    name_layout->addWidget(name_edit);
    main_layout->addLayout(name_layout);

    QHBoxLayout* content_layout = new QHBoxLayout();

    QGroupBox* left_group = new QGroupBox("Available Controllers");
    QVBoxLayout* left_layout = new QVBoxLayout();

    available_controllers = new QListWidget();
    connect(available_controllers, &QListWidget::currentRowChanged, this, &CustomControllerDialog::on_controller_selected);
    left_layout->addWidget(available_controllers);

    QHBoxLayout* granularity_layout = new QHBoxLayout();
    granularity_layout->addWidget(new QLabel("Select:"));
    granularity_combo = new QComboBox();
    granularity_combo->addItem("Whole Device");
    granularity_combo->addItem("Zone");
    granularity_combo->addItem("LED");
    connect(granularity_combo, SIGNAL(currentIndexChanged(int)), this, SLOT(on_granularity_changed(int)));
    granularity_layout->addWidget(granularity_combo);
    left_layout->addLayout(granularity_layout);

    item_combo = new QComboBox();
    item_combo->setItemDelegate(new ColorComboDelegate(this));
    left_layout->addWidget(item_combo);

    assign_button = new QPushButton("Assign to Selected Cell");
    connect(assign_button, &QPushButton::clicked, this, &CustomControllerDialog::on_assign_clicked);
    left_layout->addWidget(assign_button);

    clear_button = new QPushButton("Clear Selected Cell");
    connect(clear_button, &QPushButton::clicked, this, &CustomControllerDialog::on_clear_cell_clicked);
    left_layout->addWidget(clear_button);

    remove_from_grid_button = new QPushButton("Remove All LEDs from Grid");
    connect(remove_from_grid_button, &QPushButton::clicked, this, &CustomControllerDialog::on_remove_all_leds_clicked);
    left_layout->addWidget(remove_from_grid_button);

    left_group->setLayout(left_layout);
    content_layout->addWidget(left_group, 1);

    QVBoxLayout* right_layout = new QVBoxLayout();

    QGroupBox* dim_group = new QGroupBox("Grid Dimensions");
    QHBoxLayout* dim_layout = new QHBoxLayout();
    dim_layout->addWidget(new QLabel("Width:"));
    width_spin = new QSpinBox();
    width_spin->setRange(1, 50);
    width_spin->setValue(10);
    connect(width_spin, SIGNAL(valueChanged(int)), this, SLOT(on_dimension_changed()));
    dim_layout->addWidget(width_spin);
    dim_layout->addWidget(new QLabel("Height:"));
    height_spin = new QSpinBox();
    height_spin->setRange(1, 50);
    height_spin->setValue(10);
    connect(height_spin, SIGNAL(valueChanged(int)), this, SLOT(on_dimension_changed()));
    dim_layout->addWidget(height_spin);
    dim_layout->addWidget(new QLabel("Depth (layers):"));
    depth_spin = new QSpinBox();
    depth_spin->setRange(1, 20);
    depth_spin->setValue(1);
    connect(depth_spin, SIGNAL(valueChanged(int)), this, SLOT(on_dimension_changed()));
    dim_layout->addWidget(depth_spin);
    dim_group->setLayout(dim_layout);
    right_layout->addWidget(dim_group);

    layer_tabs = new QTabWidget();
    connect(layer_tabs, SIGNAL(currentChanged(int)), this, SLOT(on_layer_tab_changed(int)));

    QWidget* first_tab = new QWidget();
    QVBoxLayout* first_tab_layout = new QVBoxLayout(first_tab);
    first_tab_layout->setContentsMargins(0, 0, 0, 0);
    grid_table = new QTableWidget();
    grid_table->setSelectionMode(QAbstractItemView::SingleSelection);
    grid_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    connect(grid_table, &QTableWidget::cellClicked, this, &CustomControllerDialog::on_grid_cell_clicked);

    grid_table->horizontalHeader()->setDefaultSectionSize(30);
    grid_table->verticalHeader()->setDefaultSectionSize(30);
    grid_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    grid_table->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    grid_table->setShowGrid(true);
    first_tab_layout->addWidget(grid_table);
    layer_tabs->addTab(first_tab, "Layer 0");
    right_layout->addWidget(layer_tabs);

    cell_info_label = new QLabel("Click a cell to select it");
    right_layout->addWidget(cell_info_label);

    content_layout->addLayout(right_layout, 2);

    main_layout->addLayout(content_layout);

    QHBoxLayout* button_layout = new QHBoxLayout();
    button_layout->addStretch();
    save_button = new QPushButton("Save Custom Controller");
    connect(save_button, &QPushButton::clicked, this, &CustomControllerDialog::on_save_clicked);
    button_layout->addWidget(save_button);
    QPushButton* cancel_button = new QPushButton("Cancel");
    connect(cancel_button, &QPushButton::clicked, this, &QDialog::reject);
    button_layout->addWidget(cancel_button);
    main_layout->addLayout(button_layout);

    std::vector<RGBController*>& controllers = resource_manager->GetRGBControllers();
    for(unsigned int i = 0; i < controllers.size(); i++)
    {
        available_controllers->addItem(QString::fromStdString(controllers[i]->name));
    }

    color_refresh_timer = new QTimer(this);
    connect(color_refresh_timer, &QTimer::timeout, this, &CustomControllerDialog::refresh_colors);
    color_refresh_timer->start(750);

    UpdateGridDisplay();
}

void CustomControllerDialog::on_controller_selected(int)
{
    UpdateItemCombo();
}

void CustomControllerDialog::on_granularity_changed(int)
{
    UpdateItemCombo();
}

void CustomControllerDialog::UpdateItemCombo()
{
    item_combo->clear();

    int ctrl_idx = available_controllers->currentRow();
    if(ctrl_idx < 0) return;

    std::vector<RGBController*>& controllers = resource_manager->GetRGBControllers();
    if(ctrl_idx >= (int)controllers.size()) return;

    RGBController* controller = controllers[ctrl_idx];
    if(!controller) return;
    int granularity = granularity_combo->currentIndex();

    if(granularity == 0)
    {
        if(!IsItemAssigned(controller, granularity, 0))
        {
            QColor color = GetItemColor(controller, granularity, 0);
            QPixmap pixmap(16, 16);
            pixmap.fill(color);
            QIcon icon(pixmap);
            item_combo->addItem(icon, "Whole Device", 0);
        }
    }
    else if(granularity == 1)
    {
        for(unsigned int i = 0; i < controller->zones.size(); i++)
        {
            if(!IsItemAssigned(controller, granularity, i))
            {
                QColor color = GetItemColor(controller, granularity, i);
                QPixmap pixmap(16, 16);
                pixmap.fill(color);
                QIcon icon(pixmap);
                item_combo->addItem(icon, QString::fromStdString(controller->zones[i].name), i);
            }
        }
    }
    else if(granularity == 2)
    {
        for(unsigned int i = 0; i < controller->leds.size(); i++)
        {
            if(!IsItemAssigned(controller, granularity, i))
            {
                QColor color = GetItemColor(controller, granularity, i);
                QPixmap pixmap(16, 16);
                pixmap.fill(color);
                QIcon icon(pixmap);
                item_combo->addItem(icon, QString::fromStdString(controller->leds[i].name), i);
            }
        }
    }
}

void CustomControllerDialog::on_grid_cell_clicked(int row, int column)
{
    selected_row = row;
    selected_col = column;
    UpdateCellInfo();
    UpdateGridColors();
}

void CustomControllerDialog::on_layer_tab_changed(int index)
{
    current_layer = index;

    if(layer_tabs->count() > 0)
    {
        grid_table->setParent(nullptr);
        QWidget* current_tab = layer_tabs->widget(index);
        if(current_tab)
        {
            QVBoxLayout* layout = qobject_cast<QVBoxLayout*>(current_tab->layout());
            if(layout)
            {
                layout->addWidget(grid_table);
            }
        }
    }

    UpdateGridDisplay();
}

void CustomControllerDialog::on_dimension_changed()
{
    if(current_layer >= depth_spin->value())
    {
        current_layer = depth_spin->value() - 1;
        layer_tabs->setCurrentIndex(current_layer);
    }
    RebuildLayerTabs();
    UpdateGridDisplay();
}

void CustomControllerDialog::RebuildLayerTabs()
{
    int old_layer = current_layer;
    int current_tab_count = layer_tabs->count();
    int new_depth = depth_spin->value();

    if(new_depth > current_tab_count)
    {
        for(int i = current_tab_count; i < new_depth; i++)
        {
            QWidget* tab = new QWidget();
            QVBoxLayout* tab_layout = new QVBoxLayout(tab);
            tab_layout->setContentsMargins(0, 0, 0, 0);
            layer_tabs->addTab(tab, QString("Layer %1").arg(i));
        }
    }
    else if(new_depth < current_tab_count)
    {
        while(layer_tabs->count() > new_depth)
        {
            int last_idx = layer_tabs->count() - 1;
            QWidget* tab = layer_tabs->widget(last_idx);
            layer_tabs->removeTab(last_idx);
            delete tab;
        }
    }

    int target_layer = (old_layer < new_depth) ? old_layer : (new_depth - 1);
    layer_tabs->setCurrentIndex(target_layer);

    grid_table->setParent(nullptr);
    if(layer_tabs->count() > 0)
    {
        QWidget* current_tab = layer_tabs->widget(target_layer);
        if(current_tab)
        {
            QVBoxLayout* layout = qobject_cast<QVBoxLayout*>(current_tab->layout());
            if(layout)
            {
                layout->addWidget(grid_table);
            }
        }
    }
}

void CustomControllerDialog::UpdateGridDisplay()
{
    grid_table->clear();
    grid_table->setRowCount(height_spin->value());
    grid_table->setColumnCount(width_spin->value());

    for(int i = 0; i < height_spin->value(); i++)
    {
        for(int j = 0; j < width_spin->value(); j++)
        {
            QTableWidgetItem* item = new QTableWidgetItem();

            std::vector<GridLEDMapping> cell_mappings;
            for(unsigned int k = 0; k < led_mappings.size(); k++)
            {
                if(led_mappings[k].x == j && led_mappings[k].y == i && led_mappings[k].z == current_layer)
                {
                    cell_mappings.push_back(led_mappings[k]);
                }
            }

            QColor cell_color(50, 50, 50);

            if(!cell_mappings.empty())
            {
                if(cell_mappings.size() == 1)
                {
                    cell_color = GetMappingColor(cell_mappings[0]);
                }
                else
                {
                    unsigned int total_r = 0, total_g = 0, total_b = 0;
                    unsigned int valid_count = 0;

                    for(unsigned int m = 0; m < cell_mappings.size(); m++)
                    {
                        QColor ledColor = GetMappingColor(cell_mappings[m]);
                        total_r += ledColor.red();
                        total_g += ledColor.green();
                        total_b += ledColor.blue();
                        valid_count++;
                    }

                    if(valid_count > 0)
                    {
                        cell_color = QColor(total_r / valid_count, total_g / valid_count, total_b / valid_count);
                    }
                }

                // Create tooltip with all mapped LEDs
                QString tooltip_text;
                if(cell_mappings.size() == 1)
                {
                    const GridLEDMapping& mapping = cell_mappings[0];

                    if(!mapping.controller)
                    {
                        tooltip_text = "Invalid assignment";
                    }
                    else if(mapping.granularity == 0)
                    {
                        // Whole device assignment
                        tooltip_text = QString("Assigned: %1 (Whole Device)")
                                       .arg(QString::fromStdString(mapping.controller->name));
                    }
                    else if(mapping.granularity == 1)
                    {
                        // Zone assignment
                        QString zone_name = "Unknown Zone";
                        if(mapping.zone_idx < mapping.controller->zones.size())
                        {
                            zone_name = QString::fromStdString(mapping.controller->zones[mapping.zone_idx].name);
                        }
                        tooltip_text = QString("Assigned: %1\nZone: %2")
                                       .arg(QString::fromStdString(mapping.controller->name))
                                       .arg(zone_name);
                    }
                    else if(mapping.granularity == 2)
                    {
                        // LED assignment
                        QString led_name = "Unknown LED";
                        unsigned int global_led_idx = 0;
                        if(mapping.zone_idx < mapping.controller->zones.size())
                        {
                            global_led_idx = mapping.controller->zones[mapping.zone_idx].start_idx + mapping.led_idx;
                            if(global_led_idx < mapping.controller->leds.size())
                            {
                                led_name = QString::fromStdString(mapping.controller->leds[global_led_idx].name);
                            }
                        }
                        tooltip_text = QString("Assigned: %1\nLED: %2")
                                       .arg(QString::fromStdString(mapping.controller->name))
                                       .arg(led_name);
                    }
                    else
                    {
                        tooltip_text = QString("Assigned: %1 (Unknown type)")
                                       .arg(QString::fromStdString(mapping.controller->name));
                    }
                }
                else
                {
                    tooltip_text = QString("Multiple LEDs (%1):\n").arg(cell_mappings.size());
                    for(size_t i = 0; i < cell_mappings.size() && i < 5; i++) // Limit to 5 for readability
                    {
                        const GridLEDMapping& mapping = cell_mappings[i];
                        if(!mapping.controller) continue;

                        QString assignment_type;
                        if(mapping.granularity == 0)
                        {
                            assignment_type = " (Whole Device)";
                        }
                        else if(mapping.granularity == 1 && mapping.zone_idx < mapping.controller->zones.size())
                        {
                            assignment_type = QString(" [Zone: %1]").arg(QString::fromStdString(mapping.controller->zones[mapping.zone_idx].name));
                        }
                        else if(mapping.granularity == 2)
                        {
                            unsigned int global_led_idx = 0;
                            if(mapping.zone_idx < mapping.controller->zones.size())
                            {
                                global_led_idx = mapping.controller->zones[mapping.zone_idx].start_idx + mapping.led_idx;
                                if(global_led_idx < mapping.controller->leds.size())
                                {
                                    assignment_type = QString(" [LED: %1]").arg(QString::fromStdString(mapping.controller->leds[global_led_idx].name));
                                }
                            }
                        }

                        tooltip_text += QString("• %1%2\n")
                                       .arg(QString::fromStdString(mapping.controller->name))
                                       .arg(assignment_type);
                    }
                    if(cell_mappings.size() > 5)
                    {
                        tooltip_text += QString("... and %1 more").arg(cell_mappings.size() - 5);
                    }
                }


                item->setToolTip(tooltip_text);

                // Add indicator based on number of LEDs
                if(cell_mappings.size() == 1)
                {
                    item->setText("●");
                }
                else
                {
                    item->setText(QString::number(cell_mappings.size()));
                }
                item->setTextAlignment(Qt::AlignCenter);

            }

            else
            {
                item->setText("");


                item->setToolTip("Empty - click to assign");
            }

            // Check if this cell is currently selected
            bool isSelected = (i == selected_row && j == selected_col);

            if(isSelected)
            {
                // Mix the LED color with selection blue for visual feedback
                QColor selection_color = QColor(100, 150, 255); // Light blue selection
                QColor blended_color;
                if(cell_mappings.empty())
                {
                    // For empty cells, use pure selection color
                    blended_color = selection_color;
                }
                else
                {
                    // Blend LED color with selection color (70% selection, 30% LED color)
                    blended_color = QColor(
                        static_cast<int>(selection_color.red() * 0.7 + cell_color.red() * 0.3),
                        static_cast<int>(selection_color.green() * 0.7 + cell_color.green() * 0.3),
                        static_cast<int>(selection_color.blue() * 0.7 + cell_color.blue() * 0.3)
                    );
                }
                item->setBackground(QBrush(blended_color));

                // Update text color for selected cell
                QColor text_color = (blended_color.red() + blended_color.green() + blended_color.blue() > 382) ?
                                   Qt::black : Qt::white;
                item->setForeground(QBrush(text_color));
            }
            else
            {
                item->setBackground(QBrush(cell_color));

                // Set text color for non-selected cells
                if(!cell_mappings.empty())
                {
                    QColor text_color = (cell_color.red() + cell_color.green() + cell_color.blue() > 382) ?
                                       Qt::black : Qt::white;
                    item->setForeground(QBrush(text_color));
                }
            }

            grid_table->setItem(i, j, item);
        }
    }

    grid_table->horizontalHeader()->setDefaultSectionSize(30);
    grid_table->verticalHeader()->setDefaultSectionSize(30);
    grid_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    grid_table->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
}

void CustomControllerDialog::UpdateCellInfo()
{
    if(selected_row < 0 || selected_col < 0)
    {
        cell_info_label->setText("Click a cell to select it");
        return;
    }

    QString info = QString("Selected: X=%1, Y=%2, Z=%3")
                    .arg(selected_col)
                    .arg(selected_row)
                    .arg(current_layer);

    // Count all mappings for this cell
    std::vector<GridLEDMapping> cell_mappings;
    for(const GridLEDMapping& mapping : led_mappings)
    {
        if(mapping.x == selected_col && mapping.y == selected_row && mapping.z == current_layer)
        {
            cell_mappings.push_back(mapping);
        }
    }

    if(cell_mappings.empty())
    {
        info += " - Empty";
    }
    else if(cell_mappings.size() == 1)
    {
        const GridLEDMapping& mapping = cell_mappings[0];
        if(!mapping.controller)
        {
            info += " - Invalid assignment";
        }
        else if(mapping.granularity == 0)
        {
            // Whole device assignment
            info += QString(" - Assigned: %1 (Whole Device)")
                    .arg(QString::fromStdString(mapping.controller->name));
        }
        else if(mapping.granularity == 1)
        {
            // Zone assignment
            QString zone_name = "Unknown Zone";
            if(mapping.zone_idx < mapping.controller->zones.size())
            {
                zone_name = QString::fromStdString(mapping.controller->zones[mapping.zone_idx].name);
            }
            info += QString(" - Assigned: %1, Zone: %2")
                    .arg(QString::fromStdString(mapping.controller->name))
                    .arg(zone_name);
        }
        else if(mapping.granularity == 2)
        {
            // LED assignment
            QString led_name = "Unknown LED";
            unsigned int global_led_idx = 0;
            if(mapping.zone_idx < mapping.controller->zones.size())
            {
                global_led_idx = mapping.controller->zones[mapping.zone_idx].start_idx + mapping.led_idx;
                if(global_led_idx < mapping.controller->leds.size())
                {
                    led_name = QString::fromStdString(mapping.controller->leds[global_led_idx].name);
                }
            }
            info += QString(" - Assigned: %1, LED: %2")
                    .arg(QString::fromStdString(mapping.controller->name))
                    .arg(led_name);
        }
        else
        {
            info += QString(" - Assigned: %1").arg(QString::fromStdString(mapping.controller->name));
        }
    }
    else
    {
        info += QString(" - Multiple LEDs (%1)").arg(cell_mappings.size());
    }

    cell_info_label->setText(info);
}

void CustomControllerDialog::on_assign_clicked()
{
    if(selected_row < 0 || selected_col < 0)
    {
        QMessageBox::warning(this, "No Cell Selected", "Please select a grid cell first");
        return;
    }

    int ctrl_idx = available_controllers->currentRow();
    if(ctrl_idx < 0)
    {
        QMessageBox::warning(this, "No Controller Selected", "Please select a controller first");
        return;
    }

    std::vector<RGBController*>& controllers = resource_manager->GetRGBControllers();
    if(ctrl_idx >= (int)controllers.size()) return;

    RGBController* controller = controllers[ctrl_idx];
    if(!controller) return;

    int granularity = granularity_combo->currentIndex();
    int combo_idx = item_combo->currentIndex();

    if(combo_idx < 0)
    {
        QMessageBox::warning(this, "No Item Selected", "Please select an item from the dropdown");
        return;
    }

    int item_idx = item_combo->currentData().toInt();

    for(std::vector<GridLEDMapping>::iterator it = led_mappings.begin(); it != led_mappings.end();)
    {
        if(it->x == selected_col && it->y == selected_row && it->z == current_layer)
        {
            it = led_mappings.erase(it);
        }
        else
        {
            ++it;
        }
    }

    std::vector<LEDPosition3D> positions = ControllerLayout3D::GenerateLEDPositions(controller);

    if(granularity == 0)
    {
        for(unsigned int p = 0; p < positions.size(); p++)
        {
            GridLEDMapping mapping;
            mapping.x = selected_col;
            mapping.y = selected_row;
            mapping.z = current_layer;
            mapping.controller = controller;
            mapping.zone_idx = positions[p].zone_idx;
            mapping.led_idx = positions[p].led_idx;
            mapping.granularity = 0; // Whole device
            led_mappings.push_back(mapping);
        }
    }
    else if(granularity == 1)
    {
        for(unsigned int p = 0; p < positions.size(); p++)
        {
            if(positions[p].zone_idx == (unsigned int)item_idx)
            {
                GridLEDMapping mapping;
                mapping.x = selected_col;
                mapping.y = selected_row;
                mapping.z = current_layer;
                mapping.controller = controller;
                mapping.zone_idx = positions[p].zone_idx;
                mapping.led_idx = positions[p].led_idx;
                mapping.granularity = 1; // Zone
                led_mappings.push_back(mapping);
            }
        }
    }
    else if(granularity == 2)
    {
        for(unsigned int p = 0; p < positions.size(); p++)
        {
            // Add bounds checking before accessing zones array
            if(positions[p].zone_idx >= controller->zones.size())
                continue;

            unsigned int global_led_idx = controller->zones[positions[p].zone_idx].start_idx + positions[p].led_idx;
            if(global_led_idx == (unsigned int)item_idx)
            {
                GridLEDMapping mapping;
                mapping.x = selected_col;
                mapping.y = selected_row;
                mapping.z = current_layer;
                mapping.controller = controller;
                mapping.zone_idx = positions[p].zone_idx;
                mapping.led_idx = positions[p].led_idx;
                mapping.granularity = 2; // LED
                led_mappings.push_back(mapping);
            }
        }
    }

    UpdateGridColors();
    UpdateCellInfo();
    UpdateItemCombo();
}

void CustomControllerDialog::on_clear_cell_clicked()
{
    if(selected_row < 0 || selected_col < 0)
    {
        QMessageBox::warning(this, "No Cell Selected", "Please select a grid cell first");
        return;
    }

    for(std::vector<GridLEDMapping>::iterator it = led_mappings.begin(); it != led_mappings.end();)
    {
        if(it->x == selected_col && it->y == selected_row && it->z == current_layer)
        {
            it = led_mappings.erase(it);
        }
        else
        {
            ++it;
        }
    }

    UpdateGridColors();
    UpdateCellInfo();
    UpdateItemCombo();
}

void CustomControllerDialog::on_remove_all_leds_clicked()
{
    if(led_mappings.empty())
    {
        QMessageBox::information(this, "Grid Empty", "The grid is already empty - no LEDs to remove");
        return;
    }

    int reply = QMessageBox::question(this, "Remove All LEDs",
                                      QString("Are you sure you want to remove all %1 LED(s) from the grid?").arg(led_mappings.size()),
                                      QMessageBox::Yes | QMessageBox::No);

    if(reply == QMessageBox::Yes)
    {
        size_t removed_count = led_mappings.size();
        led_mappings.clear();

        QMessageBox::information(this, "Removed", QString("Removed all %1 LED(s) from grid").arg(static_cast<int>(removed_count)));

        UpdateGridColors();
        UpdateCellInfo();
        UpdateItemCombo();
    }
}

void CustomControllerDialog::on_save_clicked()
{
    if(name_edit->text().isEmpty())
    {
        QMessageBox::warning(this, "No Name", "Please enter a name for the custom controller");
        return;
    }

    if(led_mappings.empty())
    {
        QMessageBox::warning(this, "No LEDs Assigned", "Please assign at least one LED to the grid");
        return;
    }

    accept();
}

QString CustomControllerDialog::GetControllerName() const
{
    return name_edit->text();
}

int CustomControllerDialog::GetGridWidth() const
{
    return width_spin->value();
}

int CustomControllerDialog::GetGridHeight() const
{
    return height_spin->value();
}

int CustomControllerDialog::GetGridDepth() const
{
    return depth_spin->value();
}

bool CustomControllerDialog::IsItemAssigned(RGBController* controller, int granularity, int item_idx)
{
    if(granularity == 0)
    {
        for(unsigned int i = 0; i < led_mappings.size(); i++)
        {
            if(led_mappings[i].controller == controller)
            {
                return true;
            }
        }
    }
    else if(granularity == 1)
    {
        for(unsigned int i = 0; i < led_mappings.size(); i++)
        {
            if(led_mappings[i].controller == controller && led_mappings[i].zone_idx == (unsigned int)item_idx)
            {
                return true;
            }
        }
    }
    else if(granularity == 2)
    {
        for(unsigned int i = 0; i < led_mappings.size(); i++)
        {
            if(led_mappings[i].zone_idx >= controller->zones.size())
                continue;

            unsigned int global_led_idx = controller->zones[led_mappings[i].zone_idx].start_idx + led_mappings[i].led_idx;
            if(led_mappings[i].controller == controller && global_led_idx == (unsigned int)item_idx)
            {
                return true;
            }
        }
    }
    return false;
}

void CustomControllerDialog::LoadExistingController(const std::string& name, int width, int height, int depth,
                                                    const std::vector<GridLEDMapping>& mappings)
{
    name_edit->setText(QString::fromStdString(name));
    width_spin->setValue(width);
    height_spin->setValue(height);
    depth_spin->setValue(depth);
    led_mappings = mappings;

    // Infer granularity for existing mappings that might not have it set
    InferMappingGranularity();

    UpdateGridDisplay();
}

QColor CustomControllerDialog::GetItemColor(RGBController* controller, int granularity, int item_idx)
{
    if(!controller) return QColor(128, 128, 128);

    if(granularity == 0)
    {
        return GetAverageDeviceColor(controller);
    }
    else if(granularity == 1)
    {
        if(item_idx >= 0 && item_idx < (int)controller->zones.size())
        {
            return GetAverageZoneColor(controller, item_idx);
        }
    }
    else if(granularity == 2)
    {
        if(item_idx >= 0 && item_idx < (int)controller->colors.size())
        {
            return RGBToQColor(controller->colors[item_idx]);
        }
    }
    return QColor(128, 128, 128);
}

QColor CustomControllerDialog::GetAverageZoneColor(RGBController* controller, unsigned int zone_idx)
{
    if(zone_idx >= controller->zones.size()) return QColor(128, 128, 128);

    const zone& z = controller->zones[zone_idx];
    if(z.leds_count == 0) return QColor(128, 128, 128);

    unsigned int total_r = 0, total_g = 0, total_b = 0;
    unsigned int led_count = 0;

    for(unsigned int i = 0; i < z.leds_count && (z.start_idx + i) < controller->colors.size(); i++)
    {
        unsigned int color = controller->colors[z.start_idx + i];
        total_r += (color >> 0) & 0xFF;
        total_g += (color >> 8) & 0xFF;
        total_b += (color >> 16) & 0xFF;
        led_count++;
    }

    if(led_count == 0) return QColor(128, 128, 128);

    return QColor(static_cast<int>(total_r / led_count), static_cast<int>(total_g / led_count), static_cast<int>(total_b / led_count));
}

QColor CustomControllerDialog::GetAverageDeviceColor(RGBController* controller)
{
    if(!controller || controller->colors.empty()) return QColor(128, 128, 128);

    unsigned long long total_r = 0, total_g = 0, total_b = 0;  // Use larger type to prevent overflow

    for(unsigned int color : controller->colors)
    {
        total_r += (color >> 0) & 0xFF;
        total_g += (color >> 8) & 0xFF;
        total_b += (color >> 16) & 0xFF;
    }

    size_t count = controller->colors.size();
    return QColor(static_cast<int>(total_r / count), static_cast<int>(total_g / count), static_cast<int>(total_b / count));
}

QColor CustomControllerDialog::GetMappingColor(const GridLEDMapping& mapping)
{
    if(!mapping.controller)
        return QColor(128, 128, 128);

    if(mapping.zone_idx >= mapping.controller->zones.size())
        return QColor(128, 128, 128);

    const zone& z = mapping.controller->zones[mapping.zone_idx];
    unsigned int global_led_idx = z.start_idx + mapping.led_idx;

    if(global_led_idx >= mapping.controller->colors.size())
        return QColor(128, 128, 128);

    return RGBToQColor(mapping.controller->colors[global_led_idx]);
}

void CustomControllerDialog::InferMappingGranularity()
{
    for(unsigned int i = 0; i < led_mappings.size(); i++)
    {
        if(led_mappings[i].granularity < 0 || led_mappings[i].granularity > 2)
        {
            led_mappings[i].granularity = 2;
        }
    }
}

QColor CustomControllerDialog::RGBToQColor(unsigned int rgb_value)
{
    unsigned int r = (rgb_value >> 0) & 0xFF;
    unsigned int g = (rgb_value >> 8) & 0xFF;
    unsigned int b = (rgb_value >> 16) & 0xFF;
    return QColor(r, g, b);
}

ColorComboDelegate::ColorComboDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

void ColorComboDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                              const QModelIndex &index) const
{
    QStyledItemDelegate::paint(painter, option, index);

    if(index.data(Qt::DecorationRole).canConvert<QIcon>())
    {
        QIcon icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
        QRect icon_rect = option.rect;
        icon_rect.setWidth(20);
        icon_rect.adjust(4, 2, 0, -2);

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);

        QPixmap pixmap = icon.pixmap(icon_rect.size());
        painter->drawPixmap(icon_rect, pixmap);

        painter->restore();
    }
}

QSize ColorComboDelegate::sizeHint(const QStyleOptionViewItem &option,
                                  const QModelIndex &index) const
{
    QSize size = QStyledItemDelegate::sizeHint(option, index);
    size.setHeight(qMax(size.height(), 24));
    return size;
}

void CustomControllerDialog::UpdateGridColors()
{
    if(!grid_table) return;

    for(int i = 0; i < grid_table->rowCount(); i++)
    {
        for(int j = 0; j < grid_table->columnCount(); j++)
        {
            QTableWidgetItem* item = grid_table->item(i, j);
            if(!item) continue;

            std::vector<GridLEDMapping> cell_mappings;
            for(unsigned int k = 0; k < led_mappings.size(); k++)
            {
                if(led_mappings[k].x == j && led_mappings[k].y == i && led_mappings[k].z == current_layer)
                {
                    cell_mappings.push_back(led_mappings[k]);
                }
            }

            QColor cell_color(50, 50, 50);

            if(!cell_mappings.empty())
            {
                // Calculate color (same logic as UpdateGridDisplay)
                if(cell_mappings.size() == 1)
                {
                    cell_color = GetMappingColor(cell_mappings[0]);
                }
                else
                {
                    unsigned int total_r = 0, total_g = 0, total_b = 0;
                    unsigned int valid_count = 0;

                    for(unsigned int m = 0; m < cell_mappings.size(); m++)
                    {
                        QColor ledColor = GetMappingColor(cell_mappings[m]);
                        total_r += ledColor.red();
                        total_g += ledColor.green();
                        total_b += ledColor.blue();
                        valid_count++;
                    }

                    if(valid_count > 0)
                    {
                        cell_color = QColor(total_r / valid_count, total_g / valid_count, total_b / valid_count);
                    }
                }
            }

            // Check if this cell is currently selected
            bool isSelected = (i == selected_row && j == selected_col);

            if(isSelected)
            {
                // Mix the LED color with selection blue for visual feedback
                QColor selection_color = QColor(100, 150, 255);
                QColor blended_color;
                if(cell_mappings.empty())
                {
                    blended_color = selection_color;
                }
                else
                {
                    blended_color = QColor(
                        static_cast<int>(selection_color.red() * 0.7 + cell_color.red() * 0.3),
                        static_cast<int>(selection_color.green() * 0.7 + cell_color.green() * 0.3),
                        static_cast<int>(selection_color.blue() * 0.7 + cell_color.blue() * 0.3)
                    );
                }
                item->setBackground(QBrush(blended_color));

                QColor text_color = (blended_color.red() + blended_color.green() + blended_color.blue() > 382) ?
                                   Qt::black : Qt::white;
                item->setForeground(QBrush(text_color));
            }
            else
            {
                item->setBackground(QBrush(cell_color));

                if(!cell_mappings.empty())
                {
                    QColor text_color = (cell_color.red() + cell_color.green() + cell_color.blue() > 382) ?
                                       Qt::black : Qt::white;
                    item->setForeground(QBrush(text_color));
                }
            }
        }
    }
}

void CustomControllerDialog::refresh_colors()
{
    UpdateItemCombo();
    UpdateGridColors();
}