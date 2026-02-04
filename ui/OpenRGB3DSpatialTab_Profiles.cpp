// SPDX-License-Identifier: GPL-2.0-only


#include "OpenRGB3DSpatialTab.h"
#include <QPalette>
#include <QScrollArea>
#include <QFrame>
#include <QMessageBox>
#include <QTimer>

void OpenRGB3DSpatialTab::SetupProfilesTab(QTabWidget* tab_widget)
{
    QWidget* profiles_tab = new QWidget();
    QVBoxLayout* profiles_layout = new QVBoxLayout(profiles_tab);
    profiles_layout->setSpacing(4);
    profiles_layout->setContentsMargins(4, 4, 4, 4);

    // Layout Profiles Section
    QGroupBox* layout_group = new QGroupBox("Layout Profile");
    QVBoxLayout* layout_layout = new QVBoxLayout(layout_group);
    layout_layout->setSpacing(4);
    layout_layout->setContentsMargins(2, 4, 2, 4);

    QLabel* layout_label = new QLabel("Save/Load controller positions, zones, and reference points:");
    layout_label->setWordWrap(true);
    layout_label->setForegroundRole(QPalette::PlaceholderText);
    layout_layout->addWidget(layout_label);

    // Layout Profile Dropdown
    QHBoxLayout* layout_combo_layout = new QHBoxLayout();
    layout_combo_layout->setSpacing(4);
    layout_combo_layout->addWidget(new QLabel("Profile:"));

    layout_profiles_combo = new QComboBox();
    layout_profiles_combo->setMinimumWidth(200);
    connect(layout_profiles_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &OpenRGB3DSpatialTab::on_layout_profile_changed);
    layout_combo_layout->addWidget(layout_profiles_combo);
    layout_combo_layout->addStretch();
    layout_layout->addLayout(layout_combo_layout);

    // Layout Profile Buttons
    QHBoxLayout* layout_buttons = new QHBoxLayout();
    layout_buttons->setSpacing(6);
    layout_buttons->addStretch();

    // Quick save button (enabled when dirty)
    save_layout_btn = new QPushButton("Save");
    save_layout_btn->setToolTip("Save changes to current layout profile");
    save_layout_btn->setEnabled(false);
    connect(save_layout_btn, &QPushButton::clicked,
            this, &OpenRGB3DSpatialTab::on_quick_save_layout_clicked);
    layout_buttons->addWidget(save_layout_btn);

    QPushButton* save_as_layout_btn = new QPushButton("Save As...");
    save_as_layout_btn->setToolTip("Save current controller layout, zones, and reference points as a new profile");
    connect(save_as_layout_btn, &QPushButton::clicked,
            this, &OpenRGB3DSpatialTab::on_save_layout_clicked);
    layout_buttons->addWidget(save_as_layout_btn);

    QPushButton* load_layout_btn = new QPushButton("Load");
    load_layout_btn->setToolTip("Load selected layout profile");
    connect(load_layout_btn, &QPushButton::clicked,
            this, &OpenRGB3DSpatialTab::on_load_layout_clicked);
    layout_buttons->addWidget(load_layout_btn);

    QPushButton* delete_layout_btn = new QPushButton("Delete");
    delete_layout_btn->setToolTip("Delete selected layout profile");
    connect(delete_layout_btn, &QPushButton::clicked,
            this, &OpenRGB3DSpatialTab::on_delete_layout_clicked);
    layout_buttons->addWidget(delete_layout_btn);

    layout_layout->addLayout(layout_buttons);

    // Layout Auto-load Option
    auto_load_checkbox = new QCheckBox("Auto-load this profile on startup");
    auto_load_checkbox->setToolTip("Automatically load this layout when OpenRGB starts");
    connect(auto_load_checkbox, &QCheckBox::toggled,
            this, &OpenRGB3DSpatialTab::SaveCurrentLayoutName);
    layout_layout->addWidget(auto_load_checkbox);

    profiles_layout->addWidget(layout_group);

    // Effect Profiles Section
    QGroupBox* effect_group = new QGroupBox("Effect Profile");
    QVBoxLayout* effect_layout = new QVBoxLayout(effect_group);
    effect_layout->setSpacing(4);
    effect_layout->setContentsMargins(2, 4, 2, 4);

    QLabel* effect_label = new QLabel("Save/Load single effect configurations from Effects tab:");
    effect_label->setWordWrap(true);
    effect_label->setForegroundRole(QPalette::PlaceholderText);
    effect_layout->addWidget(effect_label);

    // Effect Profile Dropdown
    QHBoxLayout* effect_combo_layout = new QHBoxLayout();
    effect_combo_layout->setSpacing(4);
    effect_combo_layout->addWidget(new QLabel("Profile:"));

    effect_profiles_combo = new QComboBox();
    effect_profiles_combo->setMinimumWidth(200);
    connect(effect_profiles_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &OpenRGB3DSpatialTab::on_effect_profile_changed);
    effect_combo_layout->addWidget(effect_profiles_combo);
    effect_combo_layout->addStretch();
    effect_layout->addLayout(effect_combo_layout);

    // Effect Profile Buttons
    QHBoxLayout* effect_buttons = new QHBoxLayout();
    effect_buttons->setSpacing(6);
    effect_buttons->addStretch();

    QPushButton* save_effect_btn = new QPushButton("Save As...");
    save_effect_btn->setToolTip("Save current effect configuration from Effects tab");
    connect(save_effect_btn, &QPushButton::clicked,
            this, &OpenRGB3DSpatialTab::on_save_effect_profile_clicked);
    effect_buttons->addWidget(save_effect_btn);

    QPushButton* load_effect_btn = new QPushButton("Load");
    load_effect_btn->setToolTip("Load selected effect profile into Effects tab");
    connect(load_effect_btn, &QPushButton::clicked,
            this, &OpenRGB3DSpatialTab::on_load_effect_profile_clicked);
    effect_buttons->addWidget(load_effect_btn);

    QPushButton* delete_effect_btn = new QPushButton("Delete");
    delete_effect_btn->setToolTip("Delete selected effect profile");
    connect(delete_effect_btn, &QPushButton::clicked,
            this, &OpenRGB3DSpatialTab::on_delete_effect_profile_clicked);
    effect_buttons->addWidget(delete_effect_btn);

    effect_layout->addLayout(effect_buttons);

    // Effect Auto-load Option
    effect_auto_load_checkbox = new QCheckBox("Auto-load this profile on startup");
    effect_auto_load_checkbox->setToolTip("Automatically load this effect configuration when OpenRGB starts");
    connect(effect_auto_load_checkbox, &QCheckBox::toggled,
            this, &OpenRGB3DSpatialTab::SaveCurrentEffectProfileName);
    effect_layout->addWidget(effect_auto_load_checkbox);

    profiles_layout->addWidget(effect_group);

    // Populate dropdowns
    PopulateLayoutDropdown();
    PopulateEffectProfileDropdown();

    QScrollArea* profiles_scroll = new QScrollArea();
    profiles_scroll->setWidget(profiles_tab);
    profiles_scroll->setWidgetResizable(true);
    profiles_scroll->setFrameShape(QFrame::NoFrame);
    profiles_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    tab_widget->insertTab(0, profiles_scroll, "Profiles");
}

//==============================================================================
// Dirty Flag System
//==============================================================================

void OpenRGB3DSpatialTab::SetLayoutDirty(bool dirty)
{
    if(layout_dirty == dirty)
    {
        return; // No change
    }
    
    layout_dirty = dirty;
    
    // Update Save button state and text
    if(save_layout_btn)
    {
        save_layout_btn->setEnabled(dirty);
        if(dirty)
        {
            save_layout_btn->setText("Save *");
            save_layout_btn->setToolTip("Save changes to current layout profile (unsaved changes)");
        }
        else
        {
            save_layout_btn->setText("Save");
            save_layout_btn->setToolTip("Save changes to current layout profile");
        }
    }
}

void OpenRGB3DSpatialTab::ClearLayoutDirty()
{
    SetLayoutDirty(false);
}

bool OpenRGB3DSpatialTab::PromptSaveIfDirty()
{
    if(!layout_dirty)
    {
        return true; // No changes, proceed
    }
    
    QMessageBox msgBox;
    msgBox.setWindowTitle("Unsaved Changes");
    msgBox.setText("The current layout has unsaved changes.");
    msgBox.setInformativeText("Do you want to save your changes?");
    msgBox.setStandardButtons(QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    msgBox.setDefaultButton(QMessageBox::Save);
    msgBox.setIcon(QMessageBox::Warning);
    
    int ret = msgBox.exec();
    
    switch(ret)
    {
        case QMessageBox::Save:
            on_quick_save_layout_clicked();
            return !layout_dirty; // Return true if save succeeded (dirty cleared)
            
        case QMessageBox::Discard:
            return true; // User chose to discard changes
            
        case QMessageBox::Cancel:
        default:
            return false; // User cancelled the operation
    }
}

void OpenRGB3DSpatialTab::on_quick_save_layout_clicked()
{
    // Get current profile name from dropdown
    if(!layout_profiles_combo || layout_profiles_combo->currentIndex() < 0)
    {
        QMessageBox::warning(this, "No Profile Selected",
                           "Please select a layout profile first, or use 'Save As...' to create a new one.");
        return;
    }
    
    QString profile_name = layout_profiles_combo->currentText();
    if(profile_name.isEmpty())
    {
        QMessageBox::warning(this, "No Profile Selected",
                           "Please select a layout profile first, or use 'Save As...' to create a new one.");
        return;
    }
    
    std::string layout_path = GetLayoutPath(profile_name.toStdString());
    
    // Update all settings from UI before saving
    if(grid_x_spin) custom_grid_x = grid_x_spin->value();
    if(grid_y_spin) custom_grid_y = grid_y_spin->value();
    if(grid_z_spin) custom_grid_z = grid_z_spin->value();
    if(room_width_spin) manual_room_width = room_width_spin->value();
    if(room_height_spin) manual_room_height = room_height_spin->value();
    if(room_depth_spin) manual_room_depth = room_depth_spin->value();
    
    SaveLayout(layout_path);
    ClearLayoutDirty();
    
    // Show brief feedback
    if(save_layout_btn)
    {
        QString original_text = save_layout_btn->text();
        save_layout_btn->setText("Saved!");
        save_layout_btn->setEnabled(false);
        QTimer::singleShot(1500, this, [this, original_text]()
        {
            if(save_layout_btn && !layout_dirty)
            {
                save_layout_btn->setText(original_text);
            }
        });
    }
}
