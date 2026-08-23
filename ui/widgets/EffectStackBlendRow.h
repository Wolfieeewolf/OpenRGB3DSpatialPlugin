// SPDX-License-Identifier: GPL-2.0-only

#ifndef EFFECTSTACKBLENDROW_H
#define EFFECTSTACKBLENDROW_H

#include <QWidget>

class QComboBox;

namespace Ui {
class EffectStackBlendRow;
}

class EffectStackBlendRow : public QWidget
{
public:
    explicit EffectStackBlendRow(QWidget* parent = nullptr);
    ~EffectStackBlendRow() override;

    QComboBox* blendCombo() const;

private:
    void populateBlendItems();

    Ui::EffectStackBlendRow* ui = nullptr;
};

#endif
