// SPDX-License-Identifier: GPL-2.0-only

#include "OpenRGB3DSpatialTab.h"
#include "Audio/AudioInputManager.h"
#include "VirtualReferencePoint3D.h"
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

    QHBoxLayout* origin_row = new QHBoxLayout();
    origin_row->addWidget(new QLabel("Origin:"));
    freq_origin_combo = new QComboBox();
    UpdateFreqOriginCombo();
    connect(freq_origin_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &OpenRGB3DSpatialTab::on_freq_origin_changed);
    origin_row->addWidget(freq_origin_combo, 1);
    details_layout->addLayout(origin_row);
    
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
    for(unsigned int i = 0; i < all_effects.size(); i++)
    {
        const EffectRegistration3D& reg = all_effects[i];
        if(QString::fromStdString(reg.category).compare(QStringLiteral("Audio"), Qt::CaseInsensitive) != 0)
        {
            continue;
        }
        // AudioContainer3D is only a stack placeholder; it has no per-range parameters
        if(reg.class_name == "AudioContainer3D")
        {
            continue;
        }
        combo->addItem(QString::fromStdString(reg.ui_name));
        int row = combo->count() - 1;
        combo->setItemData(row, QString::fromStdString(reg.class_name), kEffectRoleClassName);
    }
}

void OpenRGB3DSpatialTab::UpdateFreqOriginCombo()
{
    if(!freq_origin_combo) return;

    freq_origin_combo->blockSignals(true);
    freq_origin_combo->clear();

    freq_origin_combo->addItem("Room Center", QVariant(-1));

    for(size_t i = 0; i < reference_points.size(); i++)
    {
        VirtualReferencePoint3D* ref_point = reference_points[i].get();
        if(!ref_point) continue;
        QString name = QString::fromStdString(ref_point->GetName());
        QString type = QString(VirtualReferencePoint3D::GetTypeName(ref_point->GetType()));
        freq_origin_combo->addItem(QString("%1 (%2)").arg(name).arg(type), QVariant((int)i));
    }

    freq_origin_combo->blockSignals(false);
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

    QSignalBlocker blocker(freq_ranges_list);

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
    range->effect_class_name = "";
    range->zone_index = -1;
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
        int effect_idx = 0;
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

    if(freq_origin_combo)
    {
        freq_origin_combo->blockSignals(true);
        int origin_idx = freq_origin_combo->findData(QVariant(range->origin_ref_index));
        if(origin_idx >= 0) freq_origin_combo->setCurrentIndex(origin_idx);
        freq_origin_combo->blockSignals(false);
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

    // Destroy the running instance so the new effect type takes effect immediately
    range->effect_instance.reset();
    range->effect_settings = nlohmann::json();
    
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

void OpenRGB3DSpatialTab::on_freq_origin_changed(int index)
{
    int row = freq_ranges_list->currentRow();
    if(row < 0 || row >= (int)frequency_ranges.size()) return;

    FrequencyRangeEffect3D* range = frequency_ranges[row].get();
    if(!range || !freq_origin_combo) return;

    QVariant data = freq_origin_combo->itemData(index);
    if(data.isValid())
    {
        range->origin_ref_index = data.toInt();
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

    QWidget* ui_wrapper = new QWidget(freq_effect_settings_widget);
    QVBoxLayout* wrapper_layout = new QVBoxLayout(ui_wrapper);
    wrapper_layout->setContentsMargins(0, 0, 0, 0);
    wrapper_layout->setSpacing(4);

    effect->setParent(ui_wrapper);
    effect->CreateCommonEffectControls(ui_wrapper, false);
    effect->SetupCustomUI(ui_wrapper);

    freq_effect_settings_layout->addWidget(ui_wrapper);

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
    if(current_freq_effect_ui)
    {
        disconnect(current_freq_effect_ui, nullptr, this, nullptr);
        current_freq_effect_ui = nullptr;
    }

    if(!freq_effect_settings_layout) return;

    while(QLayoutItem* item = freq_effect_settings_layout->takeAt(0))
    {
        if(QWidget* widget = item->widget())
        {
            widget->hide();
            widget->setParent(nullptr);
            delete widget;
        }
        delete item;
    }
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

void OpenRGB3DSpatialTab::RenderFrequencyRangeEffects(const GridContext3D& room_grid)
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
        }
        
        SpatialEffect3D* effect = range->effect_instance.get();
        if(!effect) continue;
        
        nlohmann::json audio_params = range->effect_settings;
        audio_params["audio_level"] = effect_level;
        audio_params["frequency_band_energy"] = raw_level;
        effect->LoadSettings(audio_params);

        if(range->origin_ref_index >= 0 && range->origin_ref_index < (int)reference_points.size())
        {
            VirtualReferencePoint3D* ref_pt = reference_points[range->origin_ref_index].get();
            if(ref_pt)
            {
                effect->SetReferenceMode(REF_MODE_CUSTOM_POINT);
                effect->SetCustomReferencePoint(ref_pt->GetPosition());
            }
        }
        else
        {
            effect->SetReferenceMode(REF_MODE_ROOM_CENTER);
        }
        
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
                    float x = world_pos.x, y = world_pos.y, z = world_pos.z;
                    effect->ApplyAxisScale(x, y, z, room_grid);
                    effect->ApplyEffectRotation(x, y, z, room_grid);
                    RGBColor color = effect->CalculateColorGrid(x, y, z, effect_time, room_grid);
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
                    float x = world_pos.x, y = world_pos.y, z = world_pos.z;
                    effect->ApplyAxisScale(x, y, z, room_grid);
                    effect->ApplyEffectRotation(x, y, z, room_grid);
                    RGBColor color = effect->CalculateColorGrid(x, y, z, effect_time, room_grid);
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
