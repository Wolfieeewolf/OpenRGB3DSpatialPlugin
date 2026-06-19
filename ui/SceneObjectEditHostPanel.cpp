// SPDX-License-Identifier: GPL-2.0-only

#include "SceneObjectEditHostPanel.h"
#include "OpenRGB3DSpatialTab.h"
#include "SceneObjectSpacingPanel.h"
#include "SceneTransformPanel.h"
#include "SpatialControllerEntryKey.h"
#include "ui_SceneObjectEditHostPanel.h"

#include <QFont>

SceneObjectEditHostPanel::SceneObjectEditHostPanel(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::SceneObjectEditHostPanel)
{
    ui->setupUi(this);

    QFont title_font = ui->titleLabel->font();
    title_font.setBold(true);
    ui->titleLabel->setFont(title_font);

    ui->editLayoutButton->setToolTip(
        tr("Open the custom 3D controller editor for LED grid and shape (library layout)."));
    ui->editReferencePointButton->setToolTip(
        tr("Open Object Creator to edit this reference point (name, type, color)."));
    ui->editDisplayPlaneButton->setToolTip(
        tr("Edit display plane name, size, and screen capture source."));
}

SceneObjectEditHostPanel::~SceneObjectEditHostPanel()
{
    delete ui;
}

SceneObjectSpacingPanel* SceneObjectEditHostPanel::spacingPanel() const
{
    return ui ? ui->spacingPanel : nullptr;
}

SceneTransformPanel* SceneObjectEditHostPanel::transformPanel() const
{
    return ui ? ui->transformPanel : nullptr;
}

void SceneObjectEditHostPanel::bindTab(OpenRGB3DSpatialTab* tab)
{
    if(tab_ == tab)
    {
        return;
    }

    tab_ = tab;

    if(spacingPanel())
    {
        spacingPanel()->bindTab(tab);
    }

    if(transformPanel())
    {
        transformPanel()->bindTab(tab);
    }

    connect(ui->backButton, &QPushButton::clicked, tab, &OpenRGB3DSpatialTab::HideSceneObjectEditPanel);
    connect(ui->editLayoutButton, &QPushButton::clicked, tab, [tab]() {
        if(tab)
        {
            tab->EditCustomControllerForCurrentSceneSelection();
        }
    });
    connect(ui->editReferencePointButton, &QPushButton::clicked, tab, [tab]() {
        if(tab)
        {
            tab->EditReferencePointForCurrentSceneSelection();
        }
    });
    connect(ui->editDisplayPlaneButton, &QPushButton::clicked, tab, [tab]() {
        if(tab)
        {
            tab->EditDisplayPlaneForCurrentSceneSelection();
        }
    });
}

void SceneObjectEditHostPanel::syncFromSceneRow(int scene_list_row)
{
    scene_list_row_ = scene_list_row;

    if(!tab_ || !ui)
    {
        return;
    }

    if(scene_list_row < 0)
    {
        ui->titleLabel->setText(tr("Scene object"));
        if(spacingPanel())
        {
            spacingPanel()->setTransformIndex(-1);
            spacingPanel()->setSpacingEnabled(false);
        }
        ui->editLayoutButton->setVisible(false);
        ui->editReferencePointButton->setVisible(false);
        ui->editDisplayPlaneButton->setVisible(false);
        return;
    }

    ui->titleLabel->setText(tab_->sceneControllerRowText(scene_list_row));

    bool show_layout  = false;
    bool show_ref     = false;
    bool show_display = false;
    int  transform_index = -1;
    bool show_spacing    = false;

    if(tab_->sceneControllerRowHasUserRole(scene_list_row))
    {
        const SpatialControllerEntryKey key = tab_->sceneControllerRowKey(scene_list_row);
        if(key.first == -2)
        {
            show_ref = true;
        }
        else if(key.first == -3)
        {
            show_display = true;
        }
    }
    else
    {
        transform_index = tab_->ControllerListRowToTransformIndex(scene_list_row);
        if(transform_index >= 0)
        {
            float x = 10.0f;
            float y = 0.0f;
            float z = 0.0f;
            if(tab_->GetTransformLedSpacing(transform_index, x, y, z))
            {
                show_spacing = true;
                if(spacingPanel())
                {
                    spacingPanel()->setSpacingMm(x, y, z);
                }
            }

            if(tab_->ControllerTransformHasVirtualController(transform_index))
            {
                show_layout = true;
            }
        }
    }

    if(spacingPanel())
    {
        spacingPanel()->setTransformIndex(transform_index);
        spacingPanel()->setSpacingEnabled(show_spacing);
    }
    ui->editLayoutButton->setVisible(show_layout);
    ui->editReferencePointButton->setVisible(show_ref);
    ui->editDisplayPlaneButton->setVisible(show_display);
}
