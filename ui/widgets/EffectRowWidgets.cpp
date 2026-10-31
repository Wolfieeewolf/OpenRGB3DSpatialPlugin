// SPDX-License-Identifier: GPL-2.0-only

#include "EffectSliderRow.h"
#include "ui_EffectSliderRow.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>

#include <algorithm>

EffectSliderRow::EffectSliderRow(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::EffectSliderRow)
{
    ui->setupUi(this);
    ui->captionLabel->setVisible(false);
    wireSpinSliderLink();
}

EffectSliderRow::~EffectSliderRow()
{
    delete ui;
}

void EffectSliderRow::wireSpinSliderLink()
{
    if(spin_slider_linked_ || !ui)
    {
        return;
    }
    spin_slider_linked_ = true;
    /* Slider = coarse jumps; spin = fine steps / typed values (Action Editor pattern).
       Do not block slider on spin edits: setValue is a no-op when unchanged, and
       existing callers connect QSlider::valueChanged for applies. */
    QObject::connect(ui->valueSlider, &QSlider::valueChanged, ui->valueSpin, &QSpinBox::setValue);
    QObject::connect(ui->valueSpin, QOverload<int>::of(&QSpinBox::valueChanged),
                     ui->valueSlider, &QSlider::setValue);
}

void EffectSliderRow::setCaptionText(const QString& text)
{
    ui->captionLabel->setText(text);
    ui->captionLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    ui->captionLabel->setVisible(!text.isEmpty());
}

void EffectSliderRow::setValueLabelMinimumWidth(int width)
{
    ui->valueLabel->setMinimumWidth(width);
}

void EffectSliderRow::setSuffix(const QString& suffix)
{
    ui->valueSpin->setSuffix(suffix);
}

void EffectSliderRow::setPrefix(const QString& prefix)
{
    ui->valueSpin->setPrefix(prefix);
}

void EffectSliderRow::setShowFormattedLabel(bool show)
{
    ui->valueLabel->setVisible(show);
}

QSlider* EffectSliderRow::slider() const
{
    return ui->valueSlider;
}

QSpinBox* EffectSliderRow::spinBox() const
{
    return ui->valueSpin;
}

QLabel* EffectSliderRow::captionLabel() const
{
    return ui->captionLabel;
}

QLabel* EffectSliderRow::valueLabel() const
{
    return ui->valueLabel;
}

void EffectSliderRow::configure(int min, int max, int value, const QString& slider_tooltip)
{
    wireSpinSliderLink();
    const int lo = std::min(min, max);
    const int hi = std::max(min, max);
    const int v = std::clamp(value, lo, hi);
    {
        QSignalBlocker bs(ui->valueSlider);
        QSignalBlocker bn(ui->valueSpin);
        ui->valueSlider->setRange(lo, hi);
        ui->valueSpin->setRange(lo, hi);
        ui->valueSlider->setValue(v);
        ui->valueSpin->setValue(v);
    }
    /* Coarse page steps on the thin slider; spin keeps single-step = 1. */
    const int span = std::max(1, hi - lo);
    ui->valueSlider->setSingleStep(std::max(1, span / 100));
    ui->valueSlider->setPageStep(std::max(ui->valueSlider->singleStep() * 5, span / 20));
    ui->valueSlider->setFixedHeight(14);
    if(!slider_tooltip.isEmpty())
    {
        ui->valueSlider->setToolTip(slider_tooltip);
        ui->valueSpin->setToolTip(slider_tooltip + QStringLiteral("\nSpin for fine tuning; slider for larger jumps."));
    }
}

void EffectSliderRow::adoptExistingSpin(QSpinBox* spin)
{
    if(!spin || !ui || spin == ui->valueSpin)
    {
        return;
    }

    auto* spin_row = qobject_cast<QHBoxLayout*>(ui->spinRow);
    if(!spin_row)
    {
        return;
    }

    disconnect(ui->valueSlider, nullptr, ui->valueSpin, nullptr);
    disconnect(ui->valueSpin, nullptr, ui->valueSlider, nullptr);
    spin_slider_linked_ = false;

    spin_row->removeWidget(ui->valueSpin);
    ui->valueSpin->hide();
    ui->valueSpin->deleteLater();

    spin->setParent(this);
    spin->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    spin->setKeyboardTracking(false);
    spin_row->insertWidget(0, spin);
    ui->valueSpin = spin;

    configure(spin->minimum(), spin->maximum(), spin->value(), spin->toolTip());
    if(!spin->suffix().isEmpty())
    {
        setSuffix(spin->suffix());
    }
    if(!spin->prefix().isEmpty())
    {
        setPrefix(spin->prefix());
    }
}

QString EffectSliderRow::captionText() const
{
    return ui->captionLabel->text();
}

void EffectSliderRow::applyFormattedLabel(int value, const std::function<QString(int)>& format_value)
{
    if(!format_value)
    {
        return;
    }
    ui->valueLabel->setVisible(true);
    ui->valueLabel->setText(format_value(value));
}

void EffectSliderRow::syncSliderValue(int value, const std::function<QString(int)>& format_value)
{
    const int min = ui->valueSpin->minimum();
    const int max = ui->valueSpin->maximum();
    const int v = std::clamp(value, min, max);
    {
        QSignalBlocker bs(ui->valueSlider);
        QSignalBlocker bn(ui->valueSpin);
        ui->valueSlider->setValue(v);
        ui->valueSpin->setValue(v);
    }
    applyFormattedLabel(v, format_value);
}

void EffectSliderRow::bindValueChanged(QObject* owner,
                                       const std::function<void(int)>& apply_value,
                                       const std::function<QString(int)>& format_value,
                                       const std::function<void()>& changed)
{
    if(!owner)
    {
        return;
    }

    applyFormattedLabel(ui->valueSpin->value(), format_value);

    /* Emit once from the spin (slider writes into spin). */
    QObject::connect(ui->valueSpin, QOverload<int>::of(&QSpinBox::valueChanged), owner,
                     [this, apply_value, format_value, changed](int v) {
                         if(apply_value)
                         {
                             apply_value(v);
                         }
                         applyFormattedLabel(v, format_value);
                         if(changed)
                         {
                             changed();
                         }
                     });
}

void EffectSliderRow::setEnabled(bool enabled)
{
    ui->valueSlider->setEnabled(enabled);
    ui->valueSpin->setEnabled(enabled);
    ui->valueLabel->setEnabled(enabled);
    ui->captionLabel->setEnabled(enabled);
}

#include "EffectDoubleSliderRow.h"
#include "ui_EffectDoubleSliderRow.h"

#include <QDoubleSpinBox>
#include <QSizePolicy>
#include <QVBoxLayout>

#include <cmath>

EffectDoubleSliderRow::EffectDoubleSliderRow(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::EffectDoubleSliderRow)
{
    ui->setupUi(this);
    spin_ = ui->valueSpin;
    slider_ = ui->valueSlider;
    ui->captionLabel->setVisible(false);
    ui->captionLabel->setMinimumWidth(0);
    ui->captionLabel->setMaximumWidth(0);
    slider_->setRange(0, kSliderSteps);
    slider_->setFixedHeight(14);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    wireSpinSliderLink();
}

EffectDoubleSliderRow::~EffectDoubleSliderRow()
{
    delete ui;
}

void EffectDoubleSliderRow::syncSliderFromSpin()
{
    if(!spin_ || !slider_)
    {
        return;
    }
    const double lo = spin_->minimum();
    const double hi = spin_->maximum();
    const double span = std::max(1e-9, hi - lo);
    const int v = (int)std::lround(std::clamp((spin_->value() - lo) / span, 0.0, 1.0) * (double)kSliderSteps);
    QSignalBlocker b(slider_);
    slider_->setRange(0, kSliderSteps);
    slider_->setValue(v);
}

void EffectDoubleSliderRow::syncSpinFromSlider(int slider_value)
{
    if(!spin_ || !slider_)
    {
        return;
    }
    const double lo = spin_->minimum();
    const double hi = spin_->maximum();
    const int s_lo = slider_->minimum();
    const int s_hi = slider_->maximum();
    const int span_i = std::max(1, s_hi - s_lo);
    spin_->setValue(lo + ((double)(slider_value - s_lo) / (double)span_i) * (hi - lo));
}

void EffectDoubleSliderRow::resetSpinSliderWiring()
{
    if(slider_)
    {
        disconnect(slider_, nullptr, this, nullptr);
    }
    if(spin_)
    {
        disconnect(spin_, nullptr, this, nullptr);
    }
    spin_slider_linked_ = false;
}

void EffectDoubleSliderRow::wireSpinSliderLink()
{
    if(spin_slider_linked_ || !slider_ || !spin_)
    {
        return;
    }
    spin_slider_linked_ = true;
    QObject::connect(slider_, &QSlider::valueChanged, this, [this](int v) {
        syncSpinFromSlider(v);
    });
    QObject::connect(spin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double) {
        syncSliderFromSpin();
    });
}

void EffectDoubleSliderRow::applySliderChrome(const QString& slider_tooltip)
{
    if(!slider_)
    {
        return;
    }
    slider_->setFixedHeight(14);
    const int span = std::max(1, slider_->maximum() - slider_->minimum());
    slider_->setSingleStep(std::max(1, span / 100));
    slider_->setPageStep(std::max(slider_->singleStep() * 5, span / 20));
    if(!slider_tooltip.isEmpty())
    {
        slider_->setToolTip(slider_tooltip);
        if(spin_)
        {
            spin_->setToolTip(slider_tooltip + QStringLiteral("\nSpin for fine tuning; slider for larger jumps."));
        }
    }
}

void EffectDoubleSliderRow::setCaptionText(const QString& text)
{
    ui->captionLabel->setText(text);
    ui->captionLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    const bool show = !text.isEmpty();
    ui->captionLabel->setVisible(show);
    ui->captionLabel->setMinimumWidth(show ? 96 : 0);
    ui->captionLabel->setMaximumWidth(show ? QWIDGETSIZE_MAX : 0);
}

void EffectDoubleSliderRow::setSuffix(const QString& suffix)
{
    if(spin_)
    {
        spin_->setSuffix(suffix);
    }
}

void EffectDoubleSliderRow::setPrefix(const QString& prefix)
{
    if(spin_)
    {
        spin_->setPrefix(prefix);
    }
}

void EffectDoubleSliderRow::setDecimals(int decimals)
{
    if(spin_)
    {
        spin_->setDecimals(decimals);
    }
}

void EffectDoubleSliderRow::setSingleStep(double step)
{
    if(spin_)
    {
        spin_->setSingleStep(step);
    }
}

QSlider* EffectDoubleSliderRow::slider() const
{
    return slider_;
}

QDoubleSpinBox* EffectDoubleSliderRow::spinBox() const
{
    return spin_;
}

QLabel* EffectDoubleSliderRow::captionLabel() const
{
    return ui->captionLabel;
}

void EffectDoubleSliderRow::configure(double min,
                                      double max,
                                      double value,
                                      int decimals,
                                      double single_step,
                                      const QString& slider_tooltip)
{
    if(!spin_ || !slider_)
    {
        return;
    }
    wireSpinSliderLink();
    const double lo = std::min(min, max);
    const double hi = std::max(min, max);
    const double v = std::clamp(value, lo, hi);
    {
        QSignalBlocker bs(slider_);
        QSignalBlocker bn(spin_);
        spin_->setDecimals(decimals);
        spin_->setSingleStep(single_step);
        spin_->setRange(lo, hi);
        spin_->setValue(v);
        slider_->setRange(0, kSliderSteps);
    }
    syncSliderFromSpin();
    applySliderChrome(slider_tooltip);
}

void EffectDoubleSliderRow::configureFromSpin(const QDoubleSpinBox* spin, const QString& slider_tooltip)
{
    if(!spin)
    {
        return;
    }
    configure(spin->minimum(),
              spin->maximum(),
              spin->value(),
              spin->decimals(),
              spin->singleStep(),
              slider_tooltip.isEmpty() ? spin->toolTip() : slider_tooltip);
    if(!spin->suffix().isEmpty())
    {
        setSuffix(spin->suffix());
    }
    if(!spin->prefix().isEmpty())
    {
        setPrefix(spin->prefix());
    }
}

void EffectDoubleSliderRow::adoptExistingSpin(QDoubleSpinBox* spin)
{
    if(!spin || !ui || spin == spin_)
    {
        return;
    }

    auto* col = qobject_cast<QVBoxLayout*>(ui->controlColumn);
    if(!col)
    {
        return;
    }

    resetSpinSliderWiring();

    if(ui->valueSpin && ui->valueSpin != spin)
    {
        col->removeWidget(ui->valueSpin);
        ui->valueSpin->hide();
        ui->valueSpin->deleteLater();
        ui->valueSpin = nullptr;
    }

    spin_ = spin;
    spin_->setParent(this);
    spin_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    spin_->setKeyboardTracking(false);
    spin_->show();
    col->insertWidget(0, spin_);

    if(slider_)
    {
        slider_->setRange(0, kSliderSteps);
        slider_->show();
    }
    syncSliderFromSpin();
    applySliderChrome(spin_->toolTip());
    wireSpinSliderLink();
}

void EffectDoubleSliderRow::adoptExistingSpinAndSlider(QDoubleSpinBox* spin, QSlider* slider, bool link_spin_slider)
{
    if(!spin || !slider || !ui)
    {
        return;
    }

    auto* col = qobject_cast<QVBoxLayout*>(ui->controlColumn);
    if(!col)
    {
        return;
    }

    resetSpinSliderWiring();

    if(ui->valueSpin && ui->valueSpin != spin)
    {
        col->removeWidget(ui->valueSpin);
        ui->valueSpin->hide();
        ui->valueSpin->deleteLater();
        ui->valueSpin = nullptr;
    }
    if(ui->valueSlider && ui->valueSlider != slider)
    {
        col->removeWidget(ui->valueSlider);
        ui->valueSlider->hide();
        ui->valueSlider->deleteLater();
        ui->valueSlider = nullptr;
    }

    spin_ = spin;
    slider_ = slider;
    spin_->setParent(this);
    slider_->setParent(this);
    spin_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    slider_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    spin_->setKeyboardTracking(false);
    slider_->setFixedHeight(14);
    slider_->setMinimumWidth(48);

    while(col->count() > 0)
    {
        col->takeAt(0);
    }
    col->addWidget(spin_);
    col->addWidget(slider_);

    applySliderChrome(spin_->toolTip().isEmpty() ? slider_->toolTip() : spin_->toolTip());
    if(link_spin_slider)
    {
        syncSliderFromSpin();
        wireSpinSliderLink();
    }
}

QString EffectDoubleSliderRow::captionText() const
{
    return ui->captionLabel->text();
}

void EffectDoubleSliderRow::syncSpinValue(double value)
{
    if(!spin_)
    {
        return;
    }
    const double lo = spin_->minimum();
    const double hi = spin_->maximum();
    const double v = std::clamp(value, lo, hi);
    {
        QSignalBlocker bs(slider_);
        QSignalBlocker bn(spin_);
        spin_->setValue(v);
    }
    if(spin_slider_linked_)
    {
        syncSliderFromSpin();
    }
}

void EffectDoubleSliderRow::bindValueChanged(QObject* owner,
                                            const std::function<void(double)>& apply_value,
                                            const std::function<void()>& changed)
{
    if(!owner || !spin_)
    {
        return;
    }
    QObject::connect(spin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), owner,
                     [apply_value, changed](double v) {
                         if(apply_value)
                         {
                             apply_value(v);
                         }
                         if(changed)
                         {
                             changed();
                         }
                     });
}

void EffectDoubleSliderRow::setEnabled(bool enabled)
{
    if(ui)
    {
        ui->captionLabel->setEnabled(enabled);
    }
    if(slider_)
    {
        slider_->setEnabled(enabled);
    }
    if(spin_)
    {
        spin_->setEnabled(enabled);
    }
}

#include "EffectLabeledComboRow.h"
#include "ui_EffectLabeledComboRow.h"

#include <QComboBox>
#include <QLabel>

#include <algorithm>

EffectLabeledComboRow::EffectLabeledComboRow(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::EffectLabeledComboRow)
{
    ui->setupUi(this);
}

EffectLabeledComboRow::~EffectLabeledComboRow()
{
    delete ui;
}

void EffectLabeledComboRow::setCaptionText(const QString& text)
{
    ui->captionLabel->setText(text);
    ui->captionLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    ui->captionLabel->setMinimumWidth(std::max(96, ui->captionLabel->sizeHint().width()));
}

QString EffectLabeledComboRow::captionText() const
{
    return ui->captionLabel->text();
}

QLabel* EffectLabeledComboRow::captionLabel() const
{
    return ui->captionLabel;
}

QComboBox* EffectLabeledComboRow::combo() const
{
    return ui->valueCombo;
}
#include "EffectLabeledSpinRow.h"
#include "ui_EffectLabeledSpinRow.h"

#include <QSpinBox>

#include <algorithm>

EffectLabeledSpinRow::EffectLabeledSpinRow(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::EffectLabeledSpinRow)
{
    ui->setupUi(this);
}

EffectLabeledSpinRow::~EffectLabeledSpinRow()
{
    delete ui;
}

void EffectLabeledSpinRow::setCaptionText(const QString& text)
{
    ui->captionLabel->setText(text);
    ui->captionLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    ui->captionLabel->setMinimumWidth(std::max(96, ui->captionLabel->sizeHint().width()));
}

QSpinBox* EffectLabeledSpinRow::spinBox() const
{
    return ui->valueSpin;
}

void EffectLabeledSpinRow::configure(int min, int max, int value, const QString& tooltip)
{
    ui->valueSpin->setRange(min, max);
    ui->valueSpin->setValue(std::clamp(value, min, max));
    if(!tooltip.isEmpty())
    {
        ui->valueSpin->setToolTip(tooltip);
    }
}

void EffectLabeledSpinRow::bindValueChanged(QObject* owner,
                                            const std::function<void(int)>& apply_value,
                                            const std::function<void()>& changed)
{
    if(!owner)
    {
        return;
    }

    QObject::connect(ui->valueSpin, QOverload<int>::of(&QSpinBox::valueChanged), owner,
                     [apply_value, changed](int v) {
                         if(apply_value)
                         {
                             apply_value(v);
                         }
                         if(changed)
                         {
                             changed();
                         }
                     });
}
#include "EffectCheckRow.h"
#include "ui_EffectCheckRow.h"

#include <QCheckBox>

EffectCheckRow::EffectCheckRow(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::EffectCheckRow)
{
    ui->setupUi(this);
}

EffectCheckRow::~EffectCheckRow()
{
    delete ui;
}

QCheckBox* EffectCheckRow::checkBox() const
{
    return ui->optionCheck;
}

void EffectCheckRow::configure(const QString& text, bool checked, const QString& tooltip)
{
    ui->optionCheck->setText(text);
    ui->optionCheck->setChecked(checked);
    if(!tooltip.isEmpty())
    {
        ui->optionCheck->setToolTip(tooltip);
    }
}

void EffectCheckRow::bindToggled(QObject* owner,
                               const std::function<void(bool)>& apply_value,
                               const std::function<void()>& changed)
{
    if(!owner)
    {
        return;
    }

    QObject::connect(ui->optionCheck, &QCheckBox::toggled, owner, [apply_value, changed](bool v) {
        if(apply_value)
        {
            apply_value(v);
        }
        if(changed)
        {
            changed();
        }
    });
}
#include "EffectInfoLabel.h"
#include "ui_EffectInfoLabel.h"

#include <QLabel>

EffectInfoLabel::EffectInfoLabel(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::EffectInfoLabel)
{
    ui->setupUi(this);
}

EffectInfoLabel::~EffectInfoLabel()
{
    delete ui;
}

void EffectInfoLabel::setText(const QString& text)
{
    ui->infoLabel->setText(text);
}

void EffectInfoLabel::setAlignment(Qt::Alignment alignment)
{
    ui->infoLabel->setAlignment(alignment);
}

QLabel* EffectInfoLabel::label() const
{
    return ui->infoLabel;
}
#include "EffectSectionHeading.h"
#include "ui_EffectSectionHeading.h"

#include <QLabel>

#include <QFont>
#include <QLabel>

EffectSectionHeading::EffectSectionHeading(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::EffectSectionHeading)
{
    ui->setupUi(this);
    QFont font = ui->headingLabel->font();
    font.setBold(true);
    ui->headingLabel->setFont(font);
}

EffectSectionHeading::~EffectSectionHeading()
{
    delete ui;
}

QLabel* EffectSectionHeading::titleLabel() const
{
    return ui->headingLabel;
}

void EffectSectionHeading::setTitle(const QString& title)
{
    ui->headingLabel->setText(title);
}

#include "EffectCollapsibleSection.h"
#include "PluginUiUtils.h"
#include "ui_EffectCollapsibleSection.h"

#include <QFont>
#include <QShowEvent>
#include <QToolButton>
#include <QVBoxLayout>

EffectCollapsibleSection::EffectCollapsibleSection(const QString& title,
                                                   const QString& tooltip,
                                                   QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::EffectCollapsibleSection)
    , title_(title)
{
    ui->setupUi(this);

    QFont font = ui->toggleButton->font();
    font.setBold(true);
    ui->toggleButton->setFont(font);
    ui->toggleButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    if(!tooltip.isEmpty())
    {
        ui->toggleButton->setToolTip(tooltip);
    }

    applyHeaderChrome();
    refreshToggleText();
    connect(ui->toggleButton, &QToolButton::toggled, this, [this](bool expanded) {
        ui->bodyWidget->setVisible(expanded);
        refreshToggleText();
        emit expandedChanged(expanded);
    });
}

void EffectCollapsibleSection::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    // Re-apply after parenting into OpenRGB so we pick up the host palette (Setup-tab charcoal).
    applyHeaderChrome();
}

void EffectCollapsibleSection::applyHeaderChrome()
{
    PluginUiApplyCollapsibleSectionHeader(ui->toggleButton);
}

EffectCollapsibleSection::~EffectCollapsibleSection()
{
    delete ui;
}

QVBoxLayout* EffectCollapsibleSection::bodyLayout() const
{
    return ui->bodyLayout;
}

void EffectCollapsibleSection::setTitle(const QString& title)
{
    title_ = title;
    refreshToggleText();
}

void EffectCollapsibleSection::setExpanded(bool expanded)
{
    if(ui->toggleButton->isChecked() == expanded)
    {
        ui->bodyWidget->setVisible(expanded);
        refreshToggleText();
        return;
    }
    ui->toggleButton->setChecked(expanded);
}

bool EffectCollapsibleSection::isExpanded() const
{
    return ui->toggleButton->isChecked();
}

void EffectCollapsibleSection::refreshToggleText()
{
    const QChar marker = ui->toggleButton->isChecked() ? QChar(0x25BC) : QChar(0x25B6);
    ui->toggleButton->setText(QStringLiteral("%1  %2").arg(marker).arg(title_));
}
