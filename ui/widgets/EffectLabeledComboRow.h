// SPDX-License-Identifier: GPL-2.0-only

#ifndef EFFECTLABELEDCOMBOROW_H
#define EFFECTLABELEDCOMBOROW_H

#include <QWidget>

class QComboBox;
class QLabel;

namespace Ui {
class EffectLabeledComboRow;
}

class EffectLabeledComboRow : public QWidget
{
public:
    explicit EffectLabeledComboRow(QWidget* parent = nullptr);
    ~EffectLabeledComboRow() override;

    void setCaptionText(const QString& text);
    QString captionText() const;
    QLabel* captionLabel() const;

    QComboBox* combo() const;

private:
    Ui::EffectLabeledComboRow* ui = nullptr;
};

#endif
