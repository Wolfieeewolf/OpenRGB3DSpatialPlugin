/*---------------------------------------------------------*\
| OpenRGB3DSpatialTab_EffectStack.cpp                       |
|                                                           |
|   Effect Stack tab implementation                        |
|                                                           |
|   Date: 2025-10-05                                        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#include "OpenRGB3DSpatialTab.h"
#include "EffectListManager3D.h"
#include "LogManager.h"
#include <QMessageBox>

void OpenRGB3DSpatialTab::SetupEffectStackTab(QTabWidget* tab_widget)
{

    QWidget* stack_tab = new QWidget();
    QVBoxLayout* stack_layout = new QVBoxLayout(stack_tab);
    stack_layout->setSpacing(8);
    stack_layout->setContentsMargins(8, 8, 8, 8);

    /*---------------------------------------------------------*\
    | Active Effects List                                      |
    \*---------------------------------------------------------*/
    QLabel* list_label = new QLabel("Active Effect Stack:");
    list_label->setStyleSheet("font-weight: bold;");
    stack_layout->addWidget(list_label);

    QLabel* hint_label = new QLabel("☑ = enabled, ☐ = disabled. Double-click to toggle.");
    hint_label->setStyleSheet("font-size: 9pt; color: gray;");
    stack_layout->addWidget(hint_label);

    effect_stack_list = new QListWidget();
    effect_stack_list->setSelectionMode(QAbstractItemView::SingleSelection);
    effect_stack_list->setMinimumHeight(150);
    connect(effect_stack_list, &QListWidget::currentRowChanged,
            this, &OpenRGB3DSpatialTab::on_effect_stack_selection_changed);
    connect(effect_stack_list, &QListWidget::itemDoubleClicked,
            this, &OpenRGB3DSpatialTab::on_effect_stack_item_double_clicked);
    stack_layout->addWidget(effect_stack_list);

    /*---------------------------------------------------------*\
    | Add/Remove Buttons                                       |
    \*---------------------------------------------------------*/
    QHBoxLayout* button_layout = new QHBoxLayout();
    button_layout->addStretch();

    QPushButton* add_effect_btn = new QPushButton("+ Add Effect");
    connect(add_effect_btn, &QPushButton::clicked,
            this, &OpenRGB3DSpatialTab::on_add_effect_to_stack_clicked);
    button_layout->addWidget(add_effect_btn);

    QPushButton* remove_effect_btn = new QPushButton("- Remove Effect");
    connect(remove_effect_btn, &QPushButton::clicked,
            this, &OpenRGB3DSpatialTab::on_remove_effect_from_stack_clicked);
    button_layout->addWidget(remove_effect_btn);

    stack_layout->addLayout(button_layout);

    /*---------------------------------------------------------*\
    | Selected Effect Settings                                 |
    \*---------------------------------------------------------*/
    QGroupBox* settings_group = new QGroupBox("Selected Effect Settings");
    QVBoxLayout* settings_layout = new QVBoxLayout(settings_group);

    // Effect Type
    QHBoxLayout* type_layout = new QHBoxLayout();
    type_layout->addWidget(new QLabel("Effect Type:"));
    stack_effect_type_combo = new QComboBox();
    stack_effect_type_combo->setToolTip("Select which effect to run on this layer");

    // Add "None" option first
    stack_effect_type_combo->addItem("None", "");

    // Populate with all available effects (display UI name, store class name)
    std::vector<EffectRegistration3D> effects = EffectListManager3D::get()->GetAllEffects();
    for(const EffectRegistration3D& effect_reg : effects)
    {
        stack_effect_type_combo->addItem(QString::fromStdString(effect_reg.ui_name),
                                         QString::fromStdString(effect_reg.class_name));
    }

    connect(stack_effect_type_combo, SIGNAL(currentIndexChanged(int)),
            this, SLOT(on_stack_effect_type_changed(int)));
    type_layout->addWidget(stack_effect_type_combo);
    settings_layout->addLayout(type_layout);

    // Target Zone
    QHBoxLayout* zone_layout = new QHBoxLayout();
    zone_layout->addWidget(new QLabel("Target Zone:"));
    stack_effect_zone_combo = new QComboBox();
    stack_effect_zone_combo->setToolTip("Choose which zone/controllers this effect applies to");
    stack_effect_zone_combo->addItem("All Controllers", -1);
    connect(stack_effect_zone_combo, SIGNAL(currentIndexChanged(int)),
            this, SLOT(on_stack_effect_zone_changed(int)));
    zone_layout->addWidget(stack_effect_zone_combo);
    settings_layout->addLayout(zone_layout);

    // Blend Mode
    QHBoxLayout* blend_layout = new QHBoxLayout();
    blend_layout->addWidget(new QLabel("Blend Mode:"));
    stack_effect_blend_combo = new QComboBox();

    // Add blend modes with tooltips
    stack_effect_blend_combo->addItem("No Blend", (int)BlendMode::NO_BLEND);
    stack_effect_blend_combo->setItemData(0, "Effect runs independently without combining with other effects", Qt::ToolTipRole);

    stack_effect_blend_combo->addItem("Replace", (int)BlendMode::REPLACE);
    stack_effect_blend_combo->setItemData(1, "Completely replaces colors from previous effects (last effect wins)", Qt::ToolTipRole);

    stack_effect_blend_combo->addItem("Add", (int)BlendMode::ADD);
    stack_effect_blend_combo->setItemData(2, "Adds colors together (brightens - good for combining lights)", Qt::ToolTipRole);

    stack_effect_blend_combo->addItem("Multiply", (int)BlendMode::MULTIPLY);
    stack_effect_blend_combo->setItemData(3, "Multiplies colors (darkens - good for shadows/filters)", Qt::ToolTipRole);

    stack_effect_blend_combo->addItem("Screen", (int)BlendMode::SCREEN);
    stack_effect_blend_combo->setItemData(4, "Screen blend (brightens without overexposure - softer than Add)", Qt::ToolTipRole);

    stack_effect_blend_combo->addItem("Max", (int)BlendMode::MAX);
    stack_effect_blend_combo->setItemData(5, "Takes the brightest color channel from each effect", Qt::ToolTipRole);

    stack_effect_blend_combo->addItem("Min", (int)BlendMode::MIN);
    stack_effect_blend_combo->setItemData(6, "Takes the darkest color channel from each effect", Qt::ToolTipRole);

    stack_effect_blend_combo->setCurrentIndex(0); // Default to No Blend

    // Set tooltip for the combo box itself
    stack_effect_blend_combo->setToolTip("How this effect combines with other effects in the stack");

    connect(stack_effect_blend_combo, SIGNAL(currentIndexChanged(int)),
            this, SLOT(on_stack_effect_blend_changed(int)));
    blend_layout->addWidget(stack_effect_blend_combo);
    settings_layout->addLayout(blend_layout);

    // Effect-specific controls container
    stack_effect_controls_container = new QWidget();
    stack_effect_controls_layout = new QVBoxLayout(stack_effect_controls_container);
    stack_effect_controls_layout->setContentsMargins(0, 0, 0, 0);
    settings_layout->addWidget(stack_effect_controls_container);

    stack_layout->addWidget(settings_group);

    /*---------------------------------------------------------*\
    | Stack Presets Section                                    |
    \*---------------------------------------------------------*/
    QLabel* presets_label = new QLabel("Saved Stack Presets:");
    presets_label->setStyleSheet("font-weight: bold; margin-top: 10px;");
    stack_layout->addWidget(presets_label);

    stack_presets_list = new QListWidget();
    stack_presets_list->setSelectionMode(QAbstractItemView::SingleSelection);
    stack_presets_list->setMinimumHeight(100);
    stack_layout->addWidget(stack_presets_list);

    QHBoxLayout* presets_button_layout = new QHBoxLayout();
    presets_button_layout->addStretch();

    QPushButton* save_stack_btn = new QPushButton("Save Stack As...");
    connect(save_stack_btn, &QPushButton::clicked,
            this, &OpenRGB3DSpatialTab::on_save_stack_preset_clicked);
    presets_button_layout->addWidget(save_stack_btn);

    QPushButton* load_stack_btn = new QPushButton("Load");
    connect(load_stack_btn, &QPushButton::clicked,
            this, &OpenRGB3DSpatialTab::on_load_stack_preset_clicked);
    presets_button_layout->addWidget(load_stack_btn);

    QPushButton* delete_stack_btn = new QPushButton("Delete");
    connect(delete_stack_btn, &QPushButton::clicked,
            this, &OpenRGB3DSpatialTab::on_delete_stack_preset_clicked);
    presets_button_layout->addWidget(delete_stack_btn);

    stack_layout->addLayout(presets_button_layout);
    stack_layout->addStretch();

    // Populate zone combo with current zones
    UpdateStackEffectZoneCombo();

    // Load saved presets
    LoadStackPresets();

    // Add tab to specified tab widget
    tab_widget->addTab(stack_tab, "Effect Stack");
}

void OpenRGB3DSpatialTab::on_add_effect_to_stack_clicked()
{

    if(!stack_effect_type_combo || stack_effect_type_combo->count() <= 1)
    {
        LOG_ERROR("[OpenRGB3DSpatialPlugin] Cannot add effect - no effects available");
        return;
    }

    // Check what's currently selected
    int current_index = stack_effect_type_combo->currentIndex();

    // If "None" is selected (index 0), refuse to add
    if(current_index == 0)
    {
        QMessageBox::information(this, "No Effect Selected",
                                "Please select an effect type before adding to the stack.");
        return;
    }

    QString class_name = stack_effect_type_combo->itemData(current_index).toString();
    QString ui_name = stack_effect_type_combo->itemText(current_index);

    if(class_name.isEmpty())
    {
        LOG_ERROR("[OpenRGB3DSpatialPlugin] Cannot add effect - effect class name is empty");
        return;
    }

    // Create new effect instance
    auto instance = std::make_unique<EffectInstance3D>();
    instance->id = next_effect_instance_id++;
    instance->name = ui_name.toStdString();
    instance->zone_index = -1; // All Controllers
    instance->blend_mode = BlendMode::NO_BLEND;
    instance->enabled = true;

    // Store the effect class name (effect will be created lazily when selected)
    instance->effect_class_name = class_name.toStdString();

    // Add to list
    effect_stack.push_back(std::move(instance));

    // Update UI
    UpdateEffectStackList();

    // Select the new effect
    effect_stack_list->setCurrentRow((int)effect_stack.size() - 1);

    // Start effect timer if not already running
    if(effect_timer && !effect_timer->isActive())
    {
        effect_timer->start(33); // ~30 FPS
    }
}

void OpenRGB3DSpatialTab::on_remove_effect_from_stack_clicked()
{
    int current_row = effect_stack_list->currentRow();

    if(current_row < 0 || current_row >= (int)effect_stack.size())
    {
        QMessageBox::information(this, "No Effect Selected",
                                "Please select an effect to remove from the stack.");
        return;
    }

    // Remove from vector
    effect_stack.erase(effect_stack.begin() + current_row);

    // Update UI
    UpdateEffectStackList();

    // Select next item (or previous if we removed the last one)
    if(!effect_stack.empty())
    {
        int new_row = std::min(current_row, (int)effect_stack.size() - 1);
        effect_stack_list->setCurrentRow(new_row);
    }
}

void OpenRGB3DSpatialTab::on_effect_stack_item_double_clicked(QListWidgetItem*)
{
    int current_row = effect_stack_list->currentRow();

    if(current_row < 0 || current_row >= (int)effect_stack.size())
    {
        return;
    }

    // Toggle enabled state
    EffectInstance3D* instance = effect_stack[current_row].get();
    instance->enabled = !instance->enabled;

    // Update list display
    UpdateEffectStackList();

    // Restore selection
    effect_stack_list->setCurrentRow(current_row);
}

void OpenRGB3DSpatialTab::on_effect_stack_selection_changed(int index)
{
    if(index < 0 || index >= (int)effect_stack.size())
    {
        // No selection - disable controls
        stack_effect_type_combo->setEnabled(false);
        stack_effect_zone_combo->setEnabled(false);
        stack_effect_blend_combo->setEnabled(false);
        return;
    }

    // Enable controls
    stack_effect_type_combo->setEnabled(true);
    stack_effect_zone_combo->setEnabled(true);
    stack_effect_blend_combo->setEnabled(true);

    // Load effect instance settings
    EffectInstance3D* instance = effect_stack[index].get();

    // Block signals while updating UI to prevent infinite recursion
    stack_effect_type_combo->blockSignals(true);
    stack_effect_zone_combo->blockSignals(true);
    stack_effect_blend_combo->blockSignals(true);

    // Set effect type
    if(!instance->effect_class_name.empty())
    {
        QString class_name = QString::fromStdString(instance->effect_class_name);
        int type_index = stack_effect_type_combo->findData(class_name);
        if(type_index >= 0)
        {
            stack_effect_type_combo->setCurrentIndex(type_index);
        }
    }
    else
    {
        // No effect - select "None"
        stack_effect_type_combo->setCurrentIndex(0);
    }

    // Set zone
    UpdateStackEffectZoneCombo();
    int zone_index = stack_effect_zone_combo->findData(instance->zone_index);
    if(zone_index >= 0)
    {
        stack_effect_zone_combo->setCurrentIndex(zone_index);
    }

    // Set blend mode
    int blend_index = stack_effect_blend_combo->findData((int)instance->blend_mode);
    if(blend_index >= 0)
    {
        stack_effect_blend_combo->setCurrentIndex(blend_index);
    }

    // Re-enable signals
    stack_effect_type_combo->blockSignals(false);
    stack_effect_zone_combo->blockSignals(false);
    stack_effect_blend_combo->blockSignals(false);

    // Load effect-specific controls
    LoadStackEffectControls(instance);
}

void OpenRGB3DSpatialTab::on_stack_effect_type_changed(int)
{
    int current_row = effect_stack_list->currentRow();
    if(current_row < 0 || current_row >= (int)effect_stack.size())
        return;

    EffectInstance3D* instance = effect_stack[current_row].get();

    // Get selected effect type
    QString class_name = stack_effect_type_combo->currentData().toString();
    QString ui_name = stack_effect_type_combo->currentText();

    // If "None" is selected, clear the effect
    if(class_name.isEmpty())
    {
        instance->effect.reset();
        instance->effect_class_name = "";
        instance->name = "None";

        // Update list display
        UpdateEffectStackList();

        // Clear effect controls
        LoadStackEffectControls(instance);
        return;
    }

    // Clear old effect and store new class name (effect will be created lazily)
    instance->effect.reset();
    instance->effect_class_name = class_name.toStdString();
    instance->name = ui_name.toStdString();

    // Update list display
    UpdateEffectStackList();

    // Reload effect controls (will create effect if needed)
    LoadStackEffectControls(instance);
}

void OpenRGB3DSpatialTab::on_stack_effect_zone_changed(int)
{
    int current_row = effect_stack_list->currentRow();
    if(current_row < 0 || current_row >= (int)effect_stack.size())
        return;

    EffectInstance3D* instance = effect_stack[current_row].get();
    instance->zone_index = stack_effect_zone_combo->currentData().toInt();

    // Update list display
    UpdateEffectStackList();
}

void OpenRGB3DSpatialTab::on_stack_effect_blend_changed(int)
{
    int current_row = effect_stack_list->currentRow();
    if(current_row < 0 || current_row >= (int)effect_stack.size())
        return;

    EffectInstance3D* instance = effect_stack[current_row].get();
    instance->blend_mode = (BlendMode)stack_effect_blend_combo->currentData().toInt();

    // Update list display
    UpdateEffectStackList();
}

void OpenRGB3DSpatialTab::UpdateEffectStackList()
{
    // Save current selection
    int current_row = effect_stack_list->currentRow();

    // Block signals to prevent selection change during clear/rebuild
    effect_stack_list->blockSignals(true);
    effect_stack_list->clear();

    for(size_t i = 0; i < effect_stack.size(); i++)
    {
        EffectInstance3D* instance = effect_stack[i].get();

        QString enabled_marker = instance->enabled ? "☑ " : "☐ ";
        QString display_name = QString::fromStdString(instance->GetDisplayName());

        QListWidgetItem* item = new QListWidgetItem(enabled_marker + display_name);
        effect_stack_list->addItem(item);
    }

    // Restore selection
    if(current_row >= 0 && current_row < (int)effect_stack.size())
    {
        effect_stack_list->setCurrentRow(current_row);
    }

    effect_stack_list->blockSignals(false);
}

void OpenRGB3DSpatialTab::UpdateStackEffectZoneCombo()
{
    if(!stack_effect_zone_combo)
        return;

    stack_effect_zone_combo->clear();
    stack_effect_zone_combo->addItem("All Controllers", -1);

    if(zone_manager)
    {
        for(int i = 0; i < zone_manager->GetZoneCount(); i++)
        {
            Zone3D* zone = zone_manager->GetZone(i);
            if(zone)
            {
                stack_effect_zone_combo->addItem(QString::fromStdString(zone->GetName()), i);
            }
        }
    }
}

void OpenRGB3DSpatialTab::LoadStackEffectControls(EffectInstance3D* instance)
{
    // Clear existing controls
    QLayoutItem* item;
    while((item = stack_effect_controls_layout->takeAt(0)) != nullptr)
    {
        if(item->widget())
        {
            // Don't delete the widget - just remove it from layout
            // The effect is owned by the EffectInstance3D
            item->widget()->setParent(nullptr);
            item->widget()->hide();
        }
        delete item;
    }

    if(!instance)
    {
        return;
    }

    // Create effect if it doesn't exist yet
    if(!instance->effect && !instance->effect_class_name.empty())
    {
        SpatialEffect3D* effect = EffectListManager3D::get()->CreateEffect(instance->effect_class_name);
        if(!effect)
        {
            LOG_ERROR("[OpenRGB3DSpatialPlugin] Failed to create effect: %s", instance->effect_class_name.c_str());
            return;
        }
        instance->effect.reset(effect);
    }

    // If no effect (None selected), just return - controls are already cleared
    if(!instance->effect)
    {
        return;
    }

    // The effect itself is a QWidget
    SpatialEffect3D* effect = instance->effect.get();

    // If the effect hasn't been initialized yet, set it up
    if(!effect->parent())
    {
        effect->setParent(stack_effect_controls_container);
        effect->CreateCommonEffectControls(stack_effect_controls_container);
        effect->SetupCustomUI(stack_effect_controls_container);
    }

    // Show the effect and add to layout
    effect->show();
    stack_effect_controls_layout->addWidget(effect);
}
