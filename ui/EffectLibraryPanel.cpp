// SPDX-License-Identifier: GPL-2.0-only

#include "EffectLibraryPanel.h"
#include "OpenRGB3DSpatialTab.h"
#include "PluginUiUtils.h"
#include "ui_EffectLibraryPanel.h"

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
    if(!tab)
    {
        return;
    }

    PluginUiApplyMutedSecondaryLabel(ui->categoryLabel);
    PluginUiApplyMutedSecondaryLabel(ui->gameLabel);

    connect(ui->categoryCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), tab,
            &OpenRGB3DSpatialTab::on_effect_library_category_changed);
    connect(ui->gameCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), tab,
            &OpenRGB3DSpatialTab::on_effect_library_game_changed);
    connect(ui->searchEdit, &QLineEdit::textChanged, tab, &OpenRGB3DSpatialTab::on_effect_library_search_changed);
    connect(ui->libraryList, &QListWidget::currentRowChanged, tab,
            &OpenRGB3DSpatialTab::on_effect_library_selection_changed);
    connect(ui->libraryList, &QListWidget::itemDoubleClicked, tab,
            &OpenRGB3DSpatialTab::on_effect_library_item_double_clicked);
    connect(ui->addToStackButton, &QPushButton::clicked, tab, &OpenRGB3DSpatialTab::on_effect_library_add_clicked);

    tab->PopulateEffectLibraryCategories();
    tab->PopulateEffectLibrary();
}

QComboBox* EffectLibraryPanel::categoryCombo() const { return ui->categoryCombo; }
QLabel* EffectLibraryPanel::gameLabel() const { return ui->gameLabel; }
QComboBox* EffectLibraryPanel::gameCombo() const { return ui->gameCombo; }
QLineEdit* EffectLibraryPanel::searchEdit() const { return ui->searchEdit; }
QListWidget* EffectLibraryPanel::libraryList() const { return ui->libraryList; }
QPushButton* EffectLibraryPanel::addToStackButton() const { return ui->addToStackButton; }
