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
#include <QMessageBox>

void OpenRGB3DSpatialTab::SetupEffectStackTab()
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

    effect_stack_list = new QListWidget();
    effect_stack_list->setSelectionMode(QAbstractItemView::SingleSelection);
    effect_stack_list->setMinimumHeight(150);
    connect(effect_stack_list, &QListWidget::currentRowChanged,
            this, &OpenRGB3DSpatialTab::on_effect_stack_selection_changed);
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
    stack_effect_zone_combo->addItem("All Controllers", -1);
    connect(stack_effect_zone_combo, SIGNAL(currentIndexChanged(int)),
            this, SLOT(on_stack_effect_zone_changed(int)));
    zone_layout->addWidget(stack_effect_zone_combo);
    settings_layout->addLayout(zone_layout);

    // Blend Mode
    QHBoxLayout* blend_layout = new QHBoxLayout();
    blend_layout->addWidget(new QLabel("Blend Mode:"));
    stack_effect_blend_combo = new QComboBox();
    stack_effect_blend_combo->addItem("Replace", (int)BlendMode::REPLACE);
    stack_effect_blend_combo->addItem("Add", (int)BlendMode::ADD);
    stack_effect_blend_combo->addItem("Multiply", (int)BlendMode::MULTIPLY);
    stack_effect_blend_combo->addItem("Screen", (int)BlendMode::SCREEN);
    stack_effect_blend_combo->addItem("Max", (int)BlendMode::MAX);
    stack_effect_blend_combo->addItem("Min", (int)BlendMode::MIN);
    stack_effect_blend_combo->setCurrentIndex(1); // Default to Add
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
    stack_layout->addStretch();

    // Add tab to main tab widget
    left_tabs->addTab(stack_tab, "Effect Stack");
}

void OpenRGB3DSpatialTab::on_add_effect_to_stack_clicked()
{
    // Create new effect instance
    auto instance = std::make_unique<EffectInstance3D>();
    instance->id = next_effect_instance_id++;
    instance->name = "New Effect";
    instance->zone_index = -1; // All Controllers
    instance->blend_mode = BlendMode::ADD;
    instance->enabled = true;

    // Create default effect (first in list)
    if(stack_effect_type_combo->count() > 0)
    {
        QString class_name = stack_effect_type_combo->itemData(0).toString();
        QString ui_name = stack_effect_type_combo->itemText(0);

        SpatialEffect3D* effect = EffectListManager3D::get()->CreateEffect(class_name.toStdString());
        if(effect)
        {
            instance->effect.reset(effect);
            instance->effect_class_name = class_name.toStdString();
            instance->name = ui_name.toStdString();
        }
    }

    // Add to list
    effect_stack.push_back(std::move(instance));

    // Update UI
    UpdateEffectStackList();

    // Select the new effect
    effect_stack_list->setCurrentRow(effect_stack.size() - 1);
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

    // Load effect-specific controls
    LoadStackEffectControls(instance);
}

void OpenRGB3DSpatialTab::on_stack_effect_type_changed(int)
{
    int current_row = effect_stack_list->currentRow();
    if(current_row < 0 || current_row >= (int)effect_stack.size())
        return;

    EffectInstance3D* instance = effect_stack[current_row].get();

    // Create new effect of selected type
    QString class_name = stack_effect_type_combo->currentData().toString();
    QString ui_name = stack_effect_type_combo->currentText();
    SpatialEffect3D* new_effect = EffectListManager3D::get()->CreateEffect(class_name.toStdString());

    if(new_effect)
    {
        instance->effect.reset(new_effect);
        instance->effect_class_name = class_name.toStdString();
        instance->name = ui_name.toStdString();

        // Update list display
        UpdateEffectStackList();
        effect_stack_list->setCurrentRow(current_row);

        // Reload effect controls
        LoadStackEffectControls(instance);
    }
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
    effect_stack_list->setCurrentRow(current_row);
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
    effect_stack_list->setCurrentRow(current_row);
}

void OpenRGB3DSpatialTab::UpdateEffectStackList()
{
    effect_stack_list->clear();

    for(size_t i = 0; i < effect_stack.size(); i++)
    {
        EffectInstance3D* instance = effect_stack[i].get();

        QString enabled_marker = instance->enabled ? "☑ " : "☐ ";
        QString display_name = QString::fromStdString(instance->GetDisplayName());

        QListWidgetItem* item = new QListWidgetItem(enabled_marker + display_name);
        effect_stack_list->addItem(item);
    }
}

void OpenRGB3DSpatialTab::UpdateStackEffectZoneCombo()
{
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
            item->widget()->deleteLater();
        }
        delete item;
    }

    if(!instance || !instance->effect)
        return;

    // The effect itself is a QWidget - set it up and add to container
    SpatialEffect3D* effect = instance->effect.get();
    effect->setParent(stack_effect_controls_container);
    effect->CreateCommonEffectControls(stack_effect_controls_container);
    effect->SetupCustomUI(stack_effect_controls_container);
    stack_effect_controls_layout->addWidget(effect);
}
