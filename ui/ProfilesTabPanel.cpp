// SPDX-License-Identifier: GPL-2.0-only

#include "ProfilesTabPanel.h"
#include "OpenRGB3DSpatialTab.h"
#include "PluginUiUtils.h"
#include "ui_ProfilesTabPanel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>

ProfilesTabPanel::ProfilesTabPanel(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::ProfilesTabPanel)
{
    ui->setupUi(this);

    PluginUiApplyMutedSecondaryLabel(ui->layoutHelpLabel->label());
    PluginUiApplyMutedSecondaryLabel(ui->effectHelpLabel->label());
}

ProfilesTabPanel::~ProfilesTabPanel()
{
    delete ui;
}

QComboBox* ProfilesTabPanel::layoutProfilesCombo() const { return ui->layoutProfilesCombo; }
QPushButton* ProfilesTabPanel::saveLayoutButton() const { return ui->saveLayoutButton; }
QPushButton* ProfilesTabPanel::saveAsLayoutButton() const { return ui->saveAsLayoutButton; }
QPushButton* ProfilesTabPanel::loadLayoutButton() const { return ui->loadLayoutButton; }
QPushButton* ProfilesTabPanel::deleteLayoutButton() const { return ui->deleteLayoutButton; }
QCheckBox* ProfilesTabPanel::autoLoadLayoutCheckbox() const { return ui->autoLoadLayoutCheckbox; }
QComboBox* ProfilesTabPanel::effectProfilesCombo() const { return ui->effectProfilesCombo; }
QPushButton* ProfilesTabPanel::saveEffectProfileButton() const { return ui->saveEffectProfileButton; }
QPushButton* ProfilesTabPanel::loadEffectProfileButton() const { return ui->loadEffectProfileButton; }
QPushButton* ProfilesTabPanel::deleteEffectProfileButton() const { return ui->deleteEffectProfileButton; }
QCheckBox* ProfilesTabPanel::autoLoadEffectProfileCheckbox() const { return ui->autoLoadEffectProfileCheckbox; }
QPushButton* ProfilesTabPanel::openConfigFolderButton() const { return ui->openConfigFolderButton; }

void ProfilesTabPanel::connectTab(OpenRGB3DSpatialTab* tab)
{
    if(!tab)
    {
        return;
    }

    ui->saveLayoutButton->setToolTip("Save changes to current layout profile");
    ui->saveLayoutButton->setEnabled(false);

    ui->saveAsLayoutButton->setToolTip(
        "Save current controller layout, zones, and reference points as a new profile");
    ui->loadLayoutButton->setToolTip("Load selected layout profile");
    ui->deleteLayoutButton->setToolTip("Delete selected layout profile");

    ui->autoLoadLayoutCheckbox->setToolTip("Automatically load this layout when OpenRGB starts");

    ui->saveEffectProfileButton->setToolTip("Save current effect configuration from Effects tab");
    ui->loadEffectProfileButton->setToolTip("Load selected effect profile into Effects tab");
    ui->deleteEffectProfileButton->setToolTip("Delete selected effect profile");

    ui->autoLoadEffectProfileCheckbox->setToolTip(
        "Automatically load this effect configuration when OpenRGB starts");

    ui->openConfigFolderButton->setToolTip(
        "Open plugins/settings/OpenRGB3DSpatialPlugin/");

    connect(ui->layoutProfilesCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), tab,
            &OpenRGB3DSpatialTab::on_layout_profile_changed);
    connect(ui->saveLayoutButton, &QPushButton::clicked, tab,
            &OpenRGB3DSpatialTab::on_quick_save_layout_clicked);
    connect(ui->saveAsLayoutButton, &QPushButton::clicked, tab,
            &OpenRGB3DSpatialTab::on_save_layout_clicked);
    connect(ui->loadLayoutButton, &QPushButton::clicked, tab,
            &OpenRGB3DSpatialTab::on_load_layout_clicked);
    connect(ui->deleteLayoutButton, &QPushButton::clicked, tab,
            &OpenRGB3DSpatialTab::on_delete_layout_clicked);
    connect(ui->autoLoadLayoutCheckbox, &QCheckBox::toggled, tab,
            &OpenRGB3DSpatialTab::SaveCurrentLayoutName);

    connect(ui->effectProfilesCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), tab,
            &OpenRGB3DSpatialTab::on_effect_profile_changed);
    connect(ui->saveEffectProfileButton, &QPushButton::clicked, tab,
            &OpenRGB3DSpatialTab::on_save_effect_profile_clicked);
    connect(ui->loadEffectProfileButton, &QPushButton::clicked, tab,
            &OpenRGB3DSpatialTab::on_load_effect_profile_clicked);
    connect(ui->deleteEffectProfileButton, &QPushButton::clicked, tab,
            &OpenRGB3DSpatialTab::on_delete_effect_profile_clicked);
    connect(ui->autoLoadEffectProfileCheckbox, &QCheckBox::toggled, tab,
            &OpenRGB3DSpatialTab::SaveCurrentEffectProfileName);
    connect(ui->openConfigFolderButton, &QPushButton::clicked, tab,
            &OpenRGB3DSpatialTab::on_open_config_folder_clicked);
}
