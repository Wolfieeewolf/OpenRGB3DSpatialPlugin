// SPDX-License-Identifier: GPL-2.0-only

#include "OpenRGB3DSpatialTab.h"
#include "Audio/AudioInputManager.h"
#include "Effects3D/Games/Minecraft/MinecraftGame.h"
#include "ZoneGrid3D.h"
#include "LogManager.h"
#include "PluginLogOnce.h"
#include "PluginUiUtils.h"
#include <algorithm>
#include <functional>
#include <unordered_set>
#include <QToolTip>

static bool TryGetGlobalLedIndexForRange(RGBController* controller,
                                         unsigned int zone_idx,
                                         unsigned int led_idx,
                                         unsigned int* global_idx)
{
    if(!controller || !global_idx)
    {
        return false;
    }
    if(zone_idx >= controller->zones.size())
    {
        return false;
    }
    if(led_idx >= controller->zones[zone_idx].leds_count)
    {
        return false;
    }

    *global_idx = controller->zones[zone_idx].start_idx + led_idx;
    return (*global_idx < controller->colors.size());
}

void OpenRGB3DSpatialTab::SetupFrequencyRangeEffectsUI(QVBoxLayout* parent_layout)
{
    freq_ranges_group = new QGroupBox("Frequency Range Effects");
    QVBoxLayout* freq_layout = new QVBoxLayout(freq_ranges_group);
    
    QLabel* header = new QLabel("Multi-Band Audio Effects");
    PluginUiApplyBoldLabel(header);
    freq_layout->addWidget(header);
    
    QLabel* description = new QLabel("Configure independent audio-reactive effects for specific frequency ranges (e.g., bass floor, treble ceiling).");
    description->setWordWrap(true);
    PluginUiApplyMutedSecondaryLabel(description);
    freq_layout->addWidget(description);
    
    QLabel* ranges_label = new QLabel("Audio Frequency Ranges:");
    PluginUiApplyBoldLabel(ranges_label);
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
    freq_low_slider = new QSlider(Qt::Horizontal);
    freq_low_slider->setRange(20, 20000);
    freq_low_slider->setValue(20);
    connect(freq_low_slider, &QSlider::valueChanged,
            this, &OpenRGB3DSpatialTab::on_freq_low_changed);
    connect(freq_low_slider, &QSlider::sliderMoved, [](int v) { QToolTip::showText(QCursor::pos(), QString("%1 Hz").arg(v), nullptr); });
    low_row->addWidget(freq_low_slider, 1);
    freq_low_label = new QLabel("20 Hz");
    freq_low_label->setMinimumWidth(52);
    low_row->addWidget(freq_low_label);
    freq_sliders->addLayout(low_row);

    QHBoxLayout* high_row = new QHBoxLayout();
    high_row->addWidget(new QLabel("High Hz:"));
    freq_high_slider = new QSlider(Qt::Horizontal);
    freq_high_slider->setRange(20, 20000);
    freq_high_slider->setValue(200);
    connect(freq_high_slider, &QSlider::valueChanged,
            this, &OpenRGB3DSpatialTab::on_freq_high_changed);
    connect(freq_high_slider, &QSlider::sliderMoved, [](int v) { QToolTip::showText(QCursor::pos(), QString("%1 Hz").arg(v), nullptr); });
    high_row->addWidget(freq_high_slider, 1);
    freq_high_label = new QLabel("200 Hz");
    freq_high_label->setMinimumWidth(52);
    high_row->addWidget(freq_high_label);
    freq_sliders->addLayout(high_row);

    QHBoxLayout* preset_row = new QHBoxLayout();
    preset_row->addWidget(new QLabel("Presets:"));
    QPushButton* preset_bass_btn = new QPushButton("Bass (20–200 Hz)");
    QPushButton* preset_mids_btn = new QPushButton("Mids (200–2k Hz)");
    QPushButton* preset_highs_btn = new QPushButton("Highs (2k–20k Hz)");
    connect(preset_bass_btn, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_freq_preset_bass_clicked);
    connect(preset_mids_btn, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_freq_preset_mids_clicked);
    connect(preset_highs_btn, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_freq_preset_highs_clicked);
    preset_row->addWidget(preset_bass_btn);
    preset_row->addWidget(preset_mids_btn);
    preset_row->addWidget(preset_highs_btn);
    preset_row->addStretch();
    freq_sliders->addLayout(preset_row);
    
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

    QGroupBox* response_group = new QGroupBox("Response (smoothing & sensitivity)");
    QVBoxLayout* response_layout = new QVBoxLayout(response_group);
    std::function<void(const QString&, QSlider*&, QLabel*&, int, int, int)> addSliderRow =
        [&response_layout](const QString& label, QSlider*& slider, QLabel*& valueLabel, int minVal, int maxVal, int defaultVal) {
        QHBoxLayout* row = new QHBoxLayout();
        row->addWidget(new QLabel(label));
        slider = new QSlider(Qt::Horizontal);
        slider->setRange(minVal, maxVal);
        slider->setValue(defaultVal);
        row->addWidget(slider, 1);
        valueLabel = new QLabel(QString::number(defaultVal));
        valueLabel->setMinimumWidth(36);
        row->addWidget(valueLabel);
        response_layout->addLayout(row);
    };
    addSliderRow("Smoothing:", freq_smoothing_slider, freq_smoothing_label, 0, 100, 70);
    addSliderRow("Sensitivity:", freq_sensitivity_slider, freq_sensitivity_label, 0, 200, 100);
    addSliderRow("Attack:", freq_attack_slider, freq_attack_label, 0, 100, 5);
    addSliderRow("Decay:", freq_decay_slider, freq_decay_label, 0, 100, 20);
    if(freq_smoothing_slider)
        connect(freq_smoothing_slider, &QSlider::valueChanged, this, &OpenRGB3DSpatialTab::on_freq_smoothing_changed);
    if(freq_sensitivity_slider)
        connect(freq_sensitivity_slider, &QSlider::valueChanged, this, &OpenRGB3DSpatialTab::on_freq_sensitivity_changed);
    if(freq_attack_slider)
        connect(freq_attack_slider, &QSlider::valueChanged, this, &OpenRGB3DSpatialTab::on_freq_attack_changed);
    if(freq_decay_slider)
        connect(freq_decay_slider, &QSlider::valueChanged, this, &OpenRGB3DSpatialTab::on_freq_decay_changed);
    details_layout->addWidget(response_group);

    QGroupBox* eq_group = new QGroupBox("Range EQ (dump bands to isolate bass/mids/highs)");
    QHBoxLayout* eq_layout = new QHBoxLayout(eq_group);
    eq_layout->addWidget(new QLabel("0% = dump, 100% = pass"));
    freq_eq_sliders.clear();
    for(int i = 0; i < FrequencyRangeEffect3D::EQ_BANDS; i++)
    {
        QSlider* s = new QSlider(Qt::Vertical);
        s->setRange(0, 200);
        s->setValue(100);
        s->setMaximumWidth(20);
        int band = i;
        connect(s, &QSlider::valueChanged, [this, band](int val) { on_freq_eq_changed(band, val); });
        connect(s, &QSlider::sliderMoved, [band](int val) { QToolTip::showText(QCursor::pos(), QString("%1%").arg(val), nullptr); });
        eq_layout->addWidget(s);
        freq_eq_sliders.push_back(s);
    }
    QPushButton* reset_eq_btn = new QPushButton("Reset EQ");
    connect(reset_eq_btn, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_freq_reset_eq_clicked);
    eq_layout->addWidget(reset_eq_btn);
    details_layout->addWidget(eq_group);
    
    freq_effect_settings_widget = new QWidget();
    freq_effect_settings_layout = new QVBoxLayout(freq_effect_settings_widget);
    freq_effect_settings_layout->setContentsMargins(0, 0, 0, 0);
    details_layout->addWidget(freq_effect_settings_widget);
    
    details_layout->addStretch();
    
    freq_layout->addWidget(freq_range_details);
    freq_range_details->setVisible(false);
    
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
        if(reg.class_name == "AudioContainer")
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

    QVariant desired_selection = freq_origin_combo->currentData();
    bool restore_signals = freq_origin_combo->blockSignals(true);
    freq_origin_combo->clear();

    freq_origin_combo->addItem("Room Center", QVariant(-1));

    for(size_t i = 0; i < reference_points.size(); i++)
    {
        VirtualReferencePoint3D* ref_point = reference_points[i].get();
        if(!ref_point) continue;
        QString name = QString::fromStdString(ref_point->GetName());
        QString type = QString(VirtualReferencePoint3D::GetTypeName(ref_point->GetType()));
        freq_origin_combo->addItem(QString("%1 (%2)").arg(name, type), QVariant((int)i));
    }

    int restore_index = freq_origin_combo->findData(desired_selection);
    if(restore_index < 0)
    {
        restore_index = 0;
    }
    freq_origin_combo->setCurrentIndex(restore_index);
    freq_origin_combo->blockSignals(restore_signals);
}

void OpenRGB3DSpatialTab::UpdateFreqZoneCombo()
{
    if(!freq_zone_combo) return;
    
    QVariant desired_selection = freq_zone_combo->currentData();
    bool restore_signals = freq_zone_combo->blockSignals(true);
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
            name = QString::fromStdString(t->controller->GetName());
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
    
    int restore_index = freq_zone_combo->findData(desired_selection);
    if(restore_index < 0)
    {
        restore_index = 0;
    }
    freq_zone_combo->setCurrentIndex(restore_index);
    freq_zone_combo->blockSignals(restore_signals);
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
    else if(!frequency_ranges.empty() && freq_ranges_list)
    {
        int new_row = std::min(row, (int)frequency_ranges.size() - 1);
        freq_ranges_list->setCurrentRow(new_row);
        on_freq_range_selected(new_row);
    }

    RenderEffectStack();
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
    for(int i = 0; i < FrequencyRangeEffect3D::EQ_BANDS; i++)
        clone->eq_gain[i] = source->eq_gain[i];
    
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
        QSignalBlocker block(*freq_range_name_edit);
        freq_range_name_edit->setText(QString::fromStdString(range->name));
    }
    
    if(freq_range_enabled_check)
    {
        QSignalBlocker block(*freq_range_enabled_check);
        freq_range_enabled_check->setChecked(range->enabled);
    }
    
    if(freq_low_slider)
    {
        QSignalBlocker block(*freq_low_slider);
        freq_low_slider->setValue((int)range->low_hz);
    }
    if(freq_high_slider)
    {
        QSignalBlocker block(*freq_high_slider);
        freq_high_slider->setValue((int)range->high_hz);
    }
    if(freq_low_label)
    {
        freq_low_label->setText(QString::number((int)range->low_hz) + " Hz");
    }
    if(freq_high_label)
    {
        freq_high_label->setText(QString::number((int)range->high_hz) + " Hz");
    }

    if(freq_effect_combo)
    {
        QSignalBlocker block(*freq_effect_combo);
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
        
        SetupFreqRangeEffectUI(range, selected_class_name);
    }
    
    if(freq_zone_combo)
    {
        QSignalBlocker block(*freq_zone_combo);
        int zone_idx = freq_zone_combo->findData(QVariant(range->zone_index));
        if(zone_idx >= 0)
        {
            freq_zone_combo->setCurrentIndex(zone_idx);
        }
        else
        {
            range->zone_index = -1;
            freq_zone_combo->setCurrentIndex(0);
        }
    }

    if(freq_origin_combo)
    {
        QSignalBlocker block(*freq_origin_combo);
        int origin_idx = freq_origin_combo->findData(QVariant(range->origin_ref_index));
        if(origin_idx >= 0)
        {
            freq_origin_combo->setCurrentIndex(origin_idx);
        }
        else
        {
            range->origin_ref_index = -1;
            freq_origin_combo->setCurrentIndex(0);
        }
    }

    if(freq_smoothing_slider)
    {
        QSignalBlocker block(*freq_smoothing_slider);
        freq_smoothing_slider->setValue((int)(range->smoothing * 100.0f));
        if(freq_smoothing_label) freq_smoothing_label->setText(QString::number((int)(range->smoothing * 100.0f)) + "%");
    }
    if(freq_sensitivity_slider)
    {
        QSignalBlocker block(*freq_sensitivity_slider);
        freq_sensitivity_slider->setValue((int)(range->sensitivity * 100.0f));
        if(freq_sensitivity_label) freq_sensitivity_label->setText(QString::number((int)(range->sensitivity * 100.0f)) + "%");
    }
    if(freq_attack_slider)
    {
        QSignalBlocker block(*freq_attack_slider);
        freq_attack_slider->setValue((int)(range->attack * 100.0f));
        if(freq_attack_label) freq_attack_label->setText(QString::number(range->attack, 'f', 2));
    }
    if(freq_decay_slider)
    {
        QSignalBlocker block(*freq_decay_slider);
        freq_decay_slider->setValue((int)(range->decay * 100.0f));
        if(freq_decay_label) freq_decay_label->setText(QString::number(range->decay, 'f', 2));
    }
    for(int i = 0; i < FrequencyRangeEffect3D::EQ_BANDS && i < (int)freq_eq_sliders.size(); i++)
    {
        QSlider* s = freq_eq_sliders[i];
        if(s)
        {
            QSignalBlocker block(*s);
            s->setValue((int)(range->eq_gain[i] * 100.0f));
        }
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

    if(value > (int)range->high_hz && freq_high_slider)
    {
        freq_high_slider->blockSignals(true);
        freq_high_slider->setValue(value);
        freq_high_slider->blockSignals(false);
        range->high_hz = (float)value;
        if(freq_high_label) freq_high_label->setText(QString::number(value) + " Hz");
    }
    range->low_hz = (float)value;
    if(freq_low_label)
        freq_low_label->setText(QString::number(value) + " Hz");
    UpdateFrequencyRangesList();
    SaveFrequencyRanges();
}

void OpenRGB3DSpatialTab::on_freq_high_changed(int value)
{
    int row = freq_ranges_list->currentRow();
    if(row < 0 || row >= (int)frequency_ranges.size()) return;

    FrequencyRangeEffect3D* range = frequency_ranges[row].get();
    if(!range) return;

    if(value < (int)range->low_hz && freq_low_slider)
    {
        freq_low_slider->blockSignals(true);
        freq_low_slider->setValue(value);
        freq_low_slider->blockSignals(false);
        range->low_hz = (float)value;
        if(freq_low_label) freq_low_label->setText(QString::number(value) + " Hz");
    }
    range->high_hz = (float)value;
    if(freq_high_label)
        freq_high_label->setText(QString::number(value) + " Hz");
    UpdateFrequencyRangesList();
    SaveFrequencyRanges();
}

void OpenRGB3DSpatialTab::applyFreqPreset(int low_hz, int high_hz, const QString& name, int isolate_band)
{
    int row = freq_ranges_list->currentRow();
    if(row < 0 || row >= (int)frequency_ranges.size()) return;

    FrequencyRangeEffect3D* range = frequency_ranges[row].get();
    if(!range) return;

    range->low_hz = (float)low_hz;
    range->high_hz = (float)high_hz;
    range->name = name.toStdString();

    if(isolate_band >= 1 && isolate_band <= 3)
    {
        for(int i = 0; i < FrequencyRangeEffect3D::EQ_BANDS; i++)
        {
            if(isolate_band == 1)
                range->eq_gain[i] = (i <= 4) ? 1.0f : 0.2f;
            else if(isolate_band == 2)
                range->eq_gain[i] = (i >= 5 && i <= 9) ? 1.0f : 0.2f;
            else
                range->eq_gain[i] = (i >= 10) ? 1.0f : 0.2f;
        }
        for(int i = 0; i < (int)freq_eq_sliders.size() && i < FrequencyRangeEffect3D::EQ_BANDS; i++)
        {
            if(freq_eq_sliders[i])
            {
                freq_eq_sliders[i]->blockSignals(true);
                freq_eq_sliders[i]->setValue((int)(range->eq_gain[i] * 100.0f));
                freq_eq_sliders[i]->blockSignals(false);
            }
        }
    }

    if(freq_low_slider)
    {
        freq_low_slider->blockSignals(true);
        freq_low_slider->setValue(low_hz);
        freq_low_slider->blockSignals(false);
    }
    if(freq_high_slider)
    {
        freq_high_slider->blockSignals(true);
        freq_high_slider->setValue(high_hz);
        freq_high_slider->blockSignals(false);
    }
    if(freq_low_label)
        freq_low_label->setText(QString::number(low_hz) + " Hz");
    if(freq_high_label)
        freq_high_label->setText(QString::number(high_hz) + " Hz");
    if(freq_range_name_edit)
        freq_range_name_edit->setText(name);

    UpdateFrequencyRangesList();
    SaveFrequencyRanges();
}

void OpenRGB3DSpatialTab::on_freq_preset_bass_clicked()
{
    applyFreqPreset(20, 200, "Bass", 1);
}

void OpenRGB3DSpatialTab::on_freq_preset_mids_clicked()
{
    applyFreqPreset(200, 2000, "Mids", 2);
}

void OpenRGB3DSpatialTab::on_freq_preset_highs_clicked()
{
    applyFreqPreset(2000, 20000, "Highs", 3);
}

void OpenRGB3DSpatialTab::on_freq_eq_changed(int band_index, int value)
{
    int row = freq_ranges_list->currentRow();
    if(row < 0 || row >= (int)frequency_ranges.size()) return;
    if(band_index < 0 || band_index >= FrequencyRangeEffect3D::EQ_BANDS) return;
    FrequencyRangeEffect3D* range = frequency_ranges[row].get();
    if(!range) return;
    range->eq_gain[band_index] = std::max(0.0f, std::min(2.0f, value / 100.0f));
    SaveFrequencyRanges();
}

void OpenRGB3DSpatialTab::on_freq_reset_eq_clicked()
{
    int row = freq_ranges_list->currentRow();
    if(row < 0 || row >= (int)frequency_ranges.size()) return;
    FrequencyRangeEffect3D* range = frequency_ranges[row].get();
    if(!range) return;
    for(int i = 0; i < FrequencyRangeEffect3D::EQ_BANDS; i++)
        range->eq_gain[i] = 1.0f;
    for(int i = 0; i < (int)freq_eq_sliders.size(); i++)
    {
        if(freq_eq_sliders[i])
        {
            freq_eq_sliders[i]->blockSignals(true);
            freq_eq_sliders[i]->setValue(100);
            freq_eq_sliders[i]->blockSignals(false);
        }
    }
    SaveFrequencyRanges();
}

void OpenRGB3DSpatialTab::on_freq_smoothing_changed(int value)
{
    int row = freq_ranges_list->currentRow();
    if(row < 0 || row >= (int)frequency_ranges.size()) return;
    FrequencyRangeEffect3D* range = frequency_ranges[row].get();
    if(!range) return;
    range->smoothing = std::max(0.0f, std::min(1.0f, value / 100.0f));
    if(freq_smoothing_label) freq_smoothing_label->setText(QString::number(value) + "%");
    SaveFrequencyRanges();
}

void OpenRGB3DSpatialTab::on_freq_sensitivity_changed(int value)
{
    int row = freq_ranges_list->currentRow();
    if(row < 0 || row >= (int)frequency_ranges.size()) return;
    FrequencyRangeEffect3D* range = frequency_ranges[row].get();
    if(!range) return;
    range->sensitivity = std::max(0.0f, std::min(3.0f, value / 100.0f));
    if(freq_sensitivity_label) freq_sensitivity_label->setText(QString::number(value) + "%");
    SaveFrequencyRanges();
}

void OpenRGB3DSpatialTab::on_freq_attack_changed(int value)
{
    int row = freq_ranges_list->currentRow();
    if(row < 0 || row >= (int)frequency_ranges.size()) return;
    FrequencyRangeEffect3D* range = frequency_ranges[row].get();
    if(!range) return;
    range->attack = std::max(0.01f, std::min(1.0f, value / 100.0f));
    if(freq_attack_label) freq_attack_label->setText(QString::number(range->attack, 'f', 2));
    SaveFrequencyRanges();
}

void OpenRGB3DSpatialTab::on_freq_decay_changed(int value)
{
    int row = freq_ranges_list->currentRow();
    if(row < 0 || row >= (int)frequency_ranges.size()) return;
    FrequencyRangeEffect3D* range = frequency_ranges[row].get();
    if(!range) return;
    range->decay = std::max(0.01f, std::min(1.0f, value / 100.0f));
    if(freq_decay_label) freq_decay_label->setText(QString::number(range->decay, 'f', 2));
    SaveFrequencyRanges();
}

void OpenRGB3DSpatialTab::on_freq_effect_changed(int index)
{
    if(!freq_effect_combo)
    {
        return;
    }
    int row = freq_ranges_list->currentRow();
    if(row < 0 || row >= (int)frequency_ranges.size()) return;
    if(index < 0 || index >= freq_effect_combo->count())
    {
        return;
    }
    
    FrequencyRangeEffect3D* range = frequency_ranges[row].get();
    if(!range) return;
    
    QString class_name = freq_effect_combo->itemData(index, kEffectRoleClassName).toString();
    range->effect_class_name = class_name.toStdString();

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
        LOG_ERROR("[OpenRGB3DSpatialPlugin] Frequency range: failed to create effect UI: %s", class_name.toStdString().c_str());
        freq_effect_settings_widget->hide();
        return;
    }

    QWidget* ui_wrapper = new QWidget(freq_effect_settings_widget);
    QVBoxLayout* wrapper_layout = new QVBoxLayout(ui_wrapper);
    wrapper_layout->setContentsMargins(0, 0, 0, 0);
    wrapper_layout->setSpacing(4);

    effect->setParent(ui_wrapper);
    effect->CreateCommonEffectControls(ui_wrapper, false);
    QWidget* custom_host = effect->GetCustomSettingsHost();
    effect->SetupCustomUI(custom_host ? custom_host : ui_wrapper);

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

void OpenRGB3DSpatialTab::RenderFrequencyRangeEffects(const GridContext3D& room_grid, const GridContext3D& world_grid)
{
    MinecraftGame::ClearRenderSampleIndexContext();
    if(!AudioInputManager::instance()->isRunning()) return;
    if(controller_transforms.empty()) return;

    std::unordered_set<RGBController*> controllers_managed_by_virtuals;
    for(const std::unique_ptr<ControllerTransform>& transform_ptr : controller_transforms)
    {
        ControllerTransform* transform = transform_ptr.get();
        if(!transform || transform->virtual_controller == nullptr)
        {
            continue;
        }
        for(const GridLEDMapping& mapping : transform->virtual_controller->GetMappings())
        {
            if(mapping.controller)
            {
                controllers_managed_by_virtuals.insert(mapping.controller);
            }
        }
    }
    
    for(unsigned int range_idx = 0; range_idx < frequency_ranges.size(); range_idx++)
    {
        FrequencyRangeEffect3D* range = frequency_ranges[range_idx].get();
        if(!range || !range->enabled) continue;
        if(range->effect_class_name.empty()) continue;
        
        float raw_level = AudioInputManager::instance()->getBandEnergyHzWithGain(
            range->low_hz, range->high_hz, range->eq_gain);
        
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
            if(!effect)
            {
                LogOnce_CreateEffectFailed("freq_range_render", range->effect_class_name);
                continue;
            }

            range->effect_instance.reset(effect);

            if(!range->effect_settings.is_null())
            {
                effect->LoadSettings(range->effect_settings);
            }
        }
        
        SpatialEffect3D* effect = range->effect_instance.get();
        if(!effect) continue;
        
        nlohmann::json audio_params = range->effect_settings;
        audio_params["low_hz"] = (int)range->low_hz;
        audio_params["high_hz"] = (int)range->high_hz;
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

        std::unique_ptr<GridContext3D> local_room_grid;
        std::unique_ptr<GridContext3D> local_world_grid;
        if(effect->UseZoneGrid())
        {
            TryMakeZoneGridContextPair(zone_manager.get(),
                                       controller_transforms,
                                       range->zone_index,
                                       &controllers_managed_by_virtuals,
                                       true,
                                       room_grid.grid_scale_mm,
                                       world_grid.grid_scale_mm,
                                       local_room_grid,
                                       local_world_grid);
            if(local_room_grid)
            {
                local_room_grid->render_sequence = room_grid.render_sequence;
            }
            if(local_world_grid)
            {
                local_world_grid->render_sequence = world_grid.render_sequence;
            }
        }
        if(effect->UseZoneGrid() && range->zone_index != -1 && (!local_room_grid || !local_world_grid))
        {
            continue;
        }

        enum class TargetMode
        {
            AllControllers,
            SingleController,
            ZoneControllers,
            None
        };
        TargetMode target_mode = TargetMode::None;
        int target_controller_idx = -1;
        std::unordered_set<int> zone_controller_indices;
        if(range->zone_index == -1)
        {
            target_mode = TargetMode::AllControllers;
        }
        else if(range->zone_index <= -1000)
        {
            target_controller_idx = -(range->zone_index + 1000);
            if(target_controller_idx >= 0 && target_controller_idx < (int)controller_transforms.size())
            {
                target_mode = TargetMode::SingleController;
            }
        }
        else if(zone_manager && range->zone_index >= 0 && range->zone_index < zone_manager->GetZoneCount())
        {
            Zone3D* zone = zone_manager->GetZone(range->zone_index);
            if(zone)
            {
                const std::vector<int>& controllers = zone->GetControllers();
                zone_controller_indices.insert(controllers.begin(), controllers.end());
                if(!zone_controller_indices.empty())
                {
                    target_mode = TargetMode::ZoneControllers;
                }
            }
        }
        if(target_mode == TargetMode::None)
        {
            continue;
        }
        
        for(unsigned int ctrl_idx = 0; ctrl_idx < controller_transforms.size(); ctrl_idx++)
        {
            ControllerTransform* transform = controller_transforms[ctrl_idx].get();
            if(!transform) continue;
            if(transform->hidden_by_virtual) continue;
            if(transform->controller &&
               controllers_managed_by_virtuals.find(transform->controller) != controllers_managed_by_virtuals.end())
            {
                continue;
            }
            
            bool is_targeted = false;
            switch(target_mode)
            {
            case TargetMode::AllControllers:
                is_targeted = true;
                break;
            case TargetMode::SingleController:
                is_targeted = ((int)ctrl_idx == target_controller_idx);
                break;
            case TargetMode::ZoneControllers:
                is_targeted = (zone_controller_indices.find((int)ctrl_idx) != zone_controller_indices.end());
                break;
            default:
                break;
            }
            
            if(!is_targeted) continue;

            const bool requires_world = effect->RequiresWorldSpaceCoordinates();
            const bool use_world_bounds = effect->UseWorldGridBounds();
            const GridContext3D& active_grid = use_world_bounds
                ? (local_world_grid ? *local_world_grid : world_grid)
                : (local_room_grid ? *local_room_grid : room_grid);

            if(transform->virtual_controller)
            {
                VirtualController3D* vctrl = transform->virtual_controller;
                const std::vector<GridLEDMapping>& mappings = vctrl->GetMappings();
                
                for(unsigned int led_idx = 0; led_idx < mappings.size(); led_idx++)
                {
                    if(led_idx >= transform->led_positions.size())
                    {
                        continue;
                    }
                    const GridLEDMapping& mapping = mappings[led_idx];
                    if(!mapping.controller) continue;
                    if(mapping.zone_idx >= mapping.controller->zones.size())
                    {
                        continue;
                    }
                    const Vector3D& sample_pos = requires_world
                        ? transform->led_positions[led_idx].world_position
                        : transform->led_positions[led_idx].room_position;
                    float x = sample_pos.x, y = sample_pos.y, z = sample_pos.z;
                    MinecraftGame::SetRenderSampleIndexContext((int)led_idx, (int)transform->led_positions.size());
                    effect->ApplyAxisScale(x, y, z, active_grid);
                    effect->ApplyEffectRotation(x, y, z, active_grid);
                    RGBColor color = effect->EvaluateColorGrid(x, y, z, effect_time, active_grid);
                    if(!effect->IsPointOnActiveSurface(x, y, z, active_grid))
                        color = 0x00000000;
                    color = effect->PostProcessColorGrid(color);
                    unsigned int physical_led_idx = 0;
                    if(TryGetGlobalLedIndexForRange(mapping.controller, mapping.zone_idx, mapping.led_idx, &physical_led_idx))
                    {
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
                if(ctrl->zones.empty())
                {
                    continue;
                }
                
                for(unsigned int led_idx = 0; led_idx < transform->led_positions.size(); led_idx++)
                {
                    const Vector3D& sample_pos = requires_world
                        ? transform->led_positions[led_idx].world_position
                        : transform->led_positions[led_idx].room_position;
                    float x = sample_pos.x, y = sample_pos.y, z = sample_pos.z;
                    MinecraftGame::SetRenderSampleIndexContext((int)led_idx, (int)transform->led_positions.size());
                    effect->ApplyAxisScale(x, y, z, active_grid);
                    effect->ApplyEffectRotation(x, y, z, active_grid);
                    RGBColor color = effect->EvaluateColorGrid(x, y, z, effect_time, active_grid);
                    if(!effect->IsPointOnActiveSurface(x, y, z, active_grid))
                        color = 0x00000000;
                    color = effect->PostProcessColorGrid(color);
                    LEDPosition3D& led_pos = transform->led_positions[led_idx];
                    if(led_pos.zone_idx >= ctrl->zones.size())
                    {
                        continue;
                    }
                    unsigned int physical_led_idx = 0;
                    if(TryGetGlobalLedIndexForRange(ctrl, led_pos.zone_idx, led_pos.led_idx, &physical_led_idx))
                    {
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

    MinecraftGame::ClearRenderSampleIndexContext();
}
