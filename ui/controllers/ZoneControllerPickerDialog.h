// SPDX-License-Identifier: GPL-2.0-only

#ifndef ZONECONTROLLERPICKERDIALOG_H
#define ZONECONTROLLERPICKERDIALOG_H

#include <QDialog>
#include <QString>
#include <vector>

class QCheckBox;

namespace Ui {
class ZoneControllerPickerDialog;
}

class ZoneControllerPickerDialog : public QDialog
{
    Q_OBJECT

public:
    struct Entry
    {
        QString label;
        bool    checked = false;
    };

    explicit ZoneControllerPickerDialog(QWidget* parent = nullptr);
    ~ZoneControllerPickerDialog() override;

    void setPromptText(const QString& text);
    void setWindowTitleText(const QString& title);
    void setEntries(const std::vector<Entry>& entries);

    std::vector<bool> checkedStates() const;

    static bool run(QWidget* parent,
                    const QString& window_title,
                    const QString& prompt,
                    const std::vector<Entry>& entries_in,
                    std::vector<bool>& checked_out);

private:
    Ui::ZoneControllerPickerDialog* ui = nullptr;
    std::vector<QCheckBox*>         checkboxes_;
};

#endif
