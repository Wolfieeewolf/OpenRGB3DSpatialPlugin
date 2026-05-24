// SPDX-License-Identifier: GPL-2.0-only

#ifndef EFFECTINFOLABEL_H
#define EFFECTINFOLABEL_H

#include <QWidget>

class QLabel;

namespace Ui {
class EffectInfoLabel;
}

class EffectInfoLabel : public QWidget
{
public:
    explicit EffectInfoLabel(QWidget* parent = nullptr);
    ~EffectInfoLabel() override;

    void setText(const QString& text);
    void setAlignment(Qt::Alignment alignment);

    QLabel* label() const;

private:
    Ui::EffectInfoLabel* ui = nullptr;
};

#endif
