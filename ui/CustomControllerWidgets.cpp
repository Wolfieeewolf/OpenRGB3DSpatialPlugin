// SPDX-License-Identifier: GPL-2.0-only

#include "CustomControllerDeviceWidget.h"
#include "CustomControllerDeviceList.h"
#include "CustomControllerDialog.h"
#include "ControllerDisplayUtils.h"
#include "ResourceManagerInterface.h"
#include "RGBController.h"
#include "OpenRGBPluginsFont.h"
#include "PluginClickableLabel.h"
#include "PluginUiUtils.h"
#include "ui_CustomControllerDeviceWidget.h"
#include "ui_CustomControllerDeviceList.h"
#include <QComboBox>
#include <QSignalBlocker>
#include <QFontMetrics>
#include <QFrame>
#include <QGridLayout>
#include <QResizeEvent>
#include <QScrollArea>
#include <QToolButton>
#include <algorithm>

CustomControllerDeviceWidget::CustomControllerDeviceWidget(RGBController* controller,
                                                             int controller_index,
                                                             CustomControllerDialog* host,
                                                             QWidget* parent)
    : QWidget(parent),
      ui(new Ui::CustomControllerDeviceWidget),
      controller_(controller),
      controller_index_(controller_index),
      host_(host),
      row_selected_(false)
{
    ui->setupUi(this);

    if(controller_)
    {
        device_name_text_ = ControllerDisplay::FormatRgbControllerTitle(controller_);
    }

    ui->innerLayout->setColumnStretch(0, 1);
    ui->innerLayout->setColumnStretch(1, 1);
    ui->innerLayout->setColumnStretch(kActionColumn, 0);

    ui->nameLabel->setWordWrap(false);
    ui->nameLabel->setMinimumWidth(0);
    connect(ui->nameLabel, &PluginClickableLabel::clicked, this, &CustomControllerDeviceWidget::on_name_clicked);

    ui->granularityCombo->addItem(tr("Device"));
    ui->granularityCombo->addItem(tr("Zone"));
    ui->granularityCombo->addItem(tr("LED"));
    applyGranularityComboStyle(ui->granularityCombo);
    ui->granularityCombo->setToolTip(tr("What to add: whole device, one zone, or one LED"));
    connect(ui->granularityCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &CustomControllerDeviceWidget::on_granularity_changed);

    applyItemComboStyle(ui->itemCombo);
    ui->itemCombo->setItemDelegate(new ColorComboDelegate(host_));
    ui->itemCombo->setToolTip(tr("Zone or LED to place on the grid"));
    connect(ui->itemCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &CustomControllerDeviceWidget::on_item_changed);

    ui->enableButton->setFont(OpenRGBPluginsFont::GetFont());
    ui->enableButton->setToolTip(tr("Add to or remove from layout grid at the selected cell"));
    connect(ui->enableButton, &QToolButton::toggled, this, &CustomControllerDeviceWidget::on_enable_toggled);

    rebuildItemCombo();
    updateNameLabelElide();
    refreshFromHost();
}

CustomControllerDeviceWidget::~CustomControllerDeviceWidget()
{
    delete ui;
}

void CustomControllerDeviceWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    updateNameLabelElide();
}

void CustomControllerDeviceWidget::updateNameLabelElide()
{
    if(!ui->nameLabel || device_name_text_.isEmpty())
    {
        return;
    }

    const int available_width = ui->nameLabel->width();
    if(available_width <= 0)
    {
        ui->nameLabel->setText(device_name_text_);
        ui->nameLabel->setToolTip(QString());
        return;
    }

    const QFontMetrics metrics(ui->nameLabel->font());
    const QString elided = metrics.elidedText(device_name_text_, Qt::ElideRight, available_width);
    ui->nameLabel->setText(elided);

    ui->nameLabel->setToolTip(elided != device_name_text_ ? device_name_text_ : QString());
}

void CustomControllerDeviceWidget::applyCompactComboStyle(QComboBox* combo)
{
    if(!combo)
    {
        return;
    }

    QFont font = combo->font();
    const int target = std::max(7, font.pointSize() - 1);
    font.setPointSize(target);
    combo->setFont(font);
    combo->setMaximumHeight(22);
}

void CustomControllerDeviceWidget::applyGranularityComboStyle(QComboBox* combo)
{
    applyCompactComboStyle(combo);
    if(!combo)
    {
        return;
    }

    combo->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    const QFontMetrics metrics(combo->font());
    int text_width = 0;
    for(int i = 0; i < combo->count(); i++)
    {
        text_width = std::max(text_width, metrics.horizontalAdvance(combo->itemText(i)));
    }

    constexpr int dropdown_chrome = 28;
    combo->setFixedWidth(text_width + dropdown_chrome);
}

void CustomControllerDeviceWidget::applyItemComboStyle(QComboBox* combo)
{
    applyCompactComboStyle(combo);
    if(!combo)
    {
        return;
    }

    combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    combo->setMinimumWidth(72);
}

CustomControllerSourceRef CustomControllerDeviceWidget::currentSource() const
{
    CustomControllerSourceRef ref;
    ref.controller_index = controller_index_;
    if(!ui->granularityCombo)
    {
        return ref;
    }

    ref.granularity = ui->granularityCombo->currentIndex();
    ref.item_idx    = 0;

    if(ref.granularity == 0)
    {
        return ref;
    }

    if(ui->itemCombo && ui->itemCombo->currentIndex() >= 0 && ui->itemCombo->currentData().isValid())
    {
        ref.item_idx = ui->itemCombo->currentData().toInt();
    }
    else
    {
        ref.granularity = -1;
        ref.item_idx    = -1;
    }

    return ref;
}

void CustomControllerDeviceWidget::applySource(const CustomControllerSourceRef& source)
{
    if(!ui->granularityCombo || source.controller_index != controller_index_ || source.granularity < 0)
    {
        return;
    }

    ui->granularityCombo->blockSignals(true);
    ui->granularityCombo->setCurrentIndex(source.granularity);
    ui->granularityCombo->blockSignals(false);

    rebuildItemCombo();

    if(source.granularity > 0 && ui->itemCombo)
    {
        for(int i = 0; i < ui->itemCombo->count(); i++)
        {
            if(ui->itemCombo->itemData(i).toInt() == source.item_idx)
            {
                ui->itemCombo->setCurrentIndex(i);
                break;
            }
        }
    }

    updatePlusFromSource();
}

void CustomControllerDeviceWidget::rebuildItemCombo()
{
    if(!ui->itemCombo || !host_ || !controller_ || !ui->granularityCombo)
    {
        return;
    }

    const int prev_data =
        ui->itemCombo->currentData().isValid() ? ui->itemCombo->currentData().toInt() : -9999;
    const int granularity = ui->granularityCombo->currentIndex();

    QSignalBlocker combo_block(ui->itemCombo);

    ui->itemCombo->clear();

    if(granularity == 0)
    {
        ui->itemCombo->setEnabled(false);
        ui->itemCombo->addItem(tr("All LEDs"), 0);
        ui->itemCombo->setCurrentIndex(0);
        updatePlusFromSource();
        return;
    }

    ui->itemCombo->setEnabled(true);
    host_->PopulateDeviceItemCombo(controller_index_, granularity, ui->itemCombo);

    int restore_index = -1;
    for(int i = 0; i < ui->itemCombo->count(); i++)
    {
        if(ui->itemCombo->itemData(i).toInt() == prev_data)
        {
            restore_index = i;
            break;
        }
    }
    if(restore_index >= 0)
    {
        ui->itemCombo->setCurrentIndex(restore_index);
    }
    else if(ui->itemCombo->count() > 0 && prev_data == -9999)
    {
        ui->itemCombo->setCurrentIndex(0);
    }

    updatePlusFromSource();
}

void CustomControllerDeviceWidget::updatePlusFromSource()
{
    if(!ui->enableButton || !host_)
    {
        return;
    }

    const CustomControllerSourceRef ref = currentSource();
    const bool on_grid                  = ref.isValid() && host_->IsSourceItemOnGrid(ref);

    ui->enableButton->blockSignals(true);
    ui->enableButton->setChecked(on_grid);
    updateEnableIcon();
    ui->enableButton->blockSignals(false);
}

void CustomControllerDeviceWidget::refreshFromHost()
{
    rebuildItemCombo();

    if(!host_)
    {
        return;
    }

    const CustomControllerSourceRef ref = currentSource();
    const bool grid_ready               = host_->selectedGridCellValid();
    const bool on_grid                  = ref.isValid() && host_->IsSourceItemOnGrid(ref);
    const bool can_add                  = ref.isValid() && host_->CanAddSourceToGrid(ref);

    updatePlusFromSource();
    setPlusEnabled(grid_ready && ref.isValid() && (on_grid || can_add));
}

void CustomControllerDeviceWidget::setRowSelected(bool selected)
{
    row_selected_ = selected;
    if(!ui->cardFrame)
    {
        return;
    }

    if(selected)
    {
        ui->cardFrame->setAutoFillBackground(true);
        QPalette pal = ui->cardFrame->palette();
        const QColor highlight = PluginUiPaletteColor(this, QPalette::Highlight);
        pal.setColor(QPalette::Window, PluginUiBlendColors(PluginUiPaletteColor(this, QPalette::Base), highlight, 0.35f));
        ui->cardFrame->setPalette(pal);
    }
    else
    {
        ui->cardFrame->setAutoFillBackground(false);
        ui->cardFrame->setPalette(parentWidget() ? parentWidget()->palette() : palette());
    }
}

void CustomControllerDeviceWidget::setPlusEnabled(bool enabled)
{
    if(ui->enableButton)
    {
        ui->enableButton->setEnabled(enabled);
    }
}

void CustomControllerDeviceWidget::on_name_clicked()
{
    emit deviceActivated(controller_index_);
    notifySourceChanged();
}

void CustomControllerDeviceWidget::on_granularity_changed(int)
{
    rebuildItemCombo();
    emit deviceActivated(controller_index_);
    notifySourceChanged();
}

void CustomControllerDeviceWidget::on_item_changed(int index)
{
    Q_UNUSED(index);
    updatePlusFromSource();
    emit deviceActivated(controller_index_);
    notifySourceChanged();
}

void CustomControllerDeviceWidget::on_enable_toggled(bool checked)
{
    updateEnableIcon();
    const CustomControllerSourceRef ref = currentSource();
    emit enableToggled(ref, checked);
    emit deviceActivated(controller_index_);
}

void CustomControllerDeviceWidget::notifySourceChanged()
{
    const CustomControllerSourceRef ref = currentSource();
    if(ref.isValid())
    {
        emit sourceChanged(ref);
    }
}

void CustomControllerDeviceWidget::updateEnableIcon()
{
    if(!ui->enableButton)
    {
        return;
    }
    ui->enableButton->setText(
        ui->enableButton->isChecked() ? OpenRGBPluginsFont::icon(OpenRGBPluginsFont::math_minus)
                                    : OpenRGBPluginsFont::icon(OpenRGBPluginsFont::math_plus));
}

CustomControllerDeviceList::CustomControllerDeviceList(QWidget* parent)
    : QWidget(parent),
      ui(new Ui::CustomControllerDeviceList),
      scroll_area_(nullptr),
      content_widget_(nullptr),
      content_layout_(nullptr),
      selected_controller_index_(-1)
{
    ui->setupUi(this);
    scroll_area_    = ui->scrollArea;
    content_widget_ = ui->contentWidget;
    content_layout_ = ui->contentLayout;
}

CustomControllerDeviceList::~CustomControllerDeviceList()
{
    delete ui;
}

void CustomControllerDeviceList::clearWidgets()
{
    device_widgets_.clear();
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

CustomControllerDeviceWidget* CustomControllerDeviceList::widgetForController(int controller_index) const
{
    for(CustomControllerDeviceWidget* widget : device_widgets_)
    {
        if(widget && widget->controllerIndex() == controller_index)
        {
            return widget;
        }
    }
    return nullptr;
}

void CustomControllerDeviceList::rebuild(ResourceManagerInterface* resource_manager, CustomControllerDialog* host)
{
    const int previous_controller = selected_controller_index_;
    clearWidgets();

    if(!resource_manager || !host)
    {
        selected_controller_index_ = -1;
        return;
    }

    std::vector<RGBController*>& controllers = resource_manager->GetRGBControllers();
    device_widgets_.reserve(controllers.size());

    for(unsigned int controller_index = 0; controller_index < controllers.size(); controller_index++)
    {
        RGBController* controller = controllers[controller_index];
        if(!controller)
        {
            continue;
        }

        auto* widget = new CustomControllerDeviceWidget(controller, static_cast<int>(controller_index), host,
                                                        content_widget_);

        connect(widget, &CustomControllerDeviceWidget::deviceActivated, this,
                [this](int index)
                {
                    if(suppress_row_activation_)
                    {
                        return;
                    }
                    setSelectedControllerIndex(index);
                    if(CustomControllerDeviceWidget* row = widgetForController(index))
                    {
                        emit selectionChanged(row->currentSource());
                    }
                });
        connect(widget, &CustomControllerDeviceWidget::sourceChanged, this,
                [this](const CustomControllerSourceRef& ref)
                {
                    if(ref.controller_index == selected_controller_index_)
                    {
                        emit selectionChanged(ref);
                    }
                });
        connect(widget, &CustomControllerDeviceWidget::enableToggled, this, &CustomControllerDeviceList::enableToggled);

        content_layout_->addWidget(widget);
        device_widgets_.push_back(widget);
    }

    content_layout_->addStretch();

    if(previous_controller >= 0 && widgetForController(previous_controller))
    {
        setSelectedControllerIndex(previous_controller);
    }
    else if(!device_widgets_.empty())
    {
        setSelectedControllerIndex(device_widgets_.front()->controllerIndex());
    }
    else
    {
        selected_controller_index_ = -1;
    }

    refreshFromHost();

    const CustomControllerSourceRef source = selectedSource();
    if(source.isValid())
    {
        emit selectionChanged(source);
    }
}

void CustomControllerDeviceList::refreshFromHost(int only_controller_index)
{
    suppress_row_activation_ = true;

    if(only_controller_index >= 0)
    {
        if(CustomControllerDeviceWidget* widget = widgetForController(only_controller_index))
        {
            widget->refreshFromHost();
        }
    }
    else
    {
        for(CustomControllerDeviceWidget* widget : device_widgets_)
        {
            if(widget)
            {
                widget->refreshFromHost();
            }
        }
    }

    suppress_row_activation_ = false;
    applySelection();
}

CustomControllerSourceRef CustomControllerDeviceList::selectedSource() const
{
    if(const CustomControllerDeviceWidget* widget = widgetForController(selected_controller_index_))
    {
        return widget->currentSource();
    }
    return {};
}

void CustomControllerDeviceList::setSelectedControllerIndex(int controller_index)
{
    selected_controller_index_ = controller_index;
    applySelection();
}

void CustomControllerDeviceList::applySelection()
{
    for(CustomControllerDeviceWidget* widget : device_widgets_)
    {
        if(!widget)
        {
            continue;
        }
        widget->setRowSelected(widget->controllerIndex() == selected_controller_index_);
    }
}

