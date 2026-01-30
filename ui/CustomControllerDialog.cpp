// SPDX-License-Identifier: GPL-2.0-only

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
#include <cmath>
#include <algorithm>
#include <climits>
#include <functional>

CustomControllerDialog::CustomControllerDialog(ResourceManagerInterface* rm, QWidget *parent)
    : QDialog(parent),
      resource_manager(rm),
      transform_locked(false),
      current_layer(0),
      selected_row(-1),
      selected_col(-1)
{
    setWindowTitle("Create Custom 3D Controller");
    resize(1000, 600);
    SetupUI();
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
    QGridLayout* dim_layout = new QGridLayout();
    dim_layout->addWidget(new QLabel("Width:"), 0, 0);
    width_spin = new QSpinBox();
    width_spin->setRange(1, 200);
    width_spin->setValue(10);
    connect(width_spin, SIGNAL(valueChanged(int)), this, SLOT(on_dimension_changed()));
    dim_layout->addWidget(width_spin, 0, 1);

    dim_layout->addWidget(new QLabel("Height:"), 0, 2);
    height_spin = new QSpinBox();
    height_spin->setRange(1, 200);
    height_spin->setValue(10);
    connect(height_spin, SIGNAL(valueChanged(int)), this, SLOT(on_dimension_changed()));
    dim_layout->addWidget(height_spin, 0, 3);

    dim_layout->addWidget(new QLabel("Depth:"), 0, 4);
    depth_spin = new QSpinBox();
    depth_spin->setRange(1, 200);
    depth_spin->setValue(1);
    connect(depth_spin, SIGNAL(valueChanged(int)), this, SLOT(on_dimension_changed()));
    dim_layout->addWidget(depth_spin, 0, 5);

    // LED Spacing Row
    dim_layout->addWidget(new QLabel("Spacing X:"), 1, 0);
    spacing_x_spin = new QDoubleSpinBox();
    spacing_x_spin->setRange(0.1, 1000.0);
    spacing_x_spin->setValue(10.0);
    spacing_x_spin->setSuffix(" mm");
    dim_layout->addWidget(spacing_x_spin, 1, 1);

    dim_layout->addWidget(new QLabel("Spacing Y:"), 1, 2);
    spacing_y_spin = new QDoubleSpinBox();
    spacing_y_spin->setRange(0.1, 1000.0);
    spacing_y_spin->setValue(10.0);
    spacing_y_spin->setSuffix(" mm");
    dim_layout->addWidget(spacing_y_spin, 1, 3);

    dim_layout->addWidget(new QLabel("Spacing Z:"), 1, 4);
    spacing_z_spin = new QDoubleSpinBox();
    spacing_z_spin->setRange(0.1, 1000.0);
    spacing_z_spin->setValue(10.0);
    spacing_z_spin->setSuffix(" mm");
    dim_layout->addWidget(spacing_z_spin, 1, 5);

    dim_group->setLayout(dim_layout);
    right_layout->addWidget(dim_group);

    layer_tabs = new QTabWidget();
    connect(layer_tabs, SIGNAL(currentChanged(int)), this, SLOT(on_layer_tab_changed(int)));

    QWidget* first_tab = new QWidget();
    QVBoxLayout* first_tab_layout = new QVBoxLayout(first_tab);
    first_tab_layout->setContentsMargins(0, 0, 0, 0);
    grid_table = new QTableWidget();
    grid_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    grid_table->setSelectionBehavior(QAbstractItemView::SelectItems);
    grid_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    connect(grid_table, &QTableWidget::cellClicked, this, &CustomControllerDialog::on_grid_cell_clicked);

    ApplyGridTableHeaderStyle();
    grid_table->setShowGrid(true);
    first_tab_layout->addWidget(grid_table);
    layer_tabs->addTab(first_tab, "Layer 0");
    right_layout->addWidget(layer_tabs);

    cell_info_label = new QLabel("Click a cell to select it");
    right_layout->addWidget(cell_info_label);

        QGroupBox* transform_group = new QGroupBox("Transform Grid Layout");
    QGridLayout* transform_grid = new QGridLayout();

    // Lock transform checkbox
    lock_transform_checkbox = new QCheckBox("Lock Effect Direction (preview-only)");
    lock_transform_checkbox->setChecked(false);
    lock_transform_checkbox->setToolTip("Keeps preview effect flowing left-to-right. Disables transforms or treats them as preview-only.");
    connect(lock_transform_checkbox, &QCheckBox::toggled, this, &CustomControllerDialog::on_lock_transform_toggled);
    transform_grid->addWidget(lock_transform_checkbox, 0, 0, 1, 4);

    QLabel* rotate_label = new QLabel("Rotate Grid:");
    transform_grid->addWidget(rotate_label, 1, 0, 1, 4);

    // Rotation slider with spinbox
    QLabel* angle_label = new QLabel("Angle:");
    transform_grid->addWidget(angle_label, 2, 0);

    rotate_angle_slider = new QSlider(Qt::Horizontal);
    rotate_angle_slider->setRange(0, 359);
    rotate_angle_slider->setValue(0);
    rotate_angle_slider->setTickPosition(QSlider::TicksBelow);
    rotate_angle_slider->setTickInterval(45);
    rotate_angle_slider->setToolTip("Rotate grid by any angle (0-359\xC2\xB0) - Lock layout first!");
    rotate_angle_slider->setEnabled(false);
    transform_grid->addWidget(rotate_angle_slider, 2, 1, 1, 2);

    rotate_angle_spin = new QSpinBox();
    rotate_angle_spin->setRange(0, 359);
    rotate_angle_spin->setValue(0);
    rotate_angle_spin->setSuffix(QString::fromUtf8("\xC2\xB0"));
    rotate_angle_spin->setToolTip("Rotation angle in degrees - Lock layout first!");
    rotate_angle_spin->setEnabled(false);
    transform_grid->addWidget(rotate_angle_spin, 2, 3);

    // Connect slider and spinbox
    connect(rotate_angle_slider, &QSlider::valueChanged, rotate_angle_spin, &QSpinBox::setValue);
    connect(rotate_angle_spin, QOverload<int>::of(&QSpinBox::valueChanged), rotate_angle_slider, &QSlider::setValue);

    // Connect to real-time rotation handler
    connect(rotate_angle_slider, &QSlider::valueChanged, this, &CustomControllerDialog::on_rotation_angle_changed);
    connect(rotate_angle_spin, QOverload<int>::of(&QSpinBox::valueChanged), this, &CustomControllerDialog::on_rotation_angle_changed);

    // Quick rotation presets
    QLabel* presets_label = new QLabel("Quick Presets:");
    transform_grid->addWidget(presets_label, 3, 0, 1, 4);

    rotate_90_button = new QPushButton(QString::fromUtf8("90\xC2\xB0"));
    rotate_90_button->setToolTip("Rotate LED grid 90\xC2\xB0 clockwise");
    connect(rotate_90_button, &QPushButton::clicked, this, &CustomControllerDialog::on_rotate_grid_90);
    transform_grid->addWidget(rotate_90_button, 4, 0);

    rotate_180_button = new QPushButton(QString::fromUtf8("180\xC2\xB0"));
    rotate_180_button->setToolTip("Rotate LED grid 180\xC2\xB0");
    connect(rotate_180_button, &QPushButton::clicked, this, &CustomControllerDialog::on_rotate_grid_180);
    transform_grid->addWidget(rotate_180_button, 4, 1);

    rotate_270_button = new QPushButton(QString::fromUtf8("270\xC2\xB0"));
    rotate_270_button->setToolTip("Rotate LED grid 270\xC2\xB0 clockwise");
    connect(rotate_270_button, &QPushButton::clicked, this, &CustomControllerDialog::on_rotate_grid_270);
    transform_grid->addWidget(rotate_270_button, 4, 2);

    QLabel* flip_label = new QLabel("Flip Grid:");
    transform_grid->addWidget(flip_label, 5, 0, 1, 4);

    flip_horizontal_button = new QPushButton("Flip Horizontal");
    flip_horizontal_button->setToolTip("Flip LED grid horizontally");
    connect(flip_horizontal_button, &QPushButton::clicked, this, &CustomControllerDialog::on_flip_grid_horizontal);
    transform_grid->addWidget(flip_horizontal_button, 6, 0, 1, 2);

    flip_vertical_button = new QPushButton("Flip Vertical");
    flip_vertical_button->setToolTip("Flip LED grid vertically");
    connect(flip_vertical_button, &QPushButton::clicked, this, &CustomControllerDialog::on_flip_grid_vertical);
    transform_grid->addWidget(flip_vertical_button, 6, 2, 1, 2);

    // Apply Preview Remap button (shown when Lock is ON)
    apply_preview_button = new QPushButton("Apply Preview Remap");
    apply_preview_button->setToolTip("Commit current preview remap to the mapping");
    apply_preview_button->setEnabled(false);
    connect(apply_preview_button, &QPushButton::clicked, this, &CustomControllerDialog::on_apply_preview_remap_clicked);
    transform_grid->addWidget(apply_preview_button, 7, 0, 1, 4);

    transform_group->setLayout(transform_grid);
    right_layout->addWidget(transform_group);

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
    // Preserve current selection where possible
    int prev_index = item_combo->currentIndex();
    int prev_data = item_combo->currentData().isValid() ? item_combo->currentData().toInt() : -9999;

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

    // Try to restore previous selection
    int restore_index = -1;
    for(int i = 0; i < item_combo->count(); i++)
    {
        if(item_combo->itemData(i).isValid() && item_combo->itemData(i).toInt() == prev_data)
        {
            restore_index = i;
            break;
        }
    }
    if(restore_index >= 0)
    {
        item_combo->setCurrentIndex(restore_index);
    }
    else if(prev_index >= 0 && prev_index < item_combo->count())
    {
        item_combo->setCurrentIndex(prev_index);
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

            const std::vector<GridLEDMapping>& mappings_ref = transform_locked ? preview_led_mappings : led_mappings;
            std::vector<GridLEDMapping> cell_mappings;
            for(unsigned int k = 0; k < mappings_ref.size(); k++)
            {
                if(mappings_ref[k].x == j && mappings_ref[k].y == i && mappings_ref[k].z == current_layer)
                {
                    cell_mappings.push_back(mappings_ref[k]);
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
                                       .arg(QString::fromStdString(mapping.controller->name), zone_name);
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
                                       .arg(QString::fromStdString(mapping.controller->name), led_name);
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

                        tooltip_text += QString("â€¢ %1%2\n")
                                       .arg(QString::fromStdString(mapping.controller->name), assignment_type);
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
                    item->setText("â—");
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

    ApplyGridTableHeaderStyle();
}

void CustomControllerDialog::ApplyGridTableHeaderStyle()
{
    if(!grid_table) return;
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
    for(unsigned int i = 0; i < led_mappings.size(); i++)
    {
        if(led_mappings[i].x == selected_col && led_mappings[i].y == selected_row && led_mappings[i].z == current_layer)
        {
            cell_mappings.push_back(led_mappings[i]);
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
                    .arg(QString::fromStdString(mapping.controller->name), zone_name);
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
                    .arg(QString::fromStdString(mapping.controller->name), led_name);
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

    std::vector<LEDPosition3D> positions = ControllerLayout3D::GenerateCustomGridLayout(controller, 10, 10);

    if(granularity == 0)
    {
        // Whole device: lay out all LEDs in a line starting at selected cell
        int grid_w = width_spin->value();
        int grid_h = height_spin->value();

        int start_x = selected_col;
        int start_y = selected_row;

        int placed = 0;
        for(unsigned int p = 0; p < positions.size(); p++)
        {
            int index = placed;
            int x = (start_x + index) % grid_w;
            int y = start_y + (start_x + index) / grid_w;
            if(y >= grid_h)
                break;

            // Clear any existing mapping at target cell
            for(std::vector<GridLEDMapping>::iterator it = led_mappings.begin(); it != led_mappings.end();)
            {
                if(it->x == x && it->y == y && it->z == current_layer)
                    it = led_mappings.erase(it);
                else
                    ++it;
            }

            GridLEDMapping mapping;
            mapping.x = x;
            mapping.y = y;
            mapping.z = current_layer;
            mapping.controller = controller;
            mapping.zone_idx = positions[p].zone_idx;
            mapping.led_idx = positions[p].led_idx;
            mapping.granularity = 0; // Whole device selection
            led_mappings.push_back(mapping);

            placed++;
        }

        if(placed < (int)positions.size())
        {
            QMessageBox::information(this, "Grid Too Small",
                                     QString("Placed %1 of %2 LEDs (expand grid or choose a different start cell)")
                                     .arg(placed).arg((int)positions.size()));
        }
    }
    else if(granularity == 1)
    {
        // Zone: lay out all LEDs in the zone in a line starting at selected cell
        int grid_w = width_spin->value();
        int grid_h = height_spin->value();

        int start_x = selected_col;
        int start_y = selected_row;

        // Collect zone LEDs in order
        std::vector<std::pair<unsigned,int>> zone_leds; zone_leds.reserve(positions.size());
        for(unsigned int p = 0; p < positions.size(); p++)
        {
            if(positions[p].zone_idx == (unsigned int)item_idx)
            {
                zone_leds.push_back({positions[p].zone_idx, (int)positions[p].led_idx});
            }
        }

        int placed = 0;
        for(unsigned int idx = 0; idx < zone_leds.size(); idx++)
        {
            int index = placed;
            int x = (start_x + index) % grid_w;
            int y = start_y + (start_x + index) / grid_w;
            if(y >= grid_h)
                break;

            // Clear existing mapping at target cell
            for(std::vector<GridLEDMapping>::iterator it = led_mappings.begin(); it != led_mappings.end();)
            {
                if(it->x == x && it->y == y && it->z == current_layer)
                    it = led_mappings.erase(it);
                else
                    ++it;
            }

            GridLEDMapping mapping;
            mapping.x = x;
            mapping.y = y;
            mapping.z = current_layer;
            mapping.controller = controller;
            mapping.zone_idx = zone_leds[idx].first;
            mapping.led_idx = zone_leds[idx].second;
            mapping.granularity = 1; // Zone selection
            led_mappings.push_back(mapping);

            placed++;
        }

        if(placed < (int)zone_leds.size())
        {
            QMessageBox::information(this, "Grid Too Small",
                                     QString("Placed %1 of %2 zone LEDs (expand grid or choose a different start cell)")
                                     .arg(placed).arg((int)zone_leds.size()));
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

    // Keep preview in sync if locked
    if(transform_locked)
    {
        preview_led_mappings = led_mappings;
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

float CustomControllerDialog::GetSpacingX() const
{
    return (float)spacing_x_spin->value();
}

float CustomControllerDialog::GetSpacingY() const
{
    return (float)spacing_y_spin->value();
}

float CustomControllerDialog::GetSpacingZ() const
{
    return (float)spacing_z_spin->value();
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

void CustomControllerDialog::LoadExistingController(const std::string& name,
                                                    int width,
                                                    int height,
                                                    int depth,
                                                    const std::vector<GridLEDMapping>& mappings,
                                                    float spacing_x_mm,
                                                    float spacing_y_mm,
                                                    float spacing_z_mm)
{
    name_edit->setText(QString::fromStdString(name));
    width_spin->setValue(width);
    height_spin->setValue(height);
    depth_spin->setValue(depth);
    led_mappings = mappings;

    std::function<double(double, double, double)> clamp_spacing = [](double value, double min_value, double max_value)
    {
        if(value < min_value) return min_value;
        if(value > max_value) return max_value;
        return value;
    };

    if(spacing_x_spin)
    {
        spacing_x_spin->setValue(clamp_spacing(spacing_x_mm,
                                               spacing_x_spin->minimum(),
                                               spacing_x_spin->maximum()));
    }
    if(spacing_y_spin)
    {
        spacing_y_spin->setValue(clamp_spacing(spacing_y_mm,
                                               spacing_y_spin->minimum(),
                                               spacing_y_spin->maximum()));
    }
    if(spacing_z_spin)
    {
        spacing_z_spin->setValue(clamp_spacing(spacing_z_mm,
                                               spacing_z_spin->minimum(),
                                               spacing_z_spin->maximum()));
    }

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

    for(unsigned int i = 0; i < controller->colors.size(); i++)
    {
        total_r += (controller->colors[i] >> 0) & 0xFF;
        total_g += (controller->colors[i] >> 8) & 0xFF;
        total_b += (controller->colors[i] >> 16) & 0xFF;
    }

    size_t count = controller->colors.size();
    if(count == 0)
    {
        return QColor(0, 0, 0);
    }
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

            const std::vector<GridLEDMapping>& mappings_ref = transform_locked ? preview_led_mappings : led_mappings;
            std::vector<GridLEDMapping> cell_mappings;
            for(unsigned int k = 0; k < mappings_ref.size(); k++)
            {
                if(mappings_ref[k].x == j && mappings_ref[k].y == i && mappings_ref[k].z == current_layer)
                {
                    cell_mappings.push_back(mappings_ref[k]);
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


void CustomControllerDialog::on_lock_transform_toggled(bool locked)
{
    transform_locked = locked;

    if(locked)
    {
        // Lock the current layout: snapshot both original and preview mappings
        original_led_mappings = led_mappings;
        preview_led_mappings = led_mappings;

        // Disable free-angle rotation; allow preview remap via 180/Flips; disable 90/270 (dimension swap)
        rotate_angle_slider->setEnabled(false);
        rotate_angle_spin->setEnabled(false);
        rotate_angle_slider->setToolTip("Disabled while effect direction is locked");
        rotate_angle_spin->setToolTip("Disabled while effect direction is locked");

        if(rotate_90_button)      { rotate_90_button->setEnabled(false); rotate_90_button->setToolTip("Rotation 90° disabled while locked (requires geometry change)"); }
        if(rotate_180_button)     { rotate_180_button->setEnabled(true);  rotate_180_button->setToolTip("Preview-only remap: rotates assignments 180°"); }
        if(rotate_270_button)     { rotate_270_button->setEnabled(false); rotate_270_button->setToolTip("Rotation 270° disabled while locked (requires geometry change)"); }
        if(flip_horizontal_button){ flip_horizontal_button->setEnabled(true); flip_horizontal_button->setToolTip("Preview-only remap: flips assignments horizontally"); }
        if(flip_vertical_button)  { flip_vertical_button->setEnabled(true);  flip_vertical_button->setToolTip("Preview-only remap: flips assignments vertically"); }

        if(apply_preview_button)  apply_preview_button->setEnabled(true);

        // Reset angle to 0
        rotate_angle_slider->blockSignals(true);
        rotate_angle_spin->blockSignals(true);
        rotate_angle_slider->setValue(0);
        rotate_angle_spin->setValue(0);
        rotate_angle_slider->blockSignals(false);
        rotate_angle_spin->blockSignals(false);
    }
    else
    {
        // Unlock - re-enable rotation/flip controls
        rotate_angle_slider->setEnabled(true);
        rotate_angle_spin->setEnabled(true);
        rotate_angle_slider->setToolTip("");
        rotate_angle_spin->setToolTip("");
        if(rotate_90_button)      { rotate_90_button->setEnabled(true); rotate_90_button->setToolTip("Rotate LED grid 90° clockwise"); }
        if(rotate_180_button)     { rotate_180_button->setEnabled(true); rotate_180_button->setToolTip("Rotate LED grid 180°"); }
        if(rotate_270_button)     { rotate_270_button->setEnabled(true); rotate_270_button->setToolTip("Rotate LED grid 270° clockwise"); }
        if(flip_horizontal_button){ flip_horizontal_button->setEnabled(true); flip_horizontal_button->setToolTip("Flip LED grid horizontally"); }
        if(flip_vertical_button)  { flip_vertical_button->setEnabled(true);  flip_vertical_button->setToolTip("Flip LED grid vertically"); }

        if(apply_preview_button)  apply_preview_button->setEnabled(false);

        // Clear snapshots
        original_led_mappings.clear();
        preview_led_mappings.clear();
    }
}

void CustomControllerDialog::on_rotation_angle_changed(int angle)
{
    if(!transform_locked || original_led_mappings.empty())
    {
        return;
    }

    float angle_degrees = (float)angle;
    float angle_radians = angle_degrees * 3.14159265f / 180.0f;
    float cos_angle = std::cos(angle_radians);
    float sin_angle = std::sin(angle_radians);

    // Calculate center of ORIGINAL grid
    int orig_width = width_spin->value();
    int orig_height = height_spin->value();
    float center_x = (orig_width - 1) / 2.0f;
    float center_y = (orig_height - 1) / 2.0f;

    // Calculate new positions and determine required grid size
    std::vector<std::pair<int, int>> new_positions;
    new_positions.reserve(original_led_mappings.size());

    int min_x = INT_MAX, max_x = INT_MIN;
    int min_y = INT_MAX, max_y = INT_MIN;

    for(unsigned int i = 0; i < original_led_mappings.size(); i++)
    {
        // Translate to origin (using ORIGINAL positions)
        float x = original_led_mappings[i].x - center_x;
        float y = original_led_mappings[i].y - center_y;

        // Rotate
        float rotated_x = x * cos_angle - y * sin_angle;
        float rotated_y = x * sin_angle + y * cos_angle;

        // Translate back and round to nearest grid position
        int new_x = (int)std::round(rotated_x + center_x);
        int new_y = (int)std::round(rotated_y + center_y);

        new_positions.push_back(std::make_pair(new_x, new_y));

        // Track bounds
        min_x = std::min(min_x, new_x);
        max_x = std::max(max_x, new_x);
        min_y = std::min(min_y, new_y);
        max_y = std::max(max_y, new_y);
    }

    // Calculate required grid size
    int required_width = max_x - min_x + 1;
    int required_height = max_y - min_y + 1;

    // Auto-expand grid if needed
    bool grid_changed = false;
    if(required_width > orig_width)
    {
        width_spin->blockSignals(true);
        width_spin->setValue(required_width);
        width_spin->blockSignals(false);
        grid_changed = true;
    }
    if(required_height > orig_height)
    {
        height_spin->blockSignals(true);
        height_spin->setValue(required_height);
        height_spin->blockSignals(false);
        grid_changed = true;
    }

    // Shift positions if any went negative
    int shift_x = (min_x < 0) ? -min_x : 0;
    int shift_y = (min_y < 0) ? -min_y : 0;

    // Apply new positions to current mappings
    for(unsigned int i = 0; i < led_mappings.size(); i++)
    {
        led_mappings[i].x = new_positions[i].first + shift_x;
        led_mappings[i].y = new_positions[i].second + shift_y;
        led_mappings[i].z = original_led_mappings[i].z;  // Preserve Z
    }

    if(grid_changed)
    {
        RebuildLayerTabs();
    }

    UpdateGridDisplay();
}

void CustomControllerDialog::on_rotate_grid_90()
{
    if((transform_locked ? preview_led_mappings : led_mappings).empty())
    {
        QMessageBox::information(this, "Grid Empty", "No LEDs to rotate");
        return;
    }

    int width = width_spin->value();
    int height = height_spin->value();

    if(transform_locked)
    {
        QMessageBox::information(this, "Rotation Disabled", "90° rotation requires geometry change. Unlock to rotate the grid.");
        return;
    }
    else
    {
        // Rotate 90° clockwise: (x, y) -> (y, width - 1 - x)
        for(unsigned int i = 0; i < led_mappings.size(); i++)
        {
            int old_x = led_mappings[i].x;
            int old_y = led_mappings[i].y;
            led_mappings[i].x = old_y;
            led_mappings[i].y = width - 1 - old_x;
        }

        // Swap width and height
        width_spin->blockSignals(true);
        height_spin->blockSignals(true);
        width_spin->setValue(height);
        height_spin->setValue(width);
        width_spin->blockSignals(false);
        height_spin->blockSignals(false);

        UpdateGridDisplay();
    }
}

void CustomControllerDialog::on_rotate_grid_180()
{
    if((transform_locked ? preview_led_mappings : led_mappings).empty())
    {
        QMessageBox::information(this, "Grid Empty", "No LEDs to rotate");
        return;
    }

    int width = width_spin->value();
    int height = height_spin->value();

    if(transform_locked)
    {
        for(unsigned int i = 0; i < preview_led_mappings.size(); i++)
        {
            preview_led_mappings[i].x = width - 1 - preview_led_mappings[i].x;
            preview_led_mappings[i].y = height - 1 - preview_led_mappings[i].y;
        }
        UpdateGridColors();
    }
    else
    {
        for(unsigned int i = 0; i < led_mappings.size(); i++)
        {
            led_mappings[i].x = width - 1 - led_mappings[i].x;
            led_mappings[i].y = height - 1 - led_mappings[i].y;
        }
        UpdateGridDisplay();
    }
}

void CustomControllerDialog::on_rotate_grid_270()
{
    if((transform_locked ? preview_led_mappings : led_mappings).empty())
    {
        QMessageBox::information(this, "Grid Empty", "No LEDs to rotate");
        return;
    }

    int width = width_spin->value();
    int height = height_spin->value();

    if(transform_locked)
    {
        QMessageBox::information(this, "Rotation Disabled", "270° rotation requires geometry change. Unlock to rotate the grid.");
        return;
    }
    else
    {
        // Rotate 270° clockwise (= 90° counter-clockwise): (x, y) -> (height - 1 - y, x)
        for(unsigned int i = 0; i < led_mappings.size(); i++)
        {
            int old_x = led_mappings[i].x;
            int old_y = led_mappings[i].y;
            led_mappings[i].x = height - 1 - old_y;
            led_mappings[i].y = old_x;
        }

        // Swap width and height
        width_spin->blockSignals(true);
        height_spin->blockSignals(true);
        width_spin->setValue(height);
        height_spin->setValue(width);
        width_spin->blockSignals(false);
        height_spin->blockSignals(false);

        UpdateGridDisplay();
    }
}

void CustomControllerDialog::on_flip_grid_horizontal()
{
    if((transform_locked ? preview_led_mappings : led_mappings).empty())
    {
        QMessageBox::information(this, "Grid Empty", "No LEDs to flip");
        return;
    }

    int width = width_spin->value();

    if(transform_locked)
    {
        for(unsigned int i = 0; i < preview_led_mappings.size(); i++)
        {
            preview_led_mappings[i].x = width - 1 - preview_led_mappings[i].x;
        }
        UpdateGridColors();
    }
    else
    {
        for(unsigned int i = 0; i < led_mappings.size(); i++)
        {
            led_mappings[i].x = width - 1 - led_mappings[i].x;
        }
        UpdateGridDisplay();
    }
}

void CustomControllerDialog::on_flip_grid_vertical()
{
    if((transform_locked ? preview_led_mappings : led_mappings).empty())
    {
        QMessageBox::information(this, "Grid Empty", "No LEDs to flip");
        return;
    }

    int height = height_spin->value();

    if(transform_locked)
    {
        for(unsigned int i = 0; i < preview_led_mappings.size(); i++)
        {
            preview_led_mappings[i].y = height - 1 - preview_led_mappings[i].y;
        }
        UpdateGridColors();
    }
    else
    {
        for(unsigned int i = 0; i < led_mappings.size(); i++)
        {
            led_mappings[i].y = height - 1 - led_mappings[i].y;
        }
        UpdateGridDisplay();
    }
}

void CustomControllerDialog::on_apply_preview_remap_clicked()
{
    if(!transform_locked)
    {
        QMessageBox::information(this, "Not Locked", "Effect direction is not locked. No preview remap to apply.");
        return;
    }

    if(preview_led_mappings.empty())
    {
        QMessageBox::information(this, "Nothing To Apply", "No preview remap is available.");
        return;
    }

    // Commit preview into actual mappings
    led_mappings = preview_led_mappings;
    UpdateGridDisplay();
}




