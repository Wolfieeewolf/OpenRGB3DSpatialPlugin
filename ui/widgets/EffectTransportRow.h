// SPDX-License-Identifier: GPL-2.0-only

#ifndef EFFECTTRANSPORTROW_H
#define EFFECTTRANSPORTROW_H

#include <QWidget>

class QPushButton;

namespace Ui {
class EffectTransportRow;
}

class EffectTransportRow : public QWidget
{
public:
    explicit EffectTransportRow(QWidget* parent = nullptr);
    ~EffectTransportRow() override;

    QPushButton* startEffectButton() const;
    QPushButton* stopEffectButton() const;

private:
    Ui::EffectTransportRow* ui = nullptr;
};

#endif
