// SPDX-License-Identifier: GPL-2.0-only

#include "CustomControllerDialog.h"
#include "CustomControllerDialog_Internal.h"
#include "CustomControllerClipboard.h"
#include "CustomControllerMappingUtils.h"
#include "CustomControllerGridKeys.h"
#include "ControllerDisplayUtils.h"
#include "ui_CustomControllerDialog.h"
#include "CustomControllerDeviceList.h"
#include "CustomControllerPreviewDialog.h"
#include "custom-controller-grid/CustomControllerLayoutGrid.h"
#include "custom-controller-grid/CustomControllerGridCell.h"
#include "custom-controller-grid/CustomControllerGridLayoutMath.h"
#include "ControllerLayout3D.h"
#include "PluginUiUtils.h"

#include <QCheckBox>
#include <QCloseEvent>
#include <QHideEvent>
#include <QShowEvent>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QSpinBox>
#include <QStyledItemDelegate>
#include <QVector>
#include <QTabWidget>
#include <QTimer>
#include <QIcon>
#include <QPixmap>
#include <QShortcut>
#include <QSplitter>
#include <QFrame>
#include <QInputDialog>
#include <QTabBar>
#include <QKeySequence>

#include <algorithm>
#include <functional>
#include <map>
#include <set>
#include <unordered_map>
#include <tuple>

namespace
{

QVBoxLayout* NewCustomControllerLayerTabLayout(QWidget* tab)
{
    auto* layout = new QVBoxLayout(tab);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(0);
    return layout;
}

} // namespace

CustomControllerDialog::CustomControllerDialog(ResourceManagerInterface* rm, QWidget *parent)
    : QDialog(parent),
      resource_manager(rm),
      layout_grid(nullptr),
      preview_dialog(nullptr),
      layout_grid_scale_mm(10.0f),
      current_layer(0),
      selected_row(-1),
      selected_col(-1)
{
    setWindowTitle(tr("Custom 3D Controller"));
    resize(1120, 720);
    SetupUI();
}

CustomControllerDialog::~CustomControllerDialog()
{
    shutting_down_ = true;

    if(color_refresh_timer)
    {
        color_refresh_timer->stop();
    }

    if(preview_dialog)
    {
        preview_dialog->blockSignals(true);
        preview_dialog->hide();
        preview_dialog->setParent(nullptr);
        delete preview_dialog;
        preview_dialog = nullptr;
    }

    RestoreAllIdentifiedLeds();

    if(layout_grid)
    {
        layout_grid->setParent(nullptr);
        layout_grid = nullptr;
    }

    delete ui;
    ui = nullptr;
}

void CustomControllerDialog::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);

    if(resource_manager)
    {
        std::vector<RGBController*>& controllers = resource_manager->GetRGBControllers();
        CustomControllerMapping::RebindAll(led_mappings, controllers);
        UpdateGridDisplay();
        UpdateCellInfo();
    }

    if(color_refresh_timer && !color_refresh_timer->isActive())
    {
        refresh_colors();
        color_refresh_timer->start();
    }
}

void CustomControllerDialog::hideEvent(QHideEvent* event)
{
    if(color_refresh_timer)
    {
        color_refresh_timer->stop();
    }

    QDialog::hideEvent(event);
}

void CustomControllerDialog::closeEvent(QCloseEvent* event)
{
    shutting_down_ = true;

    if(color_refresh_timer)
    {
        color_refresh_timer->stop();
    }

    if(preview_dialog)
    {
        preview_dialog->hide();
    }

    QDialog::closeEvent(event);
}

void CustomControllerDialog::SetupUI()
{
    if(!resource_manager)
    {
        return;
    }

    ui = new Ui::CustomControllerDialog;
    ui->setupUi(this);

    PluginUiApplyBoldLabel(ui->titleLabel);
    summary_label = ui->summaryLabel;
    PluginUiApplyBoldLabel(summary_label);
    PluginUiApplyMutedSecondaryLabel(ui->helpLabel->label());

    name_edit         = ui->nameEdit;
    width_spin        = ui->widthSpin;
    height_spin       = ui->heightSpin;
    depth_spin        = ui->depthSpin;
    leds_per_section_combo = ui->ledsPerSectionCombo;
    layer_depth_spin  = ui->layerDepthSpin;
    fit_layout_button  = ui->fitLayoutButton;
    reset_view_button_ = ui->resetViewButton;
    connect(width_spin, QOverload<int>::of(&QSpinBox::valueChanged), this, &CustomControllerDialog::dimensionChanged);
    connect(height_spin, QOverload<int>::of(&QSpinBox::valueChanged), this, &CustomControllerDialog::dimensionChanged);
    connect(depth_spin, QOverload<int>::of(&QSpinBox::valueChanged), this, &CustomControllerDialog::dimensionChanged);
    connect(leds_per_section_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int) { syncPreviewLayoutIfVisible(); });
    connect(layer_depth_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            &CustomControllerDialog::layerDepthChanged);
    connect(fit_layout_button, &QPushButton::clicked, this, &CustomControllerDialog::fitDeviceLayoutClicked);
    connect(reset_view_button_, &QPushButton::clicked, this, &CustomControllerDialog::resetGridViewClicked);

    EnsureDialogGridSizeArrays();

    PluginUiApplyBoldLabel(ui->devicesHeading);
    device_list = new CustomControllerDeviceList(ui->leftPanel);
    ui->deviceListLayout->addWidget(device_list, 1);
    connect(device_list, &CustomControllerDeviceList::selectionChanged, this,
            &CustomControllerDialog::sourceSelectionChanged);
    connect(device_list, &CustomControllerDeviceList::enableToggled, this,
            &CustomControllerDialog::sourceEnableToggled);

    identify_button = ui->identifyButton;
    connect(identify_button, &QPushButton::clicked, this, &CustomControllerDialog::identifySelectionClicked);

    clear_button            = ui->clearButton;
    copy_button             = ui->copyButton;
    cut_button              = ui->cutButton;
    paste_button            = ui->pasteButton;
    remove_from_grid_button = ui->removeFromGridButton;
    connect(clear_button, &QPushButton::clicked, this, &CustomControllerDialog::clearCellClicked);
    connect(copy_button, &QPushButton::clicked, this, &CustomControllerDialog::copySelectionClicked);
    connect(cut_button, &QPushButton::clicked, this, &CustomControllerDialog::cutSelectionClicked);
    connect(paste_button, &QPushButton::clicked, this, &CustomControllerDialog::pasteSelectionClicked);
    connect(remove_from_grid_button, &QPushButton::clicked, this, &CustomControllerDialog::removeAllLedsClicked);
    connect(ui->addLightBlockerButton, &QPushButton::clicked, this, &CustomControllerDialog::addLightBlockerClicked);

    QFrame* left_wrapped = PluginUiWrapInSettingsPanel(ui->leftPanel, 8);
    ui->mainSplitter->insertWidget(0, left_wrapped);

    PluginUiApplyBoldLabel(ui->canvasHeading);
    PluginUiApplyBoldLabel(ui->depthCaption);
    PluginUiApplyBoldLabel(ui->ledsPerClusterCaption);
    layer_tabs = ui->layerTabs;
    connect(layer_tabs, QOverload<int>::of(&QTabWidget::currentChanged), this, &CustomControllerDialog::layerTabChanged);
    connect(layer_tabs->tabBar(), &QTabBar::tabBarDoubleClicked, this, &CustomControllerDialog::layerTabDoubleClicked);
    layer_tabs->setToolTip(tr("Double-click a layer tab to rename it."));

    layout_grid = new CustomControllerLayoutGrid();
    layout_grid->setMinimumHeight(280);
    layout_grid->SetSelectionColor(PluginUiGridSelectionColor(layout_grid));
    connect(layout_grid, &CustomControllerLayoutGrid::cellClicked,
            this, &CustomControllerDialog::gridCellClicked);
    connect(layout_grid, &CustomControllerLayoutGrid::cellDoubleClicked,
            this, &CustomControllerDialog::gridCellDoubleClicked);
    connect(layout_grid, &CustomControllerLayoutGrid::selectionChanged,
            this, &CustomControllerDialog::gridSelectionChanged);
    connect(layout_grid, &CustomControllerLayoutGrid::contextMenuRequested,
            this, &CustomControllerDialog::gridContextMenuRequested);
    connect(layout_grid, &CustomControllerLayoutGrid::columnWidthChanged,
            this, &CustomControllerDialog::gridColumnWidthChanged);
    connect(layout_grid, &CustomControllerLayoutGrid::rowHeightChanged,
            this, &CustomControllerDialog::gridRowHeightChanged);
    connect(layout_grid, &CustomControllerLayoutGrid::columnHeaderClicked,
            this, &CustomControllerDialog::gridColumnHeaderClicked);
    connect(layout_grid, &CustomControllerLayoutGrid::rowHeaderClicked,
            this, &CustomControllerDialog::gridRowHeaderClicked);

    PluginUiApplyBoldLabel(ui->layerDepthCaption);

    connect(ui->addLayerButton, &QPushButton::clicked, this, &CustomControllerDialog::addLayerClicked);
    connect(ui->removeLayerButton, &QPushButton::clicked, this, &CustomControllerDialog::removeLayerClicked);
    RebuildLayerTabs();
    SyncLayerDepthSpinFromCurrentLayer();

    cell_info_label = ui->cellInfoLabel;
    PluginUiApplyMutedSecondaryLabel(cell_info_label);

    transform_group         = ui->transformGroup;
    rotate_90_button        = ui->rotate90Button;
    rotate_180_button       = ui->rotate180Button;
    rotate_270_button       = ui->rotate270Button;
    flip_horizontal_button  = ui->flipHorizontalButton;
    flip_vertical_button    = ui->flipVerticalButton;
    PluginUiApplyMutedSecondaryLabel(ui->transformHelp);
    connect(rotate_90_button, &QPushButton::clicked, this, &CustomControllerDialog::rotateGrid90);
    connect(rotate_180_button, &QPushButton::clicked, this, &CustomControllerDialog::rotateGrid180);
    connect(rotate_270_button, &QPushButton::clicked, this, &CustomControllerDialog::rotateGrid270);
    connect(flip_horizontal_button, &QPushButton::clicked, this, &CustomControllerDialog::flipGridHorizontal);
    connect(flip_vertical_button, &QPushButton::clicked, this, &CustomControllerDialog::flipGridVertical);

    QFrame* center_wrapped = PluginUiWrapInSettingsPanel(ui->centerPanel, 8);
    ui->mainSplitter->insertWidget(1, center_wrapped);
    ui->mainSplitter->setStretchFactor(0, 0);
    ui->mainSplitter->setStretchFactor(1, 1);
    ui->mainSplitter->setSizes({260, 760});

    save_button = ui->saveButton;
    PluginUiApplyPrimaryButton(save_button);
    connect(ui->previewButton, &QPushButton::clicked, this, &CustomControllerDialog::showPreview3dClicked);
    connect(save_button, &QPushButton::clicked, this, &CustomControllerDialog::saveClicked);
    connect(ui->cancelButton, &QPushButton::clicked, this, &QDialog::reject);

    device_list->rebuild(resource_manager, this);

    color_refresh_timer = new QTimer(this);
    color_refresh_timer->setTimerType(Qt::CoarseTimer);
    color_refresh_timer->setInterval(250);
    connect(color_refresh_timer, &QTimer::timeout, this, &CustomControllerDialog::refresh_colors);

    QShortcut* save_shortcut = new QShortcut(QKeySequence::Save, this);
    connect(save_shortcut, &QShortcut::activated, this, &CustomControllerDialog::saveClicked);
    QShortcut* escape_shortcut = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    connect(escape_shortcut, &QShortcut::activated, this, &QDialog::reject);
    QShortcut* delete_shortcut = new QShortcut(QKeySequence::Delete, this);
    connect(delete_shortcut, &QShortcut::activated, this, &CustomControllerDialog::clearCellClicked);
    QShortcut* copy_shortcut = new QShortcut(QKeySequence::Copy, this);
    connect(copy_shortcut, &QShortcut::activated, this, &CustomControllerDialog::copySelectionClicked);
    QShortcut* cut_shortcut = new QShortcut(QKeySequence::Cut, this);
    connect(cut_shortcut, &QShortcut::activated, this, &CustomControllerDialog::cutSelectionClicked);
    QShortcut* paste_shortcut = new QShortcut(QKeySequence::Paste, this);
    connect(paste_shortcut, &QShortcut::activated, this, &CustomControllerDialog::pasteSelectionClicked);

    if(paste_button)
    {
        paste_button->setEnabled(false);
    }

    UpdateGridDisplay();
    UpdateIdentifyButtonUi();
}

void CustomControllerDialog::addLayerClicked()
{
    if(!depth_spin)
    {
        return;
    }

    if(depth_spin->value() < depth_spin->maximum())
    {
        depth_spin->setValue(depth_spin->value() + 1);
    }
}

void CustomControllerDialog::removeLayerClicked()
{
    if(!depth_spin || depth_spin->value() <= 1)
    {
        return;
    }

    depth_spin->setValue(depth_spin->value() - 1);
}

void CustomControllerDialog::layerTabChanged(int index)
{
    if(index < 0)
    {
        return;
    }

    current_layer = index;
    AttachLayoutGridToLayerTab(index);
    SyncLayerDepthSpinFromCurrentLayer();
    UpdateGridDisplay();
    UpdateIdentifyButtonUi();
}

void CustomControllerDialog::layerTabDoubleClicked(int index)
{
    if(index < 0 || !layer_tabs)
    {
        return;
    }

    EnsureLayerNamesArray();
    if(index >= static_cast<int>(layer_names_.size()))
    {
        return;
    }

    const QString current = QString::fromStdString(layer_names_[static_cast<size_t>(index)]);
    bool ok             = false;
    const QString name  = QInputDialog::getText(this,
                                               tr("Rename layer"),
                                               tr("Layer name:"),
                                               QLineEdit::Normal,
                                               current,
                                               &ok);
    if(!ok)
    {
        return;
    }

    const QString trimmed = name.trimmed();
    if(trimmed.isEmpty())
    {
        return;
    }

    layer_names_[static_cast<size_t>(index)] = trimmed.toStdString();
    layer_tabs->setTabText(index, trimmed);
    UpdateSummaryLabel();
}

void CustomControllerDialog::dimensionChanged()
{
    const int new_width = width_spin->value();
    const int new_height = height_spin->value();
    const int new_depth = depth_spin->value();

    auto trim_out_of_bounds = [&](std::vector<GridLEDMapping>& mappings) -> size_t
    {
        size_t count = 0;
        for(std::vector<GridLEDMapping>::iterator it = mappings.begin(); it != mappings.end();)
        {
            if(it->x >= new_width || it->y >= new_height || it->z >= new_depth)
            {
                it = mappings.erase(it);
                count++;
            }
            else
            {
                ++it;
            }
        }
        return count;
    };

    size_t removed = trim_out_of_bounds(led_mappings);
    if(removed > 0)
    {
        QMessageBox::information(this, "Grid Resized",
            QString("%1 mapping(s) were outside the new grid and were removed.").arg(static_cast<qlonglong>(removed)));
    }

    const size_t blockers_before = light_blocker_cells_.size();
    TrimLightBlockerCells(new_width, new_height, new_depth);
    if(light_blocker_cells_.size() < blockers_before)
    {
        QMessageBox::information(this,
                                 tr("Grid resized"),
                                 tr("%1 light blocker cell(s) outside the new grid were removed.")
                                     .arg(static_cast<qlonglong>(blockers_before - light_blocker_cells_.size())));
    }

    if(current_layer >= new_depth)
    {
        current_layer = new_depth - 1;
        if(layer_tabs) layer_tabs->setCurrentIndex(current_layer);
    }
    const int prev_tab_count = layer_tabs->count();
    RebuildLayerTabs();
    if(new_depth > prev_tab_count)
    {
        current_layer = new_depth - 1;
        layer_tabs->setCurrentIndex(current_layer);
        AttachLayoutGridToLayerTab(current_layer);
    }
    EnsureDialogGridSizeArrays();
    SyncLayerDepthSpinFromCurrentLayer();
    EnsureLayerNamesArray();
    UpdateGridDisplay();
    UpdateCellInfo();
    refreshDeviceList();
}

void CustomControllerDialog::AttachLayoutGridToLayerTab(int layer_index)
{
    if(!layout_grid || !layer_tabs || layer_index < 0 || layer_index >= layer_tabs->count())
    {
        return;
    }

    layout_grid->setParent(nullptr);

    QWidget* tab = layer_tabs->widget(layer_index);
    if(!tab)
    {
        return;
    }

    QVBoxLayout* layout = qobject_cast<QVBoxLayout*>(tab->layout());
    if(!layout)
    {
        layout = NewCustomControllerLayerTabLayout(tab);
    }

    layout->addWidget(layout_grid, 1);
}

void CustomControllerDialog::UpdateLayerTabControls()
{
    if(ui->removeLayerButton)
    {
        ui->removeLayerButton->setEnabled(depth_spin && depth_spin->value() > 1);
    }
    if(ui->addLayerButton && depth_spin)
    {
        ui->addLayerButton->setEnabled(depth_spin->value() < depth_spin->maximum());
    }
}

void CustomControllerDialog::RebuildLayerTabs()
{
    if(!layer_tabs || !depth_spin)
    {
        return;
    }

    const int old_layer     = current_layer;
    const int current_count = layer_tabs->count();
    const int new_depth     = depth_spin->value();

    if(layout_grid)
    {
        layout_grid->setParent(nullptr);
    }

    if(new_depth > current_count)
    {
        for(int i = current_count; i < new_depth; i++)
        {
            QWidget* tab = new QWidget();
            NewCustomControllerLayerTabLayout(tab);
            layer_tabs->addTab(tab, LayerTabLabel(i));
        }
    }
    else if(new_depth < current_count)
    {
        while(layer_tabs->count() > new_depth)
        {
            const int last_idx = layer_tabs->count() - 1;
            QWidget* tab = layer_tabs->widget(last_idx);
            layer_tabs->removeTab(last_idx);
            delete tab;
        }
    }

    EnsureLayerNamesArray();
    for(int i = 0; i < layer_tabs->count(); ++i)
    {
        layer_tabs->setTabText(i, LayerTabLabel(i));
    }

    const int target_layer = (old_layer < new_depth) ? old_layer : std::max(0, new_depth - 1);
    current_layer = target_layer;

    if(layer_tabs->currentIndex() != target_layer)
    {
        layer_tabs->blockSignals(true);
        layer_tabs->setCurrentIndex(target_layer);
        layer_tabs->blockSignals(false);
    }

    AttachLayoutGridToLayerTab(target_layer);
    UpdateLayerTabControls();
}

std::vector<CustomControllerLightBlocker> CustomControllerDialog::GetLightBlockers() const
{
    std::vector<CustomControllerLightBlocker> blockers;
    blockers.reserve(light_blocker_cells_.size());
    for(const uint64_t key : light_blocker_cells_)
    {
        CustomControllerLightBlocker blocker{};
        DecodeGridCellKey3D(key, &blocker.x, &blocker.y, &blocker.z);
        blockers.push_back(blocker);
    }
    return blockers;
}

QString CustomControllerDialog::GetControllerName() const
{
    return name_edit->text();
}

int CustomControllerDialog::GetGridWidth() const
{
    return width_spin->value();
}

int CustomControllerDialog::GetGridHeight() const
{
    return height_spin->value();
}

int CustomControllerDialog::GetGridDepth() const
{
    return depth_spin->value();
}

float CustomControllerDialog::GetSpacingX() const
{
    EnsureDialogGridSizeArrays();
    if(!column_widths_mm_.empty())
    {
        return column_widths_mm_.front();
    }
    return CustomControllerGridLayoutMath::kDefaultCellSizeMm;
}

float CustomControllerDialog::GetSpacingY() const
{
    EnsureDialogGridSizeArrays();
    if(!row_heights_mm_.empty())
    {
        return row_heights_mm_.front();
    }
    return CustomControllerGridLayoutMath::kDefaultCellSizeMm;
}

float CustomControllerDialog::GetSpacingZ() const
{
    EnsureDialogGridSizeArrays();
    if(current_layer >= 0 && current_layer < static_cast<int>(layer_depths_mm_.size()))
    {
        return layer_depths_mm_[static_cast<size_t>(current_layer)];
    }
    if(!layer_depths_mm_.empty())
    {
        return layer_depths_mm_.front();
    }
    return CustomControllerGridLayoutMath::kDefaultCellSizeMm;
}

std::vector<float> CustomControllerDialog::GetColumnWidthsMm() const
{
    EnsureDialogGridSizeArrays();
    return column_widths_mm_;
}

std::vector<float> CustomControllerDialog::GetRowHeightsMm() const
{
    EnsureDialogGridSizeArrays();
    return row_heights_mm_;
}

std::vector<float> CustomControllerDialog::GetLayerDepthsMm() const
{
    EnsureDialogGridSizeArrays();
    return layer_depths_mm_;
}

std::vector<std::string> CustomControllerDialog::GetLayerNames() const
{
    EnsureLayerNamesArray();
    return layer_names_;
}

int CustomControllerDialog::GetLedsPerCluster() const
{
    if(!leds_per_section_combo)
    {
        return 1;
    }
    return (leds_per_section_combo->currentIndex() > 0) ? 3 : 1;
}

void CustomControllerDialog::EnsureDialogGridSizeArrays() const
{
    if(!width_spin || !height_spin || !depth_spin)
    {
        return;
    }

    const int grid_w = width_spin->value();
    const int grid_h = height_spin->value();
    const int grid_d = depth_spin->value();
    const float default_size = CustomControllerGridLayoutMath::kDefaultCellSizeMm;
    float new_layer_depth    = default_size;
    if(!layer_depths_mm_.empty())
    {
        new_layer_depth = layer_depths_mm_.back();
    }

    while(static_cast<int>(column_widths_mm_.size()) < grid_w)
    {
        column_widths_mm_.push_back(default_size);
    }
    while(static_cast<int>(column_widths_mm_.size()) > grid_w)
    {
        column_widths_mm_.pop_back();
    }

    while(static_cast<int>(row_heights_mm_.size()) < grid_h)
    {
        row_heights_mm_.push_back(default_size);
    }
    while(static_cast<int>(row_heights_mm_.size()) > grid_h)
    {
        row_heights_mm_.pop_back();
    }

    while(static_cast<int>(layer_depths_mm_.size()) < grid_d)
    {
        layer_depths_mm_.push_back(new_layer_depth);
    }
    while(static_cast<int>(layer_depths_mm_.size()) > grid_d)
    {
        layer_depths_mm_.pop_back();
    }
}

void CustomControllerDialog::EnsureLayerNamesArray() const
{
    if(!depth_spin)
    {
        return;
    }

    const int grid_d = depth_spin->value();
    while(static_cast<int>(layer_names_.size()) < grid_d)
    {
        layer_names_.push_back(tr("Layer %1").arg(layer_names_.size() + 1).toStdString());
    }
    while(static_cast<int>(layer_names_.size()) > grid_d)
    {
        layer_names_.pop_back();
    }
}

QString CustomControllerDialog::LayerTabLabel(int layer_index) const
{
    EnsureLayerNamesArray();
    if(layer_index >= 0 && layer_index < static_cast<int>(layer_names_.size()))
    {
        const QString name = QString::fromStdString(layer_names_[static_cast<size_t>(layer_index)]).trimmed();
        if(!name.isEmpty())
        {
            return name;
        }
    }
    return tr("Layer %1").arg(layer_index + 1);
}

QVector<float> CustomControllerDialog::ColumnWidthsQVector() const
{
    EnsureDialogGridSizeArrays();
    QVector<float> widths;
    widths.reserve(static_cast<int>(column_widths_mm_.size()));
    for(float width : column_widths_mm_)
    {
        widths.append(width);
    }
    return widths;
}

QVector<float> CustomControllerDialog::RowHeightsQVector() const
{
    EnsureDialogGridSizeArrays();
    QVector<float> heights;
    heights.reserve(static_cast<int>(row_heights_mm_.size()));
    for(float height : row_heights_mm_)
    {
        heights.append(height);
    }
    return heights;
}

void CustomControllerDialog::SyncLayerDepthSpinFromCurrentLayer() const
{
    if(!layer_depth_spin)
    {
        return;
    }

    EnsureDialogGridSizeArrays();
    float depth = CustomControllerGridLayoutMath::kDefaultCellSizeMm;
    if(current_layer >= 0 && current_layer < static_cast<int>(layer_depths_mm_.size()))
    {
        depth = layer_depths_mm_[static_cast<size_t>(current_layer)];
    }

    const QSignalBlocker blocker(layer_depth_spin);
    layer_depth_spin->setValue(depth);
}

void CustomControllerDialog::layerDepthChanged(double value_mm)
{
    EnsureDialogGridSizeArrays();
    if(current_layer < 0 || current_layer >= static_cast<int>(layer_depths_mm_.size()))
    {
        return;
    }

    layer_depths_mm_[static_cast<size_t>(current_layer)] = static_cast<float>(value_mm);
    syncPreviewLayoutIfVisible();
}

void CustomControllerDialog::gridColumnWidthChanged(int column, float width_mm)
{
    EnsureDialogGridSizeArrays();
    if(column < 0 || column >= static_cast<int>(column_widths_mm_.size()))
    {
        return;
    }

    column_widths_mm_[static_cast<size_t>(column)] = width_mm;
    syncPreviewLayoutIfVisible();
}

void CustomControllerDialog::gridRowHeightChanged(int row, float height_mm)
{
    EnsureDialogGridSizeArrays();
    if(row < 0 || row >= static_cast<int>(row_heights_mm_.size()))
    {
        return;
    }

    row_heights_mm_[static_cast<size_t>(row)] = height_mm;
    syncPreviewLayoutIfVisible();
}

ColorComboDelegate::ColorComboDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

void ColorComboDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                              const QModelIndex &index) const
{
    QStyledItemDelegate::paint(painter, option, index);

    if(index.data(Qt::DecorationRole).canConvert<QIcon>())
    {
        QIcon icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
        QRect icon_rect = option.rect;
        icon_rect.setWidth(20);
        icon_rect.adjust(4, 2, 0, -2);

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);

        QPixmap pixmap = icon.pixmap(icon_rect.size());
        painter->drawPixmap(icon_rect, pixmap);

        painter->restore();
    }
}

QSize ColorComboDelegate::sizeHint(const QStyleOptionViewItem &option,
                                  const QModelIndex &index) const
{
    QSize size = QStyledItemDelegate::sizeHint(option, index);
    size.setHeight(qMax(size.height(), 24));
    return size;
}

