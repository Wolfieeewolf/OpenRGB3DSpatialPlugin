// SPDX-License-Identifier: GPL-2.0-only

#ifndef EFFECTLABELEDSPINROW_H
#define EFFECTLABELEDSPINROW_H

#include <QWidget>
#include <functional>

class QSpinBox;
class QObject;

namespace Ui {
class EffectLabeledSpinRow;
}

class EffectLabeledSpinRow : public QWidget
{
public:
    explicit EffectLabeledSpinRow(QWidget* parent = nullptr);
    ~EffectLabeledSpinRow() override;

    void setCaptionText(const QString& text);

    QSpinBox* spinBox() const;

    void configure(int min, int max, int value, const QString& tooltip = QString());

    void bindValueChanged(QObject* owner,
                          const std::function<void(int)>& apply_value,
                          const std::function<void()>& on_changed = nullptr);

private:
    Ui::EffectLabeledSpinRow* ui = nullptr;
};

#endif
