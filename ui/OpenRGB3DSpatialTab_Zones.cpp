// SPDX-License-Identifier: GPL-2.0-only


#include "OpenRGB3DSpatialTab.h"
#include <QInputDialog>
#include <QMessageBox>
#include <QDialog>
#include <QVBoxLayout>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QVariant>

// Zone Management
void OpenRGB3DSpatialTab::on_create_zone_clicked()
{
    if(!zone_manager) return;
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
        ControllerTransform* ctrl = controller_transforms[i].get();
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
        SetLayoutDirty(); // Mark layout as modified

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
    if(!zones_list || !zone_manager) return;
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
        ControllerTransform* ctrl = controller_transforms[i].get();
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
        SetLayoutDirty(); // Mark layout as modified

        QMessageBox::information(this, "Zone Updated",
                                QString("Zone '%1' now has %2 controller(s).")
                                .arg(zone_name).arg(zone->GetControllerCount()));
    }
}

void OpenRGB3DSpatialTab::on_delete_zone_clicked()
{
    if(!zones_list || !zone_manager) return;
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
        SetLayoutDirty(); // Mark layout as modified
    }
}

void OpenRGB3DSpatialTab::on_zone_selected(int index)
{
    if(!zone_manager) return;
    bool has_selection = (index >= 0 && index < zone_manager->GetZoneCount());
    if(edit_zone_button) edit_zone_button->setEnabled(has_selection);
    if(delete_zone_button) delete_zone_button->setEnabled(has_selection);
}

void OpenRGB3DSpatialTab::UpdateZonesList()
{
    if(!zones_list || !zone_manager)
    {
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

    // Also update the zone dropdowns in effects tab, effect stack tab, and audio tab
    UpdateEffectZoneCombo();
    UpdateStackEffectZoneCombo();
    UpdateFreqZoneCombo();
}

void OpenRGB3DSpatialTab::PopulateZoneTargetCombo(QComboBox* combo, int saved_value)
{
    if(!combo)
    {
        return;
    }

    combo->blockSignals(true);
    combo->clear();

    combo->addItem("All Controllers", QVariant(-1));

    if(zone_manager)
    {
        for(int i = 0; i < zone_manager->GetZoneCount(); i++)
        {
            Zone3D* zone = zone_manager->GetZone(i);
            if(zone)
            {
                QString zone_name = QString("[Zone] ") + QString::fromStdString(zone->GetName());
                combo->addItem(zone_name, QVariant(i));
            }
        }
    }

    for(unsigned int i = 0; i < controller_transforms.size(); i++)
    {
        ControllerTransform* transform = controller_transforms[i].get();
        QString base_name;
        QString prefix = "[Controller] ";

        if(transform)
        {
            if(transform->controller)
            {
                base_name = QString::fromStdString(transform->controller->name);
            }
            if(transform->virtual_controller && base_name.isEmpty())
            {
                prefix = "[Virtual] ";
                base_name = QString::fromStdString(transform->virtual_controller->GetName());
            }
        }

        if(base_name.isEmpty())
        {
            base_name = QString("Controller %1").arg((int)i);
        }

        combo->addItem(prefix + base_name, QVariant(-(int)(i + 1000)));
    }

    int restore_index = combo->findData(saved_value);
    if(restore_index < 0)
    {
        restore_index = combo->findData(-1);
    }

    if(restore_index < 0)
    {
        restore_index = 0;
    }

    combo->setCurrentIndex(restore_index);
    combo->blockSignals(false);
}

int OpenRGB3DSpatialTab::ResolveZoneTargetSelection(const QComboBox* combo) const
{
    if(!combo)
    {
        return -1;
    }

    QVariant data = combo->currentData();
    return data.isValid() ? data.toInt() : -1;
}

void OpenRGB3DSpatialTab::UpdateEffectZoneCombo()
{
    PopulateZoneTargetCombo(effect_zone_combo, ResolveZoneTargetSelection(effect_zone_combo));
}

void OpenRGB3DSpatialTab::SaveZones()
{
    // Mark layout as dirty - zones will be saved when user saves layout profile
    SetLayoutDirty();
}

void OpenRGB3DSpatialTab::LoadZones()
{
    // Zones are automatically loaded as part of the layout JSON
    // when LoadLayout() is called. This function is kept for
    // future standalone load functionality if needed.
    
}
