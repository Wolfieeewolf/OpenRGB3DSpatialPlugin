/*---------------------------------------------------------*\
| OpenRGB3DSpatialTab_Zones.cpp                            |
|                                                           |
|   Zone management functions                              |
|                                                           |
|   Date: 2025-10-03                                        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include "OpenRGB3DSpatialTab.h"
#include "LogManager.h"
#include <QInputDialog>
#include <QMessageBox>
#include <QDialog>
#include <QVBoxLayout>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>

/*---------------------------------------------------------*\
| Zone Management                                          |
\*---------------------------------------------------------*/

void OpenRGB3DSpatialTab::on_create_zone_clicked()
{
    // Prompt for zone name
    bool ok;
    QString zone_name = QInputDialog::getText(this, "Create Zone",
                                              "Zone name:", QLineEdit::Normal,
                                              "", &ok);

    if(!ok || zone_name.isEmpty())
    {
        return;
    }

    // Check if zone already exists
    if(zone_manager->ZoneExists(zone_name.toStdString()))
    {
        QMessageBox::warning(this, "Zone Exists", "A zone with this name already exists.");
        return;
    }

    // Create the zone
    Zone3D* zone = zone_manager->CreateZone(zone_name.toStdString());

    // Show dialog to select controllers
    QDialog dialog(this);
    dialog.setWindowTitle("Select Controllers for Zone");
    QVBoxLayout* layout = new QVBoxLayout();

    QLabel* label = new QLabel(QString("Select controllers to add to zone '%1':").arg(zone_name));
    layout->addWidget(label);

    // Create checkboxes for each controller
    std::vector<QCheckBox*> checkboxes;
    for(size_t i = 0; i < controller_transforms.size(); i++)
    {
        ControllerTransform* ctrl = controller_transforms[i];
        QString name;

        if(ctrl->virtual_controller)
        {
            name = QString("[Custom] ") + QString::fromStdString(ctrl->virtual_controller->GetName());
        }
        else if(ctrl->controller)
        {
            name = QString::fromStdString(ctrl->controller->name);
            if(ctrl->granularity == 1 && ctrl->item_idx < (int)ctrl->controller->zones.size())
            {
                name += " - " + QString::fromStdString(ctrl->controller->zones[ctrl->item_idx].name);
            }
        }
        else
        {
            name = QString("Controller %1").arg(i);
        }

        QCheckBox* checkbox = new QCheckBox(name);
        layout->addWidget(checkbox);
        checkboxes.push_back(checkbox);
    }

    // OK/Cancel buttons
    QHBoxLayout* button_layout = new QHBoxLayout();
    QPushButton* ok_button = new QPushButton("OK");
    QPushButton* cancel_button = new QPushButton("Cancel");
    button_layout->addWidget(ok_button);
    button_layout->addWidget(cancel_button);
    layout->addLayout(button_layout);

    connect(ok_button, &QPushButton::clicked, &dialog, &QDialog::accept);
    connect(cancel_button, &QPushButton::clicked, &dialog, &QDialog::reject);

    dialog.setLayout(layout);

    if(dialog.exec() == QDialog::Accepted)
    {
        // Add selected controllers to zone
        for(size_t i = 0; i < checkboxes.size(); i++)
        {
            if(checkboxes[i]->isChecked())
            {
                zone->AddController((int)i);
            }
        }

        UpdateZonesList();
        SaveZones();

        QMessageBox::information(this, "Zone Created",
                                QString("Zone '%1' created with %2 controller(s).")
                                .arg(zone_name).arg(zone->GetControllerCount()));
    }
    else
    {
        // User cancelled - delete the zone
        zone_manager->DeleteZone(zone_name.toStdString());
    }
}

void OpenRGB3DSpatialTab::on_edit_zone_clicked()
{
    int selected_idx = zones_list->currentRow();
    if(selected_idx < 0 || selected_idx >= zone_manager->GetZoneCount())
    {
        return;
    }

    Zone3D* zone = zone_manager->GetZone(selected_idx);
    if(!zone)
    {
        return;
    }

    QString zone_name = QString::fromStdString(zone->GetName());

    // Show dialog to modify controller selection
    QDialog dialog(this);
    dialog.setWindowTitle(QString("Edit Zone: %1").arg(zone_name));
    QVBoxLayout* layout = new QVBoxLayout();

    QLabel* label = new QLabel(QString("Select controllers for zone '%1':").arg(zone_name));
    layout->addWidget(label);

    // Create checkboxes for each controller
    std::vector<QCheckBox*> checkboxes;
    for(size_t i = 0; i < controller_transforms.size(); i++)
    {
        ControllerTransform* ctrl = controller_transforms[i];
        QString name;

        if(ctrl->virtual_controller)
        {
            name = QString("[Custom] ") + QString::fromStdString(ctrl->virtual_controller->GetName());
        }
        else if(ctrl->controller)
        {
            name = QString::fromStdString(ctrl->controller->name);
            if(ctrl->granularity == 1 && ctrl->item_idx < (int)ctrl->controller->zones.size())
            {
                name += " - " + QString::fromStdString(ctrl->controller->zones[ctrl->item_idx].name);
            }
        }
        else
        {
            name = QString("Controller %1").arg(i);
        }

        QCheckBox* checkbox = new QCheckBox(name);
        // Check if this controller is already in the zone
        checkbox->setChecked(zone->ContainsController((int)i));
        layout->addWidget(checkbox);
        checkboxes.push_back(checkbox);
    }

    // OK/Cancel buttons
    QHBoxLayout* button_layout = new QHBoxLayout();
    QPushButton* ok_button = new QPushButton("OK");
    QPushButton* cancel_button = new QPushButton("Cancel");
    button_layout->addWidget(ok_button);
    button_layout->addWidget(cancel_button);
    layout->addLayout(button_layout);

    connect(ok_button, &QPushButton::clicked, &dialog, &QDialog::accept);
    connect(cancel_button, &QPushButton::clicked, &dialog, &QDialog::reject);

    dialog.setLayout(layout);

    if(dialog.exec() == QDialog::Accepted)
    {
        // Update zone with selected controllers
        zone->ClearControllers();
        for(size_t i = 0; i < checkboxes.size(); i++)
        {
            if(checkboxes[i]->isChecked())
            {
                zone->AddController((int)i);
            }
        }

        UpdateZonesList();
        SaveZones();

        QMessageBox::information(this, "Zone Updated",
                                QString("Zone '%1' now has %2 controller(s).")
                                .arg(zone_name).arg(zone->GetControllerCount()));
    }
}

void OpenRGB3DSpatialTab::on_delete_zone_clicked()
{
    int selected_idx = zones_list->currentRow();
    if(selected_idx < 0 || selected_idx >= zone_manager->GetZoneCount())
    {
        return;
    }

    Zone3D* zone = zone_manager->GetZone(selected_idx);
    if(!zone)
    {
        return;
    }

    QString zone_name = QString::fromStdString(zone->GetName());

    QMessageBox::StandardButton reply = QMessageBox::question(
        this, "Delete Zone",
        QString("Are you sure you want to delete zone '%1'?").arg(zone_name),
        QMessageBox::Yes | QMessageBox::No
    );

    if(reply == QMessageBox::Yes)
    {
        zone_manager->DeleteZone(selected_idx);
        UpdateZonesList();
        SaveZones();
    }
}

void OpenRGB3DSpatialTab::on_zone_selected(int index)
{
    bool has_selection = (index >= 0 && index < zone_manager->GetZoneCount());
    edit_zone_button->setEnabled(has_selection);
    delete_zone_button->setEnabled(has_selection);
}

void OpenRGB3DSpatialTab::UpdateZonesList()
{
    if(!zones_list || !zone_manager)
    {
        LOG_WARNING("[OpenRGB3DSpatialPlugin] UpdateZonesList called but UI not ready");
        return;
    }

    zones_list->clear();

    for(int i = 0; i < zone_manager->GetZoneCount(); i++)
    {
        Zone3D* zone = zone_manager->GetZone(i);
        if(zone)
        {
            QString item_text = QString("%1 (%2 controllers)")
                                .arg(QString::fromStdString(zone->GetName()))
                                .arg(zone->GetControllerCount());
            zones_list->addItem(item_text);
        }
    }

    // Also update the zone dropdown in effects tab
    UpdateEffectZoneCombo();
}

void OpenRGB3DSpatialTab::UpdateEffectZoneCombo()
{
    if(!effect_zone_combo)
    {
        LOG_WARNING("[OpenRGB3DSpatialPlugin] UpdateEffectZoneCombo called but combo not ready");
        return;
    }

    if(!zone_manager)
    {
        LOG_WARNING("[OpenRGB3DSpatialPlugin] UpdateEffectZoneCombo called but zone_manager is null");
        return;
    }

    effect_zone_combo->blockSignals(true);
    effect_zone_combo->clear();

    // Always add "All Controllers" as first option
    effect_zone_combo->addItem("All Controllers");

    // Add all zones
    for(int i = 0; i < zone_manager->GetZoneCount(); i++)
    {
        Zone3D* zone = zone_manager->GetZone(i);
        if(zone)
        {
            QString name = QString::fromStdString(zone->GetName());
            effect_zone_combo->addItem(name);
        }
    }

    effect_zone_combo->blockSignals(false);
    LOG_VERBOSE("[OpenRGB3DSpatialPlugin] Effect zone combo updated with %d zones", zone_manager->GetZoneCount());
}

void OpenRGB3DSpatialTab::SaveZones()
{
    // Zones are automatically saved as part of the layout JSON
    // when SaveLayout() is called. This function is kept for
    // future standalone save functionality if needed.
    LOG_VERBOSE("[OpenRGB3DSpatialPlugin] Zones will be saved with next layout save (%d zones)",
                zone_manager ? zone_manager->GetZoneCount() : 0);
}

void OpenRGB3DSpatialTab::LoadZones()
{
    // Zones are automatically loaded as part of the layout JSON
    // when LoadLayout() is called. This function is kept for
    // future standalone load functionality if needed.
    LOG_VERBOSE("[OpenRGB3DSpatialPlugin] Zones loaded from layout (%d zones)",
                zone_manager ? zone_manager->GetZoneCount() : 0);
}
