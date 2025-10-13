/*---------------------------------------------------------*\
| OpenRGB3DSpatialTab_Profiles.cpp                          |
|                                                           |
|   Unified Profiles tab (Layout + Effect profiles)        |
|                                                           |
|   Date: 2025-10-05                                        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#include "OpenRGB3DSpatialTab.h"
#include "LogManager.h"
#include <QMessageBox>
#include <QInputDialog>

void OpenRGB3DSpatialTab::SetupProfilesTab(QTabWidget* tab_widget)
{
    QWidget* profiles_tab = new QWidget();
    QVBoxLayout* profiles_layout = new QVBoxLayout(profiles_tab);
    profiles_layout->setSpacing(10);
    profiles_layout->setContentsMargins(10, 10, 10, 10);

    /*---------------------------------------------------------*\
    | Layout Profiles Section                                  |
    \*---------------------------------------------------------*/
    QGroupBox* layout_group = new QGroupBox("Layout Profile");
    QVBoxLayout* layout_layout = new QVBoxLayout(layout_group);
    layout_layout->setSpacing(8);

    QLabel* layout_label = new QLabel("Save/Load controller positions, zones, and reference points:");
    layout_label->setWordWrap(true);
    layout_label->setStyleSheet("color: gray; font-size: 9pt;");
    layout_layout->addWidget(layout_label);

    /*---------------------------------------------------------*\
    | Layout Profile Dropdown                                  |
    \*---------------------------------------------------------*/
    QHBoxLayout* layout_combo_layout = new QHBoxLayout();
    layout_combo_layout->addWidget(new QLabel("Profile:"));

    layout_profiles_combo = new QComboBox();
    layout_profiles_combo->setMinimumWidth(200);
    connect(layout_profiles_combo, SIGNAL(currentIndexChanged(int)),
            this, SLOT(on_layout_profile_changed(int)));
    layout_combo_layout->addWidget(layout_profiles_combo);
    layout_combo_layout->addStretch();
    layout_layout->addLayout(layout_combo_layout);

    /*---------------------------------------------------------*\
    | Layout Profile Buttons                                   |
    \*---------------------------------------------------------*/
    QHBoxLayout* layout_buttons = new QHBoxLayout();
    layout_buttons->addStretch();

    QPushButton* save_layout_btn = new QPushButton("Save As...");
    save_layout_btn->setToolTip("Save current controller layout, zones, and reference points");
    connect(save_layout_btn, &QPushButton::clicked,
            this, &OpenRGB3DSpatialTab::on_save_layout_clicked);
    layout_buttons->addWidget(save_layout_btn);

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

    /*---------------------------------------------------------*\
    | Layout Auto-load Option                                  |
    \*---------------------------------------------------------*/
    auto_load_checkbox = new QCheckBox("Auto-load this profile on startup");
    auto_load_checkbox->setToolTip("Automatically load this layout when OpenRGB starts");
    connect(auto_load_checkbox, &QCheckBox::toggled,
            this, &OpenRGB3DSpatialTab::SaveCurrentLayoutName);
    layout_layout->addWidget(auto_load_checkbox);

    profiles_layout->addWidget(layout_group);

    /*---------------------------------------------------------*\
    | Effect Profiles Section                                  |
    \*---------------------------------------------------------*/
    QGroupBox* effect_group = new QGroupBox("Effect Profile");
    QVBoxLayout* effect_layout = new QVBoxLayout(effect_group);
    effect_layout->setSpacing(8);

    QLabel* effect_label = new QLabel("Save/Load single effect configurations from Effects tab:");
    effect_label->setWordWrap(true);
    effect_label->setStyleSheet("color: gray; font-size: 9pt;");
    effect_layout->addWidget(effect_label);

    /*---------------------------------------------------------*\
    | Effect Profile Dropdown                                  |
    \*---------------------------------------------------------*/
    QHBoxLayout* effect_combo_layout = new QHBoxLayout();
    effect_combo_layout->addWidget(new QLabel("Profile:"));

    effect_profiles_combo = new QComboBox();
    effect_profiles_combo->setMinimumWidth(200);
    connect(effect_profiles_combo, SIGNAL(currentIndexChanged(int)),
            this, SLOT(on_effect_profile_changed(int)));
    effect_combo_layout->addWidget(effect_profiles_combo);
    effect_combo_layout->addStretch();
    effect_layout->addLayout(effect_combo_layout);

    /*---------------------------------------------------------*\
    | Effect Profile Buttons                                   |
    \*---------------------------------------------------------*/
    QHBoxLayout* effect_buttons = new QHBoxLayout();
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

    /*---------------------------------------------------------*\
    | Effect Auto-load Option                                  |
    \*---------------------------------------------------------*/
    effect_auto_load_checkbox = new QCheckBox("Auto-load this profile on startup");
    effect_auto_load_checkbox->setToolTip("Automatically load this effect configuration when OpenRGB starts");
    connect(effect_auto_load_checkbox, &QCheckBox::toggled,
            this, &OpenRGB3DSpatialTab::SaveCurrentEffectProfileName);
    effect_layout->addWidget(effect_auto_load_checkbox);

    profiles_layout->addWidget(effect_group);

    /*---------------------------------------------------------*\
    | Spacer                                                   |
    \*---------------------------------------------------------*/
    profiles_layout->addStretch();

    /*---------------------------------------------------------*\
    | Populate dropdowns                                       |
    \*---------------------------------------------------------*/
    PopulateLayoutDropdown();
    PopulateEffectProfileDropdown();

    /*---------------------------------------------------------*\
    | Add tab to main tab widget                               |
    \*---------------------------------------------------------*/
    tab_widget->insertTab(0, profiles_tab, "Profiles");
}