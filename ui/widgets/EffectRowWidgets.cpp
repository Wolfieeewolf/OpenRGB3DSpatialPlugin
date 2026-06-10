// SPDX-License-Identifier: GPL-2.0-only

#include "EffectSliderRow.h"
#include "ui_EffectSliderRow.h"

#include <QLabel>
#include <QSignalBlocker>
#include <QSlider>

#include <algorithm>

EffectSliderRow::EffectSliderRow(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::EffectSliderRow)
{
    ui->setupUi(this);
}

EffectSliderRow::~EffectSliderRow()
{
    delete ui;
}

void EffectSliderRow::setCaptionText(const QString& text)
{
    ui->captionLabel->setText(text);
}

void EffectSliderRow::setValueLabelMinimumWidth(int width)
{
    ui->valueLabel->setMinimumWidth(width);
}

QSlider* EffectSliderRow::slider() const
{
    return ui->valueSlider;
}

QLabel* EffectSliderRow::valueLabel() const
{
    return ui->valueLabel;
}

void EffectSliderRow::configure(int min, int max, int value, const QString& slider_tooltip)
{
    ui->valueSlider->setRange(min, max);
    ui->valueSlider->setValue(std::clamp(value, min, max));
    if(!slider_tooltip.isEmpty())
    {
        ui->valueSlider->setToolTip(slider_tooltip);
    }
}

QString EffectSliderRow::captionText() const
{
    return ui->captionLabel->text();
}

void EffectSliderRow::syncSliderValue(int value, const std::function<QString(int)>& format_value)
{
    const int min = ui->valueSlider->minimum();
    const int max = ui->valueSlider->maximum();
    const int v = std::clamp(value, min, max);
    QSignalBlocker blocker(ui->valueSlider);
    ui->valueSlider->setValue(v);
    if(format_value)
    {
        ui->valueLabel->setText(format_value(v));
    }
}

void EffectSliderRow::bindValueChanged(QObject* owner,
                                       const std::function<void(int)>& apply_value,
                                       const std::function<QString(int)>& format_value,
                                       const std::function<void()>& on_changed)
{
    if(!owner)
    {
        return;
    }

    if(format_value)
    {
        ui->valueLabel->setText(format_value(ui->valueSlider->value()));
    }

    QObject::connect(ui->valueSlider, &QSlider::valueChanged, owner, [this, apply_value, format_value, on_changed](int v) {
        if(apply_value)
        {
            apply_value(v);
        }
        if(format_value)
        {
            ui->valueLabel->setText(format_value(v));
        }
        if(on_changed)
        {
            on_changed();
        }
    });
}

void EffectSliderRow::setEnabled(bool enabled)
{
    ui->valueSlider->setEnabled(enabled);
    ui->valueLabel->setEnabled(enabled);
}
#include "EffectLabeledComboRow.h"
#include "ui_EffectLabeledComboRow.h"

#include <QComboBox>

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
}

QString EffectLabeledComboRow::captionText() const
{
    return ui->captionLabel->text();
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
                                            const std::function<void()>& on_changed)
{
    if(!owner)
    {
        return;
    }

    QObject::connect(ui->valueSpin, QOverload<int>::of(&QSpinBox::valueChanged), owner,
                     [apply_value, on_changed](int v) {
                         if(apply_value)
                         {
                             apply_value(v);
                         }
                         if(on_changed)
                         {
                             on_changed();
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
                               const std::function<void()>& on_changed)
{
    if(!owner)
    {
        return;
    }

    QObject::connect(ui->optionCheck, &QCheckBox::toggled, owner, [apply_value, on_changed](bool v) {
        if(apply_value)
        {
            apply_value(v);
        }
        if(on_changed)
        {
            on_changed();
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
#include "ui_EffectCollapsibleSection.h"

#include <QFont>
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
    if(!tooltip.isEmpty())
    {
        ui->toggleButton->setToolTip(tooltip);
    }

    refreshToggleText();
    connect(ui->toggleButton, &QToolButton::toggled, this, [this](bool expanded) {
        ui->bodyWidget->setVisible(expanded);
        refreshToggleText();
    });
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
#include "EffectColorSwatchRow.h"
#include "ui_EffectColorSwatchRow.h"

#include "Colors.h"
#include "PluginUiUtils.h"
#include "SpatialEffect3D.h"

#include <QColorDialog>
#include <QDialog>
#include <vector>

EffectColorSwatchRow::EffectColorSwatchRow(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::EffectColorSwatchRow)
{
    ui->setupUi(this);
}

EffectColorSwatchRow::~EffectColorSwatchRow()
{
    delete ui;
}

void EffectColorSwatchRow::setCaptionText(const QString& text)
{
    ui->captionLabel->setText(text);
}

void EffectColorSwatchRow::setSwatchColor(RGBColor color)
{
    PluginUiSetRgbSwatchButton(ui->swatchButton, color & 0xFF, (color >> 8) & 0xFF, (color >> 16) & 0xFF);
}

void EffectColorSwatchRow::bindColorIndex(SpatialEffect3D* effect,
                                          int color_index,
                                          const std::function<void()>& on_changed)
{
    if(!effect || color_index < 0)
    {
        return;
    }

    QObject::connect(ui->swatchButton, &QPushButton::clicked, this, [this, effect, color_index, on_changed]() {
        std::vector<RGBColor> cols = effect->GetColors();
        if(color_index >= static_cast<int>(cols.size()))
        {
            cols.resize(static_cast<size_t>(color_index + 1), COLOR_WHITE);
        }
        const RGBColor cur = cols[static_cast<size_t>(color_index)];
        QColorDialog dialog(this);
        dialog.setCurrentColor(QColor(cur & 0xFF, (cur >> 8) & 0xFF, (cur >> 16) & 0xFF));
        if(dialog.exec() != QDialog::Accepted)
        {
            return;
        }
        const QColor chosen = dialog.currentColor();
        cols[static_cast<size_t>(color_index)] =
            ((unsigned int)chosen.blue() << 16) | ((unsigned int)chosen.green() << 8) | (unsigned int)chosen.red();
        effect->SetColors(cols);
        effect->SetRainbowMode(false);
        setSwatchColor(cols[static_cast<size_t>(color_index)]);
        if(on_changed)
        {
            on_changed();
        }
    });
}
