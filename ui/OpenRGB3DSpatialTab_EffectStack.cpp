// SPDX-License-Identifier: GPL-2.0-only


#include "OpenRGB3DSpatialTab.h"
#include "EffectListManager3D.h"
#include "Effects3D/ScreenMirror3D/ScreenMirror3D.h"
#include "LogManager.h"
#include <nlohmann/json.hpp>
#include <QMessageBox>
#include <QFont>
#include <QPalette>
#include <QGroupBox>
#include <QLabel>
#include <QSignalBlocker>
#include <QTabWidget>
#include <QAbstractItemView>
#include <QHBoxLayout>

void OpenRGB3DSpatialTab::SetupEffectStackPanel(QVBoxLayout* parent_layout)
{
    QGroupBox* stack_group = new QGroupBox("Effect Layers");
    stack_group->setFlat(true);
    QVBoxLayout* stack_layout = new QVBoxLayout(stack_group);
    stack_layout->setSpacing(6);
    stack_layout->setContentsMargins(0, 0, 0, 0);

    QTabWidget* stack_tabs = new QTabWidget();
    stack_tabs->setTabPosition(QTabWidget::North);
    stack_tabs->setDocumentMode(true);
    stack_tabs->setStyleSheet("QTabWidget::pane { border: 0; top: -1px; }");

    QWidget* active_tab = new QWidget();
    QVBoxLayout* active_layout = new QVBoxLayout(active_tab);

    QLabel* list_label = new QLabel("Active Effect Stack");
    QFont list_font = list_label->font();
    list_font.setBold(true);
    list_label->setFont(list_font);
    active_layout->addWidget(list_label);

    QLabel* hint_label = new QLabel("Use the Effect Library to add layers. Checkmark = enabled, X = disabled. Double-click to toggle.");
    hint_label->setForegroundRole(QPalette::PlaceholderText);
    active_layout->addWidget(hint_label);

    effect_stack_list = new QListWidget();
    effect_stack_list->setSelectionMode(QAbstractItemView::SingleSelection);
    effect_stack_list->setMinimumHeight(180);
    connect(effect_stack_list, &QListWidget::currentRowChanged,
            this, &OpenRGB3DSpatialTab::on_effect_stack_selection_changed);
    connect(effect_stack_list, &QListWidget::itemDoubleClicked,
            this, &OpenRGB3DSpatialTab::on_effect_stack_item_double_clicked);
    active_layout->addWidget(effect_stack_list);

    QHBoxLayout* button_layout = new QHBoxLayout();
    button_layout->addStretch();

    QPushButton* remove_effect_btn = new QPushButton("- Remove Effect");
    connect(remove_effect_btn, &QPushButton::clicked,
            this, &OpenRGB3DSpatialTab::on_remove_effect_from_stack_clicked);
    button_layout->addWidget(remove_effect_btn);

    active_layout->addLayout(button_layout);
    active_layout->addStretch();

    QWidget* presets_tab = new QWidget();
    QVBoxLayout* presets_layout = new QVBoxLayout(presets_tab);

    QLabel* presets_label = new QLabel("Saved Stack Presets");
    QFont presets_font = presets_label->font();
    presets_font.setBold(true);
    presets_label->setFont(presets_font);
    presets_layout->addWidget(presets_label);

    stack_presets_list = new QListWidget();
    stack_presets_list->setSelectionMode(QAbstractItemView::SingleSelection);
    stack_presets_list->setMinimumHeight(160);
    presets_layout->addWidget(stack_presets_list);

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

    presets_layout->addLayout(presets_button_layout);
    presets_layout->addStretch();

    stack_tabs->addTab(active_tab, "Active Stack");
    stack_tabs->addTab(presets_tab, "Stack Presets");

    stack_layout->addWidget(stack_tabs);
    parent_layout->addWidget(stack_group);

    UpdateStackEffectZoneCombo();
    LoadStackPresets();
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

    /*---------------------------------------------------------*\
    | Remove from vector                                       |
    \*---------------------------------------------------------*/
    effect_stack.erase(effect_stack.begin() + current_row);

    /*---------------------------------------------------------*\
    | Update UI                                                |
    \*---------------------------------------------------------*/
    UpdateEffectStackList();

    /*---------------------------------------------------------*\
    | Select next item (or previous if we removed the last one)|
    \*---------------------------------------------------------*/
    if(!effect_stack.empty())
    {
        int new_row = std::min(current_row, (int)effect_stack.size() - 1);
        effect_stack_list->setCurrentRow(new_row);
    }

    /*---------------------------------------------------------*\
    | Auto-save effect stack                                   |
    \*---------------------------------------------------------*/
    SaveEffectStack();
}

void OpenRGB3DSpatialTab::on_effect_stack_item_double_clicked(QListWidgetItem*)
{
    int current_row = effect_stack_list->currentRow();

    if(current_row < 0 || current_row >= (int)effect_stack.size())
    {
        return;
    }

    /*---------------------------------------------------------*\
    | Toggle enabled state                                     |
    \*---------------------------------------------------------*/
    EffectInstance3D* instance = effect_stack[current_row].get();
    instance->enabled = !instance->enabled;

    /*---------------------------------------------------------*\
    | Update list display                                      |
    \*---------------------------------------------------------*/
    UpdateEffectStackList();

    /*---------------------------------------------------------*\
    | Restore selection                                        |
    \*---------------------------------------------------------*/
    effect_stack_list->setCurrentRow(current_row);

    /*---------------------------------------------------------*\
    | Auto-save effect stack                                   |
    \*---------------------------------------------------------*/
    SaveEffectStack();
}

void OpenRGB3DSpatialTab::on_effect_stack_selection_changed(int index)
{
    if(index < 0 || index >= (int)effect_stack.size())
    {
        if(stack_effect_type_combo) stack_effect_type_combo->setEnabled(false);
        if(stack_effect_zone_combo) stack_effect_zone_combo->setEnabled(false);
        if(stack_effect_blend_combo) stack_effect_blend_combo->setEnabled(false);
        if(effect_zone_combo) effect_zone_combo->setEnabled(false);
        LoadStackEffectControls(nullptr);

        if(effect_combo)
        {
            QSignalBlocker combo_blocker(effect_combo);
            if(effect_combo->count() > 0)
            {
                effect_combo->setCurrentIndex(-1);
            }
        }

        UpdateAudioPanelVisibility(nullptr);

        if(start_effect_button)
        {
            disconnect(start_effect_button, nullptr, this, nullptr);
            start_effect_button = nullptr;
        }
        if(stop_effect_button)
        {
            disconnect(stop_effect_button, nullptr, this, nullptr);
            stop_effect_button = nullptr;
        }
        current_effect_ui = nullptr;
        UpdateEffectCombo();
        if(effect_zone_combo)
        {
            QSignalBlocker blocker(effect_zone_combo);
            effect_zone_combo->setCurrentIndex(0);
        }
        return;
    }

    if(stack_effect_type_combo) stack_effect_type_combo->setEnabled(true);
    if(stack_effect_zone_combo) stack_effect_zone_combo->setEnabled(true);
    if(stack_effect_blend_combo) stack_effect_blend_combo->setEnabled(true);
    if(effect_zone_combo) effect_zone_combo->setEnabled(true);

    EffectInstance3D* instance = effect_stack[index].get();

    QSignalBlocker type_blocker(stack_effect_type_combo ? stack_effect_type_combo : nullptr);
    QSignalBlocker zone_blocker(stack_effect_zone_combo ? stack_effect_zone_combo : nullptr);
    QSignalBlocker blend_blocker(stack_effect_blend_combo ? stack_effect_blend_combo : nullptr);

    if(!instance->effect_class_name.empty())
    {
        QString class_name = QString::fromStdString(instance->effect_class_name);
        int type_index = stack_effect_type_combo->findData(class_name);
        if(type_index >= 0)
        {
            stack_effect_type_combo->setCurrentIndex(type_index);
        }
        else
        {
            stack_effect_type_combo->setCurrentIndex(0);
        }
    }
    else
    {
        stack_effect_type_combo->setCurrentIndex(0);
    }

    UpdateStackEffectZoneCombo();
    int zone_index = stack_effect_zone_combo->findData(instance->zone_index);
    if(zone_index >= 0)
    {
        stack_effect_zone_combo->setCurrentIndex(zone_index);
    }
    else
    {
        stack_effect_zone_combo->setCurrentIndex(0);
    }

    if(stack_effect_blend_combo)
    {
        int blend_index = stack_effect_blend_combo->findData((int)instance->blend_mode);
        if(blend_index >= 0)
        {
            stack_effect_blend_combo->setCurrentIndex(blend_index);
        }
        else
        {
            stack_effect_blend_combo->setCurrentIndex(0);
        }
    }

    LoadStackEffectControls(instance);
    UpdateAudioPanelVisibility(instance);

    if(effect_zone_combo)
    {
        QSignalBlocker blocker(effect_zone_combo);
        int zone_combo_index = effect_zone_combo->findData(instance->zone_index);
        if(zone_combo_index >= 0)
        {
            effect_zone_combo->setCurrentIndex(zone_combo_index);
        }
    }

    if(effect_combo)
    {
        QSignalBlocker combo_blocker(effect_combo);
        if(index >= 0 && index < effect_combo->count())
        {
            effect_combo->setCurrentIndex(index);
        }
    }

    UpdateEffectCombo();
}

void OpenRGB3DSpatialTab::on_stack_effect_type_changed(int)
{
    

    int current_row = effect_stack_list->currentRow();
    if(current_row < 0 || current_row >= (int)effect_stack.size())
    {
        
        return;
    }

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

        /*---------------------------------------------------------*\
        | Update list display                                      |
        \*---------------------------------------------------------*/
        UpdateEffectStackList();

        /*---------------------------------------------------------*\
        | Clear effect controls                                    |
        \*---------------------------------------------------------*/
        LoadStackEffectControls(instance);

        /*---------------------------------------------------------*\
        | Auto-save effect stack                                   |
        \*---------------------------------------------------------*/
        SaveEffectStack();
        return;
    }

    /*---------------------------------------------------------*\
    | Clear old effect and store new class name                |
    \*---------------------------------------------------------*/
    
    instance->effect.reset();
    instance->effect_class_name = class_name.toStdString();
    instance->name              = ui_name.toStdString();

    /*---------------------------------------------------------*\
    | Update list display                                      |
    \*---------------------------------------------------------*/
    UpdateEffectStackList();

    /*---------------------------------------------------------*\
    | Reload effect controls (will create effect if needed)    |
    \*---------------------------------------------------------*/
    LoadStackEffectControls(instance);

    /*---------------------------------------------------------*\
    | Auto-save effect stack                                   |
    \*---------------------------------------------------------*/
    SaveEffectStack();
}

void OpenRGB3DSpatialTab::on_stack_effect_zone_changed(int)
{
    int current_row = effect_stack_list->currentRow();
    if(current_row < 0 || current_row >= (int)effect_stack.size())
    {
        return;
    }

    EffectInstance3D* instance = effect_stack[current_row].get();
    instance->zone_index = stack_effect_zone_combo->currentData().toInt();

    /*---------------------------------------------------------*\
    | Update list display                                      |
    \*---------------------------------------------------------*/
    UpdateEffectStackList();

    /*---------------------------------------------------------*\
    | Auto-save effect stack                                   |
    \*---------------------------------------------------------*/
    SaveEffectStack();
}

void OpenRGB3DSpatialTab::on_stack_effect_blend_changed(int)
{
    if(!stack_effect_blend_combo)
    {
        return;
    }

    int current_row = effect_stack_list->currentRow();
    if(current_row < 0 || current_row >= (int)effect_stack.size())
    {
        return;
    }

    EffectInstance3D* instance = effect_stack[current_row].get();
    instance->blend_mode = (BlendMode)stack_effect_blend_combo->currentData().toInt();

    /*---------------------------------------------------------*\
    | Update list display                                      |
    \*---------------------------------------------------------*/
    UpdateEffectStackList();

    /*---------------------------------------------------------*\
    | Auto-save effect stack                                   |
    \*---------------------------------------------------------*/
    SaveEffectStack();
}

void OpenRGB3DSpatialTab::UpdateEffectStackList()
{
    /*---------------------------------------------------------*\
    | Save current selection                                   |
    \*---------------------------------------------------------*/
    int current_row = effect_stack_list->currentRow();

    /*---------------------------------------------------------*\
    | Block signals to prevent selection change                |
    \*---------------------------------------------------------*/
    effect_stack_list->blockSignals(true);
    effect_stack_list->clear();

    for(unsigned int i = 0; i < effect_stack.size(); i++)
    {
        EffectInstance3D* instance = effect_stack[i].get();

        QString enabled_marker = instance->enabled ? "[ON] " : "[OFF] ";
        QString display_name   = QString::fromStdString(instance->GetDisplayName());

        QListWidgetItem* item = new QListWidgetItem(enabled_marker + display_name);
        effect_stack_list->addItem(item);
    }

    /*---------------------------------------------------------*\
    | Restore selection                                        |
    \*---------------------------------------------------------*/
    if(current_row >= 0 && current_row < (int)effect_stack.size())
    {
        effect_stack_list->setCurrentRow(current_row);
    }

    effect_stack_list->blockSignals(false);

    UpdateEffectCombo();
}

void OpenRGB3DSpatialTab::UpdateStackEffectZoneCombo()
{
    PopulateZoneTargetCombo(stack_effect_zone_combo, ResolveZoneTargetSelection(stack_effect_zone_combo));
}

void OpenRGB3DSpatialTab::LoadStackEffectControls(EffectInstance3D* instance)
{
    if(current_effect_ui)
    {
        disconnect(current_effect_ui, nullptr, this, nullptr);
        current_effect_ui = nullptr;
    }
    if(start_effect_button)
    {
        disconnect(start_effect_button, nullptr, this, nullptr);
        start_effect_button = nullptr;
    }
    if(stop_effect_button)
    {
        disconnect(stop_effect_button, nullptr, this, nullptr);
        stop_effect_button = nullptr;
    }
    QLayoutItem* layout_item;
    while(effect_controls_layout && (layout_item = effect_controls_layout->takeAt(0)) != nullptr)
    {
        if(layout_item->widget())
        {
            layout_item->widget()->deleteLater();
        }
        delete layout_item;
    }
    stack_effect_blend_combo = nullptr;
    stack_blend_container = nullptr;

    if(!instance)
    {
        return;
    }

    if(instance->effect_class_name.empty())
    {
        return;
    }

    if(!instance->effect)
    {
        SpatialEffect3D* effect = EffectListManager3D::get()->CreateEffect(instance->effect_class_name);
        if(!effect)
        {
            LOG_ERROR("[OpenRGB3DSpatialPlugin] Failed to create effect: %s", instance->effect_class_name.c_str());
            ClearCustomEffectUI();
            return;
        }
        instance->effect.reset(effect);

        if(instance->effect_class_name == "ScreenMirror3D")
        {
            ScreenMirror3D* screen_mirror = dynamic_cast<ScreenMirror3D*>(effect);
            if(screen_mirror)
            {
                connect(screen_mirror, &ScreenMirror3D::ScreenPreviewChanged,
                        viewport, &LEDViewport3D::SetShowScreenPreview);
                screen_mirror->SetReferencePoints(&reference_points);
            }
        }

        if(instance->saved_settings && !instance->saved_settings->empty())
        {
            effect->LoadSettings(*instance->saved_settings);
        }
    }

    if(instance->effect)
    {
        if(!instance->saved_settings || instance->saved_settings->empty())
        {
            nlohmann::json current_settings = instance->effect->SaveSettings();
            instance->saved_settings = std::make_unique<nlohmann::json>(current_settings);
        }
    }

    DisplayEffectInstanceDetails(instance);
}

void OpenRGB3DSpatialTab::DisplayEffectInstanceDetails(EffectInstance3D* instance)
{
    ClearCustomEffectUI();

    if(!instance || !effect_controls_widget || !effect_controls_layout)
    {
        return;
    }

    if(instance->effect_class_name.empty())
    {
        return;
    }

    SpatialEffect3D* ui_effect = EffectListManager3D::get()->CreateEffect(instance->effect_class_name);
    if(!ui_effect)
    {
        LOG_ERROR("[OpenRGB3DSpatialPlugin] Failed to create UI effect for class: %s", instance->effect_class_name.c_str());
        return;
    }

    ui_effect->setParent(effect_controls_widget);
    ui_effect->CreateCommonEffectControls(effect_controls_widget);
    ui_effect->SetupCustomUI(effect_controls_widget);
    current_effect_ui = ui_effect;

    // Set reference points for ScreenMirror3D UI effect
    if(instance->effect_class_name == "ScreenMirror3D")
    {
        ScreenMirror3D* screen_mirror = dynamic_cast<ScreenMirror3D*>(ui_effect);
        if(screen_mirror)
        {
            screen_mirror->SetReferencePoints(&reference_points);
            connect(this, &OpenRGB3DSpatialTab::GridLayoutChanged, screen_mirror, &ScreenMirror3D::RefreshMonitorStatus);
            QTimer::singleShot(200, screen_mirror, &ScreenMirror3D::RefreshMonitorStatus);
            QTimer::singleShot(300, screen_mirror, &ScreenMirror3D::RefreshReferencePointDropdowns);
        }
        
        if(this->origin_label) this->origin_label->setVisible(false);
        if(this->effect_origin_combo) this->effect_origin_combo->setVisible(false);
    }
    else
    {
        if(this->origin_label) this->origin_label->setVisible(true);
        if(this->effect_origin_combo) this->effect_origin_combo->setVisible(true);
    }

    nlohmann::json settings;
    if(instance->saved_settings && !instance->saved_settings->empty())
    {
        settings = *instance->saved_settings;
    }
    else if(instance->effect)
    {
        settings = instance->effect->SaveSettings();
    }

    if(!settings.is_null())
    {
        ui_effect->LoadSettings(settings);
    }

    QPushButton* ui_start = ui_effect->GetStartButton();
    QPushButton* ui_stop  = ui_effect->GetStopButton();

    if(ui_start)
    {
        connect(ui_start, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_start_effect_clicked);
    }
    if(ui_stop)
    {
        connect(ui_stop, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_stop_effect_clicked);
    }

    start_effect_button = ui_start;
    stop_effect_button  = ui_stop;

    if(start_effect_button)
    {
        start_effect_button->setEnabled(!effect_running);
    }
    if(stop_effect_button)
    {
        stop_effect_button->setEnabled(effect_running);
    }

    SpatialEffect3D* captured_ui = ui_effect;
    connect(ui_effect, &SpatialEffect3D::ParametersChanged, this,
            [this, instance, captured_ui]()
            {
                if(!instance || !captured_ui)
                {
                    return;
                }

                if(stack_settings_updating)
                {
                    return;
                }

                stack_settings_updating = true;

                nlohmann::json updated = captured_ui->SaveSettings();
                instance->saved_settings = std::make_unique<nlohmann::json>(updated);
                if(instance->effect)
                {
                    instance->effect->LoadSettings(updated);
                }
                SaveEffectStack();
                // Keep preview + hardware in sync with UI changes.
                // When effects are running, re-render immediately so the viewport
                // and real LEDs update together, instead of waiting for the next timer tick.
                if(effect_running)
                {
                    RenderEffectStack();
                }
                else if(viewport)
                {
                    viewport->UpdateColors();
                }

                stack_settings_updating = false;
            });

    effect_controls_layout->addWidget(ui_effect);

    stack_blend_container = new QWidget(effect_controls_widget);
    QHBoxLayout* blend_layout = new QHBoxLayout(stack_blend_container);
    blend_layout->setContentsMargins(0, 6, 0, 0);
    QLabel* blend_label = new QLabel("Stack Blend Mode:", stack_blend_container);
    blend_layout->addWidget(blend_label);

    stack_effect_blend_combo = new QComboBox(stack_blend_container);
    stack_effect_blend_combo->setToolTip("How this effect combines with other layers.");
    stack_effect_blend_combo->addItem("No Blend", (int)BlendMode::NO_BLEND);
    stack_effect_blend_combo->setItemData(0, "Effect runs independently without combining with other effects", Qt::ToolTipRole);
    stack_effect_blend_combo->addItem("Replace", (int)BlendMode::REPLACE);
    stack_effect_blend_combo->setItemData(1, "Completely replaces colors from previous effects (last effect wins)", Qt::ToolTipRole);
    stack_effect_blend_combo->addItem("Add", (int)BlendMode::ADD);
    stack_effect_blend_combo->setItemData(2, "Adds colors together (brightens)", Qt::ToolTipRole);
    stack_effect_blend_combo->addItem("Multiply", (int)BlendMode::MULTIPLY);
    stack_effect_blend_combo->setItemData(3, "Multiplies colors (darkens)", Qt::ToolTipRole);
    stack_effect_blend_combo->addItem("Screen", (int)BlendMode::SCREEN);
    stack_effect_blend_combo->setItemData(4, "Screen blend (brightens without overexposure)", Qt::ToolTipRole);
    stack_effect_blend_combo->addItem("Max", (int)BlendMode::MAX);
    stack_effect_blend_combo->setItemData(5, "Takes the brightest channel from previous effects", Qt::ToolTipRole);
    stack_effect_blend_combo->addItem("Min", (int)BlendMode::MIN);
    stack_effect_blend_combo->setItemData(6, "Takes the darkest channel from previous effects", Qt::ToolTipRole);
    connect(stack_effect_blend_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &OpenRGB3DSpatialTab::on_stack_effect_blend_changed);
    blend_layout->addWidget(stack_effect_blend_combo);

    effect_controls_layout->addWidget(stack_blend_container);

    effect_controls_widget->updateGeometry();
    effect_controls_widget->update();

    int current_blend_index = stack_effect_blend_combo->findData((int)instance->blend_mode);
    if(current_blend_index < 0)
    {
        current_blend_index = 0;
    }
    stack_effect_blend_combo->blockSignals(true);
    stack_effect_blend_combo->setCurrentIndex(current_blend_index);
    stack_effect_blend_combo->blockSignals(false);
}
