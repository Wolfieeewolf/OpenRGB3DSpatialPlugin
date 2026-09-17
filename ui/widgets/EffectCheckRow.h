// SPDX-License-Identifier: GPL-2.0-only

#ifndef EFFECTCHECKROW_H
#define EFFECTCHECKROW_H

#include <QWidget>
#include <functional>

class QCheckBox;
class QObject;

namespace Ui {
class EffectCheckRow;
}

class EffectCheckRow : public QWidget
{
public:
    explicit EffectCheckRow(QWidget* parent = nullptr);
    ~EffectCheckRow() override;

    QCheckBox* checkBox() const;

    void configure(const QString& text, bool checked, const QString& tooltip = QString());

    void bindToggled(QObject* owner,
                     const std::function<void(bool)>& apply_value,
                     const std::function<void()>& changed = nullptr);

private:
    Ui::EffectCheckRow* ui = nullptr;
};

#endif
