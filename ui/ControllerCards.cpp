// SPDX-License-Identifier: GPL-2.0-only

#include "SpatialControllerCardWidget.h"
#include "SpatialControllerCardList.h"
#include "OpenRGB3DSpatialTab.h"
#include "RGBController/RGBController.h"
#include "OpenRGBPluginsFont.h"
#include "PluginClickableLabel.h"
#include "PluginUiUtils.h"
#include "ui_SpatialControllerCardWidget.h"
#include "ui_SpatialControllerCardList.h"
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFontMetrics>
#include <QFrame>
#include <QLabel>
#include <QResizeEvent>
#include <QScrollArea>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QToolButton>
#include <algorithm>
#include <limits>

SpatialControllerCardWidget::SpatialControllerCardWidget(Mode mode,
                                                           const QString& title,
                                                           const SpatialControllerEntryKey& key,
                                                           bool show_granularity_controls,
                                                           bool show_spacing_controls,
                                                           float spacing_x_mm,
                                                           float spacing_y_mm,
                                                           float spacing_z_mm,
                                                           OpenRGB3DSpatialTab* host,
                                                           int scene_list_row,
                                                           int transform_index,
                                                           QWidget* parent,
                                                           const QString& subtitle)
    : QWidget(parent),
      ui(new Ui::SpatialControllerCardWidget),
      mode_(mode),
      key_(key),
      show_granularity_controls_(show_granularity_controls),
      show_spacing_controls_(show_spacing_controls),
      host_(host),
      scene_list_row_(scene_list_row),
      transform_index_(transform_index),
      row_selected_(false)
{
    ui->setupUi(this);
    title_text_    = title;
    subtitle_text_ = subtitle;

    applyColumnStretches();

    PluginUiApplyControllerCardChrome(ui->cardFrame, false);

    // Ignored horizontal policy lets the name shrink below its text sizeHint so the
    // +/− action button is not clipped when the list is narrow or a scrollbar appears.
    ui->nameLabel->setMinimumWidth(0);
    ui->nameLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    connect(ui->nameLabel, &PluginClickableLabel::clicked, this, &SpatialControllerCardWidget::nameClicked);

    if(ui->subtitleLabel)
    {
        ui->subtitleLabel->setMinimumWidth(0);
        ui->subtitleLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        PluginUiApplyMutedSecondaryLabel(ui->subtitleLabel);
    }

    ui->actionButton->setFont(OpenRGBPluginsFont::GetFont());
    ui->actionButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    ui->actionButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    ui->actionButton->setFixedSize(30, 28);
    ui->actionButton->setToolTip(mode_ == Mode::Available ? tr("Add to 3D scene") : tr("Remove from 3D scene"));
    connect(ui->actionButton, &QToolButton::clicked, this, &SpatialControllerCardWidget::actionClicked);

    ui->editButton->setVisible(mode_ == Mode::InScene);
    ui->editButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    ui->editButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    ui->editButton->setFixedHeight(28);
    ui->editButton->setToolTip(tr("Edit position, rotation, and spacing in the settings panel"));
    connect(ui->editButton, &QToolButton::clicked, this, &SpatialControllerCardWidget::editClicked);

    ui->granularityCombo->addItem(tr("Device"));
    ui->granularityCombo->addItem(tr("Zone"));
    ui->granularityCombo->addItem(tr("LED"));
    applyGranularityComboStyle(ui->granularityCombo);
    ui->granularityCombo->setToolTip(tr("What to add: whole device, one zone, or one LED"));
    connect(ui->granularityCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &SpatialControllerCardWidget::granularityChanged);

    applyItemComboStyle(ui->itemCombo);
    ui->itemCombo->setToolTip(tr("Zone or LED to add to the 3D scene"));
    connect(ui->itemCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &SpatialControllerCardWidget::itemChanged);

    PluginUiApplyMutedSecondaryLabel(ui->spacingCaption);
    ui->spacingCaption->setToolTip(tr("Distance between LEDs along each axis when placed in the 3D scene"));
    applySpacingSpinStyle(ui->spacingXSpin);
    applySpacingSpinStyle(ui->spacingYSpin);
    applySpacingSpinStyle(ui->spacingZSpin);
    ui->spacingXSpin->setToolTip(tr("Horizontal spacing (left / right)"));
    ui->spacingYSpin->setToolTip(tr("Vertical spacing (up / down)"));
    ui->spacingZSpin->setToolTip(tr("Depth spacing (front / back)"));
    connect(ui->spacingXSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            &SpatialControllerCardWidget::spacingChanged);
    connect(ui->spacingYSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            &SpatialControllerCardWidget::spacingChanged);
    connect(ui->spacingZSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            &SpatialControllerCardWidget::spacingChanged);

    applyOptionalRowVisibility();
    setSpacingMm(spacing_x_mm, spacing_y_mm, spacing_z_mm);

    updateActionIcon();
    updateNameLabelLayout();
    refreshFromHost();
}

SpatialControllerCardWidget::~SpatialControllerCardWidget()
{
    delete ui;
}

void SpatialControllerCardWidget::applyOptionalRowVisibility()
{
    const bool show_granularity = show_granularity_controls_ && mode_ == Mode::Available;
    ui->granularityCombo->setVisible(show_granularity);
    ui->itemCombo->setVisible(show_granularity);
    ui->spacingCaption->setVisible(show_spacing_controls_);
    ui->spacingXSpin->setVisible(show_spacing_controls_);
    ui->spacingYSpin->setVisible(show_spacing_controls_);
    ui->spacingZSpin->setVisible(show_spacing_controls_);
}

void SpatialControllerCardWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    updateNameLabelLayout();
}

void SpatialControllerCardWidget::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    PluginUiApplyControllerCardChrome(ui->cardFrame, row_selected_);
}

void SpatialControllerCardWidget::applyColumnStretches()
{
    for(int col = 0; col <= kActionColumn; col++)
    {
        ui->innerLayout->setColumnStretch(col, 0);
        ui->innerLayout->setColumnMinimumWidth(col, 0);
    }

    // Only the name column absorbs leftover width; Edit / +/− stay at fixed size.
    ui->innerLayout->setColumnStretch(0, 1);
    ui->innerLayout->setColumnMinimumWidth(kActionColumn, 30);
    if(mode_ == Mode::InScene)
    {
        ui->innerLayout->setColumnMinimumWidth(kLastContentColumn, 44);
    }
}

int SpatialControllerCardWidget::nameLabelContentWidth() const
{
    const QWidget* frame = ui->cardFrame;
    if(!frame)
    {
        return 0;
    }

    const QMargins margins = ui->innerLayout->contentsMargins();
    int width              = frame->width() - margins.left() - margins.right();

    const int spacing = ui->innerLayout->horizontalSpacing();
    if(ui->editButton->isVisible())
    {
        width -= ui->editButton->width() + spacing;
    }
    if(ui->actionButton->isVisible())
    {
        width -= ui->actionButton->width() + spacing;
    }

    return std::max(0, width);
}

void SpatialControllerCardWidget::updateNameLabelLayout()
{
    if(!ui->nameLabel)
    {
        return;
    }

    ui->nameLabel->setWordWrap(false);
    ui->nameLabel->setMinimumHeight(0);

    const int available_width = std::max(ui->nameLabel->width(), nameLabelContentWidth());
    if(available_width <= 0 || title_text_.isEmpty())
    {
        ui->nameLabel->setText(title_text_);
        ui->nameLabel->setToolTip(title_text_);
    }
    else
    {
        const QFontMetrics metrics(ui->nameLabel->font());
        const QString elided = metrics.elidedText(title_text_, Qt::ElideRight, available_width);
        ui->nameLabel->setText(elided);
        ui->nameLabel->setToolTip(elided != title_text_ ? title_text_ : QString());
    }

    if(ui->subtitleLabel)
    {
        const bool show_subtitle = !subtitle_text_.isEmpty();
        ui->subtitleLabel->setVisible(show_subtitle);
        if(show_subtitle)
        {
            ui->subtitleLabel->setWordWrap(false);
            ui->subtitleLabel->setText(subtitle_text_);
            ui->subtitleLabel->setToolTip(subtitle_text_);
        }
    }

    updateGeometry();
}

void SpatialControllerCardWidget::applyCompactComboStyle(QComboBox* combo)
{
    if(!combo)
    {
        return;
    }

    // Follow host font size (OpenRGB Qt6 native theme); do not force tiny max heights.
    combo->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    combo->setMaximumHeight(QWIDGETSIZE_MAX);
}

void SpatialControllerCardWidget::applyGranularityComboStyle(QComboBox* combo)
{
    applyCompactComboStyle(combo);
    if(!combo)
    {
        return;
    }

    combo->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);

    const QFontMetrics metrics(combo->font());
    int text_width = 0;
    for(int i = 0; i < combo->count(); i++)
    {
        text_width = std::max(text_width, metrics.horizontalAdvance(combo->itemText(i)));
    }
    combo->setMinimumWidth(text_width + 40);
    combo->setMaximumWidth(QWIDGETSIZE_MAX);
}

void SpatialControllerCardWidget::applyItemComboStyle(QComboBox* combo)
{
    applyCompactComboStyle(combo);
    if(combo)
    {
        combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }
}

void SpatialControllerCardWidget::applySpacingSpinStyle(QDoubleSpinBox* spin)
{
    if(!spin)
    {
        return;
    }

    spin->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    spin->setMaximumHeight(QWIDGETSIZE_MAX);
    spin->setRange(0.0, 1000.0);
    spin->setSingleStep(1.0);
    spin->setDecimals(1);
    spin->setSuffix(tr(" mm"));
    spin->setAlignment(Qt::AlignRight);
    spin->setMinimumWidth(64);
}

void SpatialControllerCardWidget::spacingMm(float* x_mm, float* y_mm, float* z_mm) const
{
    if(x_mm)
    {
        *x_mm = show_spacing_controls_ ? (float)ui->spacingXSpin->value() : 10.0f;
    }
    if(y_mm)
    {
        *y_mm = show_spacing_controls_ ? (float)ui->spacingYSpin->value() : 0.0f;
    }
    if(z_mm)
    {
        *z_mm = show_spacing_controls_ ? (float)ui->spacingZSpin->value() : 0.0f;
    }
}

void SpatialControllerCardWidget::setSpacingMm(float x_mm, float y_mm, float z_mm)
{
    const auto set_spin = [](QDoubleSpinBox* spin, float value)
    {
        if(!spin)
        {
            return;
        }
        QSignalBlocker blocker(spin);
        spin->setValue(std::max(0.0, std::min(1000.0, (double)value)));
    };

    set_spin(ui->spacingXSpin, x_mm);
    set_spin(ui->spacingYSpin, y_mm);
    set_spin(ui->spacingZSpin, z_mm);
}

void SpatialControllerCardWidget::rebuildItemCombo()
{
    if(!host_ || !show_granularity_controls_)
    {
        return;
    }

    const int granularity = ui->granularityCombo->currentIndex();
    host_->PopulateAvailableItemCombo(key_, granularity, ui->itemCombo);
    updateActionEnabled();
}

void SpatialControllerCardWidget::updateActionIcon()
{
    if(mode_ == Mode::Available)
    {
        ui->actionButton->setText(OpenRGBPluginsFont::icon(OpenRGBPluginsFont::math_plus));
    }
    else
    {
        ui->actionButton->setText(OpenRGBPluginsFont::icon(OpenRGBPluginsFont::math_minus));
    }
}

void SpatialControllerCardWidget::updateActionEnabled()
{
    if(mode_ == Mode::InScene)
    {
        ui->actionButton->setEnabled(true);
        return;
    }

    if(!show_granularity_controls_)
    {
        ui->actionButton->setEnabled(true);
        return;
    }

    ui->actionButton->setEnabled(ui->itemCombo->count() > 0 && ui->itemCombo->currentIndex() >= 0);
}

void SpatialControllerCardWidget::setRowSelected(bool selected)
{
    row_selected_ = selected;
    PluginUiApplyControllerCardChrome(ui->cardFrame, selected);
}

void SpatialControllerCardWidget::refreshFromHost()
{
    if(!host_)
    {
        return;
    }

    if(mode_ == Mode::Available && show_granularity_controls_)
    {
        rebuildItemCombo();
    }

    if(show_spacing_controls_)
    {
        float x = 10.0f;
        float y = 0.0f;
        float z = 0.0f;
        if(mode_ == Mode::InScene && transform_index_ >= 0)
        {
            host_->GetTransformLedSpacing(transform_index_, x, y, z);
        }
        else if(mode_ == Mode::Available && key_.first >= 0)
        {
            host_->GetSuggestedSpacingForAvailableRgb(key_.first, x, y, z);
        }
        setSpacingMm(x, y, z);
    }

    updateActionEnabled();
    updateNameLabelLayout();
}

void SpatialControllerCardWidget::notifySpacingEdited()
{
    if(!host_ || !show_spacing_controls_)
    {
        return;
    }

    float x = 10.0f;
    float y = 0.0f;
    float z = 0.0f;
    spacingMm(&x, &y, &z);

    if(mode_ == Mode::InScene && transform_index_ >= 0)
    {
        host_->ApplyLedSpacingToTransform(transform_index_, x, y, z);
    }
    else if(mode_ == Mode::Available && key_.first >= 0)
    {
        host_->RememberAvailableRgbSpacingDraft(key_.first, x, y, z);
    }
}

void SpatialControllerCardWidget::nameClicked()
{
    emit cardActivated(key_);
}

void SpatialControllerCardWidget::granularityChanged(int)
{
    rebuildItemCombo();
    emit cardActivated(key_);
}

void SpatialControllerCardWidget::itemChanged(int)
{
    updateActionEnabled();
    emit cardActivated(key_);
}

void SpatialControllerCardWidget::spacingChanged(double)
{
    notifySpacingEdited();
}

void SpatialControllerCardWidget::editClicked()
{
    if(mode_ == Mode::InScene)
    {
        emit editRequested(scene_list_row_);
    }
}

void SpatialControllerCardWidget::actionClicked()
{
    if(mode_ == Mode::InScene)
    {
        emit removeRequested(scene_list_row_);
        return;
    }

    int   granularity  = 0;
    int   item_index   = 0;
    float spacing_x    = 10.0f;
    float spacing_y    = 0.0f;
    float spacing_z    = 0.0f;
    if(show_spacing_controls_)
    {
        spacingMm(&spacing_x, &spacing_y, &spacing_z);
    }
    else if(host_ && key_.first >= 0)
    {
        host_->GetSuggestedSpacingForAvailableRgb(key_.first, spacing_x, spacing_y, spacing_z);
    }

    if(show_granularity_controls_)
    {
        if(ui->itemCombo->currentIndex() < 0 || !ui->itemCombo->currentData().isValid())
        {
            return;
        }
        granularity = ui->granularityCombo->currentIndex();
        item_index  = ui->itemCombo->currentData().value<QPair<int, int>>().second;
    }

    emit addRequested(key_, granularity, item_index, spacing_x, spacing_y, spacing_z);
}

SpatialControllerCardList::SpatialControllerCardList(SpatialControllerCardWidget::Mode mode, QWidget* parent)
    : QWidget(parent),
      ui(new Ui::SpatialControllerCardList),
      mode_(mode),
      selected_available_key_(std::numeric_limits<int>::min(), 0),
      selected_scene_row_(-1)
{
    ui->setupUi(this);
    scroll_area_     = ui->scrollArea;
    content_widget_  = ui->contentWidget;
    content_layout_  = ui->contentLayout;

    // Keep a stable gutter so appearing scrollbars do not cover the +/− buttons.
    if(scroll_area_)
    {
        scroll_area_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    }
    if(content_layout_)
    {
        content_layout_->setContentsMargins(0, 0, 2, 0);
    }
}

SpatialControllerCardList::~SpatialControllerCardList()
{
    delete ui;
}

void SpatialControllerCardList::clearWidgets()
{
    cards_.clear();
    if(!content_layout_)
    {
        return;
    }

    while(QLayoutItem* item = content_layout_->takeAt(0))
    {
        if(QWidget* widget = item->widget())
        {
            widget->deleteLater();
        }
        delete item;
    }
}

SpatialControllerCardWidget* SpatialControllerCardList::widgetForAvailableKey(const SpatialControllerEntryKey& key) const
{
    for(SpatialControllerCardWidget* card : cards_)
    {
        if(card && card->mode() == SpatialControllerCardWidget::Mode::Available && card->key() == key)
        {
            return card;
        }
    }
    return nullptr;
}

SpatialControllerCardWidget* SpatialControllerCardList::widgetForSceneRow(int scene_list_row) const
{
    for(SpatialControllerCardWidget* card : cards_)
    {
        if(card && card->mode() == SpatialControllerCardWidget::Mode::InScene && card->sceneListRow() == scene_list_row)
        {
            return card;
        }
    }
    return nullptr;
}

void SpatialControllerCardList::applyAvailableSelection()
{
    for(SpatialControllerCardWidget* card : cards_)
    {
        if(!card)
        {
            continue;
        }
        const bool selected = (card->key() == selected_available_key_);
        card->setRowSelected(selected);
    }
}

void SpatialControllerCardList::applySceneSelection()
{
    for(SpatialControllerCardWidget* card : cards_)
    {
        if(!card)
        {
            continue;
        }
        card->setRowSelected(card->sceneListRow() == selected_scene_row_);
    }
}

void SpatialControllerCardList::rebuildAvailable(OpenRGB3DSpatialTab* host)
{
    const SpatialControllerEntryKey previous_key = selected_available_key_;
    clearWidgets();

    if(!host)
    {
        selected_available_key_ = SpatialControllerEntryKey(std::numeric_limits<int>::min(), 0);
        return;
    }

    const QList<SpatialControllerEntryKey> keys       = host->GetAvailableControllerKeys();
    const QList<QString>                   titles     = host->GetAvailableControllerTitles();
    const QList<QString>                   subtitles  = host->GetAvailableControllerSubtitles();
    const QList<bool> granularity_flags = host->GetAvailableControllerGranularityFlags();

    const int count = keys.size();
    cards_.reserve(count);

    for(int i = 0; i < count; i++)
    {
        const bool show_granularity_controls =
            (i < granularity_flags.size()) ? granularity_flags[i] : false;
        const QString title    = (i < titles.size()) ? titles[i] : QString();
        const QString subtitle = (i < subtitles.size()) ? subtitles[i] : QString();
        float spacing_x = 10.0f;
        float spacing_y = 0.0f;
        float spacing_z = 0.0f;
        if(show_granularity_controls)
        {
            host->GetSuggestedSpacingForAvailableRgb(keys[i].first, spacing_x, spacing_y, spacing_z);
        }
        auto* card = new SpatialControllerCardWidget(SpatialControllerCardWidget::Mode::Available,
                                                     title,
                                                     keys[i],
                                                     show_granularity_controls,
                                                     false,
                                                     spacing_x,
                                                     spacing_y,
                                                     spacing_z,
                                                     host,
                                                     -1,
                                                     -1,
                                                     content_widget_,
                                                     subtitle);

        connect(card, &SpatialControllerCardWidget::cardActivated, this,
                [this](const SpatialControllerEntryKey& key)
                {
                    selected_available_key_ = key;
                    applyAvailableSelection();
                    emit availableSelectionChanged(key);
                });
        connect(card, &SpatialControllerCardWidget::addRequested, host, &OpenRGB3DSpatialTab::availableCardAdd);

        content_layout_->addWidget(card);
        cards_.push_back(card);
    }

    content_layout_->addStretch();

    bool restored = false;
    if(previous_key.first != std::numeric_limits<int>::min())
    {
        if(widgetForAvailableKey(previous_key))
        {
            selected_available_key_ = previous_key;
            restored              = true;
        }
    }

    if(!restored && !cards_.empty())
    {
        selected_available_key_ = cards_.front()->key();
    }

    applyAvailableSelection();
    if(restored || !cards_.empty())
    {
        emit availableSelectionChanged(selected_available_key_);
    }
}

void SpatialControllerCardList::rebuildInScene(OpenRGB3DSpatialTab* host)
{
    const int previous_row = selected_scene_row_;
    clearWidgets();

    if(!host)
    {
        selected_scene_row_ = -1;
        return;
    }

    const int row_count = host->sceneControllerRowCount();
    cards_.reserve(row_count);

    for(int row = 0; row < row_count; row++)
    {
        const SpatialControllerEntryKey key = host->sceneControllerRowKey(row);

        int transform_index = -1;
        if(!host->sceneControllerRowHasUserRole(row))
        {
            transform_index = host->ControllerListRowToTransformIndex(row);
        }

        auto* card = new SpatialControllerCardWidget(SpatialControllerCardWidget::Mode::InScene,
                                                     host->sceneControllerRowText(row),
                                                     key,
                                                     false,
                                                     false,
                                                     10.0f,
                                                     0.0f,
                                                     0.0f,
                                                     host,
                                                     row,
                                                     transform_index,
                                                     content_widget_);

        connect(card, &SpatialControllerCardWidget::cardActivated, this,
                [this, row](const SpatialControllerEntryKey&)
                {
                    selected_scene_row_ = row;
                    applySceneSelection();
                    emit sceneSelectionChanged(row);
                });
        connect(card, &SpatialControllerCardWidget::removeRequested, host, &OpenRGB3DSpatialTab::sceneCardRemove);
        connect(card, &SpatialControllerCardWidget::editRequested, host, &OpenRGB3DSpatialTab::sceneCardEdit);

        content_layout_->addWidget(card);
        cards_.push_back(card);
    }

    content_layout_->addStretch();

    selected_scene_row_ = -1;
    if(previous_row >= 0 && widgetForSceneRow(previous_row))
    {
        selected_scene_row_ = previous_row;
    }
    else if(host->sceneControllerCurrentRow() >= 0)
    {
        selected_scene_row_ = host->sceneControllerCurrentRow();
    }

    setSelectedSceneRow(selected_scene_row_, selected_scene_row_ >= 0);
}

void SpatialControllerCardList::refreshAvailableFromHost()
{
    for(SpatialControllerCardWidget* card : cards_)
    {
        if(card && card->mode() == SpatialControllerCardWidget::Mode::Available)
        {
            card->refreshFromHost();
        }
    }
}

void SpatialControllerCardList::refreshInSceneSpacingFromHost()
{
    for(SpatialControllerCardWidget* card : cards_)
    {
        if(card && card->mode() == SpatialControllerCardWidget::Mode::InScene)
        {
            card->refreshFromHost();
        }
    }
}

void SpatialControllerCardList::setSelectedSceneRow(int scene_list_row, bool scroll_into_view)
{
    selected_scene_row_ = scene_list_row;
    applySceneSelection();

    if(!scroll_into_view || scene_list_row < 0 || !scroll_area_)
    {
        return;
    }

    if(SpatialControllerCardWidget* card = widgetForSceneRow(scene_list_row))
    {
        scroll_area_->ensureWidgetVisible(card, 24);
    }
}

void SpatialControllerCardList::setSelectedAvailableKey(const SpatialControllerEntryKey& key)
{
    selected_available_key_ = key;
    applyAvailableSelection();
}

