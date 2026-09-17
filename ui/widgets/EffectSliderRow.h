// SPDX-License-Identifier: GPL-2.0-only

#ifndef EFFECTSLIDERROW_H
#define EFFECTSLIDERROW_H

#include <QWidget>
#include <functional>

class QLabel;
class QObject;
class QSlider;

namespace Ui {
class EffectSliderRow;
}

class EffectSliderRow : public QWidget
{
public:
    explicit EffectSliderRow(QWidget* parent = nullptr);
    ~EffectSliderRow() override;

    void setCaptionText(const QString& text);
    void setValueLabelMinimumWidth(int width);

    QSlider* slider() const;
    QLabel* valueLabel() const;

    void configure(int min, int max, int value, const QString& slider_tooltip = QString());

    QString captionText() const;
    void syncSliderValue(int value, const std::function<QString(int)>& format_value = nullptr);

    void bindValueChanged(QObject* owner,
                          const std::function<void(int)>& apply_value,
                          const std::function<QString(int)>& format_value,
                          const std::function<void()>& changed = nullptr);

    void setEnabled(bool enabled);

private:
    Ui::EffectSliderRow* ui = nullptr;
};

#endif
