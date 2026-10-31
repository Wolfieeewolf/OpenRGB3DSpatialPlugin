// SPDX-License-Identifier: GPL-2.0-only

#ifndef EFFECTDOUBLESLIDERROW_H
#define EFFECTDOUBLESLIDERROW_H

#include <QWidget>
#include <functional>

class QDoubleSpinBox;
class QLabel;
class QObject;
class QSlider;

namespace Ui {
class EffectDoubleSliderRow;
}

/** Double twin of EffectSliderRow: QDoubleSpinBox (fine) over a thin slider (coarse). */
class EffectDoubleSliderRow : public QWidget
{
    Q_OBJECT
public:
    explicit EffectDoubleSliderRow(QWidget* parent = nullptr);
    ~EffectDoubleSliderRow() override;

    void setCaptionText(const QString& text);
    void setSuffix(const QString& suffix);
    void setPrefix(const QString& prefix);
    void setDecimals(int decimals);
    void setSingleStep(double step);

    QSlider* slider() const;
    QDoubleSpinBox* spinBox() const;
    QLabel* captionLabel() const;

    void configure(double min,
                   double max,
                   double value,
                   int decimals = 1,
                   double single_step = 0.1,
                   const QString& slider_tooltip = QString());

    /** Copy range/value/suffix/tooltip from an existing spin (does not take ownership). */
    void configureFromSpin(const QDoubleSpinBox* spin, const QString& slider_tooltip = QString());

    /** Reparent an existing spin into this row (keeps the caller's pointer valid). */
    void adoptExistingSpin(QDoubleSpinBox* spin);

    /** Reparent existing spin + slider (e.g. SceneTransform). link_spin_slider=false when
        an external controller owns sync (PositionAxisDragController / custom rotation wire). */
    void adoptExistingSpinAndSlider(QDoubleSpinBox* spin, QSlider* slider, bool link_spin_slider = true);

    QString captionText() const;
    void syncSpinValue(double value);

    void bindValueChanged(QObject* owner,
                          const std::function<void(double)>& apply_value,
                          const std::function<void()>& changed = nullptr);

    void setEnabled(bool enabled);

private:
    void wireSpinSliderLink();
    void syncSliderFromSpin();
    void syncSpinFromSlider(int slider_value);
    void applySliderChrome(const QString& slider_tooltip);

    void resetSpinSliderWiring();

    Ui::EffectDoubleSliderRow* ui = nullptr;
    QDoubleSpinBox* spin_ = nullptr;
    QSlider* slider_ = nullptr;
    bool spin_slider_linked_ = false;
    static constexpr int kSliderSteps = 1000;
};

#endif
