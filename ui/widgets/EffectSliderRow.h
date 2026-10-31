// SPDX-License-Identifier: GPL-2.0-only

#ifndef EFFECTSLIDERROW_H
#define EFFECTSLIDERROW_H

#include <QWidget>
#include <functional>

class QLabel;
class QObject;
class QSlider;
class QSpinBox;

namespace Ui {
class EffectSliderRow;
}

/** Standard numeric control: spin box (fine) over a thin slider (coarse).
    Matches Action Editor field chrome — use this plugin-wide for ranges. */
class EffectSliderRow : public QWidget
{
    Q_OBJECT
public:
    explicit EffectSliderRow(QWidget* parent = nullptr);
    ~EffectSliderRow() override;

    void setCaptionText(const QString& text);
    void setValueLabelMinimumWidth(int width);
    void setSuffix(const QString& suffix);
    void setPrefix(const QString& prefix);
    void setShowFormattedLabel(bool show);

    QSlider* slider() const;
    QSpinBox* spinBox() const;
    /** Optional pretty-print label (hidden unless format_value / setShowFormattedLabel). */
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
    void wireSpinSliderLink();
    void applyFormattedLabel(int value, const std::function<QString(int)>& format_value);

    Ui::EffectSliderRow* ui = nullptr;
    bool spin_slider_linked_ = false;
};

#endif
