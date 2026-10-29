// SPDX-License-Identifier: GPL-2.0-only

#include "EffectLibraryPanel.h"
#include "OpenRGB3DSpatialTab.h"
#include "PluginUiUtils.h"
#include "ui_EffectLibraryPanel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>

EffectLibraryPanel::EffectLibraryPanel(QWidget* parent)
    : QGroupBox(parent)
    , ui(new Ui::EffectLibraryPanel)
{
    ui->setupUi(this);
}

EffectLibraryPanel::~EffectLibraryPanel()
{
    delete ui;
}

void EffectLibraryPanel::bindTab(OpenRGB3DSpatialTab* tab)
{
    if(!tab || bound_)
    {
        return;
    }
    bound_ = true;

    PluginUiApplyMutedSecondaryLabel(ui->categoryLabel);
    PluginUiApplyMutedSecondaryLabel(ui->gameLabel);

    connect(ui->categoryCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), tab,
            &OpenRGB3DSpatialTab::effectLibraryCategoryChanged);
    connect(ui->gameCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), tab,
            &OpenRGB3DSpatialTab::effectLibraryGameChanged);
    connect(ui->searchEdit, &QLineEdit::textChanged, tab, &OpenRGB3DSpatialTab::effectLibrarySearchChanged);
    connect(ui->libraryList, &QListWidget::currentRowChanged, tab,
            &OpenRGB3DSpatialTab::effectLibrarySelectionChanged);
    connect(ui->libraryList, &QListWidget::itemDoubleClicked, tab,
            &OpenRGB3DSpatialTab::effectLibraryItemDoubleClicked);
    connect(ui->addToStackButton, &QPushButton::clicked, tab, &OpenRGB3DSpatialTab::effectLibraryAddClicked);
    connect(ui->unlockVolumeOnAddCheck, &QCheckBox::toggled, tab,
            &OpenRGB3DSpatialTab::effectLibraryUnlockVolumeOnAddToggled);

    tab->PopulateEffectLibraryCategories();
    tab->PopulateEffectLibrary();
    tab->SyncEffectLibraryUnlockVolumePreference();
}

QComboBox* EffectLibraryPanel::categoryCombo() const { return ui->categoryCombo; }
QLabel* EffectLibraryPanel::gameLabel() const { return ui->gameLabel; }
QComboBox* EffectLibraryPanel::gameCombo() const { return ui->gameCombo; }
QLineEdit* EffectLibraryPanel::searchEdit() const { return ui->searchEdit; }
QListWidget* EffectLibraryPanel::libraryList() const { return ui->libraryList; }
QCheckBox* EffectLibraryPanel::unlockVolumeOnAddCheck() const { return ui->unlockVolumeOnAddCheck; }
QPushButton* EffectLibraryPanel::addToStackButton() const { return ui->addToStackButton; }
