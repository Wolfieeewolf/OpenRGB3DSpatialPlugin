// SPDX-License-Identifier: GPL-2.0-only

#ifndef EFFECTSECTIONHEADING_H
#define EFFECTSECTIONHEADING_H

#include <QWidget>

class QLabel;

namespace Ui {
class EffectSectionHeading;
}

class EffectSectionHeading : public QWidget
{
public:
    explicit EffectSectionHeading(QWidget* parent = nullptr);
    ~EffectSectionHeading() override;

    void setTitle(const QString& title);

    QLabel* titleLabel() const;

private:
    Ui::EffectSectionHeading* ui = nullptr;
};

#endif
