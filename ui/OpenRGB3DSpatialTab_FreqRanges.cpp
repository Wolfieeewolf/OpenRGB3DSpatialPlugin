/*---------------------------------------------------------*\
| OpenRGB3DSpatialTab_FreqRanges.cpp                        |
|                                                           |
|   Frequency Range Effects UI and rendering                |
|   implementation for OpenRGB 3D Spatial Plugin            |
|                                                           |
|   Multi-band audio effects system allowing independent    |
|   effects for different frequency ranges                  |
|                                                           |
|   Date: 2026-01-27                                        |
|                                                           |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#include "OpenRGB3DSpatialTab.h"
#include "Audio/AudioInputManager.h"
#include "SettingsManager.h"
#include "LogManager.h"
#include <nlohmann/json.hpp>
#include <QMessageBox>
#include <QInputDialog>
#include <QFile>
#include <QTextStream>
#include <QGroupBox>
#include <QFormLayout>

void OpenRGB3DSpatialTab::SetupFrequencyRangeEffectsUI(QVBoxLayout* parent_layout)
{
    freq_ranges_group = new QGroupBox("Frequency Range Effects");
    QVBoxLayout* freq_layout = new QVBoxLayout(freq_ranges_group);
    
    QLabel* header = new QLabel("Multi-Band Audio Effects");
    header->setStyleSheet("font-weight: bold;");
    freq_layout->addWidget(header);
    
    QLabel* description = new QLabel("Configure independent audio-reactive effects for specific frequency ranges (e.g., bass floor, treble ceiling).");
    description->setWordWrap(true);
    description->setStyleSheet("color: gray; font-size: 10px;");
    freq_layout->addWidget(description);
    
    QLabel* ranges_label = new QLabel("Audio Frequency Ranges:");
    ranges_label->setStyleSheet("font-weight: bold;");
    freq_layout->addWidget(ranges_label);
    
    freq_ranges_list = new QListWidget();
    freq_ranges_list->setMinimumHeight(120);
    freq_ranges_list->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(freq_ranges_list, &QListWidget::currentRowChanged,
            this, &OpenRGB3DSpatialTab::on_freq_range_selected);
    freq_layout->addWidget(freq_ranges_list);
    
    QHBoxLayout* range_buttons = new QHBoxLayout();
    add_freq_range_btn = new QPushButton("Add Range");
    remove_freq_range_btn = new QPushButton("Remove Selected");
    duplicate_freq_range_btn = new QPushButton("Duplicate");
    
    connect(add_freq_range_btn, &QPushButton::clicked,
            this, &OpenRGB3DSpatialTab::on_add_freq_range_clicked);
    connect(remove_freq_range_btn, &QPushButton::clicked,
            this, &OpenRGB3DSpatialTab::on_remove_freq_range_clicked);
    connect(duplicate_freq_range_btn, &QPushButton::clicked,
            this, &OpenRGB3DSpatialTab::on_duplicate_freq_range_clicked);
    
    range_buttons->addWidget(add_freq_range_btn);
    range_buttons->addWidget(remove_freq_range_btn);
    range_buttons->addWidget(duplicate_freq_range_btn);
    range_buttons->addStretch();
    freq_layout->addLayout(range_buttons);
    
    freq_range_details = new QWidget();
    QVBoxLayout* details_layout = new QVBoxLayout(freq_range_details);
    details_layout->setContentsMargins(0, 8, 0, 0);
    
    QHBoxLayout* name_row = new QHBoxLayout();
    name_row->addWidget(new QLabel("Name:"));
    freq_range_name_edit = new QLineEdit();
    connect(freq_range_name_edit, &QLineEdit::textChanged,
            this, &OpenRGB3DSpatialTab::on_freq_range_name_changed);
    name_row->addWidget(freq_range_name_edit, 1);
    
    freq_range_enabled_check = new QCheckBox("Enabled");
    freq_range_enabled_check->setChecked(true);
    connect(freq_range_enabled_check, &QCheckBox::toggled,
            this, &OpenRGB3DSpatialTab::on_freq_enabled_toggled);
    name_row->addWidget(freq_range_enabled_check);
    details_layout->addLayout(name_row);
    
    QGroupBox* freq_group = new QGroupBox("Frequency Range");
    QVBoxLayout* freq_sliders = new QVBoxLayout(freq_group);
    
    QHBoxLayout* low_row = new QHBoxLayout();
    low_row->addWidget(new QLabel("Low Hz:"));
    freq_low_spin = new QSpinBox();
    freq_low_spin->setRange(20, 20000);
    freq_low_spin->setValue(20);
    connect(freq_low_spin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &OpenRGB3DSpatialTab::on_freq_low_changed);
    low_row->addWidget(freq_low_spin);
    
    freq_low_slider = new QSlider(Qt::Horizontal);
    freq_low_slider->setRange(20, 20000);
    freq_low_slider->setValue(20);
    connect(freq_low_slider, &QSlider::valueChanged,
            freq_low_spin, &QSpinBox::setValue);
    low_row->addWidget(freq_low_slider, 1);
    freq_sliders->addLayout(low_row);
    
    QHBoxLayout* high_row = new QHBoxLayout();
    high_row->addWidget(new QLabel("High Hz:"));
    freq_high_spin = new QSpinBox();
    freq_high_spin->setRange(20, 20000);
    freq_high_spin->setValue(200);
    connect(freq_high_spin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &OpenRGB3DSpatialTab::on_freq_high_changed);
    high_row->addWidget(freq_high_spin);
    
    freq_high_slider = new QSlider(Qt::Horizontal);
    freq_high_slider->setRange(20, 20000);
    freq_high_slider->setValue(200);
    connect(freq_high_slider, &QSlider::valueChanged,
            freq_high_spin, &QSpinBox::setValue);
    high_row->addWidget(freq_high_slider, 1);
    freq_sliders->addLayout(high_row);
    
    details_layout->addWidget(freq_group);
    
    QHBoxLayout* effect_row = new QHBoxLayout();
    effect_row->addWidget(new QLabel("Effect:"));
    freq_effect_combo = new QComboBox();
    PopulateFreqEffectCombo(freq_effect_combo);
    connect(freq_effect_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &OpenRGB3DSpatialTab::on_freq_effect_changed);
    effect_row->addWidget(freq_effect_combo, 1);
    details_layout->addLayout(effect_row);
    
    QHBoxLayout* zone_row = new QHBoxLayout();
    zone_row->addWidget(new QLabel("Zone:"));
    freq_zone_combo = new QComboBox();
    UpdateFreqZoneCombo();
    connect(freq_zone_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &OpenRGB3DSpatialTab::on_freq_zone_changed);
    zone_row->addWidget(freq_zone_combo, 1);
    details_layout->addLayout(zone_row);
    
    QHBoxLayout* scale_row = new QHBoxLayout();
    scale_row->addWidget(new QLabel("Scale:"));
    freq_scale_x_spin = new QDoubleSpinBox();
    freq_scale_x_spin->setRange(0.1, 10.0);
    freq_scale_x_spin->setDecimals(2);
    freq_scale_x_spin->setSingleStep(0.1);
    freq_scale_x_spin->setValue(1.0);
    freq_scale_x_spin->setSuffix("x");
    connect(freq_scale_x_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double) { on_freq_scale_changed(); });
    scale_row->addWidget(freq_scale_x_spin);
    scale_row->addStretch();
    details_layout->addLayout(scale_row);
    
    QGroupBox* audio_proc_group = new QGroupBox("Audio Processing");
    QGridLayout* proc_grid = new QGridLayout(audio_proc_group);
    
    proc_grid->addWidget(new QLabel("Smoothing:"), 0, 0);
    freq_smoothing_slider = new QSlider(Qt::Horizontal);
    freq_smoothing_slider->setRange(0, 99);
    freq_smoothing_slider->setValue(70);
    connect(freq_smoothing_slider, &QSlider::valueChanged,
            this, &OpenRGB3DSpatialTab::on_freq_smoothing_changed);
    proc_grid->addWidget(freq_smoothing_slider, 0, 1);
    freq_smoothing_label = new QLabel("70%");
    freq_smoothing_label->setMinimumWidth(50);
    freq_smoothing_label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    proc_grid->addWidget(freq_smoothing_label, 0, 2);
    
    proc_grid->addWidget(new QLabel("Sensitivity:"), 1, 0);
    freq_sensitivity_slider = new QSlider(Qt::Horizontal);
    freq_sensitivity_slider->setRange(1, 100);  // Maps to 0.1..10.0
    freq_sensitivity_slider->setValue(10);      // 1.0x
    connect(freq_sensitivity_slider, &QSlider::valueChanged,
            this, &OpenRGB3DSpatialTab::on_freq_sensitivity_changed);
    proc_grid->addWidget(freq_sensitivity_slider, 1, 1);
    freq_sensitivity_label = new QLabel("1.0x");
    freq_sensitivity_label->setMinimumWidth(50);
    freq_sensitivity_label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    proc_grid->addWidget(freq_sensitivity_label, 1, 2);
    
    details_layout->addWidget(audio_proc_group);
    
    freq_effect_settings_widget = new QWidget();
    freq_effect_settings_layout = new QVBoxLayout(freq_effect_settings_widget);
    freq_effect_settings_layout->setContentsMargins(0, 0, 0, 0);
    details_layout->addWidget(freq_effect_settings_widget);
    
    details_layout->addStretch();
    
    freq_layout->addWidget(freq_range_details);
    freq_range_details->setVisible(false);  // Hidden until range selected
    
    parent_layout->addWidget(freq_ranges_group);
    
    LoadFrequencyRanges();
    UpdateFrequencyRangesList();
}

void OpenRGB3DSpatialTab::PopulateFreqEffectCombo(QComboBox* combo)
{
    if(!combo) return;
    
    combo->clear();
    combo->addItem("None");
    combo->setItemData(0, QVariant(), kEffectRoleClassName);
    
    std::vector<EffectRegistration3D> all_effects = EffectListManager3D::get()->GetAllEffects();
    for(const EffectRegistration3D& reg : all_effects)
    {
        QString category = QString::fromStdString(reg.category);
        if(category.compare(QStringLiteral("Audio"), Qt::CaseInsensitive) != 0)
        {
            continue;
        }
        
        QString label = QString::fromStdString(reg.ui_name);
        combo->addItem(label);
        int row = combo->count() - 1;
        combo->setItemData(row, QString::fromStdString(reg.class_name), kEffectRoleClassName);
        combo->setItemData(row, QString::fromStdString(reg.category), Qt::ToolTipRole);
    }
}

void OpenRGB3DSpatialTab::UpdateFreqZoneCombo()
{
    if(!freq_zone_combo) return;
    
    freq_zone_combo->blockSignals(true);
    freq_zone_combo->clear();
    
    freq_zone_combo->addItem("All Controllers", QVariant(-1));
    
    if(zone_manager)
    {
        for(int i = 0; i < zone_manager->GetZoneCount(); i++)
        {
            Zone3D* zone = zone_manager->GetZone(i);
            if(zone)
            {
                QString zone_name = QString::fromStdString(zone->GetName());
                freq_zone_combo->addItem(zone_name, QVariant(i));
            }
        }
    }
    
    for(unsigned int ci = 0; ci < controller_transforms.size(); ci++)
    {
        ControllerTransform* t = controller_transforms[ci].get();
        QString name;
        if(t && t->controller)
        {
            name = QString::fromStdString(t->controller->name);
        }
        else if(t && t->virtual_controller)
        {
            name = QString("[Virtual] ") + QString::fromStdString(t->virtual_controller->GetName());
        }
        else
        {
            name = QString("Controller %1").arg((int)ci);
        }
        freq_zone_combo->addItem(QString("(Controller) %1").arg(name), QVariant(-(int)ci - 1000));
    }
    
    freq_zone_combo->blockSignals(false);
}

void OpenRGB3DSpatialTab::UpdateFrequencyRangesList()
{
    if(!freq_ranges_list) return;
    
    int selected_row = freq_ranges_list->currentRow();
    
    freq_ranges_list->clear();
    
    for(size_t i = 0; i < frequency_ranges.size(); i++)
    {
        const FrequencyRangeEffect3D* range = frequency_ranges[i].get();
        if(!range) continue;
        
        QString label = QString::fromStdString(range->name) + 
                       QString(" (%1-%2 Hz)").arg(range->low_hz, 0, 'f', 0).arg(range->high_hz, 0, 'f', 0);
        if(!range->enabled) label += " [Disabled]";
        
        freq_ranges_list->addItem(label);
    }
    
    if(selected_row >= 0 && selected_row < freq_ranges_list->count())
    {
        freq_ranges_list->setCurrentRow(selected_row);
    }
}

void OpenRGB3DSpatialTab::on_add_freq_range_clicked()
{
    std::unique_ptr<FrequencyRangeEffect3D> range = std::make_unique<FrequencyRangeEffect3D>();
    range->id = next_freq_range_id++;
    range->name = "Range " + std::to_string(range->id);
    range->low_hz = 20.0f;
    range->high_hz = 200.0f;
    range->effect_class_name = "";  // No effect by default
    range->zone_index = -1;  // All controllers
    range->enabled = true;
    
    frequency_ranges.push_back(std::move(range));
    UpdateFrequencyRangesList();
    
    freq_ranges_list->setCurrentRow((int)frequency_ranges.size() - 1);
    
    SaveFrequencyRanges();
}

void OpenRGB3DSpatialTab::on_remove_freq_range_clicked()
{
    int row = freq_ranges_list->currentRow();
    if(row < 0 || row >= (int)frequency_ranges.size()) return;
    
    frequency_ranges.erase(frequency_ranges.begin() + row);
    
    UpdateFrequencyRangesList();
    SaveFrequencyRanges();
    
    // Hide details if list is empty
    if(frequency_ranges.empty() && freq_range_details)
    {
        freq_range_details->setVisible(false);
    }
}

void OpenRGB3DSpatialTab::on_duplicate_freq_range_clicked()
{
    int row = freq_ranges_list->currentRow();
    if(row < 0 || row >= (int)frequency_ranges.size()) return;
    
    FrequencyRangeEffect3D* source = frequency_ranges[row].get();
    if(!source) return;
    
    std::unique_ptr<FrequencyRangeEffect3D> clone = std::make_unique<FrequencyRangeEffect3D>();
    clone->id = next_freq_range_id++;
    clone->name = source->name + " Copy";
    clone->enabled = source->enabled;
    clone->low_hz = source->low_hz;
    clone->high_hz = source->high_hz;
    clone->effect_class_name = source->effect_class_name;
    clone->zone_index = source->zone_index;
    clone->position = source->position;
    clone->rotation = source->rotation;
    clone->scale = source->scale;
    clone->effect_settings = source->effect_settings;
    clone->smoothing = source->smoothing;
    clone->sensitivity = source->sensitivity;
    clone->attack = source->attack;
    clone->decay = source->decay;
    
    frequency_ranges.push_back(std::move(clone));
    UpdateFrequencyRangesList();
    
    freq_ranges_list->setCurrentRow((int)frequency_ranges.size() - 1);
    
    SaveFrequencyRanges();
}

void OpenRGB3DSpatialTab::on_freq_range_selected(int row)
{
    if(row < 0 || row >= (int)frequency_ranges.size())
    {
        if(freq_range_details) freq_range_details->setVisible(false);
        return;
    }
    
    FrequencyRangeEffect3D* range = frequency_ranges[row].get();
    if(!range) return;
    
    LoadFreqRangeDetails(range);
    
    if(freq_range_details) freq_range_details->setVisible(true);
}

void OpenRGB3DSpatialTab::LoadFreqRangeDetails(FrequencyRangeEffect3D* range)
{
    if(!range) return;
    
    // Block signals to prevent triggering saves during load
    if(freq_range_name_edit)
    {
        freq_range_name_edit->blockSignals(true);
        freq_range_name_edit->setText(QString::fromStdString(range->name));
        freq_range_name_edit->blockSignals(false);
    }
    
    if(freq_range_enabled_check)
    {
        freq_range_enabled_check->blockSignals(true);
        freq_range_enabled_check->setChecked(range->enabled);
        freq_range_enabled_check->blockSignals(false);
    }
    
    if(freq_low_spin && freq_low_slider)
    {
        freq_low_spin->blockSignals(true);
        freq_low_slider->blockSignals(true);
        freq_low_spin->setValue((int)range->low_hz);
        freq_low_slider->setValue((int)range->low_hz);
        freq_low_spin->blockSignals(false);
        freq_low_slider->blockSignals(false);
    }
    
    if(freq_high_spin && freq_high_slider)
    {
        freq_high_spin->blockSignals(true);
        freq_high_slider->blockSignals(true);
        freq_high_spin->setValue((int)range->high_hz);
        freq_high_slider->setValue((int)range->high_hz);
        freq_high_spin->blockSignals(false);
        freq_high_slider->blockSignals(false);
    }
    
    if(freq_effect_combo)
    {
        freq_effect_combo->blockSignals(true);
        int effect_idx = 0;  // Default to "None"
        QString selected_class_name;
        for(int i = 0; i < freq_effect_combo->count(); i++)
        {
            QString class_name = freq_effect_combo->itemData(i, kEffectRoleClassName).toString();
            if(class_name == QString::fromStdString(range->effect_class_name))
            {
                effect_idx = i;
                selected_class_name = class_name;
                break;
            }
        }
        freq_effect_combo->setCurrentIndex(effect_idx);
        freq_effect_combo->blockSignals(false);
        
        SetupFreqRangeEffectUI(range, selected_class_name);
    }
    
    if(freq_zone_combo)
    {
        freq_zone_combo->blockSignals(true);
        int zone_idx = freq_zone_combo->findData(QVariant(range->zone_index));
        if(zone_idx >= 0) freq_zone_combo->setCurrentIndex(zone_idx);
        freq_zone_combo->blockSignals(false);
    }
    
    if(freq_pos_x_spin) { freq_pos_x_spin->blockSignals(true); freq_pos_x_spin->setValue(range->position.x); freq_pos_x_spin->blockSignals(false); }
    if(freq_pos_y_spin) { freq_pos_y_spin->blockSignals(true); freq_pos_y_spin->setValue(range->position.y); freq_pos_y_spin->blockSignals(false); }
    if(freq_pos_z_spin) { freq_pos_z_spin->blockSignals(true); freq_pos_z_spin->setValue(range->position.z); freq_pos_z_spin->blockSignals(false); }
    if(freq_rot_x_spin) { freq_rot_x_spin->blockSignals(true); freq_rot_x_spin->setValue(range->rotation.x); freq_rot_x_spin->blockSignals(false); }
    if(freq_rot_y_spin) { freq_rot_y_spin->blockSignals(true); freq_rot_y_spin->setValue(range->rotation.y); freq_rot_y_spin->blockSignals(false); }
    if(freq_rot_z_spin) { freq_rot_z_spin->blockSignals(true); freq_rot_z_spin->setValue(range->rotation.z); freq_rot_z_spin->blockSignals(false); }
    if(freq_scale_x_spin) { freq_scale_x_spin->blockSignals(true); freq_scale_x_spin->setValue(range->scale.x); freq_scale_x_spin->blockSignals(false); }
    if(freq_scale_y_spin) { freq_scale_y_spin->blockSignals(true); freq_scale_y_spin->setValue(range->scale.y); freq_scale_y_spin->blockSignals(false); }
    if(freq_scale_z_spin) { freq_scale_z_spin->blockSignals(true); freq_scale_z_spin->setValue(range->scale.z); freq_scale_z_spin->blockSignals(false); }
    
    if(freq_smoothing_slider && freq_smoothing_label)
    {
        int smooth_val = (int)(range->smoothing * 100.0f);
        freq_smoothing_slider->blockSignals(true);
        freq_smoothing_slider->setValue(smooth_val);
        freq_smoothing_label->setText(QString::number(smooth_val) + "%");
        freq_smoothing_slider->blockSignals(false);
    }
    
    if(freq_sensitivity_slider && freq_sensitivity_label)
    {
        int sens_val = (int)(range->sensitivity * 10.0f);
        freq_sensitivity_slider->blockSignals(true);
        freq_sensitivity_slider->setValue(sens_val);
        freq_sensitivity_label->setText(QString::number(range->sensitivity, 'f', 1) + "x");
        freq_sensitivity_slider->blockSignals(false);
    }
    
    if(freq_attack_slider && freq_attack_label)
    {
        int attack_val = (int)(range->attack * 100.0f);
        freq_attack_slider->blockSignals(true);
        freq_attack_slider->setValue(attack_val);
        freq_attack_label->setText(QString::number(range->attack, 'f', 2));
        freq_attack_slider->blockSignals(false);
    }
    
    if(freq_decay_slider && freq_decay_label)
    {
        int decay_val = (int)(range->decay * 100.0f);
        freq_decay_slider->blockSignals(true);
        freq_decay_slider->setValue(decay_val);
        freq_decay_label->setText(QString::number(range->decay, 'f', 2));
        freq_decay_slider->blockSignals(false);
    }
}

void OpenRGB3DSpatialTab::on_freq_range_name_changed(const QString& text)
{
    int row = freq_ranges_list->currentRow();
    if(row < 0 || row >= (int)frequency_ranges.size()) return;
    
    FrequencyRangeEffect3D* range = frequency_ranges[row].get();
    if(!range) return;
    
    range->name = text.toStdString();
    UpdateFrequencyRangesList();
    SaveFrequencyRanges();
}

void OpenRGB3DSpatialTab::on_freq_low_changed(int value)
{
    int row = freq_ranges_list->currentRow();
    if(row < 0 || row >= (int)frequency_ranges.size()) return;
    
    FrequencyRangeEffect3D* range = frequency_ranges[row].get();
    if(!range) return;
    
    range->low_hz = (float)value;
    UpdateFrequencyRangesList();
    SaveFrequencyRanges();
}

void OpenRGB3DSpatialTab::on_freq_high_changed(int value)
{
    int row = freq_ranges_list->currentRow();
    if(row < 0 || row >= (int)frequency_ranges.size()) return;
    
    FrequencyRangeEffect3D* range = frequency_ranges[row].get();
    if(!range) return;
    
    range->high_hz = (float)value;
    UpdateFrequencyRangesList();
    SaveFrequencyRanges();
}

void OpenRGB3DSpatialTab::on_freq_effect_changed(int index)
{
    int row = freq_ranges_list->currentRow();
    if(row < 0 || row >= (int)frequency_ranges.size()) return;
    
    FrequencyRangeEffect3D* range = frequency_ranges[row].get();
    if(!range) return;
    
    QString class_name = freq_effect_combo->itemData(index, kEffectRoleClassName).toString();
    range->effect_class_name = class_name.toStdString();
    
    SetupFreqRangeEffectUI(range, class_name);
    
    SaveFrequencyRanges();
}

void OpenRGB3DSpatialTab::on_freq_zone_changed(int index)
{
    int row = freq_ranges_list->currentRow();
    if(row < 0 || row >= (int)frequency_ranges.size()) return;
    
    FrequencyRangeEffect3D* range = frequency_ranges[row].get();
    if(!range || !freq_zone_combo) return;
    
    QVariant data = freq_zone_combo->itemData(index);
    if(data.isValid())
    {
        range->zone_index = data.toInt();
        SaveFrequencyRanges();
    }
}

void OpenRGB3DSpatialTab::on_freq_enabled_toggled(bool checked)
{
    int row = freq_ranges_list->currentRow();
    if(row < 0 || row >= (int)frequency_ranges.size()) return;
    
    FrequencyRangeEffect3D* range = frequency_ranges[row].get();
    if(!range) return;
    
    range->enabled = checked;
    UpdateFrequencyRangesList();
    SaveFrequencyRanges();
}

void OpenRGB3DSpatialTab::on_freq_position_changed()
{
    int row = freq_ranges_list->currentRow();
    if(row < 0 || row >= (int)frequency_ranges.size()) return;
    
    FrequencyRangeEffect3D* range = frequency_ranges[row].get();
    if(!range) return;
    
    if(freq_pos_x_spin) range->position.x = freq_pos_x_spin->value();
    if(freq_pos_y_spin) range->position.y = freq_pos_y_spin->value();
    if(freq_pos_z_spin) range->position.z = freq_pos_z_spin->value();
    SaveFrequencyRanges();
}

void OpenRGB3DSpatialTab::on_freq_rotation_changed()
{
    int row = freq_ranges_list->currentRow();
    if(row < 0 || row >= (int)frequency_ranges.size()) return;
    
    FrequencyRangeEffect3D* range = frequency_ranges[row].get();
    if(!range) return;
    
    if(freq_rot_x_spin) range->rotation.x = freq_rot_x_spin->value();
    if(freq_rot_y_spin) range->rotation.y = freq_rot_y_spin->value();
    if(freq_rot_z_spin) range->rotation.z = freq_rot_z_spin->value();
    SaveFrequencyRanges();
}

void OpenRGB3DSpatialTab::on_freq_scale_changed()
{
    int row = freq_ranges_list->currentRow();
    if(row < 0 || row >= (int)frequency_ranges.size()) return;
    
    FrequencyRangeEffect3D* range = frequency_ranges[row].get();
    if(!range) return;
    
    if(freq_scale_x_spin) range->scale.x = freq_scale_x_spin->value();
    if(freq_scale_y_spin) range->scale.y = freq_scale_y_spin->value();
    if(freq_scale_z_spin) range->scale.z = freq_scale_z_spin->value();
    SaveFrequencyRanges();
}

void OpenRGB3DSpatialTab::on_freq_smoothing_changed(int value)
{
    int row = freq_ranges_list->currentRow();
    if(row < 0 || row >= (int)frequency_ranges.size()) return;
    
    FrequencyRangeEffect3D* range = frequency_ranges[row].get();
    if(!range) return;
    
    range->smoothing = value / 100.0f;
    if(freq_smoothing_label) freq_smoothing_label->setText(QString::number(value) + "%");
    SaveFrequencyRanges();
}

void OpenRGB3DSpatialTab::on_freq_sensitivity_changed(int value)
{
    int row = freq_ranges_list->currentRow();
    if(row < 0 || row >= (int)frequency_ranges.size()) return;
    
    FrequencyRangeEffect3D* range = frequency_ranges[row].get();
    if(!range) return;
    
    range->sensitivity = value / 10.0f;
    if(freq_sensitivity_label) freq_sensitivity_label->setText(QString::number(range->sensitivity, 'f', 1) + "x");
    SaveFrequencyRanges();
}

void OpenRGB3DSpatialTab::on_freq_attack_changed(int value)
{
    int row = freq_ranges_list->currentRow();
    if(row < 0 || row >= (int)frequency_ranges.size()) return;
    
    FrequencyRangeEffect3D* range = frequency_ranges[row].get();
    if(!range) return;
    
    range->attack = value / 100.0f;
    if(freq_attack_label) freq_attack_label->setText(QString::number(range->attack, 'f', 2));
    SaveFrequencyRanges();
}

void OpenRGB3DSpatialTab::on_freq_decay_changed(int value)
{
    int row = freq_ranges_list->currentRow();
    if(row < 0 || row >= (int)frequency_ranges.size()) return;
    
    FrequencyRangeEffect3D* range = frequency_ranges[row].get();
    if(!range) return;
    
    range->decay = value / 100.0f;
    if(freq_decay_label) freq_decay_label->setText(QString::number(range->decay, 'f', 2));
    SaveFrequencyRanges();
}

void OpenRGB3DSpatialTab::SaveFrequencyRanges()
{
    nlohmann::json ranges_json = nlohmann::json::array();
    
    for(unsigned int i = 0; i < frequency_ranges.size(); i++)
    {
        FrequencyRangeEffect3D* range = frequency_ranges[i].get();
        if(!range) continue;
        ranges_json.push_back(range->SaveToJSON());
    }
    
    nlohmann::json settings = GetPluginSettings();
    settings["frequency_ranges"] = ranges_json;
    settings["next_freq_range_id"] = next_freq_range_id;
    SetPluginSettings(settings);
}

void OpenRGB3DSpatialTab::LoadFrequencyRanges()
{
    frequency_ranges.clear();
    
    nlohmann::json settings = GetPluginSettings();
    if(!settings.contains("frequency_ranges")) return;
    
    const nlohmann::json& ranges_json = settings["frequency_ranges"];
    if(!ranges_json.is_array()) return;
    
    for(unsigned int i = 0; i < ranges_json.size(); i++)
    {
        std::unique_ptr<FrequencyRangeEffect3D> range = std::make_unique<FrequencyRangeEffect3D>();
        range->LoadFromJSON(ranges_json[i]);
        frequency_ranges.push_back(std::move(range));
    }
    
    if(settings.contains("next_freq_range_id"))
    {
        next_freq_range_id = settings["next_freq_range_id"].get<int>();
    }
}

void OpenRGB3DSpatialTab::SetupFreqRangeEffectUI(FrequencyRangeEffect3D* range, const QString& class_name)
{
    if(!range || !freq_effect_settings_widget || !freq_effect_settings_layout) return;
    
    ClearFreqRangeEffectUI();

    if(class_name.isEmpty())
    {
        freq_effect_settings_widget->hide();
        return;
    }
    
    SpatialEffect3D* effect = EffectListManager3D::get()->CreateEffect(class_name.toStdString());
    if(!effect)
    {
        freq_effect_settings_widget->hide();
        return;
    }
    
    effect->setParent(freq_effect_settings_widget);
    effect->SetupCustomUI(freq_effect_settings_widget);

    if(!range->effect_settings.is_null())
    {
        effect->LoadSettings(range->effect_settings);
    }
    
    current_freq_effect_ui = effect;
    connect(effect, &SpatialEffect3D::ParametersChanged, 
            this, &OpenRGB3DSpatialTab::OnFreqRangeEffectParamsChanged);
    
    freq_effect_settings_widget->show();
    freq_effect_settings_widget->updateGeometry();
}

void OpenRGB3DSpatialTab::ClearFreqRangeEffectUI()
{
    if(!freq_effect_settings_layout) return;
    
    while(QLayoutItem* item = freq_effect_settings_layout->takeAt(0))
    {
        if(QWidget* widget = item->widget())
        {
            widget->deleteLater();
        }
        delete item;
    }
    
    current_freq_effect_ui = nullptr;
}

void OpenRGB3DSpatialTab::OnFreqRangeEffectParamsChanged()
{
    int row = freq_ranges_list->currentRow();
    if(row < 0 || row >= (int)frequency_ranges.size()) return;
    
    FrequencyRangeEffect3D* range = frequency_ranges[row].get();
    if(!range || !current_freq_effect_ui) return;
    
    range->effect_settings = current_freq_effect_ui->SaveSettings();
    
    SaveFrequencyRanges();
}

void OpenRGB3DSpatialTab::RenderFrequencyRangeEffects()
{
    if(!AudioInputManager::instance()->isRunning()) return;
    if(controller_transforms.empty()) return;
    
    for(unsigned int range_idx = 0; range_idx < frequency_ranges.size(); range_idx++)
    {
        FrequencyRangeEffect3D* range = frequency_ranges[range_idx].get();
        if(!range || !range->enabled) continue;
        if(range->effect_class_name.empty()) continue;
        
        float raw_level = AudioInputManager::instance()->getBandEnergyHz(
            range->low_hz, range->high_hz);
        
        if(raw_level > range->current_level)
        {
            range->current_level += (raw_level - range->current_level) * range->attack;
        }
        else
        {
            range->current_level += (raw_level - range->current_level) * range->decay;
        }

        range->smoothed_level = range->smoothing * range->smoothed_level +
                               (1.0f - range->smoothing) * range->current_level;

        float effect_level = range->smoothed_level * range->sensitivity;
        effect_level = std::min(1.0f, std::max(0.0f, effect_level));
        
        if(!range->effect_instance)
        {
            SpatialEffect3D* effect = EffectListManager3D::get()->CreateEffect(range->effect_class_name);
            if(!effect) continue;

            range->effect_instance.reset(effect);

            if(!range->effect_settings.is_null())
            {
                effect->LoadSettings(range->effect_settings);
            }

            // Set reference point (room center for now)
            effect->SetReferenceMode(REF_MODE_ROOM_CENTER);
            Vector3D room_center = {0.0f, 0.0f, 0.0f};
            effect->SetCustomReferencePoint(room_center);
        }
        
        SpatialEffect3D* effect = range->effect_instance.get();
        if(!effect) continue;
        
        nlohmann::json audio_params = range->effect_settings;
        audio_params["audio_level"] = effect_level;
        audio_params["frequency_band_energy"] = raw_level;
        effect->LoadSettings(audio_params);
        
        for(unsigned int ctrl_idx = 0; ctrl_idx < controller_transforms.size(); ctrl_idx++)
        {
            ControllerTransform* transform = controller_transforms[ctrl_idx].get();
            if(!transform) continue;
            
            bool is_targeted = false;
            if(range->zone_index == -1)
            {
                is_targeted = true;
            }
            else if(range->zone_index >= 0)
            {
                if(zone_manager && range->zone_index < zone_manager->GetZoneCount())
                {
                    Zone3D* zone = zone_manager->GetZone(range->zone_index);
                    if(zone && zone->ContainsController(ctrl_idx))
                    {
                        is_targeted = true;
                    }
                }
            }
            else if(range->zone_index <= -1000)
            {
                int target_ctrl = -(range->zone_index + 1000);
                is_targeted = ((int)ctrl_idx == target_ctrl);
            }
            
            if(!is_targeted) continue;
            
            if(transform->virtual_controller)
            {
                VirtualController3D* vctrl = transform->virtual_controller;
                const std::vector<GridLEDMapping>& mappings = vctrl->GetMappings();
                
                for(unsigned int led_idx = 0; led_idx < mappings.size(); led_idx++)
                {
                    const GridLEDMapping& mapping = mappings[led_idx];
                    if(!mapping.controller) continue;
                    
                    Vector3D world_pos = transform->led_positions[led_idx].world_position;
                    Vector3D relative_pos;
                    relative_pos.x = (world_pos.x - range->position.x) / range->scale.x;
                    relative_pos.y = (world_pos.y - range->position.y) / range->scale.y;
                    relative_pos.z = (world_pos.z - range->position.z) / range->scale.z;
                    RGBColor color = effect->CalculateColor(relative_pos.x, relative_pos.y, relative_pos.z, effect_time);
                    unsigned int physical_led_idx = mapping.controller->zones[mapping.zone_idx].start_idx + mapping.led_idx;
                    if(physical_led_idx < mapping.controller->colors.size())
                    {
                        // Blend with existing color (additive)
                        RGBColor existing = mapping.controller->colors[physical_led_idx];
                        int r = std::min(255, (int)RGBGetRValue(existing) + (int)RGBGetRValue(color));
                        int g = std::min(255, (int)RGBGetGValue(existing) + (int)RGBGetGValue(color));
                        int b = std::min(255, (int)RGBGetBValue(existing) + (int)RGBGetBValue(color));
                        mapping.controller->colors[physical_led_idx] = ToRGBColor(r, g, b);
                    }
                }
            }
            else if(transform->controller)
            {
                RGBController* ctrl = transform->controller;
                
                for(unsigned int led_idx = 0; led_idx < transform->led_positions.size(); led_idx++)
                {
                    Vector3D world_pos = transform->led_positions[led_idx].world_position;
                    Vector3D relative_pos;
                    relative_pos.x = (world_pos.x - range->position.x) / range->scale.x;
                    relative_pos.y = (world_pos.y - range->position.y) / range->scale.y;
                    relative_pos.z = (world_pos.z - range->position.z) / range->scale.z;
                    RGBColor color = effect->CalculateColor(relative_pos.x, relative_pos.y, relative_pos.z, effect_time);
                    LEDPosition3D& led_pos = transform->led_positions[led_idx];
                    unsigned int physical_led_idx = ctrl->zones[led_pos.zone_idx].start_idx + led_pos.led_idx;
                    if(physical_led_idx < ctrl->colors.size())
                    {
                        // Blend with existing color (additive)
                        RGBColor existing = ctrl->colors[physical_led_idx];
                        int r = std::min(255, (int)RGBGetRValue(existing) + (int)RGBGetRValue(color));
                        int g = std::min(255, (int)RGBGetGValue(existing) + (int)RGBGetGValue(color));
                        int b = std::min(255, (int)RGBGetBValue(existing) + (int)RGBGetBValue(color));
                        ctrl->colors[physical_led_idx] = ToRGBColor(r, g, b);
                    }
                }
            }
        }
    }
}
