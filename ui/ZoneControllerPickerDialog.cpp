// SPDX-License-Identifier: GPL-2.0-only

#include "ZoneControllerPickerDialog.h"
#include "ui_ZoneControllerPickerDialog.h"

#include <QCheckBox>
#include <QVBoxLayout>

ZoneControllerPickerDialog::ZoneControllerPickerDialog(QWidget* parent)
    : QDialog(parent)
    , ui(new Ui::ZoneControllerPickerDialog)
{
    ui->setupUi(this);
    connect(ui->okButton, &QPushButton::clicked, this, &QDialog::accept);
    connect(ui->cancelButton, &QPushButton::clicked, this, &QDialog::reject);
}

ZoneControllerPickerDialog::~ZoneControllerPickerDialog()
{
    delete ui;
}

void ZoneControllerPickerDialog::setPromptText(const QString& text)
{
    ui->promptLabel->setText(text);
}

void ZoneControllerPickerDialog::setWindowTitleText(const QString& title)
{
    setWindowTitle(title);
}

void ZoneControllerPickerDialog::setEntries(const std::vector<Entry>& entries)
{
    checkboxes_.clear();
    QVBoxLayout* list_layout = ui->controllerListLayout;
    while(QLayoutItem* item = list_layout->takeAt(0))
    {
        if(QWidget* w = item->widget())
        {
            w->deleteLater();
        }
        delete item;
    }

    checkboxes_.reserve(entries.size());
    for(const Entry& entry : entries)
    {
        QCheckBox* checkbox = new QCheckBox(entry.label, ui->controllerListHost);
        checkbox->setChecked(entry.checked);
        list_layout->addWidget(checkbox);
        checkboxes_.push_back(checkbox);
    }
    list_layout->addStretch();
}

std::vector<bool> ZoneControllerPickerDialog::checkedStates() const
{
    std::vector<bool> out;
    out.reserve(checkboxes_.size());
    for(const QCheckBox* checkbox : checkboxes_)
    {
        out.push_back(checkbox && checkbox->isChecked());
    }
    return out;
}

bool ZoneControllerPickerDialog::run(QWidget* parent,
                                     const QString& window_title,
                                     const QString& prompt,
                                     const std::vector<Entry>& entries_in,
                                     std::vector<bool>& checked_out)
{
    ZoneControllerPickerDialog dialog(parent);
    dialog.setWindowTitleText(window_title);
    dialog.setPromptText(prompt);
    dialog.setEntries(entries_in);
    if(dialog.exec() != QDialog::Accepted)
    {
        return false;
    }
    checked_out = dialog.checkedStates();
    return checked_out.size() == entries_in.size();
}
